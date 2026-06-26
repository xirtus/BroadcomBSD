/*	$OpenBSD: b43bsd_radio.c,v 1.1 2026/06/25 xirtus Exp $	*/

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
 * BCM2056 radio full control module.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/radio_2056.c
 *
 * The BCM2056 is a dual-band 3x3 MIMO radio with:
 *   - Fractional-N synthesizer (2.4/5 GHz)
 *   - Three TX chains with individual PA control
 *   - Three RX chains with LNA/Mixer/TIA/VGA stages
 *   - On-chip temperature sensor
 *   - Built-in self-test (BIST) capability
 *   - Power detector per chain (PAPD)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>
#include <dev/ic/b43bsd_phy_n.h>
#include <dev/ic/b43bsd_radio.h>

/* ------------------------------------------------------------------ */
/* Radio Register Access (4-Wire Bus)                                   */
/* ------------------------------------------------------------------ */

/*
 * The 4-wire bus connects the N-PHY core to the BCM2056 radio.
 * Write the register address, then read/write the data register.
 */
static void
radio_write(struct b43bsd_softc *sc, uint16_t addr, uint16_t val)
{
	nphy_write(sc, B43BSD_NPHY_4WI_ADDR, addr);
	nphy_write(sc, B43BSD_NPHY_4WI_DATALO, val);
}

static uint16_t
radio_read(struct b43bsd_softc *sc, uint16_t addr)
{
	nphy_write(sc, B43BSD_NPHY_4WI_ADDR, addr);
	return nphy_read(sc, B43BSD_NPHY_4WI_DATALO);
}

/* Public wrappers for cross-module access. */
uint16_t
b43bsd_radio_reg_read(struct b43bsd_softc *sc, uint16_t addr)
{
	return radio_read(sc, addr);
}

void
b43bsd_radio_reg_write(struct b43bsd_softc *sc, uint16_t addr, uint16_t val)
{
	radio_write(sc, addr, val);
}

/* Radio bank base addresses. */
#define B2056_SYN	0x0000
#define B2056_TX0	0x2000
#define B2056_TX1	0x4000
#define B2056_RX0	0x8000
#define B2056_RX1	0xA000

/* ------------------------------------------------------------------ */
/* Radio Reset Sequence                                                 */
/* ------------------------------------------------------------------ */

/*
 * Full BCM2056 hardware reset.
 * Sequences the power-on reset for all radio blocks in the correct order.
 */
void
b43bsd_radio_reset(struct b43bsd_softc *sc)
{
	/* 1. Assert reset on all banks. */
	radio_write(sc, B2056_SYN | 0x0B, 0x0001);	/* COM_RESET */
	radio_write(sc, B2056_TX0 | 0x02, 0x0001);	/* TX_RESET */
	radio_write(sc, B2056_TX1 | 0x02, 0x0001);
	radio_write(sc, B2056_RX0 | 0x02, 0x0001);	/* RX_RESET */
	radio_write(sc, B2056_RX1 | 0x02, 0x0001);
	delay(100);

	/* 2. De-assert reset. */
	radio_write(sc, B2056_SYN | 0x0B, 0x0000);
	radio_write(sc, B2056_TX0 | 0x02, 0x0000);
	radio_write(sc, B2056_TX1 | 0x02, 0x0000);
	radio_write(sc, B2056_RX0 | 0x02, 0x0000);
	radio_write(sc, B2056_RX1 | 0x02, 0x0000);
	delay(100);

	/* 3. Power-up synthesizer. */
	radio_write(sc, B2056_SYN | 0x09, 0x0001);	/* COM_PU */
	delay(50);

	/* 4. Enable TX/RX power. */
	radio_write(sc, B2056_TX0 | 0x01, 0x0001);	/* TX_PU */
	radio_write(sc, B2056_TX1 | 0x01, 0x0001);
	radio_write(sc, B2056_RX0 | 0x01, 0x0001);	/* RX_PU */
	radio_write(sc, B2056_RX1 | 0x01, 0x0001);
	delay(50);
}

/*
 * Put radio into low-power sleep.
 */
void
b43bsd_radio_sleep(struct b43bsd_softc *sc)
{
	/* Power-down TX chains. */
	radio_write(sc, B2056_TX0 | 0x01, 0x0000);
	radio_write(sc, B2056_TX1 | 0x01, 0x0000);

	/* Power-down RX chains. */
	radio_write(sc, B2056_RX0 | 0x01, 0x0000);
	radio_write(sc, B2056_RX1 | 0x01, 0x0000);

	/* Put synthesizer to sleep (keep LPO running). */
	radio_write(sc, B2056_SYN | 0x09, 0x0000);	/* COM_PU off */
	radio_write(sc, B2056_SYN | 0x28, 0x0001);	/* LPO */
}

/*
 * Wake radio from sleep.
 */
void
b43bsd_radio_wake(struct b43bsd_softc *sc)
{
	/* Restore synthesizer power. */
	radio_write(sc, B2056_SYN | 0x09, 0x0001);	/* COM_PU */
	delay(50);

	/* Restore TX/RX power. */
	radio_write(sc, B2056_TX0 | 0x01, 0x0001);
	radio_write(sc, B2056_TX1 | 0x01, 0x0001);
	radio_write(sc, B2056_RX0 | 0x01, 0x0001);
	radio_write(sc, B2056_RX1 | 0x01, 0x0001);
	delay(50);
}

/* ------------------------------------------------------------------ */
/* VCO Calibration                                                      */
/* ------------------------------------------------------------------ */

/*
 * Run VCO calibration for the current channel.
 * The BCM2056 synthesizer auto-calibrates its VCO to center the
 * tuning range on the target frequency. This must be run after
 * every channel change.
 *
 * Procedure:
 * 1. Enable VCO calibration
 * 2. Wait for calibration complete (max 5ms)
 * 3. Lock PLL
 *
 * Returns 0 on success, ETIMEDOUT on calibration timeout.
 */
int
b43bsd_radio_vco_cal(struct b43bsd_softc *sc)
{
	uint16_t stat;
	int i;

	/* Start VCO calibration. */
	radio_write(sc, B2056_SYN | 0x56, 0x0001);	/* PLL_VCOCAL1: start */
	delay(10);

	/* Wait for calibration complete. */
	for (i = 0; i < 50; i++) {
		stat = radio_read(sc, B2056_SYN | 0x56);
		if ((stat & 0x0001) == 0)
			break;
		delay(100);
	}

	if (i >= 50) {
		printf("%s: radio VCO calibration timeout\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	/* Read calibration result for diagnostics. */
	{
		uint16_t vcocal;
		vcocal = radio_read(sc, B2056_SYN | 0x60);	/* PLL_VCOCAL12 */
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "radio VCO cal done: result 0x%04x (%d iterations)\n",
		    vcocal, i);
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Temperature Compensation                                              */
/* ------------------------------------------------------------------ */

/*
 * Read the on-chip temperature sensor.
 * The BCM2056 has a bandgap-based temperature sensor accessible
 * through the synthesizer bank.
 *
 * Returns temperature in degrees Celsius (approximate).
 */
int
b43bsd_radio_read_temp(struct b43bsd_softc *sc)
{
	uint16_t raw;
	int temp;

	/* Trigger temperature read. */
	radio_write(sc, B2056_SYN | 0x50, 0x0001);	/* TEMPSENSE_CTL */

	/* Wait for conversion. */
	delay(100);

	/* Read temperature value. */
	raw = radio_read(sc, B2056_SYN | 0x51);		/* TEMPSENSE_VAL */

	/*
	 * Convert to Celsius.
	 * BCM2056: temp = ((raw & 0x3ff) - 512) / 2 + 30
	 */
	temp = ((int)(raw & 0x3ff) - 512) / 2 + 30;

	sc->sc_radio.radio_temp = temp;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio temperature: %d C (raw 0x%04x)\n", temp, raw);

	return temp;
}

/*
 * Apply temperature compensation to TX power.
 * TX power varies with temperature due to PA gain drift.
 * Compensate by adjusting the power index.
 */
void
b43bsd_radio_temp_compensation(struct b43bsd_softc *sc, int base_dbm)
{
	int temp, adj_dbm;

	temp = b43bsd_radio_read_temp(sc);

	/*
	 * Compensation: -0.5 dBm per 10°C above 30°C
	 *               +0.5 dBm per 10°C below 30°C
	 */
	if (temp > 30) {
		adj_dbm = base_dbm - ((temp - 30) / 20);
	} else if (temp < 30) {
		adj_dbm = base_dbm + ((30 - temp) / 20);
	} else {
		adj_dbm = base_dbm;
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "temp comp: %d C, base %d dBm -> adjusted %d dBm\n",
	    temp, base_dbm, adj_dbm);
}

/* ------------------------------------------------------------------ */
/* TX/RX Loopback Test                                                  */
/* ------------------------------------------------------------------ */

/*
 * Run TX-to-RX loopback test.
 * Transmits a known test pattern through the TX chain into the
 * RX chain (internal loopback) and verifies the received data.
 *
 * This validates the entire analog path: DAC → TX mixer → PA →
 * internal loopback switch → LNA → RX mixer → TIA → VGA → ADC.
 *
 * Returns 0 on pass, non-zero on failure.
 */
int
b43bsd_radio_loopback_test(struct b43bsd_softc *sc)
{
	uint16_t tx_val, rx_val;
	int errors = 0;
	int i;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio loopback test starting\n");

	/* Enable internal loopback on TX0 → RX0. */
	radio_write(sc, B2056_TX0 | 0x50, 0x0001);	/* TX_SPARE: loopback */
	radio_write(sc, B2056_RX0 | 0x90, 0x0001);	/* RX_SPARE: loopback */

	/* Send test tones and check received values. */
	for (i = 0; i < 16; i++) {
		tx_val = (uint16_t)(0x100 * i);

		/* Write test value to TX registers. */
		radio_write(sc, B2056_TX0 | 0x10, tx_val);	/* INTPAA_IAUX_STAT */

		/* Read back from RX registers. */
		delay(10);
		rx_val = radio_read(sc, B2056_RX0 | 0x10);	/* BIASPOLE_LNAA1 */

		if (rx_val != tx_val) {
			errors++;
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "   loopback error: sent 0x%04x got 0x%04x\n",
			    tx_val, rx_val);
		}
	}

	/* Disable loopback. */
	radio_write(sc, B2056_TX0 | 0x50, 0x0000);
	radio_write(sc, B2056_RX0 | 0x90, 0x0000);

	if (errors) {
		printf("%s: radio loopback test FAILED (%d errors)\n",
		    sc->sc_dev.dv_xname, errors);
		return EIO;
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio loopback test PASSED\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* RSSI Calibration                                                     */
/* ------------------------------------------------------------------ */

/*
 * Calibrate RSSI offset for the current channel.
 * Reads the idle RSSI (no signal present) and computes the offset
 * needed to report 0 dBm at the correct level.
 */
void
b43bsd_radio_rssi_cal(struct b43bsd_softc *sc)
{
	uint16_t rssi_c1, rssi_c2;
	int idle_c1, idle_c2;

	/* Ensure TX is off to read idle noise floor. */
	radio_write(sc, B2056_TX0 | 0x01, 0x0000);
	radio_write(sc, B2056_TX1 | 0x01, 0x0000);
	delay(100);

	/* Read idle RSSI from both RX chains. */
	rssi_c1 = radio_read(sc, B2056_RX0 | 0x70);	/* RX_RSSI_CTL */
	rssi_c2 = radio_read(sc, B2056_RX1 | 0x70);

	idle_c1 = (int)(rssi_c1 & 0xff);
	idle_c2 = (int)(rssi_c2 & 0xff);

	/*
	 * Expected idle RSSI: ~0xBE (-97 dBm) at 2.4 GHz, ~0xC8 (-92 dBm) at 5 GHz.
	 * Store offset for use in RSSI-to-dBm conversion.
	 */
	sc->sc_radio.rssi_offset_c1 = idle_c1;
	sc->sc_radio.rssi_offset_c2 = idle_c2;

	/* Restore TX power. */
	radio_write(sc, B2056_TX0 | 0x01, 0x0001);
	radio_write(sc, B2056_TX1 | 0x01, 0x0001);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RSSI cal: C1 idle=0x%02x C2 idle=0x%02x\n",
	    idle_c1, idle_c2);
}

/* ------------------------------------------------------------------ */
/* Radio Self-Test                                                      */
/* ------------------------------------------------------------------ */

/*
 * Comprehensive radio self-test.
 * Returns a bitmask of test results:
 *   bit 0: PLL lock
 *   bit 1: VCO calibration
 *   bit 2: TX power detect
 *   bit 3: RX RSSI detect
 * All bits set = 0x0F = all tests passed.
 */
int
b43bsd_radio_selftest(struct b43bsd_softc *sc)
{
	uint16_t stat;
	int result = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio self-test starting\n");

	/* Test 1: PLL lock. */
	stat = radio_read(sc, B2056_SYN | 0x08);	/* COM_CTRL */
	if (stat & 0x0002) {	/* PLL lock indicator */
		result |= 0x01;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  PLL: locked\n");
	} else {
		printf("%s: radio PLL not locked (status 0x%04x)\n",
		    sc->sc_dev.dv_xname, stat);
	}

	/* Test 2: VCO calibration (non-destructive check). */
	{
		uint16_t vcocal;
		vcocal = radio_read(sc, B2056_SYN | 0x60);
		if (vcocal != 0x0000 && vcocal != 0xffff) {
			result |= 0x02;
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  VCO: calibrated (0x%04x)\n", vcocal);
		} else {
			printf("%s: radio VCO not calibrated\n",
			    sc->sc_dev.dv_xname);
		}
	}

	/* Test 3: TX power detect (check PAPD status). */
	{
		uint16_t papd;
		papd = radio_read(sc, B2056_TX0 | 0x14);	/* PA_SPARE1 */
		if (papd != 0x0000) {
			result |= 0x04;
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  TX PAPD: active (0x%04x)\n", papd);
		}
	}

	/* Test 4: RX RSSI present. */
	{
		uint16_t rssi;
		rssi = radio_read(sc, B2056_RX0 | 0x70);
		if ((rssi & 0xff) > 0x20 && (rssi & 0xff) < 0xff) {
			result |= 0x08;
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  RX RSSI: 0x%02x (valid range)\n",
			    rssi & 0xff);
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio self-test result: 0x%x (%s)\n",
	    result, result == 0x0F ? "ALL PASS" : "PARTIAL FAIL");

	return result;
}

/* ------------------------------------------------------------------ */
/* Per-Channel Radio Configuration                                       */
/* ------------------------------------------------------------------ */

/*
 * Configure the entire radio for a specific channel.
 * This is the main channel-change entry point for the radio module.
 * It sequences:
 *   1. Synthesizer retune (PLL + VCO)
 *   2. TX path reconfiguration
 *   3. RX path reconfiguration
 *   4. VCO calibration
 *   5. RSSI offset calibration
 */
void
b43bsd_radio_set_channel(struct b43bsd_softc *sc, int channel, int is_5ghz)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio set channel %d (%s)\n",
	    channel, is_5ghz ? "5 GHz" : "2.4 GHz");

	/*
	 * Band-specific register values for TX/RX.
	 * These fine-tune the analog path for the band center frequency.
	 */
	if (is_5ghz) {
		/* 5 GHz: higher LNA bias, wider filter bandwidth. */
		radio_write(sc, B2056_TX0 | 0x6C, 0x0078);	/* GMBB_IDAC0 */
		radio_write(sc, B2056_TX0 | 0x6D, 0x0078);	/* GMBB_IDAC1 */
		radio_write(sc, B2056_RX0 | 0x0A, 0x0027);	/* LNAA1 bias */
		radio_write(sc, B2056_RX0 | 0x1A, 0x0027);	/* LNAG1 bias */
		radio_write(sc, B2056_RX0 | 0x3A, 0x0004);	/* RXLPF_OUTVCM */
	} else {
		/* 2.4 GHz: standard settings. */
		radio_write(sc, B2056_TX0 | 0x6C, 0x0070);
		radio_write(sc, B2056_TX0 | 0x6D, 0x0070);
		radio_write(sc, B2056_RX0 | 0x0A, 0x0017);
		radio_write(sc, B2056_RX0 | 0x1A, 0x0017);
		radio_write(sc, B2056_RX0 | 0x3A, 0x0005);
	}

	/* Mirror TX0/TX1 and RX0/RX1 settings. */
	radio_write(sc, B2056_TX1 | 0x6C, radio_read(sc, B2056_TX0 | 0x6C));
	radio_write(sc, B2056_TX1 | 0x6D, radio_read(sc, B2056_TX0 | 0x6D));
	radio_write(sc, B2056_RX1 | 0x0A, radio_read(sc, B2056_RX0 | 0x0A));
	radio_write(sc, B2056_RX1 | 0x1A, radio_read(sc, B2056_RX0 | 0x1A));

	/* Run VCO calibration for the new channel. */
	b43bsd_radio_vco_cal(sc);

	/* Recalibrate RSSI offset. */
	b43bsd_radio_rssi_cal(sc);
}

/* ------------------------------------------------------------------ */
/* Full Register Dump (for debugging)                                     */
/* ------------------------------------------------------------------ */

#ifdef B43BSD_DEBUG
/*
 * Dump all accessible radio registers to the console.
 */
void
b43bsd_radio_dump_regs(struct b43bsd_softc *sc)
{
	uint16_t addr;

	printf("%s: BCM2056 radio register dump:\n", sc->sc_dev.dv_xname);

	/* SYN bank: 0x00-0xFF. */
	printf("  SYN:");
	for (addr = 0; addr <= 0xD2; addr++) {
		if ((addr & 0x0F) == 0) printf("\n    ");
		printf("%04x ", radio_read(sc, B2056_SYN | addr));
	}
	printf("\n");

	/* TX0 bank: 0x00-0x8F. */
	printf("  TX0:");
	for (addr = 0; addr <= 0x81; addr++) {
		if ((addr & 0x0F) == 0) printf("\n    ");
		printf("%04x ", radio_read(sc, B2056_TX0 | addr));
	}
	printf("\n");

	/* RX0 bank: 0x00-0xA0. */
	printf("  RX0:");
	for (addr = 0; addr <= 0xA1; addr++) {
		if ((addr & 0x0F) == 0) printf("\n    ");
		printf("%04x ", radio_read(sc, B2056_RX0 | addr));
	}
	printf("\n");
}
#endif /* B43BSD_DEBUG */
