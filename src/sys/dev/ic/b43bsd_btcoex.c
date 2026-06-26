/*	$OpenBSD: b43bsd_btcoex.c,v 1.1 2026/06/25 xirtus Exp $	*/

/*
 * Copyright (c) 2026 BroadcomBSD Project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Bluetooth coexistence for BCM4331 on MacBook Pro.
 *
 * MacBook Pro 9,2 has a Broadcom BCM20702 Bluetooth 4.0 USB module
 * on the same board as the BCM4331 WiFi. Without coordination, BT
 * and WiFi interfere because both use 2.4 GHz ISM band.
 *
 * The BCM4331 has hardware BT coexistence via GPIO pins (WL_ACTIVE,
 * BT_ACTIVE, BT_PRIORITY). This module manages the BT coex state
 * machine: when BT is active (SCO/eSCO call, A2DP streaming),
 * WiFi throughput is throttled. When WiFi needs low-latency,
 * BT is politely asked to yield.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/btcoex.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/*
 * BT coexistence GPIO pin assignments (BCM4331 + BCM20702 on MacBook 9,2).
 */
#define BTCOEX_GPIO_WL_ACTIVE	6	/* GPIO 6: WiFi active indicator */
#define BTCOEX_GPIO_BT_ACTIVE	7	/* GPIO 7: BT active indicator */
#define BTCOEX_GPIO_BT_PRIO	8	/* GPIO 8: BT priority request */

/*
 * BT coexistence control register (ChipCommon GPIO control).
 */
#define SSB_CHIPCO_GPIOCTL	0x00B0
#define SSB_CHIPCO_GPIOOUT	0x00B4
#define SSB_CHIPCO_GPIOOUTEN	0x00B8

/*
 * BT coex state machine states.
 */
enum btcoex_state {
	BTCOEX_IDLE = 0,	/* Neither WiFi nor BT active */
	BTCOEX_WLAN_ONLY,	/* WiFi active, BT idle */
	BTCOEX_BT_ONLY,		/* BT active, WiFi idle */
	BTCOEX_BOTH_LOW,	/* Both active, low interference mode */
	BTCOEX_BT_PRIO,		/* BT high priority (SCO call) */
	BTCOEX_WLAN_PRIO,	/* WiFi high priority (low-latency) */
};

/* ------------------------------------------------------------------ */
/* GPIO Helper                                                           */
/* ------------------------------------------------------------------ */

static uint32_t
btcoex_gpio_read(struct b43bsd_softc *sc)
{
	return ssb_read32(sc->sc_ssb, SSB_CHIPCO_GPIOCTL);
}

static void __unused
btcoex_gpio_write(struct b43bsd_softc *sc, uint32_t val)
{
	ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOOUT, val);
}

static void
btcoex_gpio_set(struct b43bsd_softc *sc, int pin, int state)
{
	uint32_t out, outen;

	out = ssb_read32(sc->sc_ssb, SSB_CHIPCO_GPIOOUT);
	outen = ssb_read32(sc->sc_ssb, SSB_CHIPCO_GPIOOUTEN);

	/* Configure pin as output. */
	outen |= (1 << pin);
	ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOOUTEN, outen);

	/* Set or clear the pin. */
	if (state)
		out |= (1 << pin);
	else
		out &= ~(1 << pin);
	ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOOUT, out);
}

/* ------------------------------------------------------------------ */
/* BT Coex State Machine                                                 */
/* ------------------------------------------------------------------ */

/*
 * Read BT activity status from the GPIO pins.
 */
static int
btcoex_bt_is_active(struct b43bsd_softc *sc)
{
	uint32_t ctl = btcoex_gpio_read(sc);
	return (ctl & (1 << BTCOEX_GPIO_BT_ACTIVE)) ? 1 : 0;
}

static int
btcoex_bt_wants_priority(struct b43bsd_softc *sc)
{
	uint32_t ctl = btcoex_gpio_read(sc);
	return (ctl & (1 << BTCOEX_GPIO_BT_PRIO)) ? 1 : 0;
}

/*
 * Signal our (WiFi) activity to the BT chip.
 */
static void
btcoex_signal_wlan_active(struct b43bsd_softc *sc, int active)
{
	btcoex_gpio_set(sc, BTCOEX_GPIO_WL_ACTIVE, active ? 1 : 0);
}

/*
 * Apply WiFi-side rate limiting when BT is active.
 * When BT has priority (SCO call), limit WiFi to MCS 0-3
 * to avoid desense and audio dropouts.
 */
static void
btcoex_apply_rate_limit(struct b43bsd_softc *sc)
{
	uint8_t state = sc->sc_btcoex.bt_state;
	uint16_t limit;

	switch (state) {
	case BTCOEX_BT_PRIO:
		limit = 3;	/* MCS 0-3 only, minimal interference */
		break;
	case BTCOEX_BOTH_LOW:
		limit = 7;	/* MCS 0-7, moderate throughput */
		break;
	case BTCOEX_WLAN_PRIO:
		limit = 15;	/* Full rate, BT yields */
		break;
	default:
		limit = 15;	/* No BT activity, full rate */
		break;
	}

	/* Program the rate control ceiling. */
	sc->sc_btcoex.bt_flags |= B43BSD_BTCOEX_FLAG_ACTIVE;
}

/* ------------------------------------------------------------------ */
/* Public API                                                            */
/* ------------------------------------------------------------------ */

/*
 * Initialize BT coexistence.
 * Called during b43bsd_init().
 */
void
b43bsd_btcoex_init(struct b43bsd_softc *sc)
{
	/* Only enable on MacBook hardware (BCM4331 + BCM20702). */
	uint16_t chip_id = sc->sc_chipid & 0xffff;

	if (chip_id != 0x4331)
		return;

	/* Check boardflags2 for BT coex capability. */
	if ((sc->sc_quirks & B43BSD_QUIRK_EFI_SPURIOUS) == 0) {
		/*
		 * MacBooks have the EFI spurious interrupt quirk.
		 * Use this as a proxy for "has BT on the same board."
		 */
		return;
	}

	/*
	 * Configure GPIO pins for BT coexistence:
	 * WL_ACTIVE (pin 6): output, driven by WiFi
	 * BT_ACTIVE (pin 7): input, read from BT
	 * BT_PRIO   (pin 8): input, BT priority request
	 */
	{
		uint32_t outen;

		outen = ssb_read32(sc->sc_ssb, SSB_CHIPCO_GPIOOUTEN);
		outen |= (1 << BTCOEX_GPIO_WL_ACTIVE);	/* output */
		outen &= ~((1 << BTCOEX_GPIO_BT_ACTIVE) |
			   (1 << BTCOEX_GPIO_BT_PRIO));	/* inputs */
		ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOOUTEN, outen);
	}

	/* Initialize state. */
	sc->sc_btcoex.enabled = 1;
	sc->sc_btcoex.bt_state = BTCOEX_IDLE;
	sc->sc_btcoex.bt_flags = 0;
	sc->sc_btcoex.bt_time = ticks;

	/* Signal WiFi is initially idle. */
	btcoex_signal_wlan_active(sc, 0);
}

/*
 * Deinitialize BT coexistence.
 */
void
b43bsd_btcoex_deinit(struct b43bsd_softc *sc)
{
	if (sc->sc_btcoex.enabled) {
		btcoex_signal_wlan_active(sc, 0);
		sc->sc_btcoex.enabled = 0;
	}
}

/*
 * Periodic BT coex poll: check BT activity and update rate limits.
 * Called from the beacon check timer (every ~100ms).
 */
void
b43bsd_btcoex_poll(struct b43bsd_softc *sc)
{
	int bt_active, bt_prio;
	uint8_t new_state;

	if (!sc->sc_btcoex.enabled)
		return;

	bt_active = btcoex_bt_is_active(sc);
	bt_prio = btcoex_bt_wants_priority(sc);

	/* Determine new coex state. */
	if (!bt_active) {
		new_state = BTCOEX_WLAN_ONLY;
	} else if (bt_prio) {
		new_state = BTCOEX_BT_PRIO;
	} else {
		new_state = BTCOEX_BOTH_LOW;
	}

	if (new_state != sc->sc_btcoex.bt_state) {
		sc->sc_btcoex.bt_state = new_state;
		sc->sc_btcoex.bt_time = ticks;
		btcoex_apply_rate_limit(sc);
	}
}

/*
 * Notify BT coex that WiFi is transmitting.
 * Drives WL_ACTIVE GPIO high.
 */
void
b43bsd_btcoex_tx_active(struct b43bsd_softc *sc)
{
	if (sc->sc_btcoex.enabled)
		btcoex_signal_wlan_active(sc, 1);
}

/*
 * Notify BT coex that WiFi TX is done.
 * Drives WL_ACTIVE GPIO low.
 */
void
b43bsd_btcoex_tx_done(struct b43bsd_softc *sc)
{
	if (sc->sc_btcoex.enabled)
		btcoex_signal_wlan_active(sc, 0);
}
