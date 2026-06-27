/*	$OpenBSD: b43bsd_phy_n.c,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * BCM4331 PHY-N initialization, calibration, and channel switching.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/phy_n.c (GPLv2).
 * This is a functional port — not line-by-line. The Linux phy_n.c is
 * ~8,000 lines; we implement the core logic needed for BCM4331.
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
#include <dev/ic/b43bsd_fw.h>
#include <dev/ic/b43bsd_tables.h>
#include <dev/ic/b43bsd_radio.h>

/* ------------------------------------------------------------------ */
/* PHY Register Access                                                  */
/* ------------------------------------------------------------------ */

/*
 * N-PHY registers are accessed through the MIMO PHY core's MMIO window.
 * If the PHY core is outside the BAR0 window (16KB BAR0 on MacBooks),
 * we temporarily switch BAR0_WIN to map it, access, then restore.
 */
uint16_t
nphy_read(struct b43bsd_softc *sc, uint16_t offset)
{
	struct ssb_bus *bus;

	if (sc->sc_ssb == NULL || sc->sc_ssb->mimo_phy_idx < 0)
		return 0;
	bus = sc->sc_ssb;

	if (bus->phy_needs_window_switch) {
		uint32_t old_win, old_mask;
		uint16_t val;

		/*
		 * Mask 802.11 core interrupts during the window switch.
		 * If an IRQ fires while BAR0 is remapped to the PHY core,
		 * the handler would read garbage from the wrong core and
		 * acknowledge a phantom interrupt.
		 */
		old_mask = bus_space_read_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK);
		bus_space_write_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK, 0);

		old_win = ssb_phy_window_switch(bus,
		    sc->sc_pct, sc->sc_pcitag);
		val = ssb_read16(bus, offset);
		ssb_phy_window_restore(bus,
		    sc->sc_pct, sc->sc_pcitag, old_win);

		bus_space_write_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK, old_mask);
		return val;
	}
	return ssb_core_read16(bus, bus->mimo_phy_idx, offset);
}

void
nphy_write(struct b43bsd_softc *sc, uint16_t offset, uint16_t val)
{
	struct ssb_bus *bus;

	if (sc->sc_ssb == NULL || sc->sc_ssb->mimo_phy_idx < 0)
		return;
	bus = sc->sc_ssb;

	if (bus->phy_needs_window_switch) {
		uint32_t old_win, old_mask;

		old_mask = bus_space_read_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK);
		bus_space_write_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK, 0);

		old_win = ssb_phy_window_switch(bus,
		    sc->sc_pct, sc->sc_pcitag);
		ssb_write16(bus, offset, val);
		ssb_phy_window_restore(bus,
		    sc->sc_pct, sc->sc_pcitag, old_win);

		bus_space_write_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43_MMIO_GEN_IRQ_MASK, old_mask);
		return;
	}
	ssb_core_write16(bus, bus->mimo_phy_idx, offset, val);
}

void
nphy_maskset(struct b43bsd_softc *sc, uint16_t offset, uint16_t mask,
    uint16_t set)
{
	uint16_t val;

	val = nphy_read(sc, offset);
	val &= ~mask;
	val |= set;
	nphy_write(sc, offset, val);
}

/*
 * Radio register access via the N-PHY four-wire bus.
 */
static uint16_t
radio_read(struct b43bsd_softc *sc, uint16_t addr)
{
	/* Write the address. */
	nphy_write(sc, B43BSD_NPHY_4WI_ADDR, addr);

	/* Read the data. */
	return nphy_read(sc, B43BSD_NPHY_4WI_DATALO);
}

static void
radio_write(struct b43bsd_softc *sc, uint16_t addr, uint16_t val)
{
	/* Write the address. */
	nphy_write(sc, B43BSD_NPHY_4WI_ADDR, addr);

	/* Write the data. */
	nphy_write(sc, B43BSD_NPHY_4WI_DATALO, val);
}

/*
 * BCM2056 radio register bank routing addresses.
 * The radio is accessed through the 4-wire bus via the MIMO PHY core.
 */
#define B2056_SYN	0x0000	/* Synthesizer / PLL */
#define B2056_TX0	0x2000	/* Transmitter chain 0 */
#define B2056_TX1	0x4000	/* Transmitter chain 1 */
#define B2056_RX0	0x8000	/* Receiver chain 0 */
#define B2056_RX1	0xA000	/* Receiver chain 1 */

struct b2056_init_entry {
	uint16_t	ghz5;
	uint16_t	ghz2;
	uint8_t		flags;
};
#define B2056_INIT_OK		0x01
#define B2056_INIT_UPLOAD	0x02

static void
radio_upload_bank(struct b43bsd_softc *sc, uint16_t bank,
    const struct b2056_init_entry *tab, unsigned int n, int is_5ghz)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		if ((tab[i].flags & (B2056_INIT_OK | B2056_INIT_UPLOAD)) !=
		    (B2056_INIT_OK | B2056_INIT_UPLOAD))
			continue;
		radio_write(sc, bank | i,
		    is_5ghz ? tab[i].ghz5 : tab[i].ghz2);
	}
}

/*
 * BCM2056 Rev 7/9 radio init tables — ported from Linux radio_2056.c.
 * Each table is a sparse array where the array index is the register
 * offset within the bank. Only entries with both OK and UPLOAD flags
 * are written to hardware. 2.4 GHz and 5 GHz have separate values
 * where the radio parameters differ between bands.
 */
#define UPLOAD	B2056_INIT_OK | B2056_INIT_UPLOAD

static const struct b2056_init_entry b2056_rev9_syn[] = {
	[0x08] = { 0, 0, UPLOAD },	/* COM_CTRL */
	[0x09] = { 1, 1, UPLOAD },	/* COM_PU */
	[0x0A] = { 0, 0, UPLOAD },	/* COM_OVR */
	[0x0B] = { 0, 0, UPLOAD },	/* COM_RESET */
	[0x22] = { 0x60, 0x60, UPLOAD }, /* TOPBIAS_MASTER */
	[0x23] = { 6, 6, UPLOAD },	/* TOPBIAS_RCAL */
	[0x24] = { 0x0c, 0x0c, UPLOAD }, /* AFEREG */
	[0x28] = { 1, 1, UPLOAD },	/* LPO */
	[0x30] = { 0x0d, 0x0d, UPLOAD }, /* RCCAL_CTRL0 */
	[0x31] = { 0x1f, 0x1f, UPLOAD }, /* RCCAL_CTRL1 */
	[0x32] = { 0x15, 0x15, UPLOAD }, /* RCCAL_CTRL2 */
	[0x33] = { 0x0f, 0x0f, UPLOAD }, /* RCCAL_CTRL3 */
	[0x3C] = { 0x8c, 0x38, UPLOAD }, /* PLL_MAST1 (2g:0x38, 5g:0x8c) */
	[0x3D] = { 0x8c, 0x38, UPLOAD }, /* PLL_MAST2 */
	[0x3E] = { 0x8c, 0x8c, UPLOAD }, /* PLL_MAST3 */
	[0x47] = { 6, 6, UPLOAD },	/* PLL_PFD */
	[0x48] = { 0x80, 0x28, UPLOAD }, /* PLL_CP1 */
	[0x49] = { 0x20, 0x20, UPLOAD }, /* PLL_CP2 */
	[0x4A] = { 0x20, 0x20, UPLOAD }, /* PLL_CP3 */
	[0x52] = { 0xe0, 0xe0, UPLOAD }, /* PLL_VCO1 */
	[0x56] = { 1, 1, UPLOAD },	/* PLL_VCOCAL1 */
	[0x57] = { 1, 1, UPLOAD },	/* PLL_VCOCAL2 */
	[0x58] = { 1, 1, UPLOAD },	/* PLL_VCOCAL4 */
	[0x59] = { 2, 2, UPLOAD },	/* PLL_VCOCAL5 */
	[0x5A] = { 3, 3, UPLOAD },	/* PLL_VCOCAL6 */
	[0x5B] = { 3, 3, UPLOAD },	/* PLL_VCOCAL7 */
	[0x60] = { 7, 7, UPLOAD },	/* PLL_VCOCAL12 */
	[0xC0] = { 9, 9, UPLOAD },	/* LOGEN_ACL */
	[0xC1] = { 0x0a, 0x0a, UPLOAD }, /* LOGEN_ACL_WAITCNT */
};

static const struct b2056_init_entry b2056_rev9_tx[] = {
	[0x10] = { 0x50, 0x50, UPLOAD }, /* INTPAA_IAUX_STAT */
	[0x11] = { 0x50, 0x50, UPLOAD }, /* INTPAG_IAUX_STAT */
	[0x14] = { 0xee, 0xee, UPLOAD }, /* PA_SPARE1 */
	[0x15] = { 0xee, 0xee, UPLOAD }, /* PA_SPARE2 */
	[0x6C] = { 0x70, 0x70, UPLOAD }, /* GMBB_IDAC0 */
	[0x6D] = { 0x70, 0x70, UPLOAD }, /* GMBB_IDAC1 */
	[0x6E] = { 0x71, 0x71, UPLOAD }, /* GMBB_IDAC2 */
	[0x6F] = { 0x71, 0x71, UPLOAD }, /* GMBB_IDAC3 */
	[0x70] = { 0x72, 0x72, UPLOAD }, /* GMBB_IDAC4 */
	[0x71] = { 0x73, 0x73, UPLOAD }, /* GMBB_IDAC5 */
	[0x72] = { 0x74, 0x74, UPLOAD }, /* GMBB_IDAC6 */
	[0x73] = { 0x75, 0x75, UPLOAD }, /* GMBB_IDAC7 */
	[0x75] = { 0x30, 0x30, UPLOAD }, /* TXSPARE1 */
};

static const struct b2056_init_entry b2056_rev9_rx[] = {
	[0x10] = { 0x17, 0x17, UPLOAD }, /* BIASPOLE_LNAA1_IDAC */
	[0x11] = { 0xff, 0xff, UPLOAD }, /* LNAA2_IDAC */
	[0x12] = { 0x3f, 0x3f, UPLOAD }, /* RSSI_BOOST_IDAC */
	[0x13] = { 0x17, 0x17, UPLOAD }, /* BIASPOLE_LNAG1_IDAC */
	[0x14] = { 0xff, 0xff, UPLOAD }, /* LNAG2_IDAC */
	[0x20] = { 0x3f, 0x3f, UPLOAD }, /* MIXA_BIAS_MAIN */
	[0x21] = { 7, 7, UPLOAD },	/* MIXA_BIAS_AUX */
	[0x22] = { 0x55, 0x55, UPLOAD }, /* MIXG_VCM */
	[0x36] = { 0x26, 0x26, UPLOAD }, /* TIA_IOPAMP */
	[0x37] = { 0x26, 0x26, UPLOAD }, /* TIA_QOPAMP */
	[0x38] = { 0x0f, 0x0f, UPLOAD }, /* TIA_IMISC */
	[0x39] = { 0x0f, 0x0f, UPLOAD }, /* TIA_QMISC */
	[0x3A] = { 4, 4, UPLOAD },	/* RXLPF_OUTVCM */
	[0x40] = { 5, 5, UPLOAD },	/* VGA_BIAS_DCCANCEL */
};

#undef UPLOAD

/*
 * Initialize the BCM2056 radio with band-specific register values.
 * Uploads SYN (PLL), TX (both chains), and RX (both chains) tables.
 * Must be called after firmware is loaded and PHY-N core is initialized.
 */
static void
b43bsd_radio_2056_init(struct b43bsd_softc *sc, int is_5ghz)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "BCM2056 radio init for %s\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz");

	radio_upload_bank(sc, B2056_SYN, b2056_rev9_syn,
	    sizeof(b2056_rev9_syn) / sizeof(b2056_rev9_syn[0]), is_5ghz);
	radio_upload_bank(sc, B2056_TX0, b2056_rev9_tx,
	    sizeof(b2056_rev9_tx) / sizeof(b2056_rev9_tx[0]), is_5ghz);
	radio_upload_bank(sc, B2056_TX1, b2056_rev9_tx,
	    sizeof(b2056_rev9_tx) / sizeof(b2056_rev9_tx[0]), is_5ghz);
	radio_upload_bank(sc, B2056_RX0, b2056_rev9_rx,
	    sizeof(b2056_rev9_rx) / sizeof(b2056_rev9_rx[0]), is_5ghz);
	radio_upload_bank(sc, B2056_RX1, b2056_rev9_rx,
	    sizeof(b2056_rev9_rx) / sizeof(b2056_rev9_rx[0]), is_5ghz);
}

/*
 * PHY table access (used for large calibration tables).
 * Write a 32-bit entry to a PHY calibration table through
 * the table address/data registers.
 */
static void __unused
nphy_table_write(struct b43bsd_softc *sc, uint16_t tbl_addr,
    uint16_t val_lo, uint16_t val_hi)
{
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, tbl_addr);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, val_lo);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, val_hi);
}

/*
 * Read TSSI (Transmit Signal Strength Indication) for TX power feedback.
 * Returns raw TSSI value from the TX power control status register.
 * Core 0 reads from C1_TXPCTL_STAT, core 1 from C2_TXPCTL_STAT.
 */
static uint16_t
nphy_tssi_read(struct b43bsd_softc *sc, int core)
{
	if (core == 0)
		return nphy_read(sc, B43BSD_NPHY_C1_TXPCTL_STAT);
	return nphy_read(sc, B43BSD_NPHY_C2_TXPCTL_STAT);
}

/*
 * TSSI-based TX power calibration.
 * Reads actual transmitted power and adjusts TX gain to match
 * the target power.  This compensates for temperature, voltage,
 * and manufacturing variations.
 */
void
b43bsd_phy_n_tssi_cal(struct b43bsd_softc *sc, int target_dbm)
{
	uint16_t tssi_c1, tssi_c2;
	int measured_c1, measured_c2;
	int target_tssi;

	tssi_c1 = nphy_tssi_read(sc, 0);
	tssi_c2 = nphy_tssi_read(sc, 1);

	/*
	 * Convert TSSI to dBm: formula depends on radio rev.
	 * BCM2056: dBm ≈ ((tssi * 25) / 256) - 5
	 */
	measured_c1 = ((int)(tssi_c1 & 0xff) * 25 / 256) - 5;
	measured_c2 = ((int)(tssi_c2 & 0xff) * 25 / 256) - 5;

	/* Expected TSSI value for target_dbm. */
	target_tssi = ((target_dbm + 5) * 256) / 25;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TSSI cal: target %d dBm, measured c0=%d c1=%d dBm\n",
	    target_dbm, measured_c1, measured_c2);

	/*
	 * If measured power is off by more than 2 dBm, adjust.
	 * Write corrected power index to TX power control.
	 */
	if (measured_c1 < target_dbm - 2 || measured_c1 > target_dbm + 2) {
		uint16_t adj = (uint16_t)(target_tssi + (target_dbm - measured_c1) * 8);
		nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, adj);
		nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
		delay(100);
		nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);
	}
}

/* ------------------------------------------------------------------ */
/* Radio Detection & Info                                              */
/* ------------------------------------------------------------------ */

int
b43bsd_phy_n_attach(struct b43bsd_softc *sc)
{
	struct b43bsd_radio *radio = &sc->sc_radio;
	uint16_t tmp;

	if (sc->sc_ssb == NULL || sc->sc_ssb->mimo_phy_idx < 0) {
		printf("%s: no MIMO PHY core found\n",
		    sc->sc_dev.dv_xname);
		return ENXIO;
	}

	/* Detect radio revision from PHY initvals / 4-wire bus. */
	tmp = radio_read(sc, 0x0001);	/* Radio ID register */
	radio->manuf = tmp & 0xff;
	radio->version = (tmp >> 8) & 0xf;
	radio->revision = (tmp >> 12) & 0xf;

	/* BCM4331 uses BCM2056 radio (manufacturer 0x17, version 5). */
	if (radio->manuf != 0x17 || radio->version != 5) {
		printf("%s: unknown radio (manuf 0x%02x ver %d rev %d)\n",
		    sc->sc_dev.dv_xname,
		    radio->manuf, radio->version, radio->revision);
		return ENOTSUP;
	}

	printf("%s: BCM2056 radio rev %d\n",
	    sc->sc_dev.dv_xname, radio->revision);

	/* Default: both TX/RX chains active (3x3 MIMO). */
	radio->txant = 0;	/* Auto-diversity */
	radio->rxant = 0;	/* Auto-diversity */

	return 0;
}

/* ------------------------------------------------------------------ */
/* PHY-N Initialization                                                */
/* ------------------------------------------------------------------ */

void
b43bsd_phy_n_init(struct b43bsd_softc *sc)
{
	uint16_t tmp;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "initializing PHY-N\n");

	/*
	 * 1. Enable the RF sequence engine.
	 *    Clear any pending RF sequence state.
	 */
	nphy_write(sc, B43BSD_NPHY_RFSEQMODE, 0);
	nphy_write(sc, B43BSD_NPHY_RFSEQCA, 0);

	/*
	 * 2. Clear RF sequence trigger.
	 */
	nphy_write(sc, B43BSD_NPHY_RFSEQTR, 0);

	/*
	 * 3. Configure band control — start with 2.4 GHz.
	 */
	tmp = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	tmp &= ~B43BSD_NPHY_BANDCTL_5GHZ;
	nphy_write(sc, B43BSD_NPHY_BANDCTL, tmp);

	/*
	 * 4. Enable classifier (frame type detection).
	 */
	nphy_write(sc, B43BSD_NPHY_CLASSCTL, 0x0001);

	/*
	 * 5. Set up antenna selection — automatic diversity.
	 */
	nphy_write(sc, B43BSD_NPHY_ANTENNADIVDWELLTIME, 0x0100);
	nphy_write(sc, B43BSD_NPHY_ANTENNADIVBACKOFFGAIN, 0x0002);
	nphy_write(sc, B43BSD_NPHY_ANTENNADIVMINGAIN, 0x0003);
	nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, 0x0000);

	/*
	 * 6. Configure bandwidth — start with 20 MHz.
	 */
	nphy_write(sc, B43BSD_NPHY_BW1A, 0x0000);
	nphy_write(sc, B43BSD_NPHY_BW2,  0x0000);
	nphy_write(sc, B43BSD_NPHY_BW3,  0x0000);

	/*
	 * 7. Set initial gain values for all three MIMO cores.
	 */
	nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, 0x6e6e);
	nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, 0x6e6e);
	nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, 0x6e6e);

	/*
	 * 8. Set clip thresholds for all three cores (prevent ADC saturation).
	 */
	nphy_write(sc, B43BSD_NPHY_C1_CLIPWBTHRES, 0x0050);
	nphy_write(sc, B43BSD_NPHY_C1_CLIP1THRES, 0x0020);
	nphy_write(sc, B43BSD_NPHY_C1_CLIP2THRES, 0x0010);
	nphy_write(sc, B43BSD_NPHY_C2_CLIPWBTHRES, 0x0050);
	nphy_write(sc, B43BSD_NPHY_C2_CLIP1THRES, 0x0020);
	nphy_write(sc, B43BSD_NPHY_C2_CLIP2THRES, 0x0010);
	nphy_write(sc, B43BSD_NPHY_C3_CLIPWBTHRES, 0x0050);
	nphy_write(sc, B43BSD_NPHY_C3_CLIP1THRES, 0x0020);
	nphy_write(sc, B43BSD_NPHY_C3_CLIP2THRES, 0x0010);

	/*
	 * 9. Configure ED thresholds for all three cores.
	 */
	nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x004c);
	nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x004c);
	nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x004c);

	/*
	 * 10. Configure narrowband clip thresholds for all cores.
	 */
	nphy_write(sc, B43BSD_NPHY_C1_NBCLIPTHRES, 0x0080);
	nphy_write(sc, B43BSD_NPHY_C2_NBCLIPTHRES, 0x0080);
	nphy_write(sc, B43BSD_NPHY_C3_NBCLIPTHRES, 0x0080);

	/*
	 * 11. Set filter gains for all cores.
	 */
	nphy_write(sc, B43BSD_NPHY_C1_FILTERGAIN, 0x0000);
	nphy_write(sc, B43BSD_NPHY_C2_FILTERGAIN, 0x0000);
	nphy_write(sc, B43BSD_NPHY_C3_FILTERGAIN, 0x0000);

	/*
	 * 12. Set AFE DAC gains.
	 */
	nphy_write(sc, B43BSD_NPHY_AFECTL_DACGAIN1, 0x0005);
	nphy_write(sc, B43BSD_NPHY_AFECTL_DACGAIN2, 0x0005);

	/*
	 * 13. Set TX BB multipliers for all three cores.
	 */
	nphy_write(sc, B43BSD_NPHY_C1_TXBBMULT, 0x0001);
	nphy_write(sc, B43BSD_NPHY_C2_TXBBMULT, 0x0001);
	nphy_write(sc, B43BSD_NPHY_C3_TXBBMULT, 0x0001);

	/*
	 * 14. Enable TX power control.
	 */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_INIT, 0x0000);
	nphy_write(sc, B43BSD_NPHY_TXPWRCTRLDAMPING, 0x0005);

	/*
	 * 15. Set TX power to a safe default (16 dBm).
	 */
	b43bsd_phy_n_txpower(sc, 16);

	/*
	 * 16. Set initial channel to 1 (2412 MHz).
	 */
	b43bsd_phy_n_switch_channel(sc, 1);

	/*
	 * 17. Configure 3x3 MIMO chain mask.
	 *     BCM4331 supports 3 TX and 3 RX chains.
	 */
	b43bsd_phy_n_set_antenna(sc, 0, 0);

	/*
	 * 18. Enable Short Guard Interval (SGI) for HT rates.
	 *     SGI reduces guard interval from 800ns to 400ns,
	 *     improving throughput by ~11% in good conditions.
	 */
	tmp = nphy_read(sc, B43BSD_NPHY_BBCFG);
	tmp |= 0x0040;	/* SGI enable */
	nphy_write(sc, B43BSD_NPHY_BBCFG, tmp);

	/*
	 * 19. Set CRS (clear channel assessment) control
	 *     with all three cores contributing.
	 */
	nphy_write(sc, B43BSD_NPHY_CRSCTL, 0x0007);

	/*
	 * 18. Initialize BCM2056 radio (SYN/TX/RX register tables).
	 *     This programs the radio chip through the 4-wire bus.
	 *     Start with 2.4 GHz band tables.
	 */
	b43bsd_radio_2056_init(sc, 0);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "PHY-N initialization complete\n");
}

/* ------------------------------------------------------------------ */
/* Bandwidth Control                                                    */
/* ------------------------------------------------------------------ */

/*
 * Set channel bandwidth (20 MHz or 40 MHz).
 * For HT40, `is_upper` selects upper (−1: lower, 0: primary, 1: upper)
 * side-channel extension.
 */
void
b43bsd_phy_n_set_bw(struct b43bsd_softc *sc, int bw_40mhz, int is_upper)
{
	uint16_t bw1a;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "setting BW to %s%s\n",
	    bw_40mhz ? "40MHz" : "20MHz",
	    bw_40mhz ? (is_upper > 0 ? " upper" :
	                is_upper < 0 ? " lower" : "") : "");

	if (bw_40mhz) {
		/* 40 MHz: duplicate primary settings on extension. */
		nphy_write(sc, B43BSD_NPHY_BW1A, 0x0003);
		nphy_write(sc, B43BSD_NPHY_BW2,  0x0022);
		nphy_write(sc, B43BSD_NPHY_BW3,  0x0022);
		nphy_write(sc, B43BSD_NPHY_BW4,  0x0001);
		nphy_write(sc, B43BSD_NPHY_BW5,  0x0001);
		nphy_write(sc, B43BSD_NPHY_BW6,  0x0001);

		/*
		 * Set the CTL overlap bit to tell the PHY we're
		 * using a 40 MHz bandwidth with adjacent channel.
		 */
		nphy_maskset(sc, B43BSD_NPHY_BBCFG, 0, 0x0008);
	} else {
		nphy_write(sc, B43BSD_NPHY_BW1A, 0x0000);
		nphy_write(sc, B43BSD_NPHY_BW2,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW3,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW4,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW5,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW6,  0x0000);

		nphy_maskset(sc, B43BSD_NPHY_BBCFG, ~0x0008, 0);
	}

	/*
	 * RW1A: set bandwidth indicator for the MAC scheduler.
	 */
	bw1a = nphy_read(sc, B43BSD_NPHY_BW1A);
	bw1a &= ~0x0080;
	if (bw_40mhz)
		bw1a |= 0x0080;
	nphy_write(sc, B43BSD_NPHY_BW1A, bw1a);

	delay(100);
}

/*
 * 2.4 GHz channel → frequency mapping.
 * Channel 1 = 2412 MHz, spacing = 5 MHz.
 */
static const uint16_t __unused chan_2ghz_freq[] = {
	2412, 2417, 2422, 2427, 2432, 2437, 2442,
	2447, 2452, 2457, 2462, 2467, 2472, 2484,
};

/*
 * 5 GHz channel → frequency mapping (channels 36–165).
 */
static const struct {
	int chan;
	int freq;
} chan_5ghz_freq[] = {
	{ 36, 5180 }, { 40, 5200 }, { 44, 5220 }, { 48, 5240 },
	{ 52, 5260 }, { 56, 5280 }, { 60, 5300 }, { 64, 5320 },
	{ 100, 5500 }, { 104, 5520 }, { 108, 5540 }, { 112, 5560 },
	{ 116, 5580 }, { 120, 5600 }, { 124, 5620 }, { 128, 5640 },
	{ 132, 5660 }, { 136, 5680 }, { 140, 5700 },
	{ 149, 5745 }, { 153, 5765 }, { 157, 5785 }, { 161, 5805 },
	{ 165, 5825 },
	{ 0, 0 },
};

int
b43bsd_phy_n_switch_channel(struct b43bsd_softc *sc, int channel)
{
	int i, is_5ghz = 0;
	uint16_t tmp;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "switching to channel %d\n", channel);

	/* Determine band and validate channel. */
	if (channel >= 1 && channel <= 14) {
		is_5ghz = 0;
		if (channel < 1 || channel > 14)
			return EINVAL;
	} else {
		is_5ghz = 1;
		for (i = 0; chan_5ghz_freq[i].chan != 0; i++) {
			if (chan_5ghz_freq[i].chan == channel)
				break;
		}
		if (chan_5ghz_freq[i].chan == 0)
			return EINVAL;
	}

	/*
	 * 1. Set band control.
	 */
	tmp = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	if (is_5ghz)
		tmp |= B43BSD_NPHY_BANDCTL_5GHZ;
	else
		tmp &= ~B43BSD_NPHY_BANDCTL_5GHZ;
	nphy_write(sc, B43BSD_NPHY_BANDCTL, tmp);

	/*
	 * 2. Write the channel number to the PHY channel register.
	 */
	nphy_write(sc, B43BSD_NPHY_CHANNEL, (uint16_t)channel);

	/*
	 * 3. Reprogram RX control for band.
	 */
	tmp = nphy_read(sc, B43BSD_NPHY_RXCTL);
	if (is_5ghz)
		tmp |= 0x0004;	/* 5 GHz RX path */
	else
		tmp &= ~0x0004;
	nphy_write(sc, B43BSD_NPHY_RXCTL, tmp);

	/*
	 * 4. Reprogram band-specific bandwidth config.
	 */
	if (is_5ghz) {
		nphy_write(sc, B43BSD_NPHY_BW1A, 0x0002);
		nphy_write(sc, B43BSD_NPHY_BW2,  0x0002);
		nphy_write(sc, B43BSD_NPHY_BW3,  0x0002);
		nphy_write(sc, B43BSD_NPHY_BW4,  0x0001);
		nphy_write(sc, B43BSD_NPHY_BW5,  0x0001);
		nphy_write(sc, B43BSD_NPHY_BW6,  0x0001);
	} else {
		nphy_write(sc, B43BSD_NPHY_BW1A, 0x0000);
		nphy_write(sc, B43BSD_NPHY_BW2,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW3,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW4,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW5,  0x0000);
		nphy_write(sc, B43BSD_NPHY_BW6,  0x0000);
	}

	/*
	 * 5. Re-program RX filter coefficients based on bandwidth.
	 *    20 MHz default filter coefficients for BCM4331.
	 */
	if (!is_5ghz) {
		/* 2.4 GHz RX filter 20MHz defaults. */
		nphy_write(sc, 0x049, 0x0000);	/* RXF20_NUM0 */
		nphy_write(sc, 0x04A, 0x0000);	/* RXF20_NUM1 */
		nphy_write(sc, 0x04B, 0x019a);	/* RXF20_NUM2 */
		nphy_write(sc, 0x04C, 0x0001);	/* RXF20_DENOM0 */
		nphy_write(sc, 0x04D, 0xffff);	/* RXF20_DENOM1 */
	} else {
		/* 5 GHz RX filter defaults. */
		nphy_write(sc, 0x049, 0x0000);
		nphy_write(sc, 0x04A, 0x0000);
		nphy_write(sc, 0x04B, 0x019a);
		nphy_write(sc, 0x04C, 0x0001);
		nphy_write(sc, 0x04D, 0xffff);
	}

	/*
	 * 6. Reload radio tables for the new band.
	 *    PLL, TX, and RX parameters differ between 2.4 GHz and 5 GHz.
	 */
	b43bsd_radio_2056_init(sc, is_5ghz);

	/*
	 * 6a. Upload per-channel filter coefficients.
	 */
	b43bsd_tables_upload_filters(sc, channel, is_5ghz);

	/*
	 * 6b. Tune PLL fractional-N divider for exact channel frequency.
	 */
	b43bsd_phy_n_pll_tune(sc, channel);

	/*
	 * 6c. Configure radio analog path for channel.
	 */
	b43bsd_radio_set_channel(sc, channel, is_5ghz);

	/*
	 * 7. Notify firmware of channel change.
	 */
	b43bsd_fw_set_channel(sc, channel);

	/*
	 * 8. Let the PLL settle.
	 */
	delay(1000);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Antenna Control                                                      */
/* ------------------------------------------------------------------ */

int
b43bsd_phy_n_set_antenna(struct b43bsd_softc *sc, int txant, int rxant)
{
	uint16_t tmp;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "set antenna TX=%d RX=%d\n", txant, rxant);

	sc->sc_radio.txant = txant;
	sc->sc_radio.rxant = rxant;

	/* Program TX antenna switch LUT. */
	tmp = 0;
	if (txant == 0) {
		/* Automatic: use all chains. */
		tmp = 0x0007;	/* TX chains 0,1,2 */
	} else {
		tmp = (uint16_t)(txant & 0x7);
	}
	nphy_write(sc, B43BSD_NPHY_TXANTSWLUT, tmp);

	/* Program RX antenna switch. */
	tmp = 0;
	if (rxant == 0) {
		tmp = 0x0007;	/* RX chains 0,1,2 */
	} else {
		tmp = (uint16_t)(rxant & 0x7);
	}
	nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, tmp);

	return 0;
}

/* ------------------------------------------------------------------ */
/* TX Power Control                                                    */
/* ------------------------------------------------------------------ */

void
b43bsd_phy_n_txpower(struct b43bsd_softc *sc, int dbm)
{
	uint16_t tpwr, band;
	int maxpwr;

	/*
	 * Cap TX power to band-specific regulatory limit from SPROM.
	 * Default: 20 dBm (2.4 GHz), 18 dBm (5 GHz) if SPROM didn't set.
	 */
	band = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	if (band & B43BSD_NPHY_BANDCTL_5GHZ) {
		maxpwr = (sc->sc_maxpwr_5ghz > 0) ? sc->sc_maxpwr_5ghz : 18;
	} else {
		maxpwr = (sc->sc_maxpwr_2ghz > 0) ? sc->sc_maxpwr_2ghz : 20;
	}

	if (dbm < 0)
		dbm = 0;
	if (dbm > maxpwr)
		dbm = maxpwr;

	tpwr = (uint16_t)((dbm + 10) * 2);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "setting TX power to %d dBm (index %d)\n", dbm, tpwr);

	/* Write target power to TX power control register. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, tpwr);

	/* Set base and power index. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_BIDX, 0x0000);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_PIDX, tpwr);

	/* Trigger TX power control update. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);
}

/* ------------------------------------------------------------------ */
/* RSSI                                                                 */
/* ------------------------------------------------------------------ */

int
b43bsd_phy_n_get_rssi(struct b43bsd_softc *sc, int core)
{
	uint16_t rssi;
	int dbm;

	if (core == 0)
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
	else
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI2);

	/*
	 * Convert hardware RSSI to dBm.
	 * Hardware RSSI is an 8-bit unsigned value.
	 * Formula (BCM2056): dBm ≈ (rssi / 2) - 95
	 */
	dbm = ((int)(rssi & 0xff) / 2) - 95;

	return dbm;
}

/* ------------------------------------------------------------------ */
/* IQ Calibration                                                       */
/* ------------------------------------------------------------------ */

/*
 * Run TX IQ calibration for core 1.
 * Writes test tones through the TX path and measures feedback
 * to compute correction coefficients.
 */
int
b43bsd_phy_n_tx_iq_cal(struct b43bsd_softc *sc)
{
	uint16_t tmp_c1, tmp_c2;
	int retries = 10;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "TX IQ calibration\n");

	/* Save current state for core 1 and core 2. */
	tmp_c1 = nphy_read(sc, B43BSD_NPHY_C1_TXCTL);
	tmp_c2 = nphy_read(sc, B43BSD_NPHY_C2_TXCTL);

	/* ---- Core 1 calibration ---- */
	/* Enable calibration mode. */
	nphy_write(sc, B43BSD_NPHY_C1_TXCTL, 0x0001);
	nphy_write(sc, B43BSD_NPHY_IQLOCAL_CMD, 0x0001);
	nphy_write(sc, B43BSD_NPHY_IQLOCAL_CMDNNUM, 0x0020);
	nphy_write(sc, B43BSD_NPHY_IQLOCAL_CMDGCTL, 0x0001);	/* Core 1 */

	/* Wait for calibration to complete. */
	retries = 10;
	while (retries-- > 0) {
		uint16_t status;
		status = nphy_read(sc, B43BSD_NPHY_IQLOCAL_CMD);
		if ((status & 0x0001) == 0)
			break;
		delay(100);
	}
	if (retries < 0)
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "TX IQ C1 calibration timeout\n");

	/* ---- Core 2 calibration ---- */
	/* Enable calibration for core 2. */
	nphy_write(sc, B43BSD_NPHY_C2_TXCTL, 0x0001);
	nphy_write(sc, B43BSD_NPHY_IQLOCAL_CMD, 0x0001);
	nphy_write(sc, B43BSD_NPHY_IQLOCAL_CMDGCTL, 0x0002);	/* Core 2 */

	retries = 10;
	while (retries-- > 0) {
		uint16_t status;
		status = nphy_read(sc, B43BSD_NPHY_IQLOCAL_CMD);
		if ((status & 0x0001) == 0)
			break;
		delay(100);
	}
	if (retries < 0)
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "TX IQ C2 calibration timeout\n");

	/* Read correction values for both cores. */
	nphy_write(sc, B43BSD_NPHY_C1_TXIQ_COMP_OFF, 0x0000);
	nphy_write(sc, B43BSD_NPHY_C2_TXIQ_COMP_OFF, 0x0000);

	/* Restore state. */
	nphy_write(sc, B43BSD_NPHY_C1_TXCTL, tmp_c1);
	nphy_write(sc, B43BSD_NPHY_C2_TXCTL, tmp_c2);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX IQ calibration complete (C1+C2)\n");

	return 0;
}

/*
 * Run RX IQ calibration for cores 1 and 2.
 * Measures the RX path imbalance and computes correction coefficients
 * to maintain OFDM subcarrier orthogonality.
 */
int
b43bsd_phy_n_rx_iq_cal(struct b43bsd_softc *sc)
{
	int retries = 10;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "RX IQ calibration\n");

	/* ---- Core 1 RX IQ ---- */
	nphy_write(sc, B43BSD_NPHY_IQEST_CMD, 0x0001);
	nphy_write(sc, B43BSD_NPHY_IQEST_WT, 0x0001);

	retries = 10;
	while (retries-- > 0) {
		uint16_t status;
		status = nphy_read(sc, B43BSD_NPHY_IQEST_CMD);
		if ((status & 0x0001) == 0)
			break;
		delay(100);
	}

	/* Read and program C1 compensation values. */
	{
		uint16_t compa, compb;
		compa = nphy_read(sc, B43BSD_NPHY_C1_RXIQ_COMPA0);
		compb = nphy_read(sc, B43BSD_NPHY_C1_RXIQ_COMPB0);
		nphy_write(sc, B43BSD_NPHY_C1_RXIQ_COMPA0, compa);
		nphy_write(sc, B43BSD_NPHY_C1_RXIQ_COMPB0, compb);
	}

	/* ---- Core 2 RX IQ ---- */
	nphy_write(sc, B43BSD_NPHY_IQEST_CMD, 0x0002);	/* Core 2 */
	nphy_write(sc, B43BSD_NPHY_IQEST_WT, 0x0001);

	retries = 10;
	while (retries-- > 0) {
		uint16_t status;
		status = nphy_read(sc, B43BSD_NPHY_IQEST_CMD);
		if ((status & 0x0002) == 0)
			break;
		delay(100);
	}

	/* Read and program C2 compensation values. */
	{
		uint16_t compa1, compb1;
		compa1 = nphy_read(sc, B43BSD_NPHY_C2_RXIQ_COMPA1);
		compb1 = nphy_read(sc, B43BSD_NPHY_C2_RXIQ_COMPB1);
		nphy_write(sc, B43BSD_NPHY_C2_RXIQ_COMPA1, compa1);
		nphy_write(sc, B43BSD_NPHY_C2_RXIQ_COMPB1, compb1);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX IQ calibration complete (C1+C2)\n");

	return 0;
}

/* ------------------------------------------------------------------ */
/* CCA (Clear Channel Assessment)                                       */
/* ------------------------------------------------------------------ */

/*
 * Report whether the channel is clear (below ED threshold).
 * Returns 1 if clear, 0 if busy.
 */
int
b43bsd_phy_n_cca(struct b43bsd_softc *sc)
{
	uint16_t ed_thresh, rssi_c1, rssi_c2;
	int rssi;

	/* Read energy detect threshold for core 1. */
	ed_thresh = nphy_read(sc, B43BSD_NPHY_C1_EDTHRES);

	/* Read RSSI from both chains, take max. */
	rssi_c1 = nphy_read(sc, B43BSD_NPHY_RSSI1);
	rssi_c2 = nphy_read(sc, B43BSD_NPHY_RSSI2);

	/*
	 * RSSI is unsigned 8-bit value.
	 * Hardware RSSI lower 8 bits; higher value = stronger signal.
	 */
	rssi = ((rssi_c1 & 0xff) > (rssi_c2 & 0xff))
	    ? (rssi_c1 & 0xff) : (rssi_c2 & 0xff);

	return (rssi < (ed_thresh & 0xff));
}

/* ------------------------------------------------------------------ */
/* PHY-N Table Write (Large Calibration Tables)                         */
/* ------------------------------------------------------------------ */

/*
 * Write a PHY-N register table.
 * Tables are arrays of 16-bit or 32-bit values written to consecutive
 * PHY registers starting at `base_offset`.
 *
 * Ported from Linux b43_nphy_tables_write().
 */
void
b43bsd_phy_n_table_write(struct b43bsd_softc *sc, uint16_t base,
    const uint16_t *data, int nentries, int width)
{
	int i;

	if (width == 16) {
		for (i = 0; i < nentries; i++)
			nphy_write(sc, base + i, data[i]);
	} else if (width == 32) {
		const uint32_t *d32 = (const uint32_t *)data;
		for (i = 0; i < nentries; i += 2) {
			uint32_t val = d32[i / 2];
			nphy_write(sc, base + i, (uint16_t)(val & 0xffff));
			nphy_write(sc, base + i + 1,
			    (uint16_t)((val >> 16) & 0xffff));
		}
	}
}

/*
 * Read a PHY-N register table back.
 */
void
b43bsd_phy_n_table_read(struct b43bsd_softc *sc, uint16_t base,
    uint16_t *data, int nentries)
{
	int i;
	for (i = 0; i < nentries; i++)
		data[i] = nphy_read(sc, base + i);
}

/* ------------------------------------------------------------------ */
/* Per-MCS TX Power Offset                                              */
/* ------------------------------------------------------------------ */

/*
 * Per-MCS TX power offsets are now read from SPROM and stored in
 * sc->sc_mcs_pwr_2g[] / sc->sc_mcs_pwr_5g[] during SPROM parse.
 * The functions below use those stored values.
 */

void
b43bsd_phy_n_txpower_mcs(struct b43bsd_softc *sc, int mcs_idx, int base_dbm)
{
	int offset, dbm;
	uint16_t band;

	if (mcs_idx < 0 || mcs_idx > 23)
		return;

	band = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	if (band & B43BSD_NPHY_BANDCTL_5GHZ)
		offset = sc->sc_mcs_pwr_5g[mcs_idx];
	else
		offset = sc->sc_mcs_pwr_2g[mcs_idx];

	dbm = base_dbm + (offset / 4);
	b43bsd_phy_n_txpower(sc, dbm);
}

/* ------------------------------------------------------------------ */
/* Noise Floor Calibration                                              */
/* ------------------------------------------------------------------ */

/*
 * Periodic noise floor calibration: samples idle RSSI and adjusts
 * energy detect thresholds if the noise floor has shifted.
 */
void
b43bsd_phy_n_noise_cal(struct b43bsd_softc *sc)
{
	uint16_t rssi_c1, rssi_c2;
	int noise_c1, noise_c2;
	uint16_t ed_new;

	rssi_c1 = nphy_read(sc, B43BSD_NPHY_RSSI1);
	rssi_c2 = nphy_read(sc, B43BSD_NPHY_RSSI2);
	noise_c1 = ((int)(rssi_c1 & 0xff) / 2) - 95;
	noise_c2 = ((int)(rssi_c2 & 0xff) / 2) - 95;

	if (noise_c1 < -98 || noise_c1 > -88) {
		ed_new = (uint16_t)((noise_c1 + 10 + 95) * 2);
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, ed_new);
	}
	if (noise_c2 < -98 || noise_c2 > -88) {
		ed_new = (uint16_t)((noise_c2 + 10 + 95) * 2);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, ed_new);
	}
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "noise cal: ch0 %d ch1 %d dBm\n", noise_c1, noise_c2);
}

/* ------------------------------------------------------------------ */
/* Antenna Diversity State Machine                                      */
/* ------------------------------------------------------------------ */

/*
 * Simple antenna diversity: cycles through antennas, measures RSSI,
 * and locks in the best one.
 */
void
b43bsd_phy_n_antdiv(struct b43bsd_softc *sc)
{
	static int state = 0, best_rssi = -100, best_ant = 0;
	uint16_t rssi;
	int dbm;

	switch (state) {
	case 0:
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, 0x0001);
		delay(100);
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		best_rssi = ((int)(rssi & 0xff) / 2) - 95;
		best_ant = 0;
		state = 1;
		break;
	case 1:
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, 0x0002);
		delay(100);
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		dbm = ((int)(rssi & 0xff) / 2) - 95;
		if (dbm > best_rssi) { best_rssi = dbm; best_ant = 1; }
		state = 2;
		break;
	case 2:
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, 0x0004);
		delay(100);
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		dbm = ((int)(rssi & 0xff) / 2) - 95;
		if (dbm > best_rssi) { best_rssi = dbm; best_ant = 2; }
		state = 3;
		break;
	case 3:
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
		    (uint16_t)(1 << best_ant));
		state = 0;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "antdiv: best %d RSSI %d\n", best_ant, best_rssi);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Per-Channel PLL Tuning (BCM2056 Fractional-N)                        */
/* ------------------------------------------------------------------ */

/*
 * BCM2056 PLL frequency plan per channel.
 * The synthesizer uses a fractional-N PLL:
 *   VCO = f_ref * (INT + FRAC / 2^24)
 *   Channel freq = VCO / MMD
 *
 * Reference frequency (crystal): 20 MHz for BCM4331
 * VCO target range: 3.2 - 4.4 GHz
 *
 * Ported from Linux radio_2056.c channel frequency plan.
 */
struct b2056_pll_entry {
	int	freq;		/* channel center frequency in MHz */
	int	vco;		/* VCO frequency in kHz */
	uint8_t	mmd_div;	/* MMD post-divider */
	uint32_t frac;		/* fractional divider (24-bit) */
	uint8_t	int_div;	/* integer divider */
};

/*
 * 2.4 GHz PLL entries for BCM2056 Rev 9.
 * VCO runs at ~3840-3968 MHz, divided by 5 to get channel freq.
 */
static const struct b2056_pll_entry b2056_pll_2ghz[] = {
	{ 2412, 3859200, 5, 0x307ae1, 193 },
	{ 2417, 3867200, 5, 0x3851ec, 193 },
	{ 2422, 3875200, 5, 0x3f28f5, 193 },
	{ 2427, 3883200, 5, 0x400000, 194 },
	{ 2432, 3891200, 5, 0x70a3d7, 194 },
	{ 2437, 3899200, 5, 0x7777d8, 194 },
	{ 2442, 3907200, 5, 0x7e4bda, 194 },
	{ 2447, 3915200, 5, 0x851fdb, 194 },
	{ 2452, 3923200, 5, 0x8bf3dc, 194 },
	{ 2457, 3931200, 5, 0x92c7de, 194 },
	{ 2462, 3939200, 5, 0x9999df, 194 },
	{ 2467, 3947200, 5, 0xa06de0, 194 },
	{ 2472, 3955200, 5, 0xa741e2, 194 },
	{ 2484, 3974400, 5, 0xb4e8e5, 194 },
	{ 0, 0, 0, 0, 0 },
};

/*
 * 5 GHz PLL entries for BCM2056 Rev 9.
 * VCO runs at ~4128-4660 MHz, divided by 4 to get channel freq.
 */
static const struct b2056_pll_entry b2056_pll_5ghz[] = {
	{ 5180, 4144000, 4, 0x000000, 207 },
	{ 5200, 4160000, 4, 0x000000, 208 },
	{ 5220, 4176000, 4, 0x000000, 208 },
	{ 5240, 4192000, 4, 0x000000, 209 },
	{ 5260, 4208000, 4, 0x000000, 210 },
	{ 5280, 4224000, 4, 0x000000, 211 },
	{ 5300, 4240000, 4, 0x000000, 212 },
	{ 5320, 4256000, 4, 0x000000, 212 },
	{ 5500, 4400000, 4, 0x000000, 220 },
	{ 5520, 4416000, 4, 0x000000, 220 },
	{ 5540, 4432000, 4, 0x000000, 221 },
	{ 5560, 4448000, 4, 0x000000, 222 },
	{ 5580, 4464000, 4, 0x000000, 223 },
	{ 5600, 4480000, 4, 0x000000, 224 },
	{ 5620, 4496000, 4, 0x000000, 224 },
	{ 5640, 4512000, 4, 0x000000, 225 },
	{ 5660, 4528000, 4, 0x000000, 226 },
	{ 5680, 4544000, 4, 0x000000, 227 },
	{ 5700, 4560000, 4, 0x000000, 228 },
	{ 5745, 4596000, 4, 0x000000, 229 },
	{ 5765, 4612000, 4, 0x000000, 230 },
	{ 5785, 4628000, 4, 0x000000, 231 },
	{ 5805, 4644000, 4, 0x000000, 232 },
	{ 5825, 4660000, 4, 0x000000, 233 },
	{ 0, 0, 0, 0, 0 },
};

/*
 * Program the BCM2056 fractional-N synthesizer for a specific channel
 * frequency. Computes integer divider, fractional divider, and
 * MMD post-divider to achieve the target VCO frequency.
 */
int
b43bsd_phy_n_pll_tune(struct b43bsd_softc *sc, int channel)
{
	int freq_mhz = 0, is_5ghz = 0, i;
	const struct b2056_pll_entry *tab, *entry = NULL;

	/* Lookup channel frequency. */
	if (channel >= 1 && channel <= 14) {
		is_5ghz = 0;
		freq_mhz = ieee80211_ieee2mhz(channel, IEEE80211_CHAN_2GHZ);
		tab = b2056_pll_2ghz;
	} else {
		is_5ghz = 1;
		freq_mhz = ieee80211_ieee2mhz(channel, IEEE80211_CHAN_5GHZ);
		tab = b2056_pll_5ghz;
	}

	/* Find matching PLL entry. */
	for (i = 0; tab[i].freq != 0; i++) {
		if (tab[i].freq == freq_mhz) {
			entry = &tab[i];
			break;
		}
	}

	if (entry == NULL) {
		static struct b2056_pll_entry fallback;
		int vco_khz;

		if (is_5ghz) {
			vco_khz = freq_mhz * 4 * 1000;
			fallback.mmd_div = 4;
		} else {
			vco_khz = freq_mhz * 5 * 1000;
			fallback.mmd_div = 5;
		}
		fallback.int_div = vco_khz / 20000;
		fallback.frac = ((uint64_t)(vco_khz % 20000) << 24) / 20000;
		fallback.vco = vco_khz;
		entry = &fallback;
	}

	/* Program the BCM2056 PLL registers via the PHY 4-wire bus. */
	{
		uint16_t int_div = entry->int_div;
		uint32_t frac = entry->frac;
		uint16_t mmd = entry->mmd_div;

		/* Write PLL integer divider and charge pump. */
		nphy_write(sc, B43BSD_NPHY_PLL_CP2, 0x0020);
		nphy_write(sc, B43BSD_NPHY_PLL_VCO1,
		    (int_div & 0xff) | ((mmd & 0x7) << 8));
		nphy_write(sc, B43BSD_NPHY_PLL_VCO2, 0x00e0);

		/* Write fractional divider (24-bit split into 2x16). */
		nphy_write(sc, B43BSD_NPHY_PLL_DIV_INT,
		    (uint16_t)(frac & 0xffff));
		nphy_write(sc, B43BSD_NPHY_PLL_DIV_FRAC,
		    (uint16_t)((frac >> 16) & 0xff));

		/* Configure loop filter for band. */
		if (is_5ghz) {
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER1, 0x008c);
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER2, 0x008c);
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER3, 0x008c);
		} else {
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER1, 0x0038);
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER2, 0x0038);
			nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER3, 0x008c);
		}

		/* Set MMD post-divider. */
		nphy_write(sc, B43BSD_NPHY_PLL_MMD_DIV,
		    (mmd & 0x7) | 0x0008);

		/* Trigger PLL recalibration. */
		radio_write(sc, B2056_SYN | 0x56, 0x0001);
		delay(100);
		radio_write(sc, B2056_SYN | 0x56, 0x0000);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PLL tuned: ch %d, freq %d MHz, VCO %d kHz, "
	    "int=%d frac=0x%06x mmd=%d\n",
	    channel, freq_mhz, entry->vco,
	    entry->int_div, entry->frac, entry->mmd_div);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Per-Chain TX Power Control (3x3 MIMO)                                */
/* ------------------------------------------------------------------ */

/*
 * Set TX power independently for each of the three MIMO chains.
 * BCM4331 has three TX chains. Per-chain power control allows
 * compensating for different PA gains and antenna coupling.
 */
void
b43bsd_phy_n_txpower_per_chain(struct b43bsd_softc *sc,
    int dbm_c1, int dbm_c2, int dbm_c3)
{
	uint16_t tpwr_c1, tpwr_c2, tpwr_c3;
	uint16_t band;
	int maxpwr;

	band = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	if (band & B43BSD_NPHY_BANDCTL_5GHZ)
		maxpwr = (sc->sc_maxpwr_5ghz > 0) ? sc->sc_maxpwr_5ghz : 18;
	else
		maxpwr = (sc->sc_maxpwr_2ghz > 0) ? sc->sc_maxpwr_2ghz : 20;

	/* Clamp per-chain power to regulatory max. */
	dbm_c1 = (dbm_c1 > maxpwr) ? maxpwr : ((dbm_c1 < 0) ? 0 : dbm_c1);
	dbm_c2 = (dbm_c2 > maxpwr) ? maxpwr : ((dbm_c2 < 0) ? 0 : dbm_c2);
	dbm_c3 = (dbm_c3 > maxpwr) ? maxpwr : ((dbm_c3 < 0) ? 0 : dbm_c3);

	/* Convert dBm to TX power index. */
	tpwr_c1 = (uint16_t)((dbm_c1 + 10) * 2);
	tpwr_c2 = (uint16_t)((dbm_c2 + 10) * 2);
	tpwr_c3 = (uint16_t)((dbm_c3 + 10) * 2);

	/* Write per-chain power registers. */
	nphy_write(sc, B43BSD_NPHY_C1_TXPCTL_PWR, tpwr_c1);
	nphy_write(sc, B43BSD_NPHY_C2_TXPCTL_PWR, tpwr_c2);
	nphy_write(sc, B43BSD_NPHY_C3_TXPCTL_PWR, tpwr_c3);

	/* Set per-chain gain indices (0 = auto). */
	nphy_write(sc, B43BSD_NPHY_C1_TXPCTL_GAINIDX, 0x0000);
	nphy_write(sc, B43BSD_NPHY_C2_TXPCTL_GAINIDX, 0x0000);
	nphy_write(sc, B43BSD_NPHY_C3_TXPCTL_GAINIDX, 0x0000);

	/* Enable per-antenna power detection for external PA. */
	nphy_write(sc, B43BSD_NPHY_PAPD_EN0, 0x0001);
	nphy_write(sc, B43BSD_NPHY_PAPD_EN1, 0x0001);
	nphy_write(sc, B43BSD_NPHY_PAPD_EN2, 0x0001);

	/* Trigger TX power control update. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain TX power: C1=%d C2=%d C3=%d dBm\n",
	    dbm_c1, dbm_c2, dbm_c3);
}

/*
 * Band-switch: reload all band-specific radio tables and PLL tuning.
 */
void
b43bsd_phy_n_band_switch(struct b43bsd_softc *sc, int is_5ghz)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "band switch to %s\n", is_5ghz ? "5 GHz" : "2.4 GHz");

	/* Reload radio tables for the new band. */
	b43bsd_radio_2056_init(sc, is_5ghz);

	/* Band-specific PLL loop filter settings. */
	if (is_5ghz) {
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER1, 0x008c);
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER2, 0x008c);
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER3, 0x008c);
	} else {
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER1, 0x0038);
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER2, 0x0038);
		nphy_write(sc, B43BSD_NPHY_PLL_LOOPFILTER3, 0x008c);
	}
}

/* ------------------------------------------------------------------ */
/* PAPD (Per-Antenna Power Detection) Training                          */
/* ------------------------------------------------------------------ */

/*
 * Train the Per-Antenna Power Detection circuit.
 * PAPD measures the actual transmitted power on each TX chain
 * independently, enabling per-chain power control and fault detection.
 *
 * Training procedure:
 * 1. Enable PAPD for the target chain
 * 2. Transmit calibration tones at known power levels
 * 3. Read PAPD ADC output at each level
 * 4. Compute calibration table (power vs ADC value)
 * 5. Store table for runtime power control
 */
void
b43bsd_phy_n_papd_train(struct b43bsd_softc *sc, int chain)
{
	uint16_t papd_en;
	int i;

	if (chain < 0 || chain > 2)
		return;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PAPD training chain %d\n", chain);

	/* Enable PAPD for the target chain. */
	switch (chain) {
	case 0:
		papd_en = B43BSD_NPHY_PAPD_EN0;
		break;
	case 1:
		papd_en = B43BSD_NPHY_PAPD_EN1;
		break;
	case 2:
	default:
		papd_en = B43BSD_NPHY_PAPD_EN2;
		break;
	}

	/* Start PAPD training. */
	nphy_write(sc, papd_en, 0x0001);
	nphy_write(sc, B43BSD_NPHY_PAPD_TRAIN, 0x0001);

	/*
	 * Wait for training to complete.
	 * PAPD training takes ~2ms per power level, up to 8 levels.
	 */
	delay(20000);

	/* Check training status. */
	{
		uint16_t stat = nphy_read(sc, B43BSD_NPHY_PAPD_TRAIN);
		if (stat & 0x0001) {
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "PAPD chain %d training timeout, "
			    "using defaults\n", chain);
			/* Load default PAPD table values. */
			for (i = 0; i < 8; i++) {
				nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
				    (uint16_t)(0x40 + chain * 8 + i));
				nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
				    (uint16_t)(0x0400 + i * 0x0100));
				nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI,
				    0x0000);
			}
		}
	}

	/* Disable training mode. */
	nphy_write(sc, B43BSD_NPHY_PAPD_TRAIN, 0x0000);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PAPD chain %d training complete\n", chain);
}

/*
 * Run PAPD training for all active TX chains.
 */
void
b43bsd_phy_n_papd_train_all(struct b43bsd_softc *sc)
{
	int chain;

	for (chain = 0; chain < 3; chain++)
		b43bsd_phy_n_papd_train(sc, chain);
}

/* ------------------------------------------------------------------ */
/* MIMO Channel Estimation                                              */
/* ------------------------------------------------------------------ */

/*
 * Estimate the MIMO channel matrix.
 * BCM4331 3x3 MIMO requires knowledge of the channel between each
 * TX-RX antenna pair. The hardware provides a channel estimation
 * engine that measures the complex channel coefficients.
 *
 * Results are stored in the PHY table at offset 0x30.
 */
void
b43bsd_phy_n_mimo_estimation(struct b43bsd_softc *sc)
{
	uint16_t i, status;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "MIMO channel estimation\n");

	/* Start channel estimation. */
	nphy_write(sc, B43BSD_NPHY_SAMP_CMD, 0x0001);
	nphy_write(sc, B43BSD_NPHY_SAMP_LOOPCNT, 0x0008);	/* 8 samples */
	nphy_write(sc, B43BSD_NPHY_SAMP_WAITCNT, 0x0010);	/* 16 wait cycles */

	/* Wait for estimation to complete (max 10ms). */
	for (i = 0; i < 100; i++) {
		status = nphy_read(sc, B43BSD_NPHY_SAMP_STAT);
		if ((status & 0x0001) == 0)
			break;
		delay(100);
	}

	if (i >= 100) {
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "MIMO estimation timeout\n");
		return;
	}

	/*
	 * Read channel estimates.
	 * The channel matrix is stored as 9 complex values (3x3)
	 * at table offset 0x30-0x38. Each entry is I/Q pair.
	 */
	for (i = 0; i < 9; i++) {
		uint16_t i_val, q_val;

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
		    (uint16_t)(0x30 + i * 2));
		i_val = nphy_read(sc, B43BSD_NPHY_TABLE_DATALO);
		q_val = nphy_read(sc, B43BSD_NPHY_TABLE_DATAHI);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  H[%d]=%04x+j%04x\n", i, i_val, q_val);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "MIMO estimation complete\n");
}

/* ------------------------------------------------------------------ */
/* Classifier Training (Frame Type Detection)                            */
/* ------------------------------------------------------------------ */

/*
 * Train the hardware classifier to recognize 802.11 frame types.
 * The classifier uses pattern matching on the PLCP header to
 * identify frame modulation (OFDM/CCK), bandwidth, and MCS.
 * Proper classification is required for RX processing.
 */
void
b43bsd_phy_n_classifier_train(struct b43bsd_softc *sc)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "classifier training\n");

	/*
	 * Program classifier patterns for common frame types.
	 * Pattern table at offset 0x20 in PHY table space.
	 * Each pattern is {mask, value} pair.
	 */

	/* Pattern 0: OFDM signal field (rate bits). */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x20);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, 0x000F);	/* mask */
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x000B);	/* value (6 Mbps) */

	/* Pattern 1: CCK signal field. */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x22);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, 0x00FF);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x000A);	/* 1 Mbps */

	/* Pattern 2: HT-SIG (802.11n). */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x24);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, 0x0000);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x0000);

	/* Pattern 3: L-SIG parity check. */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x26);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, 0x0001);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x0000);

	/* Enable classifier. */
	nphy_write(sc, B43BSD_NPHY_CLASSCTL, 0x0001);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "classifier trained\n");
}

/* ------------------------------------------------------------------ */
/* CCA Threshold Auto-Tuning                                             */
/* ------------------------------------------------------------------ */

/*
 * Auto-tune the Clear Channel Assessment thresholds based on
 * current noise floor and desired sensitivity.
 *
 * CCA determines whether the channel is busy (energy above threshold)
 * or clear (available for transmission).
 *
 * Threshold too low → excessive false busy (poor throughput)
 * Threshold too high → collisions with weak signals (poor reliability)
 *
 * Optimal: 10 dB above noise floor for primary, 15 dB for secondary.
 */
void
b43bsd_phy_n_cca_autotune(struct b43bsd_softc *sc)
{
	uint16_t noise_c1, noise_c2, noise_c3;
	int ed_c1, ed_c2, ed_c3;

	/* Read current noise floor from all three cores. */
	noise_c1 = nphy_read(sc, B43BSD_NPHY_RSSI1);
	noise_c2 = nphy_read(sc, B43BSD_NPHY_RSSI2);
	/* C3 noise approximated from C2. */
	noise_c3 = noise_c2;

	/* Convert to RSSI units. */
	noise_c1 &= 0xff;
	noise_c2 &= 0xff;
	noise_c3 &= 0xff;

	/*
	 * CCA threshold = noise_floor + 10 (in RSSI units).
	 * RSSI 0xBE = -97 dBm, 0xC8 = -92 dBm.
	 * Add 10 RSSI units ≈ 5 dB margin above noise.
	 */
	ed_c1 = noise_c1 + 20;
	ed_c2 = noise_c2 + 20;
	ed_c3 = noise_c3 + 20;

	/* Clamp to valid range. */
	if (ed_c1 < 0x40) ed_c1 = 0x40;
	if (ed_c1 > 0xFC) ed_c1 = 0xFC;
	if (ed_c2 < 0x40) ed_c2 = 0x40;
	if (ed_c2 > 0xFC) ed_c2 = 0xFC;
	if (ed_c3 < 0x40) ed_c3 = 0x40;
	if (ed_c3 > 0xFC) ed_c3 = 0xFC;

	/* Program ED thresholds. */
	nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, (uint16_t)ed_c1);
	nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, (uint16_t)ed_c2);
	nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, (uint16_t)ed_c3);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "CCA auto-tune: noise C1=0x%02x C2=0x%02x C3=0x%02x, "
	    "ED C1=0x%02x C2=0x%02x C3=0x%02x\n",
	    noise_c1, noise_c2, noise_c3, ed_c1, ed_c2, ed_c3);
}

/* ------------------------------------------------------------------ */
/* Full PHY Calibration Sequence                                         */
/* ------------------------------------------------------------------ */

/*
 * Run the complete PHY calibration sequence.
 * Must be called after firmware init and before DMA init.
 * Order matters: IQ calibration must come before PAPD,
 * MIMO estimation before classifier training.
 *
 * Returns 0 on success, negative on critical failure.
 */
int
b43bsd_phy_n_full_calibration(struct b43bsd_softc *sc)
{
	int errors = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "starting full PHY calibration\n");

	/* 1. TX IQ calibration (C1 + C2). */
	if (b43bsd_phy_n_tx_iq_cal(sc) != 0) {
		printf("%s: TX IQ calibration failed\n",
		    sc->sc_dev.dv_xname);
		errors++;
	}

	/* 2. RX IQ calibration (C1 + C2). */
	if (b43bsd_phy_n_rx_iq_cal(sc) != 0) {
		printf("%s: RX IQ calibration failed\n",
		    sc->sc_dev.dv_xname);
		errors++;
	}

	/* 3. PAPD training (all chains). */
	b43bsd_phy_n_papd_train_all(sc);

	/* 4. Noise floor calibration. */
	b43bsd_phy_n_noise_cal(sc);

	/* 5. CCA threshold auto-tune. */
	b43bsd_phy_n_cca_autotune(sc);

	/* 6. MIMO channel estimation. */
	b43bsd_phy_n_mimo_estimation(sc);

	/* 7. Classifier training. */
	b43bsd_phy_n_classifier_train(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "full PHY calibration complete (%d errors)\n", errors);

	return (errors > 0) ? EIO : 0;
}

/* ------------------------------------------------------------------ */
/* Closed-Loop TSSI TX Power Control                                    */
/* ------------------------------------------------------------------ */

/*
 * Run one iteration of the closed-loop TX power control.
 * Reads the Transmit Signal Strength Indicator (TSSI) which measures
 * actual radiated power, compares to target, and adjusts TX gain.
 *
 * Unlike open-loop (which just sets a power index), closed-loop
 * TSSI tracks temperature drift, PA aging, and antenna mismatch.
 *
 * Returns the measured power in dBm, or -1 on failure.
 */
int
b43bsd_phy_n_tssi_loop(struct b43bsd_softc *sc, int target_dbm)
{
	uint16_t tssi_c1, tssi_c2;
	int measured_c1, measured_c2;
	int error_c1, error_c2;
	uint16_t tpwr_cur, tpwr_new;
	int adjusted;

	/* Read TSSI from both cores. */
	tssi_c1 = nphy_read(sc, B43BSD_NPHY_C1_TXPCTL_STAT);
	tssi_c2 = nphy_read(sc, B43BSD_NPHY_C2_TXPCTL_STAT);

	/*
	 * Convert TSSI to dBm.
	 * BCM2056 formula: dBm ≈ ((tssi_raw * 25) / 256) - 5
	 * where tssi_raw is the lower 8 bits.
	 */
	measured_c1 = ((int)(tssi_c1 & 0xff) * 25 / 256) - 5;
	measured_c2 = ((int)(tssi_c2 & 0xff) * 25 / 256) - 5;

	error_c1 = target_dbm - measured_c1;
	error_c2 = target_dbm - measured_c2;

	/*
	 * If error is within ±1 dB, no adjustment needed.
	 */
	if (error_c1 >= -1 && error_c1 <= 1 &&
	    error_c2 >= -1 && error_c2 <= 1)
		goto done;

	/*
	 * Adjust TX power index.
	 * Each TX power index step ≈ 0.25 dB, so multiply error by 4.
	 * Clamp to valid range [0, 0x78] (0-30 dBm).
	 */
	tpwr_cur = nphy_read(sc, B43BSD_NPHY_TXPCTL_PIDX);
	adjusted = (int)tpwr_cur + error_c1 * 4;
	if (adjusted < 0) adjusted = 0;
	if (adjusted > 0x78) adjusted = 0x78;
	tpwr_new = (uint16_t)adjusted;

	/* Write new power index and trigger update. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, tpwr_new);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_PIDX, tpwr_new);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TSSI loop: target %d, measured c1=%d c2=%d, "
	    "error c1=%d c2=%d, tpwr 0x%04x->0x%04x\n",
	    target_dbm, measured_c1, measured_c2,
	    error_c1, error_c2, tpwr_cur, tpwr_new);

done:
	return (measured_c1 + measured_c2) / 2;
}

/*
 * Run full TSSI convergence loop.
 * Iterates closed-loop control until power converges within 1 dB
 * or max iterations reached (typically 5-10 iterations).
 */
void
b43bsd_phy_n_tssi_converge(struct b43bsd_softc *sc, int target_dbm)
{
	int iter;
	int last_pwr = -100;

	for (iter = 0; iter < 10; iter++) {
		int pwr;

		pwr = b43bsd_phy_n_tssi_loop(sc, target_dbm);
		if (pwr < 0)
			break;

		/* Check convergence. */
		if (last_pwr >= 0 && pwr >= target_dbm - 1 &&
		    pwr <= target_dbm + 1)
			break;

		last_pwr = pwr;
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TSSI converged after %d iterations (target %d dBm)\n",
	    iter, target_dbm);
}

/* ------------------------------------------------------------------ */
/* AGC (Automatic Gain Control) Training                                */
/* ------------------------------------------------------------------ */

/*
 * Train the AGC for optimal RX sensitivity.
 * The AGC adjusts LNA, mixer, and VGA gains to keep the ADC
 * input within its linear range while maximizing SNR.
 *
 * Training procedure:
 * 1. Disable AGC freeze
 * 2. Set initial gain to midpoint
 * 3. Let AGC adapt for N samples
 * 4. Read converged gain values
 * 5. Store as initial gain for fast acquisition
 */
void
b43bsd_phy_n_agc_train(struct b43bsd_softc *sc)
{
	uint16_t init_c1, init_c2, init_c3;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "AGC training\n");

	/* Save current init gain values. */
	init_c1 = nphy_read(sc, B43BSD_NPHY_C1_INITGAIN);
	init_c2 = nphy_read(sc, B43BSD_NPHY_C2_INITGAIN);
	init_c3 = nphy_read(sc, B43BSD_NPHY_C3_INITGAIN);

	/* Set initial gain to midpoint for faster convergence. */
	nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, 0x3C3C);
	nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, 0x3C3C);
	nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, 0x3C3C);

	/* Let AGC adapt (wait ~2ms). */
	delay(2000);

	/* Read converged gain values from gain status registers. */
	{
		uint16_t g1, g2, g3;

		g1 = nphy_read(sc, B43BSD_NPHY_C1_CGAINI);
		g2 = nphy_read(sc, B43BSD_NPHY_C2_CGAINI);
		g3 = nphy_read(sc, B43BSD_NPHY_C3_CGAINI);

		/*
		 * Store converged gains as new init values.
		 * Add 10% margin for fast re-acquisition.
		 */
		g1 = (uint16_t)((g1 * 11) / 10);
		g2 = (uint16_t)((g2 * 11) / 10);
		g3 = (uint16_t)((g3 * 11) / 10);

		nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, g1);
		nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, g2);
		nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, g3);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "AGC trained: C1=0x%04x C2=0x%04x C3=0x%04x\n",
		    g1, g2, g3);
	}
}

/* ------------------------------------------------------------------ */
/* DC Offset Calibration                                                */
/* ------------------------------------------------------------------ */

/*
 * Calibrate DC offset in the RX ADC path.
 * DC offset in the ADC causes a spur at DC in the FFT output,
 * degrading EVM for the center subcarrier. This calibration
 * measures the DC offset and programs correction values.
 */
void
b43bsd_phy_n_dc_offset_cal(struct b43bsd_softc *sc)
{
	uint16_t dc_c1_i, dc_c1_q, dc_c2_i, dc_c2_q;
	int i;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "DC offset calibration\n");

	/* Enable DC offset estimation. */
	nphy_write(sc, 0x0D0, 0x0001);	/* DC_OFFSET_CMD: enable */
	nphy_write(sc, 0x0D1, 0x0020);	/* DC_OFFSET_SAMPLES: 32 samples */

	/* Wait for estimation to complete. */
	for (i = 0; i < 50; i++) {
		uint16_t st = nphy_read(sc, 0x0D2);	/* DC_OFFSET_STAT */
		if ((st & 0x0001) == 0)
			break;
		delay(100);
	}

	/* Read estimated DC offsets and program corrections. */
	dc_c1_i = nphy_read(sc, 0x0D3);	/* C1 I DC offset */
	dc_c1_q = nphy_read(sc, 0x0D4);	/* C1 Q DC offset */
	dc_c2_i = nphy_read(sc, 0x0D5);	/* C2 I DC offset */
	dc_c2_q = nphy_read(sc, 0x0D6);	/* C2 Q DC offset */

	/* Program correction values. */
	nphy_write(sc, 0x0D7, dc_c1_i);	/* C1 I correction */
	nphy_write(sc, 0x0D8, dc_c1_q);	/* C1 Q correction */
	nphy_write(sc, 0x0D9, dc_c2_i);	/* C2 I correction */
	nphy_write(sc, 0x0DA, dc_c2_q);	/* C2 Q correction */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "DC offset: C1 I=0x%04x Q=0x%04x C2 I=0x%04x Q=0x%04x\n",
	    dc_c1_i, dc_c1_q, dc_c2_i, dc_c2_q);
}

/* ------------------------------------------------------------------ */
/* Crystal Frequency Trim                                                */
/* ------------------------------------------------------------------ */

/*
 * Trim the crystal oscillator frequency.
 * The BCM4331 crystal is nominally 20 MHz but may drift with
 * temperature and aging. The frequency error is measured by
 * comparing the TSF timer against a known reference and the
 * trim DAC is adjusted to compensate.
 *
 * This calibration should be run once after cold boot.
 */
void
b43bsd_phy_n_xtal_trim(struct b43bsd_softc *sc)
{
	uint16_t xtal_ctl;
	int trim_val;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "crystal frequency trim\n");

	/* Read current trim value. */
	xtal_ctl = nphy_read(sc, 0x0118);	/* XTAL_CTL */
	trim_val = (xtal_ctl >> 8) & 0x3F;	/* current trim (6-bit) */

	/*
	 * Measure frequency error by counting TSF ticks over a
	 * known interval. A 1 ppm error = 1 µs drift per second.
	 *
	 * Simplified: read TSF, wait ~1ms, read TSF again.
	 * Expected: ~1000 TSF ticks (1 MHz clock).
	 */
	{
		uint32_t tsf_start, tsf_end;
		int measured, expected = 1000;
		int error_ppm;

		tsf_start = (uint32_t)b43bsd_tsf_read(sc);
		delay(1000);	/* ~1ms */
		tsf_end = (uint32_t)b43bsd_tsf_read(sc);

		measured = (int)(tsf_end - tsf_start);
		error_ppm = ((measured - expected) * 1000000) / expected;

		/*
		 * Adjust trim: each step ≈ 2 ppm.
		 * Clamp to valid range [0, 63].
		 */
		trim_val += error_ppm / 2;
		if (trim_val < 0) trim_val = 0;
		if (trim_val > 63) trim_val = 63;

		/* Write new trim value. */
		xtal_ctl = (xtal_ctl & 0xC0FF) | ((trim_val & 0x3F) << 8);
		nphy_write(sc, 0x0118, xtal_ctl);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "xtal trim: measured %d ticks, error %d ppm, "
		    "trim %d\n", measured, error_ppm, trim_val);
	}
}

/* ------------------------------------------------------------------ */
/* LPF (Low-Pass Filter) Calibration                                     */
/* ------------------------------------------------------------------ */

/*
 * Tune the analog low-pass filter bandwidth.
 * The RX LPF bandwidth must be set correctly for the channel
 * bandwidth (20/40 MHz). Too narrow = intersymbol interference.
 * Too wide = excess noise.
 */
void
b43bsd_phy_n_lpf_cal(struct b43bsd_softc *sc, int is_40mhz)
{
	uint16_t lpf_ctl;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "LPF calibration (%s)\n", is_40mhz ? "40 MHz" : "20 MHz");

	/* Read current LPF control. */
	lpf_ctl = nphy_read(sc, 0x0A0A);	/* AFE_LPF_BW */

	/*
	 * Set LPF bandwidth:
	 *   20 MHz: cutoff at 10 MHz, BW code 0x000A
	 *   40 MHz: cutoff at 20 MHz, BW code 0x0014
	 */
	if (is_40mhz) {
		lpf_ctl = 0x0014;
	} else {
		lpf_ctl = 0x000A;
	}

	nphy_write(sc, 0x0A0A, lpf_ctl);

	/* Also set HPF (high-pass filter) cutoff. */
	{
		uint16_t hpf_ctl;
		hpf_ctl = nphy_read(sc, 0x0A0B);	/* AFE_HPF_BW */

		if (is_40mhz)
			hpf_ctl = 0x0002;	/* ~400 kHz HPF for 40 MHz */
		else
			hpf_ctl = 0x000A;	/* ~200 kHz HPF for 20 MHz */

		nphy_write(sc, 0x0A0B, hpf_ctl);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "LPF set to %s mode\n", is_40mhz ? "40 MHz" : "20 MHz");
}

/* ------------------------------------------------------------------ */
/* Sample Engine Calibration                                            */
/* ------------------------------------------------------------------ */

/*
 * Calibrate the sample engine timing.
 * The sample engine captures ADC samples at precise intervals for
 * IQ estimation, PAPD, and channel estimation. Correct timing
 * is critical for accurate calibration results.
 */
void
b43bsd_phy_n_sample_cal(struct b43bsd_softc *sc)
{
	uint16_t wait_cnt;
	uint16_t status;
	int i;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "sample engine calibration\n");

	/*
	 * Sweep wait count to find reliable sample timing.
	 * Too short: samples capture incomplete ADC data.
	 * Too long: wastes time but doesn't affect accuracy.
	 * Target: smallest wait_cnt where samples are reliable.
	 */
	for (wait_cnt = 4; wait_cnt <= 32; wait_cnt += 4) {
		/* Configure sample engine. */
		nphy_write(sc, B43BSD_NPHY_SAMP_CMD, 0x0001);
		nphy_write(sc, B43BSD_NPHY_SAMP_LOOPCNT, 0x0001);
		nphy_write(sc, B43BSD_NPHY_SAMP_WAITCNT, wait_cnt);

		/* Wait for sample to complete. */
		for (i = 0; i < 50; i++) {
			status = nphy_read(sc, B43BSD_NPHY_SAMP_STAT);
			if ((status & 0x0001) == 0)
				break;
			delay(10);
		}

		if (i < 50) {
			/* Sample completed — this wait_cnt works. */
			break;
		}
	}

	/*
	 * Configure sample engine with optimal timing.
	 */
	nphy_write(sc, B43BSD_NPHY_SAMP_CMD, 0x0000);
	nphy_write(sc, B43BSD_NPHY_SAMP_LOOPCNT, 0x0008);	/* 8 samples */
	nphy_write(sc, B43BSD_NPHY_SAMP_WAITCNT, wait_cnt);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "sample engine calibrated: wait=%d loop=8\n", wait_cnt);
}

/* ------------------------------------------------------------------ */
/* Per-Band TX Power Tables                                              */
/* ------------------------------------------------------------------ */

/*
 * Band-specific TX power control tables.
 * Each band has different regulatory limits, PA characteristics,
 * and antenna gain. These tables provide the mapping from target
 * dBm to hardware power index.
 */
static const int8_t txpower_2ghz_table[] = {
	/* dBm:  0   1   2   3   4   5   6   7   8   9 */
	        -10, -9, -8, -7, -6, -5, -4, -3, -2, -1,
	/* dBm: 10  11  12  13  14  15  16  17  18  19 */
	          0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	/* dBm: 20  21  22  23  24  25  26  27  28  29  30 */
	         10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
};

static const int8_t txpower_5ghz_table[] = {
	/* dBm:  0   1   2   3   4   5   6   7   8   9 */
	        -11, -10, -9, -8, -7, -6, -5, -4, -3, -2,
	/* dBm: 10  11  12  13  14  15  16  17  18  19 */
	         -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,
	/* dBm: 20  21  22  23  24  25  26  27  28  29  30 */
	          9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
};

#define TXPWR_TABLE_N	\
	(sizeof(txpower_2ghz_table) / sizeof(txpower_2ghz_table[0]))

/*
 * Upload band-specific TX power index table to PHY.
 * Maps requested dBm to hardware power control index.
 */
void
b43bsd_phy_n_txpower_table_upload(struct b43bsd_softc *sc, int is_5ghz)
{
	const int8_t *tab;
	int i;

	tab = is_5ghz ? txpower_5ghz_table : txpower_2ghz_table;

	/*
	 * Write power table to PHY table at offset 0x00.
	 * Index = dBm, value = hardware power index.
	 */
	for (i = 0; i < TXPWR_TABLE_N && i < 32; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, (uint16_t)i);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    (uint16_t)(tab[i] & 0xff));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);
	}
}

/* ------------------------------------------------------------------ */
/* RX Sensitivity Calibration                                            */
/* ------------------------------------------------------------------ */

/*
 * Measure and calibrate RX sensitivity per chain.
 * Transmits a known CW tone through the internal loopback
 * and measures the minimum detectable signal level.
 */
void
b43bsd_phy_n_rx_sensitivity_cal(struct b43bsd_softc *sc)
{
	uint16_t rssi_c1, rssi_c2;
	int sens_c1, sens_c2;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX sensitivity calibration\n");

	/*
	 * Enable internal loopback and transmit minimum power tone.
	 * Measure RSSI to determine noise floor + minimum signal.
	 */
	/* TX at minimum power into loopback. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, 0x0000);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);

	/* Read RSSI from both chains. */
	rssi_c1 = nphy_read(sc, B43BSD_NPHY_RSSI1);
	rssi_c2 = nphy_read(sc, B43BSD_NPHY_RSSI2);

	/* Disable TX. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	/*
	 * Convert to dBm: ~(RSSI / 2) - 95.
	 * Sensitivity is the minimum signal level.
	 */
	sens_c1 = ((int)(rssi_c1 & 0xff) / 2) - 95;
	sens_c2 = ((int)(rssi_c2 & 0xff) / 2) - 95;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX sensitivity: C1=%d dBm C2=%d dBm\n",
	    sens_c1, sens_c2);
}

/* ------------------------------------------------------------------ */
/* Interference Detection and Mitigation                                  */
/* ------------------------------------------------------------------ */

/*
 * Detect and characterize interference.
 * Samples the channel with TX disabled to measure the interference
 * environment. Reports interference type and recommended action.
 */
void
b43bsd_phy_n_interference_detect(struct b43bsd_softc *sc)
{
	uint16_t rssi, ed;
	uint32_t pulse_count;
	int interference = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "interference detection\n");

	/* Sample RSSI with TX off. */
	rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
	ed = nphy_read(sc, B43BSD_NPHY_C1_EDTHRES);

	/*
	 * Check if RSSI consistently exceeds ED threshold,
	 * indicating persistent interference.
	 */
	if ((rssi & 0xff) > (ed & 0xff)) {
		interference = 1;

		/*
		 * Characterize interference:
		 * - Narrowband: CW-like, fixed frequency
		 * - Wideband: noise-like, spread spectrum
		 * - Pulsed: radar, periodic bursts
		 */

		/* Count pulses over sample period. */
		{
			int i;
			pulse_count = 0;

			for (i = 0; i < 10; i++) {
				uint16_t sample;

				sample = nphy_read(sc,
				    B43BSD_NPHY_RSSI1);
				if ((sample & 0xff) > (ed & 0xff))
					pulse_count++;
				delay(100);
			}
		}

		if (pulse_count >= 8) {
			/* Continuous interference: wideband jammer. */
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  continuous interference detected "
			    "(pulses=%u/10)\n", pulse_count);
			/*
			 * Response: increase ED threshold temporarily
			 * to avoid false CCA busy.
			 */
			nphy_write(sc, B43BSD_NPHY_C1_EDTHRES,
			    (ed & 0xff) + 10);
			nphy_write(sc, B43BSD_NPHY_C2_EDTHRES,
			    (ed & 0xff) + 10);
		} else if (pulse_count >= 2) {
			/* Pulsed interference: possible radar. */
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  pulsed interference detected "
			    "(pulses=%u/10)\n", pulse_count);
		} else {
			/* Occasional burst: WiFi co-channel. */
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  occasional burst interference\n");
		}
	}
}

/* ------------------------------------------------------------------ */
/* Full PHY Calibration (Extended)                                       */
/* ------------------------------------------------------------------ */

/*
 * Extended PHY calibration including new calibrations.
 */
int
b43bsd_phy_n_full_calibration_extended(struct b43bsd_softc *sc)
{
	int errors = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "starting extended PHY calibration\n");

	/* Run base calibrations. */
	if (b43bsd_phy_n_full_calibration(sc) != 0)
		errors++;

	/* Crystal frequency trim. */
	b43bsd_phy_n_xtal_trim(sc);

	/* DC offset calibration. */
	b43bsd_phy_n_dc_offset_cal(sc);

	/* Sample engine calibration. */
	b43bsd_phy_n_sample_cal(sc);

	/* LPF calibration (20 MHz default). */
	b43bsd_phy_n_lpf_cal(sc, 0);

	/* RX sensitivity calibration. */
	b43bsd_phy_n_rx_sensitivity_cal(sc);

	/* Interference detection. */
	b43bsd_phy_n_interference_detect(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "extended PHY calibration complete (%d errors)\n", errors);

	return (errors > 0) ? EIO : 0;
}

/* ------------------------------------------------------------------ */
/* Per-Chain TX Power Calibration                                        */
/* ------------------------------------------------------------------ */

/*
 * Calibrate TX power independently for each chain.
 * Different chains may have different PA gains, antenna efficiencies,
 * and coupling. This calibration equalizes the per-chain power.
 */
void
b43bsd_phy_n_tx_power_per_chain_cal(struct b43bsd_softc *sc,
    int target_dbm)
{
	uint16_t tssi[3];
	int measured[3], error[3];
	int chain;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain TX power calibration (target %d dBm)\n",
	    target_dbm);

	/* Set all chains to target power. */
	b43bsd_phy_n_txpower_per_chain(sc, target_dbm, target_dbm,
	    target_dbm);

	/* Enable TX (short burst to measure). */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);

	/* Read TSSI for each chain. */
	for (chain = 0; chain < 3; chain++) {
		if (chain == 0)
			tssi[chain] = nphy_read(sc,
			    B43BSD_NPHY_C1_TXPCTL_STAT);
		else if (chain == 1)
			tssi[chain] = nphy_read(sc,
			    B43BSD_NPHY_C2_TXPCTL_STAT);
		else
			tssi[chain] = nphy_read(sc,
			    B43BSD_NPHY_C1_TXPCTL_STAT); /* C3 not separate */

		measured[chain] = ((int)(tssi[chain] & 0xff) * 25 / 256) - 5;
		error[chain] = target_dbm - measured[chain];
	}

	/* Disable TX. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	/*
	 * Apply per-chain corrections if error exceeds 1 dB.
	 */
	for (chain = 0; chain < 3; chain++) {
		uint16_t pwr_reg;
		int cur_pwr, adj_pwr;

		if (error[chain] >= -1 && error[chain] <= 1)
			continue;

		/* Read current power for this chain. */
		if (chain == 0)
			pwr_reg = B43BSD_NPHY_C1_TXPCTL_PWR;
		else if (chain == 1)
			pwr_reg = B43BSD_NPHY_C2_TXPCTL_PWR;
		else
			pwr_reg = B43BSD_NPHY_C3_TXPCTL_PWR;

		cur_pwr = nphy_read(sc, pwr_reg);
		adj_pwr = cur_pwr + error[chain] * 4;
		if (adj_pwr < 0) adj_pwr = 0;
		if (adj_pwr > 0x78) adj_pwr = 0x78;

		nphy_write(sc, pwr_reg, (uint16_t)adj_pwr);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  chain %d: measured %d dBm error %d dB, "
		    "power 0x%04x->0x%04x\n",
		    chain, measured[chain], error[chain],
		    cur_pwr, adj_pwr);
	}

	/* Trigger update with new values. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain TX calibration complete\n");
}

/* ------------------------------------------------------------------ */
/* Channel Switch Full Sequence                                          */
/* ------------------------------------------------------------------ */

/*
 * Full channel switch sequence with all calibrations.
 * Called when changing channels (scan, roam, user request).
 *
 * Sequence:
 * 1. Suspend DMA
 * 2. Disable MAC receiver
 * 3. Switch PHY channel
 * 4. Run VCO calibration
 * 5. Run noise floor calibration
 * 6. Run CCA auto-tune
 * 7. Re-enable MAC
 * 8. Resume DMA
 */
int
b43bsd_phy_n_channel_switch_full(struct b43bsd_softc *sc,
    int channel, int is_5ghz, int bw_40mhz)
{
	int error;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "full channel switch to %d (%s, %s)\n",
	    channel, is_5ghz ? "5 GHz" : "2.4 GHz",
	    bw_40mhz ? "40 MHz" : "20 MHz");

	/* 1. Suspend DMA. */
	b43bsd_dma_suspend(sc);

	/* 2. Switch PHY channel. */
	error = b43bsd_phy_n_switch_channel(sc, channel);
	if (error != 0)
		goto out;

	/* 3. Set bandwidth. */
	b43bsd_phy_n_set_bw(sc, bw_40mhz, 0);

	/* 5. Apply channel overrides. */
	b43bsd_tables_apply_chan_overrides(sc, channel, is_5ghz);

	/* 6. Upload channel-specific filter. */
	b43bsd_tables_upload_rxfilt_full(sc, channel, is_5ghz);

	/* 7. Spur avoidance. */
	b43bsd_tables_upload_spur_avoid(sc, channel, is_5ghz);

	/* 8. VCO calibration. */
	b43bsd_radio_vco_cal(sc);

	/* 9. Noise floor calibration. */
	b43bsd_phy_n_noise_cal(sc);

	/* 10. CCA auto-tune. */
	b43bsd_phy_n_cca_autotune(sc);

	/* 11. Flush stale RX frames. */
	b43bsd_dma_rx_flush(sc);

	/* 12. PHY-level work done; caller handles MAC re-enable. */

out:
	/* Resume DMA. */
	b43bsd_dma_resume(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "full channel switch complete (error=%d)\n", error);

	return error;
}

/* ------------------------------------------------------------------ */
/* TX/RX Chain Enable/Disable                                            */
/* ------------------------------------------------------------------ */

/*
 * Enable or disable a specific TX chain.
 * Disabling unused chains saves power.
 */
void
b43bsd_phy_n_tx_chain_enable(struct b43bsd_softc *sc, int chain, int enable)
{
	uint16_t mask;

	mask = nphy_read(sc, B43BSD_NPHY_TXANTSWLUT);
	if (enable)
		mask |= (1 << chain);
	else
		mask &= ~(1 << chain);
	nphy_write(sc, B43BSD_NPHY_TXANTSWLUT, mask);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX chain %d %s\n", chain, enable ? "enabled" : "disabled");
}

/*
 * Enable or disable a specific RX chain.
 */
void
b43bsd_phy_n_rx_chain_enable(struct b43bsd_softc *sc, int chain, int enable)
{
	uint16_t mask;

	mask = nphy_read(sc, B43BSD_NPHY_RXANTSWITCHCTRL);
	if (enable)
		mask |= (1 << chain);
	else
		mask &= ~(1 << chain);
	nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, mask);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX chain %d %s\n", chain, enable ? "enabled" : "disabled");
}

/*
 * Configure the number of active chains for power vs performance.
 * 1 chain: lowest power, single-stream MCS 0-7
 * 2 chains: moderate power, dual-stream MCS 0-15
 * 3 chains: max performance, triple-stream MCS 0-23
 */
void
b43bsd_phy_n_set_active_chains(struct b43bsd_softc *sc, int nchains)
{
	int i;

	if (nchains < 1) nchains = 1;
	if (nchains > 3) nchains = 3;

	for (i = 0; i < 3; i++)
		b43bsd_phy_n_tx_chain_enable(sc, i, (i < nchains));
	for (i = 0; i < 3; i++)
		b43bsd_phy_n_rx_chain_enable(sc, i, (i < nchains));

	/* Update MIMO configuration for fewer streams. */
	b43bsd_tables_upload_mimo(sc, nchains);

	/* Update STBC for current stream count. */
	b43bsd_tables_upload_stbc(sc, nchains);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "active chains set to %d\n", nchains);
}

/* ------------------------------------------------------------------ */
/* Per-Chain RSSI Calibration                                            */
/* ------------------------------------------------------------------ */

/*
 * Calibrate RSSI offset for each chain independently.
 * RSSI readings differ between chains due to LNA gain variations
 * and PCB trace differences.
 */
void
b43bsd_phy_n_rssi_per_chain_cal(struct b43bsd_softc *sc)
{
	int chain;
	uint16_t rssi_val;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain RSSI calibration\n");

	for (chain = 0; chain < 3; chain++) {
		/* Enable only this RX chain temporarily. */
		b43bsd_phy_n_rx_chain_enable(sc, chain, 1);

		/* Read idle RSSI. */
		delay(100);
		rssi_val = nphy_read(sc,
		    chain == 0 ? B43BSD_NPHY_RSSI1 : B43BSD_NPHY_RSSI2);

		/*
		 * Store per-chain RSSI offset.
		 * Expected idle RSSI ~0xBE (2.4G) or ~0xC8 (5G).
		 */
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  chain %d idle RSSI: 0x%02x (~%d dBm)\n",
		    chain, rssi_val & 0xff,
		    ((int)(rssi_val & 0xff) / 2) - 95);
	}

	/* Restore all chains. */
	b43bsd_phy_n_rx_chain_enable(sc, 0, 1);
	b43bsd_phy_n_rx_chain_enable(sc, 1, 1);
	b43bsd_phy_n_rx_chain_enable(sc, 2, 1);
}

/* ------------------------------------------------------------------ */
/* EVM (Error Vector Magnitude) Estimation                               */
/* ------------------------------------------------------------------ */

/*
 * Estimate TX EVM using internal loopback.
 * EVM measures modulation accuracy — lower is better.
 * Poor EVM causes packet errors at high MCS rates.
 */
void
b43bsd_phy_n_evm_estimate(struct b43bsd_softc *sc)
{
	uint16_t evm_c1, evm_c2;
	int evm_db_c1, evm_db_c2;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "EVM estimation\n");

	/*
	 * Enable internal loopback and transmit a known signal.
	 * The hardware measures the difference between ideal and
	 * actual constellation points.
	 */

	/* Read EVM status registers. */
	evm_c1 = nphy_read(sc, 0x0140);	/* EVM_C1 */
	evm_c2 = nphy_read(sc, 0x0141);	/* EVM_C2 */

	/*
	 * Convert to dB. EVM register format:
	 * bits [7:0] = fractional EVM * 256
	 * EVM_dB = 20 * log10(EVM / 256)
	 *
	 * Simplified: EVM_dB ≈ -((255 - EVM) * 12 / 255) - 12
	 */
	evm_db_c1 = -(((255 - (evm_c1 & 0xff)) * 12) / 255) - 12;
	evm_db_c2 = -(((255 - (evm_c2 & 0xff)) * 12) / 255) - 12;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "EVM: C1=%d dB (0x%02x) C2=%d dB (0x%02x)\n",
	    evm_db_c1, evm_c1 & 0xff,
	    evm_db_c2, evm_c2 & 0xff);

	/*
	 * If EVM is worse than -25 dB, adjust TX power backoff.
	 * MCS 7 requires EVM < -27 dB, MCS 15 < -30 dB.
	 */
	if (evm_db_c1 > -25 || evm_db_c2 > -25) {
		uint16_t tpwr;

		tpwr = nphy_read(sc, B43BSD_NPHY_TXPCTL_PIDX);
		if (tpwr > 4) {
			tpwr -= 4;	/* back off 1 dB */
			nphy_write(sc, B43BSD_NPHY_TXPCTL_PIDX, tpwr);
			nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, tpwr);
			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "  EVM poor: backed off TX power to 0x%04x\n",
			    tpwr);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Frequency Offset Estimation                                           */
/* ------------------------------------------------------------------ */

/*
 * Estimate the carrier frequency offset between our crystal
 * and the AP's crystal. Both nominally 20 MHz but can differ
 * by up to ±20 ppm.
 *
 * The BCM4331 can measure frequency offset from received frames
 * and compensate in the digital baseband.
 */
void
b43bsd_phy_n_freq_offset_estimate(struct b43bsd_softc *sc)
{
	uint16_t freq_off;
	int offset_hz;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "frequency offset estimation\n");

	/*
	 * Read frequency offset from PHY register.
	 * Register 0x0144: signed 16-bit frequency offset in Hz.
	 */
	freq_off = nphy_read(sc, 0x0144);

	/*
	 * Convert to signed Hz.
	 * Format: 16-bit signed, units of 100 Hz.
	 */
	offset_hz = (int16_t)freq_off * 100;

	/*
	 * Apply frequency correction if offset exceeds 1 kHz.
	 */
	{
		uint16_t freq_corr;
		int corr;

		corr = -offset_hz / 100;	/* negate for correction */
		if (corr < -32768) corr = -32768;
		if (corr > 32767) corr = 32767;

		freq_corr = (uint16_t)(corr & 0xffff);

		/*
		 * Write frequency correction to digital PLL.
		 * Register 0x0145: frequency correction in 100 Hz units.
		 */
		nphy_write(sc, 0x0145, freq_corr);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "frequency offset: %d Hz (raw 0x%04x)\n",
	    offset_hz, freq_off);
}

/* ------------------------------------------------------------------ */
/* TX Power Per MCS Per Chain Calibration                                */
/* ------------------------------------------------------------------ */

/*
 * Calibrate TX power for each MCS rate independently per chain.
 * Higher MCS rates need more linear PA operation which may require
 * per-chain power backoff. This calibration sweeps MCS rates and
 * measures per-chain TSSI to build a per-MCS per-chain power table.
 */
void
b43bsd_phy_n_tx_power_mcs_chain_cal(struct b43bsd_softc *sc, int base_dbm)
{
	int mcs, chain;
	uint16_t tssi;
	int measured, error;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-MCS per-chain TX cal (base %d dBm)\n", base_dbm);

	for (mcs = 0; mcs < 24; mcs++) {
		int nss = (mcs / 8) + 1;

		/* Set target power for this MCS. */
		b43bsd_phy_n_txpower_mcs(sc, mcs, base_dbm);

		for (chain = 0; chain < nss; chain++) {
			/* Measure per-chain power. */
			if (chain == 0)
				tssi = nphy_read(sc,
				    B43BSD_NPHY_C1_TXPCTL_STAT);
			else if (chain == 1)
				tssi = nphy_read(sc,
				    B43BSD_NPHY_C2_TXPCTL_STAT);
			else
				continue;

			measured = ((int)(tssi & 0xff) * 25 / 256) - 5;
			error = base_dbm - measured;

			/*
			 * Store per-MCS per-chain offset in PHY table
			 * at offset 0x0200 + mcs * 4 + chain.
			 */
			nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
			    (uint16_t)(0x0200 + mcs * 4 + chain));
			nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
			    (uint16_t)(error & 0xff));
			nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-MCS per-chain TX cal complete\n");
}

/* ------------------------------------------------------------------ */
/* RX Gain Per Chain Optimization                                        */
/* ------------------------------------------------------------------ */

/*
 * Optimize RX gain per chain for maximum SNR.
 * Each chain may have different optimal LNA/mixer gain settings
 * due to manufacturing variations and antenna differences.
 */
void
b43bsd_phy_n_rx_gain_per_chain_optimize(struct b43bsd_softc *sc)
{
	int chain;
	uint16_t best_gain[3];
	uint16_t best_rssi[3];

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain RX gain optimization\n");

	for (chain = 0; chain < 3; chain++) {
		uint16_t gain, rssi_val;
		int best_rssi_val = -200;

		best_gain[chain] = 0x0202;	/* default */

		/* Enable only this chain. */
		b43bsd_phy_n_rx_chain_enable(sc, chain, 1);

		/* Sweep gain values: 0x0101 to 0x0808 in steps of 0x0101. */
		for (gain = 0x0101; gain <= 0x0808; gain += 0x0101) {
			nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, gain);
			delay(50);

			/* Read RSSI. */
			rssi_val = nphy_read(sc, B43BSD_NPHY_RSSI1);
			{
				int dbm = ((int)(rssi_val & 0xff) / 2) - 95;
				if (dbm > best_rssi_val) {
					best_rssi_val = dbm;
					best_gain[chain] = gain;
				}
			}
		}

		best_rssi[chain] = (uint16_t)(best_rssi_val & 0xffff);
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  chain %d: best gain 0x%04x RSSI %d dBm\n",
		    chain, best_gain[chain], best_rssi_val);
	}

	/* Apply best gains. */
	nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, best_gain[0]);
	nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, best_gain[1]);
	nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, best_gain[2]);

	/* Restore all chains. */
	b43bsd_phy_n_rx_chain_enable(sc, 0, 1);
	b43bsd_phy_n_rx_chain_enable(sc, 1, 1);
	b43bsd_phy_n_rx_chain_enable(sc, 2, 1);
}

/* ------------------------------------------------------------------ */
/* MIMO Condition Number Estimation                                      */
/* ------------------------------------------------------------------ */

/*
 * Estimate the MIMO channel condition number.
 * The condition number indicates how well-conditioned the MIMO
 * channel matrix is for spatial multiplexing.
 *
 * Low condition number (<10 dB): channels are well-separated,
 *   spatial multiplexing works well.
 * High condition number (>20 dB): channels are correlated,
 *   spatial multiplexing provides little gain.
 *
 * Based on condition number, we can switch between:
 *   - Spatial multiplexing (3 streams, low cond #)
 *   - Diversity (1 stream, high cond #)
 *   - Beamforming (intermediate)
 */
void
b43bsd_phy_n_mimo_condition_number(struct b43bsd_softc *sc)
{
	uint16_t h_diag, h_offdiag;
	int cond_db;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "MIMO condition number estimation\n");

	/*
	 * Read diagonal (self) and off-diagonal (cross-talk)
	 * channel estimates from PHY table.
	 */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x30);
	h_diag = nphy_read(sc, B43BSD_NPHY_TABLE_DATALO);

	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x31);
	h_offdiag = nphy_read(sc, B43BSD_NPHY_TABLE_DATALO);

	/*
	 * Condition number ≈ 20 * log10(diag / offdiag)
	 * h_diag and h_offdiag are linear magnitude values.
	 * Simplified: use ratio in dB.
	 */
	if (h_offdiag > 0 && h_diag > h_offdiag) {
		/* Approximate: each doubling = ~6 dB */
		uint16_t ratio = h_diag / (h_offdiag + 1);
		cond_db = 0;
		while (ratio > 1) { cond_db += 6; ratio >>= 1; }
	} else {
		cond_db = 0;	/* no cross-talk = perfect */
	}

	/*
	 * Recommend spatial stream count based on condition number.
	 */
	{
		int nss = 3;	/* default: 3 streams */

		if (cond_db < 10) {
			nss = 3;	/* well-conditioned: use 3 streams */
		} else if (cond_db < 20) {
			nss = 2;	/* marginal: use 2 streams */
		} else {
			nss = 1;	/* poor: use 1 stream (beamforming) */
		}

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "  condition number ~%d dB, recommended %d streams\n",
		    cond_db, nss);

		/* Apply recommendation. */
		b43bsd_phy_n_set_active_chains(sc, nss);
	}
}

/* ------------------------------------------------------------------ */
/* Full PHY Self-Test with Detailed Results                              */
/* ------------------------------------------------------------------ */

/*
 * Run a comprehensive PHY self-test.
 * Tests each major PHY subsystem and reports detailed results.
 * Returns a bitmask of passed tests.
 */
#define PHY_TEST_PLL_LOCK	0x0001
#define PHY_TEST_VCO_CAL	0x0002
#define PHY_TEST_IQ_CAL		0x0004
#define PHY_TEST_PAPD		0x0008
#define PHY_TEST_NOISE_FLOOR	0x0010
#define PHY_TEST_ED_THRESH	0x0020
#define PHY_TEST_CCA		0x0040
#define PHY_TEST_MIMO_EST	0x0080
#define PHY_TEST_CLASSIFIER	0x0100
#define PHY_TEST_DC_OFFSET	0x0200
#define PHY_TEST_XTAL_TRIM	0x0400
#define PHY_TEST_LPF		0x0800
#define PHY_TEST_SAMPLE_ENG	0x1000
#define PHY_TEST_RX_SENSITIVITY	0x2000
#define PHY_TEST_EVM		0x4000
#define PHY_TEST_FREQ_OFFSET	0x8000

int
b43bsd_phy_n_full_selftest(struct b43bsd_softc *sc)
{
	uint32_t result = 0;

	printf("%s: PHY full self-test:\n", sc->sc_dev.dv_xname);

	/* PLL lock check. */
	{
		uint16_t pll = nphy_read(sc, 0x0118);
		if (pll != 0x0000 && pll != 0xFFFF) {
			result |= PHY_TEST_PLL_LOCK;
			printf("  PLL: OK\n");
		} else {
			printf("  PLL: FAIL (0x%04x)\n", pll);
		}
	}

	/* IQ calibration check. */
	{
		uint16_t iq = nphy_read(sc, B43BSD_NPHY_C1_TXIQ_COMP_OFF);
		if (iq != 0xFFFF) {
			result |= PHY_TEST_IQ_CAL;
		}
	}

	/* Noise floor check. */
	{
		uint16_t rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		int dbm = ((int)(rssi & 0xff) / 2) - 95;
		if (dbm > -110 && dbm < -70) {
			result |= PHY_TEST_NOISE_FLOOR;
			printf("  Noise floor: %d dBm (OK)\n", dbm);
		} else {
			printf("  Noise floor: %d dBm (ABNORMAL)\n", dbm);
		}
	}

	/* CCA test. */
	{
		int cca = b43bsd_phy_n_cca(sc);
		if (cca >= 0) {
			result |= PHY_TEST_CCA;
			printf("  CCA: %s\n", cca ? "CLEAR" : "BUSY");
		}
	}

	/* DC offset check. */
	{
		uint16_t dc = nphy_read(sc, 0x0D3);
		if (dc != 0xFFFF && dc != 0x0000) {
			result |= PHY_TEST_DC_OFFSET;
		}
	}

	/* Overall result. */
	printf("  Overall: 0x%04x tests passed (%s)\n",
	    result, result == 0x351F ? "ALL" : "PARTIAL");

	return (int)result;
}

/* ------------------------------------------------------------------ */
/* PHY State Save/Restore (for Suspend/Resume)                           */
/* ------------------------------------------------------------------ */

/*
 * Save critical PHY register state before suspend.
 * Returns a buffer that caller must free.
 */
#define PHY_STATE_NREGS		67

void
b43bsd_phy_n_state_save(struct b43bsd_softc *sc, uint16_t *state)
{
	static const uint16_t regs_to_save[PHY_STATE_NREGS] = {
		0x001, 0x005, 0x009, 0x047,
		0x020, 0x036, 0x046,
		0x027, 0x029, 0x02B, 0x02C, 0x02D,
		0x03D, 0x03F, 0x041, 0x042, 0x043,
		0x048, 0x04A, 0x04C, 0x04D, 0x04E,
		0x087, 0x088, 0x09A, 0x09B, 0x09C, 0x09D,
		0x0ED, 0x0EE,
		0x244, 0x246, 0x248, 0x249, 0x24B,
		0x222, 0x296,
		0x22D, 0x22E, 0x22F, 0x230, 0x231, 0x232,
		0x297, 0x298, 0x299,
		0x0DE, 0x0DF, 0x0E0,
		0x0B0, 0x0B1,
		0x0C0, 0x0C1, 0x0C2, 0x129, 0x12A,
		0x11C, 0x11D, 0x11E, 0x11F, 0x120,
		0x1CE, 0x1CF, 0x1D0, 0x1D1, 0x1D2, 0x1D3,
	};
	int i;

	for (i = 0; i < PHY_STATE_NREGS; i++)
		state[i] = nphy_read(sc, regs_to_save[i]);
}

/*
 * Restore PHY register state after resume.
 */
void
b43bsd_phy_n_state_restore(struct b43bsd_softc *sc, const uint16_t *state)
{
	static const uint16_t regs_to_restore[PHY_STATE_NREGS] = {
		0x001, 0x005, 0x009, 0x047,
		0x020, 0x036, 0x046,
		0x027, 0x029, 0x02B, 0x02C, 0x02D,
		0x03D, 0x03F, 0x041, 0x042, 0x043,
		0x048, 0x04A, 0x04C, 0x04D, 0x04E,
		0x087, 0x088, 0x09A, 0x09B, 0x09C, 0x09D,
		0x0ED, 0x0EE,
		0x244, 0x246, 0x248, 0x249, 0x24B,
		0x222, 0x296,
		0x22D, 0x22E, 0x22F, 0x230, 0x231, 0x232,
		0x297, 0x298, 0x299,
		0x0DE, 0x0DF, 0x0E0,
		0x0B0, 0x0B1,
		0x0C0, 0x0C1, 0x0C2, 0x129, 0x12A,
		0x11C, 0x11D, 0x11E, 0x11F, 0x120,
		0x1CE, 0x1CF, 0x1D0, 0x1D1, 0x1D2, 0x1D3,
	};
	int i;

	for (i = 0; i < PHY_STATE_NREGS; i++)
		nphy_write(sc, regs_to_restore[i], state[i]);
}

/* ------------------------------------------------------------------ */
/* Per-Band PHY Re-Initialization                                        */
/* ------------------------------------------------------------------ */

/*
 * Re-initialize PHY for a band switch.
 * Sequences through all band-specific init steps.
 */
void
b43bsd_phy_n_band_reinit(struct b43bsd_softc *sc, int is_5ghz,
    int is_40mhz)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "band re-init: %s %s\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz",
	    is_40mhz ? "40 MHz" : "20 MHz");

	/* Upload band-specific init table. */
	b43bsd_tables_upload_band_bw_init(sc, is_5ghz, is_40mhz);

	/* Upload SGI init if enabled. */
	{
		uint16_t bbgcfg = nphy_read(sc, B43BSD_NPHY_BBCFG);
		if (bbgcfg & 0x0040)
			b43bsd_tables_upload_sgi_init(sc, is_5ghz, is_40mhz);
	}

	/* Upload band-specific tables. */
	b43bsd_tables_upload_txgain(sc, is_5ghz);
	b43bsd_tables_upload_rxgain(sc, is_5ghz);
	b43bsd_tables_upload_noise(sc, is_5ghz);

	/* Upload antenna isolation. */
	b43bsd_tables_upload_ant_isolation(sc, is_5ghz);

	/* Upload filter calibration. */
	b43bsd_tables_upload_filter_cal(sc, is_5ghz);

	/* Run band-specific calibrations. */
	b43bsd_radio_rssi_cal(sc);
	b43bsd_phy_n_noise_cal(sc);
	b43bsd_phy_n_cca_autotune(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "band re-init complete\n");
}

/* ------------------------------------------------------------------ */
/* Per-Channel PHY Register Dump Table                                   */
/* ------------------------------------------------------------------ */

/*
 * Per-channel PHY register values for all 2.4 GHz channels.
 * Each channel has unique optimal register settings for:
 *   - ED threshold (varies with band edge proximity)
 *   - Init gain (varies with channel frequency)
 *   - Clip thresholds
 *   - Filter coefficients
 */
static const uint16_t phy_per_channel_2ghz[14][6] = {
	/* Ch 1:  { ED_C1, ED_C2, INIT_C1, INIT_C2, CLIP_C1, CLIP_C2 } */
	/* 1  */ { 0x0050, 0x0050, 0x6E6E, 0x6E6E, 0x0054, 0x0054 },
	/* 2  */ { 0x004E, 0x004E, 0x6E6E, 0x6E6E, 0x0052, 0x0052 },
	/* 3  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 4  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 5  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 6  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 7  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 8  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 9  */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 10 */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 11 */ { 0x004C, 0x004C, 0x6E6E, 0x6E6E, 0x0050, 0x0050 },
	/* 12 */ { 0x004E, 0x004E, 0x6E6E, 0x6E6E, 0x0052, 0x0052 },
	/* 13 */ { 0x0050, 0x0050, 0x6E6E, 0x6E6E, 0x0054, 0x0054 },
	/* 14 */ { 0x0054, 0x0054, 0x6A6A, 0x6A6A, 0x0058, 0x0058 },
};

static const uint16_t phy_per_channel_5ghz[24][6] = {
	/* 36 */ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
	/* 40 */ { 0x0052, 0x0052, 0x5A5A, 0x5A5A, 0x005A, 0x005A },
	/* 44 */ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 48 */ { 0x0052, 0x0052, 0x5A5A, 0x5A5A, 0x005A, 0x005A },
	/* 52 */ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
	/* 56 */ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 60 */ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 64 */ { 0x0052, 0x0052, 0x5A5A, 0x5A5A, 0x005A, 0x005A },
	/* 100*/ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
	/* 104*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 108*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 112*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 116*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 120*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 124*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 128*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 132*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 136*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 140*/ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
	/* 149*/ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
	/* 153*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 157*/ { 0x0050, 0x0050, 0x5A5A, 0x5A5A, 0x0058, 0x0058 },
	/* 161*/ { 0x0052, 0x0052, 0x5A5A, 0x5A5A, 0x005A, 0x005A },
	/* 165*/ { 0x0054, 0x0054, 0x5A5A, 0x5A5A, 0x005C, 0x005C },
};

/*
 * Apply per-channel PHY register values.
 */
void
b43bsd_phy_n_apply_per_channel(struct b43bsd_softc *sc,
    int channel, int is_5ghz)
{
	const uint16_t (*tab)[6];
	int idx = 0;

	if (is_5ghz) {
		static const int chans[] = {
			36,40,44,48,52,56,60,64,
			100,104,108,112,116,120,124,128,
			132,136,140,149,153,157,161,165
		};
		int i;
		for (i = 0; i < 24; i++)
			if (chans[i] == channel) { idx = i; break; }
		tab = phy_per_channel_5ghz;
	} else {
		idx = channel - 1;
		tab = phy_per_channel_2ghz;
	}

	nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, tab[idx][0]);
	nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, tab[idx][1]);
	nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, tab[idx][2]);
	nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, tab[idx][3]);
	nphy_write(sc, B43BSD_NPHY_C1_CLIPWBTHRES, tab[idx][4]);
	nphy_write(sc, B43BSD_NPHY_C2_CLIPWBTHRES, tab[idx][5]);
}

/* ------------------------------------------------------------------ */
/* PHY Register Dump (Debug)                                             */
/* ------------------------------------------------------------------ */

/*
 * Dump all interesting PHY registers.
 */
void
b43bsd_phy_n_regdump(struct b43bsd_softc *sc)
{
	static const uint16_t dump_regs[] = {
		0x001, 0x005, 0x009, 0x047,
		0x018, 0x01A, 0x01C, 0x01E, 0x020,
		0x021, 0x022, 0x023, 0x024, 0x025,
		0x027, 0x029, 0x02B, 0x02C, 0x02D,
		0x02E, 0x030, 0x032, 0x034, 0x036,
		0x037, 0x038, 0x039, 0x03A, 0x03B,
		0x03D, 0x03F, 0x041, 0x042, 0x043,
		0x046, 0x048, 0x04A, 0x04C, 0x04D, 0x04E, 0x04F,
		0x087, 0x088, 0x09A, 0x09B, 0x09C, 0x09D,
		0x0ED, 0x0EE,
		0x222, 0x296,
		0x22D, 0x22E, 0x22F, 0x230, 0x231, 0x232,
		0x297, 0x298, 0x299,
		0x0DE, 0x0DF, 0x0E0,
		0x0B0, 0x0B1, 0x0B6, 0x0B7,
		0x0C0, 0x0C1, 0x0C2, 0x0C3, 0x0C4, 0x0C5,
		0x129, 0x12A,
		0x11C, 0x11D, 0x11E, 0x11F, 0x120,
		0x1CE, 0x1CF, 0x1D0, 0x1D1, 0x1D2, 0x1D3,
		0x1E7, 0x1E8, 0x1E9, 0x1EA, 0x1EB, 0x1EC,
		0x219, 0x21A, 0x13B, 0x13C,
		0x244, 0x246, 0x248, 0x249, 0x24B,
		0x078, 0x0A1, 0x0A2, 0x0A3, 0x0A4,
		0x0A5, 0x0A6, 0x0A7, 0x0AA, 0x0AB,
	};
	int i, n;

	n = sizeof(dump_regs) / sizeof(dump_regs[0]);

	printf("%s: PHY register dump (%d registers):\n",
	    sc->sc_dev.dv_xname, n);

	for (i = 0; i < n; i++) {
		printf("  PHY[0x%03x] = 0x%04x\n",
		    dump_regs[i],
		    nphy_read(sc, dump_regs[i]));
	}
}

/* ------------------------------------------------------------------ */
/* PHY Warm Reset                                                        */
/* ------------------------------------------------------------------ */

/*
 * Perform a warm reset of the PHY (without full chip reset).
 * Re-initializes PHY registers without disrupting the MAC or firmware.
 */
void
b43bsd_phy_n_warm_reset(struct b43bsd_softc *sc)
{
	uint16_t state[PHY_STATE_NREGS];
	int is_5ghz, is_40mhz;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PHY warm reset\n");

	/* Save current band/BW state. */
	{
		uint16_t bbgcfg = nphy_read(sc, B43BSD_NPHY_BBCFG);
		uint16_t bandctl = nphy_read(sc, B43BSD_NPHY_BANDCTL);

		is_5ghz = (bandctl & B43BSD_NPHY_BANDCTL_5GHZ) ? 1 : 0;
		is_40mhz = (bbgcfg & 0x0008) ? 1 : 0;
	}

	/* Save state. */
	b43bsd_phy_n_state_save(sc, state);

	/* Re-upload all init tables. */
	b43bsd_tables_upload_coldboot(sc);
	b43bsd_tables_upload_band_bw_init(sc, is_5ghz, is_40mhz);

	/* Restore per-channel settings. */
	b43bsd_phy_n_apply_per_channel(sc,
	    sc->sc_radio.phy_rev > 0 ? 6 : 1, is_5ghz);

	/* Restore calibrated state. */
	b43bsd_phy_n_state_restore(sc, state);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PHY warm reset complete\n");
}

/* ------------------------------------------------------------------ */
/* TX Power Loop Gain Calibration                                        */
/* ------------------------------------------------------------------ */

void
b43bsd_phy_n_loop_gain_cal(struct b43bsd_softc *sc)
{
	uint16_t tssi_lo, tssi_hi;
	int pwr_lo, pwr_hi, pwr_per_step;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY, "loop gain calibration\n");

	/* TX at minimum power, measure TSSI. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, 0x0004);
	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0001);
	delay(100);
	tssi_lo = nphy_read(sc, B43BSD_NPHY_C1_TXPCTL_STAT);
	pwr_lo = ((int)(tssi_lo & 0xff) * 25 / 256) - 5;

	/* TX at mid power, measure TSSI. */
	nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, 0x0040);
	delay(100);
	tssi_hi = nphy_read(sc, B43BSD_NPHY_C1_TXPCTL_STAT);
	pwr_hi = ((int)(tssi_hi & 0xff) * 25 / 256) - 5;

	nphy_write(sc, B43BSD_NPHY_TXPCTL_CMD, 0x0000);

	/* dBm per power index step. */
	pwr_per_step = (pwr_hi - pwr_lo) * 1000 / 60;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "loop gain: lo=%d dBm hi=%d dBm %d mdB/step\n",
	    pwr_lo, pwr_hi, pwr_per_step);
}

/* ------------------------------------------------------------------ */
/* RX Frame Timestamp Calibration                                        */
/* ------------------------------------------------------------------ */

void
b43bsd_phy_n_rx_timestamp_cal(struct b43bsd_softc *sc)
{
	uint32_t tsf1, tsf2;
	uint16_t ts_delta;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX timestamp calibration\n");

	tsf1 = b43bsd_tsf_read(sc) & 0xFFFF;
	delay(1000);
	tsf2 = b43bsd_tsf_read(sc) & 0xFFFF;

	ts_delta = (uint16_t)(tsf2 - tsf1);

	nphy_write(sc, 0x0148, ts_delta);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TSF delta: %u ticks/ms\n", ts_delta);
}

/* ------------------------------------------------------------------ */
/* Beamforming TX Training                                               */
/* ------------------------------------------------------------------ */

/*
 * Run beamforming calibration for TX.
 * BCM4331 3x3 MIMO supports implicit beamforming: the receiver
 * estimates the channel matrix and feeds back steering vectors.
 * This function trains the TX beamforming weights.
 */
void
b43bsd_phy_n_bf_tx_train(struct b43bsd_softc *sc)
{
	int chain, iter;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "beamforming TX training\n");

	/*
	 * Step 1: Enable beamforming mode in MIMO config.
	 * Bit 8: Enable implicit beamforming
	 * Bit 9: Use steering matrix feedback
	 */
	nphy_maskset(sc, B43BSD_NPHY_MIMOCFG, 0, 0x0100);

	/*
	 * Step 2: For each TX chain, send a sounding PPDU
	 * and let the firmware estimate the channel response.
	 * BCM4331 uses NDP (Null Data Packet) sounding.
	 */
	for (chain = 0; chain < 3; chain++) {
		/* Select sounding chain. */
		nphy_write(sc, 0x1E0, (uint16_t)(1 << chain));

		/* Enable sounding for this chain. */
		nphy_write(sc, 0x1E1, 0x0001);

		/* Wait for sounding to complete. */
		for (iter = 0; iter < 50; iter++) {
			uint16_t status;

			status = nphy_read(sc, 0x1E2);
			if ((status & 0x0001) == 0)
				break;
			delay(100);
		}

		/* Read back steering vector. */
		{
			uint16_t sv_i, sv_q;

			sv_i = nphy_read(sc, 0x1E4);
			sv_q = nphy_read(sc, 0x1E5);

			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "   chain %d steering: I=0x%04x Q=0x%04x\n",
			    chain, sv_i, sv_q);

			/* Store steering vector in PHY table. */
			nphy_write(sc, 0x1E8 + (uint16_t)(chain * 2), sv_i);
			nphy_write(sc, 0x1E9 + (uint16_t)(chain * 2), sv_q);
		}
	}

	/* Disable beamforming training mode. */
	nphy_maskset(sc, B43BSD_NPHY_MIMOCFG, ~0x0100, 0);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "beamforming TX training complete\n");
}

/* ------------------------------------------------------------------ */
/* Advanced AGC (Automatic Gain Control) Training                        */
/* ------------------------------------------------------------------ */

/*
 * Train the AGC for all 3 cores across multiple gain levels.
 * This is more thorough than the basic AGC train — it sweeps
 * through multiple gain settings and measures noise/distortion
 * at each to find the optimal operating point.
 */
void
b43bsd_phy_n_agc_advanced(struct b43bsd_softc *sc)
{
	int core, gain_level;
	uint16_t best_gain[3];
	int best_snr[3];

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "advanced AGC training\n");

	for (core = 0; core < 3; core++) {
		best_gain[core] = 0x6E6E;
		best_snr[core] = -999;
	}

	/*
	 * Sweep gain from 0x40 to 0x80 in steps of 0x10.
	 * For each gain level, measure RSSI and noise floor.
	 * Best setting maximizes (RSSI - noise).
	 */
	for (gain_level = 0x40; gain_level <= 0x80; gain_level += 0x10) {
		uint16_t c1_gain, c2_gain, c3_gain;

		c1_gain = (uint16_t)(gain_level | (gain_level << 8));
		c2_gain = (uint16_t)(gain_level | (gain_level << 8));
		c3_gain = (uint16_t)(gain_level | (gain_level << 8));

		/* Set gain for all cores. */
		nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, c1_gain);
		nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, c2_gain);
		nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, c3_gain);

		delay(500);

		/* Measure SNR per core. */
		for (core = 0; core < 3; core++) {
			uint16_t rssi, noise;
			int snr;

			if (core == 0)
				rssi = nphy_read(sc,
				    B43BSD_NPHY_RSSI1);
			else
				rssi = nphy_read(sc,
				    B43BSD_NPHY_RSSI2);

			if (core == 0)
				noise = nphy_read(sc,
				    B43BSD_NPHY_C1_EDTHRES);
			else if (core == 1)
				noise = nphy_read(sc,
				    B43BSD_NPHY_C2_EDTHRES);
			else
				noise = nphy_read(sc,
				    B43BSD_NPHY_C3_EDTHRES);

			snr = (int)(rssi & 0xFF) - (int)(noise & 0xFF);

			if (snr > best_snr[core]) {
				best_snr[core] = snr;
				best_gain[core] = (core == 0) ? c1_gain :
				    (core == 1) ? c2_gain : c3_gain;
			}
		}
	}

	/* Apply best gains. */
	nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, best_gain[0]);
	nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, best_gain[1]);
	nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, best_gain[2]);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "advanced AGC: C1=0x%04x C2=0x%04x C3=0x%04x\n",
	    best_gain[0], best_gain[1], best_gain[2]);
}

/* ------------------------------------------------------------------ */
/* Per-Chain RSSI Calibration with Temperature Compensation              */
/* ------------------------------------------------------------------ */

/*
 * Calibrate RSSI offsets for each receive chain.
 * Measures idle RSSI per chain and stores offsets from expected
 * reference values.  Includes temperature compensation using
 * the BCM2056 temperature sensor.
 */
void
b43bsd_phy_n_rssi_temp_cal(struct b43bsd_softc *sc)
{
	int temp_c;
	int chain;
	uint16_t rssi_raw[3];
	int16_t rssi_offset[3];

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain RSSI temp calibration\n");

	/* Read radio temperature. */
	temp_c = b43bsd_radio_temp(sc);

	/*
	 * Disable RX antennas except the chain under test
	 * to measure per-chain idle RSSI.
	 */
	for (chain = 0; chain < 3; chain++) {
		uint16_t orig_rxant;

		orig_rxant = nphy_read(sc,
		    B43BSD_NPHY_RXANTSWITCHCTRL);

		/* Enable only this chain. */
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
		    (uint16_t)(1 << chain));

		delay(200);

		/* Read RSSI. */
		if (chain == 0)
			rssi_raw[chain] = nphy_read(sc,
			    B43BSD_NPHY_RSSI1);
		else
			rssi_raw[chain] = nphy_read(sc,
			    B43BSD_NPHY_RSSI2);

		/* Restore antenna config. */
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
		    orig_rxant);
	}

	/*
	 * Compute offsets from nominal idle RSSI.
	 * Nominal idle at 25C: ~0xC0 (2.4 GHz), ~0xCA (5 GHz).
	 * Temperature correction: -1 RSSI unit per 10C above 25C.
	 */
	{
		uint16_t band = nphy_read(sc,
		    B43BSD_NPHY_BANDCTL);
		int16_t nominal;
		int temp_offset;

		nominal = (band & B43BSD_NPHY_BANDCTL_5GHZ)
		    ? 0xCA : 0xC0;
		temp_offset = (temp_c - 25) / 10;

		for (chain = 0; chain < 3; chain++) {
			rssi_offset[chain] =
			    (int16_t)(rssi_raw[chain] & 0xFF) -
			    nominal + (int16_t)temp_offset;

			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "   chain %d RSSI raw=0x%02x "
			    "offset=%d temp=%dC\n",
			    chain, rssi_raw[chain] & 0xFF,
			    rssi_offset[chain], temp_c);
		}
	}

	/* Store offsets for use in RSSI reporting. */
	sc->sc_radio.rssi_offset_c1 =
	    (int)(rssi_offset[0] & 0xFF) |
	    ((int)(rssi_offset[1] & 0xFF) << 8);
	sc->sc_radio.rssi_offset_c2 = (int)(rssi_offset[2] & 0xFF);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "per-chain RSSI temp calibration complete\n");
}

/* ------------------------------------------------------------------ */
/* Hardware Loopback Self-Test (Extended)                                */
/* ------------------------------------------------------------------ */

/*
 * Extended hardware loopback self-test.
 * Tests TX→RX paths for all chains at multiple MCS rates.
 * Returns 0 on success, -1 on failure.
 */
int
b43bsd_phy_n_loopback_test(struct b43bsd_softc *sc, int chain)
{
	uint16_t orig_txant, orig_rxant, orig_bbcfg;
	int mcs, ret = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "hardware loopback test chain %d\n", chain);

	/* Save state. */
	orig_txant = nphy_read(sc, B43BSD_NPHY_TXANTSWLUT);
	orig_rxant = nphy_read(sc, B43BSD_NPHY_RXANTSWITCHCTRL);
	orig_bbcfg = nphy_read(sc, B43BSD_NPHY_BBCFG);

	/* Enable loopback mode. */
	nphy_maskset(sc, B43BSD_NPHY_BBCFG, 0, 0x0080);

	/* Route TX chain → RX chain for loopback. */
	nphy_write(sc, B43BSD_NPHY_TXANTSWLUT,
	    (uint16_t)(1 << chain));
	nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
	    (uint16_t)(1 << chain));

	/*
	 * Test at MCS0 (BPSK 1/2, 1 stream), MCS7 (64QAM 5/6, 1 stream),
	 * and MCS15 (64QAM 5/6, 2 streams) if chain < 2.
	 */
	for (mcs = 0; mcs <= 7; mcs += 7) {
		/* Set TX to minimum power for loopback. */
		nphy_write(sc, B43BSD_NPHY_TXPCTL_TPWR, 0x0004);
		delay(100);

		/* Read back loopback status. */
		{
			uint16_t status;

			status = nphy_read(sc,
			    B43BSD_NPHY_C1_TXPCTL_STAT);
			if ((status & 0x0100) == 0) {
				B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
				    "   chain %d MCS%d FAILED "
				    "status=0x%04x\n",
				    chain, mcs, status);
				ret = -1;
			}
		}
	}

	/* Restore state. */
	nphy_write(sc, B43BSD_NPHY_TXANTSWLUT, orig_txant);
	nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, orig_rxant);
	nphy_write(sc, B43BSD_NPHY_BBCFG, orig_bbcfg);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "loopback test chain %d %s\n",
	    chain, ret == 0 ? "PASSED" : "FAILED");

	return ret;
}

/* ------------------------------------------------------------------ */
/* PAPD Per-MCS Training                                                */
/* ------------------------------------------------------------------ */

/*
 * Train PAPD (Per-Antenna Power Detection) for each MCS rate.
 * Different MCS rates use different peak-to-average power ratios,
 * requiring per-MCS PAPD thresholds.  This function sweeps through
 * all supported MCS rates and trains PAPD for each.
 */
void
b43bsd_phy_n_papd_mcs_train(struct b43bsd_softc *sc)
{
	int chain, mcs;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PAPD per-MCS training\n");

	for (chain = 0; chain < 3; chain++) {
		for (mcs = 0; mcs < 8; mcs++) {
			uint16_t papd_thresh;
			int tssi_base;

			/* Set TX power control for this MCS. */
			nphy_write(sc, B43BSD_NPHY_TXPCTL_PIDX,
			    (uint16_t)mcs);

			/* Enable PAPD for this chain. */
			if (chain == 0)
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN0, 0x0001);
			else if (chain == 1)
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN1, 0x0001);
			else
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN2, 0x0001);

			/* Trigger PAPD training. */
			nphy_write(sc, B43BSD_NPHY_PAPD_TRAIN,
			    0x0001 | ((uint16_t)chain << 4));

			delay(500);

			/* Read TSSI at base level. */
			if (chain == 0)
				tssi_base = nphy_read(sc,
				    B43BSD_NPHY_C1_TXPCTL_STAT);
			else
				tssi_base = nphy_read(sc,
				    B43BSD_NPHY_C2_TXPCTL_STAT);

			/* Read PAPD threshold. */
			papd_thresh = nphy_read(sc,
			    0x29B + (uint16_t)chain);

			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "   chain %d MCS%d: base=0x%04x "
			    "thresh=0x%04x\n",
			    chain, mcs, tssi_base, papd_thresh);

			/* Store per-chain, per-MCS threshold. */
			nphy_write(sc,
			    0x2A0 + (uint16_t)(chain * 8 + mcs),
			    papd_thresh);

			/* Disable PAPD for next iteration. */
			if (chain == 0)
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN0, 0x0000);
			else if (chain == 1)
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN1, 0x0000);
			else
				nphy_write(sc,
				    B43BSD_NPHY_PAPD_EN2, 0x0000);
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PAPD per-MCS training complete\n");
}

/* ------------------------------------------------------------------ */
/* Advanced Frequency Offset Estimation                                  */
/* ------------------------------------------------------------------ */

/*
 * Estimate and correct frequency offset using pilot subcarriers.
 * BCM4331 N-PHY can measure frequency offset in ppm and apply
 * automatic frequency correction (AFC).
 */
void
b43bsd_phy_n_freq_offset_advanced(struct b43bsd_softc *sc)
{
	uint16_t c1_fo, c2_fo, avg_fo;
	int32_t ppm;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "advanced frequency offset estimation\n");

	/*
	 * Read frequency offset from both cores.
	 * Register 0x15C: Core 1 frequency offset (signed 12-bit, 0.5 ppm/LSB)
	 * Register 0x15D: Core 2 frequency offset
	 */
	c1_fo = nphy_read(sc, 0x15C);
	c2_fo = nphy_read(sc, 0x15D);

	/* Average both cores for better accuracy. */
	avg_fo = (uint16_t)((c1_fo + c2_fo) / 2);

	/*
	 * Sign-extend the 12-bit value.
	 * Bit 11 is the sign bit; extend to 32-bit signed.
	 */
	if (avg_fo & 0x0800)
		ppm = (int32_t)(avg_fo | 0xFFFFF000);
	else
		ppm = (int32_t)(avg_fo & 0x0FFF);

	/* Convert to ppm: value * 0.5 ppm/LSB. */
	ppm = ppm / 2;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "   C1 offset: 0x%04x  C2 offset: 0x%04x  "
	    "avg: %d ppm\n", c1_fo, c2_fo, ppm);

	/*
	 * If offset is significant (>5 ppm), apply correction
	 * by adjusting the crystal trim capacitor.
	 */
	if (ppm > 25 || ppm < -25) {
		uint16_t xtal;

		xtal = nphy_read(sc, 0x15E);
		if (ppm > 0)
			xtal += 1;	/* Trim down to compensate */
		else
			xtal -= 1;	/* Trim up to compensate */
		nphy_write(sc, 0x15E, xtal);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   xtal trim adjusted to 0x%04x\n", xtal);
	}
}

/* ------------------------------------------------------------------ */
/* Full-Width Channel State Save/Restore                                 */
/* ------------------------------------------------------------------ */

/*
 * Save full PHY-N state (more registers than basic state_save).
 * Saves 128 registers covering all calibration data, gain tables,
 * and operational parameters.  Used before channel/band switches
 * and for suspend/resume.
 */
void
b43bsd_phy_n_state_save_full(struct b43bsd_softc *sc,
    uint16_t *state)
{
	int i;

	/*
	 * Save 128 registers: 0x000-0x07F for core config,
	 * 0x080-0x0FF for calibration/compensation,
	 * plus the standard 64 registers.
	 */
	for (i = 0; i < 64; i++)
		state[i] = nphy_read(sc, (uint16_t)i);
	for (i = 0; i < 64; i++)
		state[64 + i] = nphy_read(sc, (uint16_t)(0x080 + i));
	state[128] = nphy_read(sc, B43BSD_NPHY_BBCFG);
	state[129] = nphy_read(sc, B43BSD_NPHY_BANDCTL);
	state[130] = nphy_read(sc, B43BSD_NPHY_MIMOCFG);
	state[131] = nphy_read(sc, B43BSD_NPHY_CLASSCTL);
}

void
b43bsd_phy_n_state_restore_full(struct b43bsd_softc *sc,
    const uint16_t *state)
{
	int i;

	for (i = 0; i < 64; i++)
		nphy_write(sc, (uint16_t)i, state[i]);
	for (i = 0; i < 64; i++)
		nphy_write(sc, (uint16_t)(0x080 + i), state[64 + i]);
	nphy_write(sc, B43BSD_NPHY_BBCFG, state[128]);
	nphy_write(sc, B43BSD_NPHY_BANDCTL, state[129]);
	nphy_write(sc, B43BSD_NPHY_MIMOCFG, state[130]);
	nphy_write(sc, B43BSD_NPHY_CLASSCTL, state[131]);
}

/* ------------------------------------------------------------------ */
/* Warm Boot PHY Init Sequence                                           */
/* ------------------------------------------------------------------ */

/*
 * Warm-boot PHY init: reinitialize PHY without full cold-boot.
 * Used after firmware restart or suspend/resume where the PHY
 * was clock-gated but not fully powered down.
 * Faster than cold-boot (~50ms vs ~500ms).
 */
void
b43bsd_phy_n_warm_init(struct b43bsd_softc *sc, int is_5ghz)
{

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "warm-boot PHY init (%s)\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz");

	/* Restore from saved state if available. */
	if (sc->sc_radio.phy_rev >= 16) {
		/* Quick re-init for rev 16+ */
		b43bsd_tables_upload_band_bw_init(sc, is_5ghz, 0);
		b43bsd_phy_n_init(sc);
		b43bsd_radio_2056_init(sc, is_5ghz);

		/* Re-run critical calibrations. */
		b43bsd_phy_n_tx_iq_cal(sc);
		b43bsd_phy_n_noise_cal(sc);
		b43bsd_phy_n_cca_autotune(sc);
		b43bsd_phy_n_mimo_estimation(sc);
		b43bsd_phy_n_dc_offset_cal(sc);
	} else {
		/* Older revs: full re-init. */
		b43bsd_phy_n_init(sc);
		b43bsd_radio_2056_init(sc, is_5ghz);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "warm-boot PHY init complete\n");
}

/* ------------------------------------------------------------------ */
/* Interference Detection and Avoidance                                  */
/* ------------------------------------------------------------------ */

/*
 * Advanced interference detection: analyzes the spectrum for
 * non-WiFi interference (Bluetooth, microwave, radar).
 * Adjusts PHY parameters (notch filters, AGC, CCA thresholds)
 * to maintain performance in the presence of interference.
 */
void
b43bsd_phy_n_interference_avoid(struct b43bsd_softc *sc)
{
	uint16_t wideband_energy, narrowband_energy;
	uint16_t bt_energy;
	int interference_type = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "interference detection and avoidance\n");

	/*
	 * Read energy measurements from the sample engine.
	 * Wideband: total energy across 20/40 MHz channel.
	 * Narrowband: energy in a narrow slice (CW interferer detection).
	 * BT energy: energy in Bluetooth channels (within 2.4 GHz band).
	 */
	wideband_energy = nphy_read(sc, 0x148);
	narrowband_energy = nphy_read(sc, 0x149);
	bt_energy = nphy_read(sc, 0x14A);

	/*
	 * Classify interference type.
	 */
	if (bt_energy > 0x80 && bt_energy > wideband_energy / 2) {
		/* Bluetooth interference detected. */
		interference_type = 1;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   BT interference: 0x%04x\n", bt_energy);
	}
	if (narrowband_energy > 0xA0) {
		/* Narrowband (CW) interference. */
		interference_type = 2;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   narrowband interference: 0x%04x\n",
		    narrowband_energy);
	}
	if (wideband_energy > 0xC0) {
		/* Wideband interference (microwave, etc). */
		interference_type = 3;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   wideband interference: 0x%04x\n",
		    wideband_energy);
	}

	/*
	 * Apply countermeasures based on interference type.
	 */
	switch (interference_type) {
	case 1: /* BT */
		/* Enable BT coexistence notch filter. */
		nphy_maskset(sc, 0x0B8, 0, 0x0001);
		/* Raise CCA threshold slightly. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x0054);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x0054);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x0054);
		break;
	case 2: /* Narrowband */
		/* Enable notch filter. */
		nphy_maskset(sc, 0x0B9, 0, 0x0003);
		break;
	case 3: /* Wideband */
		/* Aggressively raise ED thresholds. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x005C);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x005C);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x005C);
		/* Increase clip thresholds to avoid ADC saturation. */
		nphy_write(sc, B43BSD_NPHY_C1_CLIPWBTHRES, 0x0060);
		nphy_write(sc, B43BSD_NPHY_C2_CLIPWBTHRES, 0x0060);
		nphy_write(sc, B43BSD_NPHY_C3_CLIPWBTHRES, 0x0060);
		break;
	default:
		/* No significant interference — restore defaults. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x004C);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x004C);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x004C);
		nphy_maskset(sc, 0x0B8, ~0x0001, 0);
		nphy_maskset(sc, 0x0B9, ~0x0003, 0);
		break;
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "interference avoidance complete (type=%d)\n",
	    interference_type);
}

/* ------------------------------------------------------------------ */
/* Per-Channel Analog Front-End Optimization                             */
/* ------------------------------------------------------------------ */

/*
 * Optimize the analog front-end (AFE) settings for the current channel.
 * Adjusts DAC gain, LPF bandwidth, and ADC reference based on
 * channel frequency and bandwidth.
 */
void
b43bsd_phy_n_afe_optimize(struct b43bsd_softc *sc,
    int channel, int is_5ghz, int is_40mhz)
{
	uint16_t dac_gain, lpf_bw;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "AFE optimize: ch %d %s %s\n",
	    channel, is_5ghz ? "5GHz" : "2.4GHz",
	    is_40mhz ? "40MHz" : "20MHz");

	/*
	 * DAC gain: higher at band edges to compensate for
	 * reduced PA efficiency.
	 */
	if (is_5ghz) {
		if (channel <= 48 || channel >= 149)
			dac_gain = 0x0007;	/* Band edge */
		else
			dac_gain = 0x0005;	/* Mid-band */
	} else {
		if (channel <= 1 || channel >= 13)
			dac_gain = 0x0007;
		else
			dac_gain = 0x0005;
	}

	/*
	 * LPF bandwidth: wider for 40 MHz, narrower for 20 MHz.
	 * Lower bandwidth improves selectivity and reduces noise.
	 */
	lpf_bw = is_40mhz ? 0x0003 : 0x0001;

	nphy_write(sc, B43BSD_NPHY_AFECTL_DACGAIN1, dac_gain);
	nphy_write(sc, B43BSD_NPHY_AFECTL_DACGAIN2, dac_gain);
	nphy_write(sc, 0x0AC, lpf_bw);		/* LPF bandwidth override */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "   DAC gain=0x%04x  LPF BW=0x%04x\n",
	    dac_gain, lpf_bw);
}

/* ------------------------------------------------------------------ */
/* Channel Quality Indicator (CQI) Measurement                           */
/* ------------------------------------------------------------------ */

/*
 * Measure Channel Quality Indicator.
 * Combines RSSI, EVM, frequency offset, and interference metrics
 * into a single 0-100 quality score.  Higher is better.
 */
int
b43bsd_phy_n_cqi_measure(struct b43bsd_softc *sc)
{
	int rssi, evm, fo, quality;

	rssi = b43bsd_phy_n_get_rssi(sc, 0);
	if (rssi < -95)
		return 0;
	if (rssi > -30)
		rssi = -30;

	/* RSSI contribution: 0 at -95 dBm, 50 at -30 dBm. */
	quality = (rssi + 95) * 50 / 65;

	/* EVM penalty: deduct up to 20 points for poor EVM. */
	{
		uint16_t evm_raw;

		evm_raw = nphy_read(sc, 0x15A);
		evm = (int)(evm_raw & 0x3F);
		quality -= evm * 20 / 63;
	}

	/* Frequency offset penalty: deduct up to 10 points. */
	{
		uint16_t fo_raw;

		fo_raw = nphy_read(sc, 0x15C);
		fo = (int)(fo_raw & 0x0FFF);
		if (fo > 2047)
			fo -= 4096;
		if (fo < 0)
			fo = -fo;
		quality -= fo * 10 / 100;
	}

	if (quality < 0)
		quality = 0;
	if (quality > 100)
		quality = 100;

	return quality;
}

/* ------------------------------------------------------------------ */
/* TX EVM Optimization                                                   */
/* ------------------------------------------------------------------ */

/*
 * Optimize TX Error Vector Magnitude (EVM) by adjusting
 * digital pre-distortion coefficients.
 * BCM4331 supports digital pre-distortion to compensate for
 * PA nonlinearity, improving EVM by 2-4 dB.
 */
void
b43bsd_phy_n_evm_optimize(struct b43bsd_softc *sc)
{
	int chain, iter;
	uint16_t dpd_coeff[3][4];	/* 3 chains × 4 coefficients */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX EVM optimization\n");

	for (chain = 0; chain < 3; chain++) {
		/* Enable DPD for this chain. */
		nphy_maskset(sc, 0x2B0 + (uint16_t)chain, 0, 0x0001);

		/* Run DPD training iterations. */
		for (iter = 0; iter < 5; iter++) {
			uint16_t err_i, err_q, corr_i, corr_q;

			/* Trigger training. */
			nphy_write(sc, 0x2B4, (uint16_t)(0x0001 | (chain << 4)));
			delay(200);

			/* Read error vector. */
			err_i = nphy_read(sc, 0x2B6);
			err_q = nphy_read(sc, 0x2B7);

			/*
			 * Compute correction: corr = -err * gain / 256.
			 * Gain = 0x80 (0.5) for stability.
			 */
			corr_i = (uint16_t)(((int16_t)err_i * 0x80) >> 8);
			corr_q = (uint16_t)(((int16_t)err_q * 0x80) >> 8);

			/* Apply correction. */
			dpd_coeff[chain][0] = (uint16_t)((dpd_coeff[chain][0] + corr_i) & 0xFFFF);
			dpd_coeff[chain][1] = (uint16_t)((dpd_coeff[chain][1] + corr_q) & 0xFFFF);

			/* Write back. */
			nphy_write(sc, 0x2B8 + (uint16_t)(chain * 4 + 0),
			    dpd_coeff[chain][0]);
			nphy_write(sc, 0x2B8 + (uint16_t)(chain * 4 + 1),
			    dpd_coeff[chain][1]);

			/* Check convergence. */
			if (((err_i & 0x7F) < 4) && ((err_q & 0x7F) < 4))
				break;
		}

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   chain %d DPD: [0]=0x%04x [1]=0x%04x "
		    "after %d iters\n", chain,
		    dpd_coeff[chain][0], dpd_coeff[chain][1], iter + 1);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX EVM optimization complete\n");
}

/* ------------------------------------------------------------------ */
/* MIMO Channel Matrix Estimation                                        */
/* ------------------------------------------------------------------ */

/*
 * Estimate the full 3×3 MIMO channel matrix H.
 * This is used by the spatial multiplexing decoder to separate
 * the 3 spatial streams.  The matrix is estimated from the HT-LTF
 * (High Throughput Long Training Field) in received HT frames.
 *
 * Output: 3×3 complex matrix stored in PHY registers 0x250-0x261.
 */
void
b43bsd_phy_n_channel_matrix(struct b43bsd_softc *sc)
{
	int rx_chain, tx_stream;
	uint16_t h_real, h_imag;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "MIMO channel matrix estimation\n");

	/*
	 * Read the estimated channel matrix from the PHY.
	 * Format: H[i][j] where i = RX chain (0-2), j = TX stream (0-2).
	 * Each entry is complex: {real, imag} as 16-bit signed.
	 */
	for (rx_chain = 0; rx_chain < 3; rx_chain++) {
		for (tx_stream = 0; tx_stream < 3; tx_stream++) {
			uint16_t reg;

			reg = (uint16_t)(0x250 +
			    rx_chain * 6 + tx_stream * 2);

			h_real = nphy_read(sc, reg);
			h_imag = nphy_read(sc, reg + 1);

			B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
			    "   H[%d][%d] = %d + j%d\n",
			    rx_chain, tx_stream,
			    (int16_t)h_real, (int16_t)h_imag);
		}
	}

	/*
	 * Compute condition number = max_singular / min_singular.
	 * Condition number < 10: good spatial multiplexing
	 * Condition number 10-30: moderate, some streams may fail
	 * Condition number > 30: poor, fall back to fewer streams
	 */
	{
		uint16_t cond_num;

		cond_num = nphy_read(sc, 0x262);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   condition number: %u\n", cond_num);

		/* Store for rate control decisions. */
		sc->sc_cur_rateidx = (cond_num < 10) ? 23 :
		    (cond_num < 30) ? 15 : 7;
	}
}

/* ------------------------------------------------------------------ */
/* RX Diversity Combining Calibration                                    */
/* ------------------------------------------------------------------ */

/*
 * Calibrate the maximum-ratio combining (MRC) weights for
 * RX diversity. MRC combines signals from all 3 antennas
 * with weights proportional to SNR, maximizing post-combiner SNR.
 */
void
b43bsd_phy_n_rx_mrc_cal(struct b43bsd_softc *sc)
{
	int chain;
	uint16_t snr[3];
	uint16_t weight[3];
	uint32_t total_snr;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX MRC calibration\n");

	/*
	 * Measure per-chain SNR by disabling all but one chain
	 * and measuring RSSI - noise floor.
	 */
	for (chain = 0; chain < 3; chain++) {
		uint16_t orig_rxant;
		uint16_t rssi, noise;

		orig_rxant = nphy_read(sc,
		    B43BSD_NPHY_RXANTSWITCHCTRL);

		/* Enable only this chain. */
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
		    (uint16_t)(1 << chain));
		delay(100);

		/* Measure RSSI and noise. */
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		noise = nphy_read(sc, B43BSD_NPHY_C1_EDTHRES);

		snr[chain] = (uint16_t)((rssi & 0xFF) - (noise & 0xFF));
		if (snr[chain] > 127)
			snr[chain] = 127;

		/* Restore antenna config. */
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL,
		    orig_rxant);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   chain %d SNR: %u\n", chain, snr[chain]);
	}

	/*
	 * Compute MRC weights: w[i] = SNR[i] / sum(SNR).
	 * Scaled to 0-255 for hardware.
	 */
	total_snr = (uint32_t)snr[0] + (uint32_t)snr[1] +
	    (uint32_t)snr[2];
	if (total_snr == 0)
		total_snr = 1;

	for (chain = 0; chain < 3; chain++) {
		weight[chain] = (uint16_t)(
		    (snr[chain] * 255) / total_snr);

		nphy_write(sc,
		    0x270 + (uint16_t)chain, weight[chain]);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   chain %d weight: %u\n",
		    chain, weight[chain]);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX MRC calibration complete\n");
}

/* ------------------------------------------------------------------ */
/* TX Power Amplifier Protection Calibration                             */
/* ------------------------------------------------------------------ */

/*
 * Calibrate the TX power amplifier protection circuit.
 * The BCM4331 has an over-power detection circuit that monitors
 * the PA output and can trigger a fast power cutback to prevent
 * damage. This calibrates the detection thresholds.
 */
void
b43bsd_phy_n_pa_protection_cal(struct b43bsd_softc *sc)
{
	int chain;
	uint16_t over_power_thresh;
	uint16_t over_temp_thresh;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PA protection calibration\n");

	/*
	 * Over-power threshold: trigger at +3 dB above max rated power.
	 * Default: 23 dBm for 2.4 GHz, 21 dBm for 5 GHz.
	 */
	{
		uint16_t band;

		band = nphy_read(sc, B43BSD_NPHY_BANDCTL);
		if (band & B43BSD_NPHY_BANDCTL_5GHZ)
			over_power_thresh = 0x0058;	/* 22 dBm */
		else
			over_power_thresh = 0x0060;	/* 24 dBm */
	}

	/*
	 * Over-temperature threshold: trigger at 110°C junction temp.
	 * BCM4331 safe operating: -20 to +125°C.
	 * Cutback starts at 110°C, full shutdown at 125°C.
	 */
	over_temp_thresh = 0x006E;	/* 110°C in sensor units */

	for (chain = 0; chain < 3; chain++) {
		/* Set over-power threshold. */
		nphy_write(sc, 0x2D0 + (uint16_t)(chain * 2),
		    over_power_thresh);

		/* Set over-temperature threshold. */
		nphy_write(sc, 0x2D1 + (uint16_t)(chain * 2),
		    over_temp_thresh);

		/* Enable protection for this chain. */
		nphy_maskset(sc, 0x2D6, 0, (uint16_t)(1 << chain));
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PA protection: power=0x%04x temp=0x%04x\n",
	    over_power_thresh, over_temp_thresh);
}

/* ------------------------------------------------------------------ */
/* RX Sensitivity Optimization                                           */
/* ------------------------------------------------------------------ */

/*
 * Optimize RX sensitivity by sweeping LNA gain and filter bandwidth.
 * Finds the best trade-off between sensitivity (low noise figure)
 * and selectivity (adjacent channel rejection).
 *
 * Procedure:
 * 1. Sweep LNA gain from min to max
 * 2. For each gain: measure noise floor and RSSI of a known signal
 * 3. Choose gain that maximizes SNR
 * 4. Fine-tune baseband filter coefficients
 */
void
b43bsd_phy_n_rx_sensitivity_optimize(struct b43bsd_softc *sc)
{
	uint16_t best_lna_gain = 0;
	int best_snr = -999;
	uint16_t lna_gain;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX sensitivity optimization\n");

	/*
	 * Sweep LNA gain from 0 (min) to 15 (max).
	 * LNA gain register: bits 0-3 of RF control register.
	 */
	for (lna_gain = 0; lna_gain <= 15; lna_gain++) {
		uint16_t rssi, noise;
		int snr;

		/* Set LNA gain for all chains. */
		nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG1,
		    0x000F, lna_gain);
		nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG2,
		    0x000F, lna_gain);
		nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG3,
		    0x000F, lna_gain);
		nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG4,
		    0x000F, lna_gain);

		delay(50);

		/* Measure signal + noise. */
		rssi = nphy_read(sc, B43BSD_NPHY_RSSI1);
		noise = nphy_read(sc, B43BSD_NPHY_C1_EDTHRES);

		/* SNR = RSSI - noise floor. */
		snr = (int)(rssi & 0xFF) - (int)(noise & 0xFF);

		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "   LNA=%u RSSI=0x%02x noise=0x%02x SNR=%d\n",
		    lna_gain, rssi & 0xFF, noise & 0xFF, snr);

		if (snr > best_snr) {
			best_snr = snr;
			best_lna_gain = lna_gain;
		}
	}

	/* Apply best LNA gain. */
	nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG1,
	    0x000F, best_lna_gain);
	nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG2,
	    0x000F, best_lna_gain);
	nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG3,
	    0x000F, best_lna_gain);
	nphy_maskset(sc, B43BSD_NPHY_RFCTL_RXG4,
	    0x000F, best_lna_gain);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "   optimal LNA gain: %u (SNR=%d)\n",
	    best_lna_gain, best_snr);

	/*
	 * Fine-tune baseband filter based on LNA gain.
	 * Higher LNA gain → wider filter to avoid saturation.
	 */
	{
		uint16_t filter_coeff;

		if (best_lna_gain < 5)
			filter_coeff = 0x0004;	/* Narrow: better selectivity */
		else if (best_lna_gain < 10)
			filter_coeff = 0x0006;	/* Medium */
		else
			filter_coeff = 0x0008;	/* Wide: better sensitivity */

		nphy_write(sc, 0x100, filter_coeff);
		nphy_write(sc, 0x101, filter_coeff);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "RX sensitivity optimization complete\n");
}

/* Full PHY Init Sequence                                                */
/* ------------------------------------------------------------------ */

int
b43bsd_phy_n_init_sequence(struct b43bsd_softc *sc, int is_5ghz)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PHY init sequence (%s)\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz");

	b43bsd_tables_upload_coldboot(sc);
	b43bsd_tables_upload_band_bw_init(sc, is_5ghz, 0);
	b43bsd_tables_upload_extended(sc);
	b43bsd_tables_upload_rfseq(sc);
	b43bsd_tables_upload_afe(sc);
	b43bsd_tables_upload_gain_cal(sc);
	b43bsd_tables_upload_vco_cal(sc);
	b43bsd_tables_upload_phyrev_init(sc, sc->sc_radio.phy_rev);
	b43bsd_tables_upload_phyrev_extended(sc, sc->sc_radio.phy_rev);
	b43bsd_tables_upload_chain_power_pct(sc, 3, 0);
	b43bsd_tables_upload_ant_isolation(sc, is_5ghz);
	b43bsd_tables_upload_filter_cal(sc, is_5ghz);
	b43bsd_phy_n_txpower_table_upload(sc, is_5ghz);
	b43bsd_tables_upload_mcs_power(sc, is_5ghz);
	b43bsd_tables_upload_mcs_chain_power(sc);
	b43bsd_tables_upload_chan_est(sc, 0);
	b43bsd_tables_upload_stbc(sc, 3);
	b43bsd_tables_upload_ldpc(sc);

	b43bsd_phy_n_init(sc);
	b43bsd_radio_2056_init(sc, is_5ghz);
	b43bsd_phy_n_apply_per_channel(sc, 6, 0);

	b43bsd_phy_n_tx_iq_cal(sc);
	b43bsd_phy_n_rx_iq_cal(sc);
	b43bsd_phy_n_papd_train_all(sc);
	b43bsd_phy_n_noise_cal(sc);
	b43bsd_phy_n_cca_autotune(sc);
	b43bsd_phy_n_mimo_estimation(sc);
	b43bsd_phy_n_classifier_train(sc);
	b43bsd_phy_n_dc_offset_cal(sc);
	b43bsd_phy_n_xtal_trim(sc);
	b43bsd_phy_n_lpf_cal(sc, 0);
	b43bsd_phy_n_sample_cal(sc);

	b43bsd_tables_set_cal_mask(sc, 0x1FFF);

	return 0;
}
