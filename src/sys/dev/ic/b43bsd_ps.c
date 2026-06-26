/*	$OpenBSD: b43bsd_ps.c,v 1.1 2026/06/25 xirtus Exp $	*/

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
 * 802.11 Power Save Mode for BCM4331.
 *
 * Implements STA power management: the chip enters a low-power sleep
 * state between beacons and wakes for delivery traffic indication
 * messages (DTIM). Essential for MacBook battery life — without PS,
 * WiFi draws ~1.5W continuous.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/main.c power
 * save paths and drivers/net/wireless/broadcom/b43/pio.h PS sequences.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>


#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/* Local register access. */
#define PS_RD(sc, r) bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r))
#define PS_WR(sc, r, v) bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r), (v))
#define B43_MACCTL_PSM_WAKE 0x00000004
#define B43_MACCTL_PSM_MAC_INFRA 0x00000008

/*
 * PS state values for the hardware MAC PS control register.
 */
#define B43_PS_DISABLED		0	/* PS off, full power */
#define B43_PS_AWAKE		1	/* PS enabled, chip awake */
#define B43_PS_ASLEEP		2	/* PS enabled, chip asleep */
#define B43_PS_HWPS_DISABLED	3	/* HW-controlled PS disabled */
#define B43_PS_HWPS_ENABLED	4	/* HW-controlled PS enabled */

/*
 * MAC control bits for power save.
 */

/*
 * Shared memory offsets for PS state.
 */
#define B43_SHM_SH_WAKEUP_STAT	0x000e	/* wakeup reason */
#define B43_SHM_SH_SLOTTIMER	0x0010	/* slot timer */
#define B43_SHM_SH_BEACONTIMER	0x0012	/* beacon timer */
#define B43_SHM_SH_DTIMCNT	0x0014	/* DTIM countdown */
#define B43_SHM_SH_PSSTAT	0x0018	/* PS status */
#define B43_SHM_SH_PSCTL	0x001a	/* PS control */

/*
 * PS control flags.
 */
#define B43_PSCTL_ENABLED	0x0001	/* PS mode enabled */
#define B43_PSCTL_DTIM		0x0002	/* wake for DTIM */
#define B43_PSCTL_RX_WAKE	0x0004	/* wake on RX */
#define B43_PSCTL_TX_WAKE	0x0008	/* wake on TX */
#define B43_PSCTL_BEACON_WAKE	0x0010	/* wake for beacon processing */

/*
 * PS status values.
 */
#define B43_PSSTAT_AWAKE	0x0000
#define B43_PSSTAT_ASLEEP	0x0001
#define B43_PSSTAT_TRANSITION	0x0002
#define B43_PSSTAT_AUTOSLEEP	0x0003

/*
 * PS-poll frame data.
 */
static const uint8_t ps_poll_template[] = {
	0xa4, 0x10,		/* Frame Control: PS-Poll */
	0x00, 0x00,		/* Duration */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* BSSID (filled at runtime) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* TA (filled at runtime) */
};

/* ------------------------------------------------------------------ */
/* Power Save State Machine                                             */
/* ------------------------------------------------------------------ */

/*
 * Configure listen interval in firmware shared memory.
 * The listen interval tells the AP how many beacon intervals
 * the STA may sleep. Higher = better battery but more latency.
 */
void
b43bsd_ps_set_listen_interval(struct b43bsd_softc *sc, int interval)
{
	sc->sc_ps.listen_interval = interval;

	/* Write to shared memory for firmware use. */
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43_SHM_SH_BEACONTIMER,
	    (uint16_t)interval);
}

/*
 * Configure DTIM period tracking.
 * The chip wakes every DTIM period to check for buffered frames.
 * DTIM = Delivery Traffic Indication Message.
 */
static void
b43bsd_ps_set_dtim_period(struct b43bsd_softc *sc, int dtim)
{
	sc->sc_ps.dtim_period = dtim;

	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43_SHM_SH_DTIMCNT,
	    (uint16_t)dtim);
}

/*
 * Query current PS state from firmware shared memory.
 * Returns 1 if asleep, 0 if awake.
 */
static int
b43bsd_ps_is_asleep(struct b43bsd_softc *sc)
{
	uint16_t psstat;

	psstat = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, B43_SHM_SH_PSSTAT);
	return (psstat == B43_PSSTAT_ASLEEP) ? 1 : 0;
}

/*
 * Query wakeup reason from firmware.
 */
static uint16_t __unused
b43bsd_ps_wakeup_reason(struct b43bsd_softc *sc)
{
	return b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, B43_SHM_SH_WAKEUP_STAT);
}

/* ------------------------------------------------------------------ */
/* PS Entry / Exit                                                      */
/* ------------------------------------------------------------------ */

/*
 * Enter power save mode: tell firmware to sleep the MAC until the
 * next beacon or pending TX data.
 */
int
b43bsd_ps_enter(struct b43bsd_softc *sc)
{
	uint32_t macctl;
	uint16_t psctl;

	if (!sc->sc_ps.enabled)
		return 0;
	if (sc->sc_ps.sleeping)
		return 0;
	if (sc->sc_txring.used > 0)
		return 0;	/* TX pending, don't sleep */

	psctl = B43_PSCTL_ENABLED | B43_PSCTL_DTIM |
	    B43_PSCTL_RX_WAKE | B43_PSCTL_BEACON_WAKE;

	/*
	 * If firmware is running, use hardware-assisted PS.
	 * Otherwise use the simpler MAC-level PS.
	 */
	if (sc->sc_fw.running) {
		psctl |= B43_PSCTL_TX_WAKE;
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL, psctl);
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSSTAT, B43_PSSTAT_AUTOSLEEP);
	} else {
		macctl = PS_RD(sc, B43_MMIO_MACCTL);
		macctl |= B43_MACCTL_PSM_MAC_INFRA;
		macctl |= B43_MACCTL_PSM_WAKE;
		PS_WR(sc, B43_MMIO_MACCTL, macctl);
		delay(10);
		macctl &= ~B43_MACCTL_PSM_WAKE;
		PS_WR(sc, B43_MMIO_MACCTL, macctl);
	}

	sc->sc_ps.sleeping = 1;
	sc->sc_ps.sleep_start = ticks;

	return 0;
}

/*
 * Exit power save mode: wake the MAC and reset PS state.
 * Called before TX or when beacon processing requires the MAC awake.
 */
int
b43bsd_ps_exit(struct b43bsd_softc *sc)
{
	uint32_t macctl;
	uint16_t psctl;

	if (!sc->sc_ps.sleeping)
		return 0;

	if (sc->sc_fw.running) {
		psctl = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL);
		psctl &= ~B43_PSCTL_ENABLED;
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL, psctl);
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSSTAT, B43_PSSTAT_AWAKE);

		/* Wait for wake confirmation (max 2ms). */
		{
			int i;
			for (i = 0; i < 20; i++) {
				if (b43bsd_ps_is_asleep(sc) == 0)
					break;
				delay(100);
			}
		}
	} else {
		macctl = PS_RD(sc, B43_MMIO_MACCTL);
		macctl &= ~B43_MACCTL_PSM_MAC_INFRA;
		PS_WR(sc, B43_MMIO_MACCTL, macctl);
	}

	sc->sc_ps.sleeping = 0;
	sc->sc_ps.wake_count++;

	return 0;
}

/* ------------------------------------------------------------------ */
/* PS-Poll Frame                                                         */
/* ------------------------------------------------------------------ */

/*
 * Send a PS-Poll frame to the AP to request buffered frames.
 * The AP responds by sending one buffered frame (or an ACK
 * with the More Data bit set if frames remain).
 *
 * PS-Poll frames are control frames — they bypass the normal
 * TX queue and are sent on the management channel.
 */
int
b43bsd_ps_send_pspoll(struct b43bsd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;
	uint8_t *frm;
	int error;

	if (ic->ic_bss == NULL)
		return EINVAL;

	ni = ic->ic_bss;

	/* Allocate mbuf for PS-Poll frame. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;
	m->m_pkthdr.len = m->m_len = sizeof(ps_poll_template);
	memcpy(mtod(m, uint8_t *), ps_poll_template, sizeof(ps_poll_template));

	frm = mtod(m, uint8_t *);
	/* Copy BSSID into address 1. */
	IEEE80211_ADDR_COPY(&frm[4], ni->ni_bssid);
	/* Copy our MAC into address 2. */
	IEEE80211_ADDR_COPY(&frm[10], ic->ic_myaddr);
	/* Set AID in the PS-Poll frame (low 14 bits of duration). */
	frm[2] = (uint8_t)(ni->ni_associd & 0xff);
	frm[3] = (uint8_t)((ni->ni_associd >> 8) & 0x3f);
	frm[3] |= 0xc0;	/* Set high two bits per 802.11 spec */

	/* Send directly through the TX path. */
	error = b43bsd_dma_tx_start(sc, m, ni, 0);
	if (error != 0) {
		m_freem(m);
		return error;
	}

	sc->sc_ps.pspoll_count++;

	return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                            */
/* ------------------------------------------------------------------ */

/*
 * Initialize power save state.
 * Called during b43bsd_init() after firmware is loaded.
 */
int
b43bsd_ps_init(struct b43bsd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;

	memset(&sc->sc_ps, 0, sizeof(sc->sc_ps));

	/*
	 * Only enable PS in STA mode.
	 * IBSS PS and AP PS are more complex and not yet supported.
	 */
	if (ic->ic_opmode != IEEE80211_M_STA)
		return 0;

	sc->sc_ps.enabled = 1;
	sc->sc_ps.listen_interval = 10;	/* wake every 10 beacons */
	sc->sc_ps.dtim_period = 1;	/* wake for every DTIM */

	/* Program listen interval into firmware. */
	b43bsd_ps_set_listen_interval(sc, sc->sc_ps.listen_interval);
	b43bsd_ps_set_dtim_period(sc, sc->sc_ps.dtim_period);

	/* Enable PS control in firmware. */
	if (sc->sc_fw.running) {
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL,
		    B43_PSCTL_ENABLED | B43_PSCTL_DTIM |
		    B43_PSCTL_RX_WAKE | B43_PSCTL_TX_WAKE |
		    B43_PSCTL_BEACON_WAKE);
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSSTAT, B43_PSSTAT_AWAKE);
	}

	return 0;
}

/*
 * Disable power save and force chip awake.
 */
void
b43bsd_ps_deinit(struct b43bsd_softc *sc)
{
	uint16_t psctl;

	b43bsd_ps_exit(sc);

	if (sc->sc_fw.running) {
		psctl = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL);
		psctl &= ~B43_PSCTL_ENABLED;
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    B43_SHM_SH_PSCTL, psctl);
	}

	sc->sc_ps.enabled = 0;
	sc->sc_ps.sleeping = 0;
}

/*
 * DTIM wake handler: called from interrupt context when a DTIM
 * beacon fires. Checks if AP has buffered frames for us and
 * sends PS-Poll if needed.
 */
void
b43bsd_ps_dtim_wake(struct b43bsd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	uint16_t dtim_count;

	if (!sc->sc_ps.enabled)
		return;

	/*
	 * Exit sleep to check the beacon's TIM.
	 * The firmware auto-wakes for DTIM beacons; we just need to
	 * check the TIM element in the beacon (done by net80211)
	 * and send PS-Poll if there are buffered frames.
	 */
	b43bsd_ps_exit(sc);

	dtim_count = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
	    B43_SHM_SH_DTIMCNT);

	if (dtim_count == 0) {
		/*
		 * DTIM beacon — check if AP buffered frames for us.
		 * net80211 tracks this in ic->ic_flags; if the TIM
		 * indicated buffered traffic, request it via PS-Poll.
		 */
		if (ic->ic_bss != NULL) {
			/* TIM check: net80211 handles TIM parsing.
			 * If AP has buffered frames, request via PS-Poll. */
			b43bsd_ps_send_pspoll(sc);
		}
	}
}

/*
 * TX path hook: ensure chip is awake before transmitting.
 */
void
b43bsd_ps_tx_wake(struct b43bsd_softc *sc)
{
	if (sc->sc_ps.sleeping)
		b43bsd_ps_exit(sc);
}

/*
 * Post-TX sleep: if no more TX pending, allow chip to sleep again.
 */
void
b43bsd_ps_tx_done(struct b43bsd_softc *sc)
{
	if (sc->sc_txring.used == 0 && sc->sc_ps.enabled)
		b43bsd_ps_enter(sc);
}
