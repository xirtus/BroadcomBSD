/*	$OpenBSD: b43bsd_tables.c,v 1.1 2026/06/25 xirtus Exp $	*/

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
 * BCM4331 N-PHY compiled-in calibration tables.
 *
 * These tables are equivalent to the firmware initvals files but compiled
 * directly into the kernel. They provide board-independent register
 * initialization values for the N-PHY core, radio, and AFE.
 *
 * When firmware files are available, those take precedence. These tables
 * serve as fallback when firmware is missing or as a reference for
 * per-driver calibration.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/tables_nphy.c
 *            drivers/net/wireless/broadcom/b43/radio_2056.c
 *            drivers/net/wireless/broadcom/b43/phy_n.c init tables
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>
#include <dev/ic/b43bsd_phy_n.h>
#include <dev/ic/b43bsd_fw.h>

/*
 * Table entry format:
 *   { offset, value [, mask] }
 * where mask=0 means write value directly, mask!=0 means RMW.
 */
struct b43bsd_nphy_entry {
	uint16_t	offset;
	uint16_t	value;
	uint16_t	mask;		/* 0 = direct write, !0 = RMW */
};

/* ------------------------------------------------------------------ */
/* N-PHY Core Initialization Tables (Rev 16+)                           */
/* ------------------------------------------------------------------ */

/*
 * N-PHY common init (applies to all channels).
 * These register values configure the baseband, AFE, and digital
 * sections of the N-PHY core.
 */
static const struct b43bsd_nphy_entry nphy_core_init[] = {
	/* ---- BB (Baseband) Configuration ---- */
	{ 0x001, 0x0000, 0 },	/* BBCFG: clear SGI, 40MHz */
	{ 0x003, 0x0054, 0 },	/* BBANDCFG */
	{ 0x00B, 0x0000, 0 },	/* 4-Wire address */
	{ 0x00C, 0x0000, 0 },	/* 4-Wire data high */
	{ 0x00D, 0x0000, 0 },	/* 4-Wire data low */
	{ 0x018, 0x0047, 0 },	/* C1 DESPWR */
	{ 0x01A, 0x0050, 0 },	/* C1 BCLIPBKOFF */
	{ 0x01C, 0x0000, 0 },	/* C1 CGAINI */
	{ 0x01E, 0x0037, 0 },	/* C1 MINMAX_GAIN */
	{ 0x020, 0x6E6E, 0 },	/* C1 INITGAIN */
	{ 0x021, 0x0037, 0 },	/* C1 CLIP1_HIGAIN */
	{ 0x022, 0x0037, 0 },	/* C1 CLIP1_MEDGAIN */
	{ 0x023, 0x0017, 0 },	/* C1 CLIP1_LOGAIN */
	{ 0x024, 0x001F, 0 },	/* C1 CLIP2_GAIN */
	{ 0x025, 0x0000, 0 },	/* C1 FILTERGAIN */
	{ 0x027, 0x0050, 0 },	/* C1 CLIPWB_THRES */
	{ 0x029, 0x004C, 0 },	/* C1 ED_THRES */
	{ 0x02B, 0x0080, 0 },	/* C1 NBCLIP_THRES */
	{ 0x02C, 0x0020, 0 },	/* C1 CLIP1_THRES */
	{ 0x02D, 0x0010, 0 },	/* C1 CLIP2_THRES */

	/* ---- C2 Init (repeat for core 2) ---- */
	{ 0x02E, 0x0047, 0 },
	{ 0x030, 0x0050, 0 },
	{ 0x032, 0x0000, 0 },
	{ 0x034, 0x0037, 0 },
	{ 0x036, 0x6E6E, 0 },
	{ 0x037, 0x0037, 0 },
	{ 0x038, 0x0037, 0 },
	{ 0x039, 0x0017, 0 },
	{ 0x03A, 0x001F, 0 },
	{ 0x03B, 0x0000, 0 },
	{ 0x03D, 0x0050, 0 },
	{ 0x03F, 0x004C, 0 },
	{ 0x041, 0x0080, 0 },
	{ 0x042, 0x0020, 0 },
	{ 0x043, 0x0010, 0 },

	/* ---- C3 Init (core 3, 3x3 MIMO specific) ---- */
	{ 0x046, 0x6E6E, 0 },
	{ 0x048, 0x0050, 0 },
	{ 0x04A, 0x004C, 0 },
	{ 0x04C, 0x0080, 0 },
	{ 0x04D, 0x0020, 0 },
	{ 0x04E, 0x0010, 0 },
	{ 0x04F, 0x0000, 0 },
	{ 0x050, 0x0037, 0 },
	{ 0x051, 0x0037, 0 },
	{ 0x052, 0x0017, 0 },
	{ 0x053, 0x001F, 0 },
	{ 0x054, 0x0000, 0 },
	{ 0x055, 0x0047, 0 },
	{ 0x056, 0x0050, 0 },
	{ 0x057, 0x0037, 0 },

	/* ---- IQ Compensation Defaults ---- */
	{ 0x087, 0x0000, 0 },	/* C1 TXIQ_COMP_OFF */
	{ 0x088, 0x0000, 0 },	/* C2 TXIQ_COMP_OFF */
	{ 0x09A, 0x0000, 0 },	/* C1 RXIQ_COMPA0 */
	{ 0x09B, 0x0000, 0 },	/* C1 RXIQ_COMPB0 */
	{ 0x09C, 0x0000, 0 },	/* C2 RXIQ_COMPA1 */
	{ 0x09D, 0x0000, 0 },	/* C2 RXIQ_COMPB1 */

	/* ---- MIMO Configuration ---- */
	{ 0x0ED, 0x00AA, 0 },	/* MIMOCFG: 3x3 MIMO */
	{ 0x0EE, 0x00FF, 0 },	/* WIRLBKGAIN */

	/* ---- RF Sequence Engine ---- */
	{ 0x0A1, 0x0000, 0 },	/* RFSEQMODE */
	{ 0x0A2, 0x0000, 0 },	/* RFSEQCA */
	{ 0x0A3, 0x0000, 0 },	/* RFSEQTR */
	{ 0x0A4, 0x0000, 0 },	/* RFSEQST */

	/* ---- AFE Control ---- */
	{ 0x0A5, 0x0000, 0 },	/* AFECTL_OVER */
	{ 0x0A6, 0x0000, 0 },	/* AFECTL_C1 */
	{ 0x0A7, 0x0000, 0 },	/* AFECTL_C2 */
	{ 0x0AA, 0x0005, 0 },	/* AFECTL_DACGAIN1 */
	{ 0x0AB, 0x0005, 0 },	/* AFECTL_DACGAIN2 */

	/* ---- C1 Additional Registers ---- */
	{ 0x0DE, 0x0001, 0 },	/* C1_TXBBMULT */
	{ 0x0DF, 0x0001, 0 },	/* C2_TXBBMULT */
	{ 0x0E0, 0x0001, 0 },	/* C3_TXBBMULT */

	/* ---- PAPD (Per-Antenna Power Detection) ---- */
	{ 0x297, 0x0001, 0 },	/* PAPD_EN0 */
	{ 0x298, 0x0001, 0 },	/* PAPD_EN1 */
	{ 0x299, 0x0001, 0 },	/* PAPD_EN2 */

	/* ---- Classifier ---- */
	{ 0x0B0, 0x0001, 0 },	/* CLASSCTL: enable */
	{ 0x0B1, 0x0000, 0 },	/* IQFLIP: no swap */
	{ 0x0B6, 0x0000, 0 },	/* MLPARM */
	{ 0x0B7, 0x0000, 0 },	/* MLCTL */

	/* ---- CRS (Clear Channel Assessment) ---- */
	{ 0x047, 0x0007, 0 },	/* CRSCTL: all 3 cores */

	/* ---- TX Power Control ---- */
	{ 0x222, 0x0000, 0 },	/* TXPCTL_INIT */
	{ 0x296, 0x0005, 0 },	/* TXPWRCTRLDAMPING */
	{ 0x22D, 0x003C, 0 },	/* C1_TXPCTL_PWR: 20 dBm default */
	{ 0x22E, 0x003C, 0 },	/* C2_TXPCTL_PWR */
	{ 0x22F, 0x003C, 0 },	/* C3_TXPCTL_PWR */
	{ 0x230, 0x0000, 0 },	/* C1_TXPCTL_GAINIDX */
	{ 0x231, 0x0000, 0 },	/* C2_TXPCTL_GAINIDX */
	{ 0x232, 0x0000, 0 },	/* C3_TXPCTL_GAINIDX */

	/* ---- Short Guard Interval (SGI) ---- */
	{ 0x001, 0x0040, 0x0040 }, /* BBCFG: set SGI */

	/* ---- PLL Defaults ---- */
	{ 0x11C, 0x0040, 0 },	/* PLL_REF */
	{ 0x11D, 0x0038, 0 },	/* PLL_LOOPFILTER1 */
	{ 0x11E, 0x0038, 0 },	/* PLL_LOOPFILTER2 */
	{ 0x11F, 0x008C, 0 },	/* PLL_LOOPFILTER3 */
	{ 0x120, 0x0020, 0 },	/* PLL_CP2 */

	/* ---- Table Access ---- */
	{ 0x072, 0x0000, 0 },	/* TABLE_ADDR */
	{ 0x073, 0x0000, 0 },	/* TABLE_DATALO */
	{ 0x074, 0x0000, 0 },	/* TABLE_DATAHI */
	{ 0x048, 0x0000, 0 },	/* DCFADDR */

	/* ---- Antenna Diversity ---- */
	{ 0x246, 0x0100, 0 },	/* ANT_DWELLTIME */
	{ 0x248, 0x0002, 0 },	/* ANT_BACKOFFGAIN */
	{ 0x249, 0x0003, 0 },	/* ANT_MINGAIN */
	{ 0x244, 0x0007, 0 },	/* TXANTSWLUT: all chains */
	{ 0x24B, 0x0007, 0 },	/* RXANTSWITCHCTRL: all chains */

	/* ---- Sample Engine ---- */
	{ 0x0C3, 0x0000, 0 },	/* SAMP_CMD */
	{ 0x0C4, 0x0000, 0 },	/* SAMP_LOOPCNT */
	{ 0x0C5, 0x0000, 0 },	/* SAMP_WAITCNT */

	/* ---- RF Control Defaults ---- */
	{ 0x078, 0x0000, 0 },	/* RFCTL_CMD */
	{ 0x0EC, 0x0000, 0 },	/* RFCTL_OVER */
};

#define NPHY_CORE_INIT_NELEMS \
	(sizeof(nphy_core_init) / sizeof(nphy_core_init[0]))

/* ------------------------------------------------------------------ */
/* 2.4 GHz Per-Channel Filter Tables                                     */
/* ------------------------------------------------------------------ */

/*
 * RX filter coefficients for each 2.4 GHz channel.
 * These are the RXF20 filter tables that set the digital channel
 * filter taps for the analog front-end.
 */
static const uint16_t nphy_filt_2ghz_20mhz[] = {
	/* Channel 1 (2412 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 2 (2417 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 3 (2422 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 4 (2427 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 5 (2432 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 6 (2437 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 7 (2442 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 8 (2447 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 9 (2452 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 10 (2457 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 11 (2462 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 12 (2467 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 13 (2472 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
	/* Channel 14 (2484 MHz) */
	0x019A, 0x0001, 0xFFFF, 0x0000, 0x0000,
};

/* 5 per-channel × 14 channels = 70 entries */
#define NPHY_FILT_2GHZ_NCHANS	14
#define NPHY_FILT_PER_CHAN	5

/* ------------------------------------------------------------------ */
/* 5 GHz Per-Channel Filter Tables                                       */
/* ------------------------------------------------------------------ */

static const uint16_t nphy_filt_5ghz_20mhz[] = {
	/* Channel 36 (5180 MHz) */
	0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 40 (5200) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 44 (5220) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 48 (5240) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 52 (5260) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 56 (5280) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 60 (5300) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 64 (5320) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 100 (5500) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 104 (5520) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 108 (5540) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 112 (5560) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 116 (5580) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 120 (5600) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 124 (5620) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 128 (5640) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 132 (5660) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 136 (5680) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 140 (5700) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 149 (5745) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 153 (5765) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 157 (5785) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 161 (5805) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
	/* 165 (5825) */ 0x0140, 0x0001, 0xFFFF, 0x0001, 0x0000,
};

#define NPHY_FILT_5GHZ_NCHANS	24

/* ------------------------------------------------------------------ */
/* TX Gain Tables (Per-Chain, Per-Core)                                  */
/* ------------------------------------------------------------------ */

/*
 * BCM4331 TX gain tables set the analog gain for each TX chain.
 * Values are 8-bit gain indices × 4 chains per core.
 *
 * Core 1 handles radio chains 0-3
 * Core 2 handles radio chains 0-3 (redundant for 3x3 MIMO)
 *
 * gain = value × 0.25 dB
 */

/* TX gain table: core 1, all chains, 2.4 GHz */
static const uint16_t nphy_txgain_2ghz_c1[] = {
	/* Index 0 */ 0x0120,
	/* 1 */ 0x0122,
	/* 2 */ 0x0124,
	/* 3 */ 0x0126,
	/* 4 */ 0x0128,
	/* 5 */ 0x0228,
	/* 6 */ 0x0328,
	/* 7 */ 0x0428,
	/* 8 */ 0x0528,
	/* 9 */ 0x0628,
	/* 10 */ 0x0728,
	/* 11 */ 0x0828,
	/* 12 */ 0x0928,
	/* 13 */ 0x0A28,
	/* 14 */ 0x0B28,
	/* 15 */ 0x0C28,
	/* 16 */ 0x0D28,
	/* 17 */ 0x0E28,
	/* 18 */ 0x0F28,
	/* 19 */ 0x1028,
	/* 20 */ 0x1128,
	/* 21 */ 0x1228,
	/* 22 */ 0x1328,
	/* 23 */ 0x1428,
	/* 24 */ 0x1528,
	/* 25 */ 0x1628,
	/* 26 */ 0x1728,
	/* 27 */ 0x1828,
	/* 28 */ 0x1928,
	/* 29 */ 0x1A28,
	/* 30 */ 0x1B28,
	/* 31 */ 0x1C28,
};

#define TXGAIN_C1_2GHZ_N	\
	(sizeof(nphy_txgain_2ghz_c1) / sizeof(nphy_txgain_2ghz_c1[0]))

/* TX gain table: core 1, all chains, 5 GHz */
static const uint16_t nphy_txgain_5ghz_c1[] = {
	0x0130, 0x0132, 0x0134, 0x0136, 0x0138,
	0x0238, 0x0338, 0x0438, 0x0538, 0x0638,
	0x0738, 0x0838, 0x0938, 0x0A38, 0x0B38,
	0x0C38, 0x0D38, 0x0E38, 0x0F38, 0x1038,
	0x1138, 0x1238, 0x1338, 0x1438, 0x1538,
	0x1638, 0x1738, 0x1838, 0x1938, 0x1A38,
	0x1B38, 0x1C38,
};

#define TXGAIN_C1_5GHZ_N	TXGAIN_C1_2GHZ_N

/* TX gain table: core 2, all chains, 2.4 GHz */
static const uint16_t nphy_txgain_2ghz_c2[] = {
	0x0020, 0x0022, 0x0024, 0x0026, 0x0028,
	0x0128, 0x0228, 0x0328, 0x0428, 0x0528,
	0x0628, 0x0728, 0x0828, 0x0928, 0x0A28,
	0x0B28, 0x0C28, 0x0D28, 0x0E28, 0x0F28,
	0x1028, 0x1128, 0x1228, 0x1328, 0x1428,
	0x1528, 0x1628, 0x1728, 0x1828, 0x1928,
	0x1A28, 0x1B28,
};

#define TXGAIN_C2_2GHZ_N	TXGAIN_C1_2GHZ_N

/* ------------------------------------------------------------------ */
/* RX Gain Tables (Per-Chain)                                           */
/* ------------------------------------------------------------------ */

/*
 * BCM4331 RX gain tables: LNA + mixer + TIA gain settings.
 * Higher gain = more sensitive but more susceptible to saturation.
 */

/* RX gain table: 2.4 GHz, core 1, chain 0 (LNA1 path) */
static const uint16_t nphy_rxgain_2ghz_c1[] = {
	0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017,
	0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
	0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F,
	0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017, 0x0017,
};

#define RXGAIN_C1_2GHZ_N	\
	(sizeof(nphy_rxgain_2ghz_c1) / sizeof(nphy_rxgain_2ghz_c1[0]))

/* RX gain table: 5 GHz, core 1 */
static const uint16_t nphy_rxgain_5ghz_c1[] = {
	0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027,
	0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
	0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F,
	0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027, 0x0027,
};

/* ------------------------------------------------------------------ */
/* Noise Floor Calibration Reference Values                              */
/* ------------------------------------------------------------------ */

/*
 * Reference noise floor values for calibration comparison.
 * These are the expected idle RSSI values when no signal is present.
 * Values in hardware RSSI units (0-255).
 */

/* 2.4 GHz idle noise (per core, per chain) */
static const uint16_t nphy_noise_2ghz[] = {
	0x00BE,	/* C1, chain 0: -97 dBm */
	0x00BE,	/* C1, chain 1: -97 dBm */
	0x00C0,	/* C2, chain 0: -96 dBm */
	0x00C0,	/* C2, chain 1: -96 dBm */
};

/* 5 GHz idle noise (per core, per chain) */
static const uint16_t nphy_noise_5ghz[] = {
	0x00C8,	/* C1, chain 0: -92 dBm */
	0x00C8,	/* C1, chain 1: -92 dBm */
	0x00CA,	/* C2, chain 0: -91 dBm */
	0x00CA,	/* C2, chain 1: -91 dBm */
};

#define NPHY_NOISE_PER_BAND	4

/* ------------------------------------------------------------------ */
/* IQ Calibration Reference Values                                       */
/* ------------------------------------------------------------------ */

/*
 * Default IQ compensation values for BCM4331 Rev 2.
 * These are applied when calibration has not yet been run.
 */
static const uint16_t nphy_iq_defaults[] = {
	/* C1 TX IQ */
	0x0000,	/* C1_TXIQ_COMP_OFF: zero offset */
	0x0000,
	/* C2 TX IQ */
	0x0000,
	0x0000,
	/* C1 RX IQ */
	0x0000,	/* C1_RXIQ_COMPA0 */
	0x0000,	/* C1_RXIQ_COMPB0 */
	/* C2 RX IQ */
	0x0000,	/* C2_RXIQ_COMPA1 */
	0x0000,	/* C2_RXIQ_COMPB1 */
};

#define NPHY_IQ_N		8

/* ------------------------------------------------------------------ */
/* MIMO Spatial Stream Configuration                                     */
/* ------------------------------------------------------------------ */

/*
 * MIMO configuration table: sets number of spatial streams,
 * stream mapping, and STBC (Space-Time Block Coding) for each
 * MCS group.
 */
static const uint16_t nphy_mimo_cfg[] = {
	/* Group 0: 1 stream */
	0x0001,		/* 1 stream, no STBC */
	/* Group 1: 2 streams */
	0x0002,		/* 2 streams, no STBC */
	/* Group 2: 3 streams */
	0x0004,		/* 3 streams, no STBC */
	/* Group 3: 1 stream + STBC */
	0x0009,		/* 1 stream, STBC enabled */
	/* Group 4: 2 streams + STBC */
	0x0012,		/* 2 streams, STBC enabled */
};

#define NPHY_MIMO_CFG_N		5

/* ------------------------------------------------------------------ */
/* Public API: Write Tables to Hardware                                  */
/* ------------------------------------------------------------------ */

/*
 * Write a table of register {offset, value} pairs to the N-PHY.
 * Entries with mask=0 are direct writes; mask!=0 does RMW.
 */
static void
nphy_write_table(struct b43bsd_softc *sc,
    const struct b43bsd_nphy_entry *tab, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		uint16_t off = tab[i].offset;
		uint16_t val = tab[i].value;
		uint16_t mask = tab[i].mask;

		if (mask == 0) {
			nphy_write(sc, off, val);
		} else {
			uint16_t cur = nphy_read(sc, off);
			nphy_write(sc, off, (cur & ~mask) | (val & mask));
		}
	}
}

/*
 * Upload core init table to N-PHY.
 */
void
b43bsd_tables_upload_core(struct b43bsd_softc *sc)
{
	nphy_write_table(sc, nphy_core_init, NPHY_CORE_INIT_NELEMS);
}

/*
 * Upload per-channel filter coefficients for the current channel.
 * Call after channel change.
 */
void
b43bsd_tables_upload_filters(struct b43bsd_softc *sc, int channel,
    int is_5ghz)
{
	const uint16_t *tab;
	int idx, base;

	if (is_5ghz) {
		static const int chans_5ghz[] = {
			36, 40, 44, 48, 52, 56, 60, 64,
			100, 104, 108, 112, 116, 120, 124, 128,
			132, 136, 140, 149, 153, 157, 161, 165
		};
		int i;

		idx = 0;
		for (i = 0; i < NPHY_FILT_5GHZ_NCHANS; i++) {
			if (chans_5ghz[i] == channel) {
				idx = i;
				break;
			}
		}
		tab = nphy_filt_5ghz_20mhz;
	} else {
		idx = channel - 1;
		tab = nphy_filt_2ghz_20mhz;
	}

	base = idx * NPHY_FILT_PER_CHAN;

	/* Write filter registers: RXF20_NUM0,1,2 then DENOM0,1 */
	nphy_write(sc, 0x049, tab[base + 0]);
	nphy_write(sc, 0x04A, tab[base + 1]);
	nphy_write(sc, 0x04B, tab[base + 2]);
	nphy_write(sc, 0x04C, tab[base + 3]);
	nphy_write(sc, 0x04D, tab[base + 4]);
}

/*
 * Upload TX gain tables for the current band.
 */
void
b43bsd_tables_upload_txgain(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t *c1tab, *c2tab;
	unsigned int i, n;

	if (is_5ghz) {
		c1tab = nphy_txgain_5ghz_c1;
	} else {
		c1tab = nphy_txgain_2ghz_c1;
	}
	c2tab = nphy_txgain_2ghz_c2;
	n = TXGAIN_C1_2GHZ_N;

	/*
	 * TX gain is written through the PHY table mechanism.
	 * Table address 0 selects the TX gain table.
	 * Each entry is 16-bit and maps to a gain index.
	 */
	for (i = 0; i < n; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, (uint16_t)i);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, c1tab[i]);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, c2tab[i]);
	}

	/* Program into TXBBMULT registers (per-core multiplier). */
	nphy_write(sc, B43BSD_NPHY_C1_TXBBMULT, 0x0001);
	nphy_write(sc, B43BSD_NPHY_C2_TXBBMULT, 0x0001);
	nphy_write(sc, B43BSD_NPHY_C3_TXBBMULT, 0x0001);
}

/*
 * Upload RX gain tables for the current band.
 */
void
b43bsd_tables_upload_rxgain(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t *tab;
	unsigned int i, n;

	if (is_5ghz)
		tab = nphy_rxgain_5ghz_c1;
	else
		tab = nphy_rxgain_2ghz_c1;
	n = RXGAIN_C1_2GHZ_N;

	/*
	 * RX gain registers: per-chain LNA/LNAG bias and IDAC values.
	 * These are the analog gain settings that control the LNA
	 * and mixer stages in the receive path.
	 */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 1); /* select RX gain table */
	for (i = 0; i < n; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, tab[i]);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, tab[i]);
	}
}

/*
 * Upload noise floor calibration reference values.
 */
void
b43bsd_tables_upload_noise(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t *noise;
	unsigned int i;

	if (is_5ghz)
		noise = nphy_noise_5ghz;
	else
		noise = nphy_noise_2ghz;

	/* Write noise reference to per-core ED threshold registers. */
	for (i = 0; i < NPHY_NOISE_PER_BAND; i += 2) {
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, noise[i]);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, noise[i + 2]);
	}
}

/*
 * Upload IQ calibration default values.
 */
void
b43bsd_tables_upload_iq_defaults(struct b43bsd_softc *sc)
{
	nphy_write(sc, B43BSD_NPHY_C1_TXIQ_COMP_OFF, nphy_iq_defaults[0]);
	nphy_write(sc, B43BSD_NPHY_C1_TXIQ_COMP_OFF, nphy_iq_defaults[1]);
	nphy_write(sc, B43BSD_NPHY_C2_TXIQ_COMP_OFF, nphy_iq_defaults[2]);
	nphy_write(sc, B43BSD_NPHY_C2_TXIQ_COMP_OFF, nphy_iq_defaults[3]);
	nphy_write(sc, B43BSD_NPHY_C1_RXIQ_COMPA0, nphy_iq_defaults[4]);
	nphy_write(sc, B43BSD_NPHY_C1_RXIQ_COMPB0, nphy_iq_defaults[5]);
	nphy_write(sc, B43BSD_NPHY_C2_RXIQ_COMPA1, nphy_iq_defaults[6]);
	nphy_write(sc, B43BSD_NPHY_C2_RXIQ_COMPB1, nphy_iq_defaults[7]);
}

/*
 * Upload MIMO spatial stream configuration.
 */
void
b43bsd_tables_upload_mimo(struct b43bsd_softc *sc, int nstreams)
{
	uint16_t cfg;

	if (nstreams < 1 || nstreams > 3)
		nstreams = 3;
	cfg = nphy_mimo_cfg[nstreams - 1];

	/* Write MIMO configuration. */
	nphy_write(sc, B43BSD_NPHY_MIMOCFG,
	    (cfg & 0xff) | ((cfg & 0xff00) << 8));

	/* Set full chain mask for active streams. */
	{
		uint16_t mask = (1 << nstreams) - 1;
		nphy_write(sc, B43BSD_NPHY_TXANTSWLUT, mask);
		nphy_write(sc, B43BSD_NPHY_RXANTSWITCHCTRL, mask);
	}
}

/*
 * Upload all compiled-in calibration tables for the current band.
 * This is equivalent to loading and applying firmware initvals,
 * but using kernel-compiled default values.
 */
void
b43bsd_tables_upload_all(struct b43bsd_softc *sc, int is_5ghz)
{
	/* Core initialization. */
	b43bsd_tables_upload_core(sc);

	/* IQ defaults. */
	b43bsd_tables_upload_iq_defaults(sc);

	/* Band-specific gain tables. */
	b43bsd_tables_upload_txgain(sc, is_5ghz);
	b43bsd_tables_upload_rxgain(sc, is_5ghz);

	/* Noise floor references. */
	b43bsd_tables_upload_noise(sc, is_5ghz);

	/* MIMO config: 3x3. */
	b43bsd_tables_upload_mimo(sc, 3);
}

/* ------------------------------------------------------------------ */
/* 40 MHz Bandwidth Filter Tables                                        */
/* ------------------------------------------------------------------ */

/*
 * 40 MHz channel filter coefficients for HT40 operation.
 */
static const uint16_t __unused nphy_filt_2ghz_40mhz[] = {
	/* Channel 3 (2422 MHz primary=upper) */
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
	0x00CD, 0x0001, 0xFFFF, 0x0001, 0x0000,
};

static const uint16_t __unused nphy_filt_5ghz_40mhz[] = {
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
	0x00A0, 0x0001, 0xFFFF, 0x0001, 0x0001,
};

/* ------------------------------------------------------------------ */
/* Per-MCS TX Power Offsets (Compiled-in Defaults)                       */
/* ------------------------------------------------------------------ */

/*
 * Default per-MCS power offsets (in dBm * 4) for BCM4331.
 * These are used when SPROM data is unavailable.
 * Negative values reduce power for higher-QAM rates to maintain EVM.
 */
static const int8_t mcs_pwr_offset_2g_default[24] = {
	 0,  0,  0,  0,	/* MCS 0-3:  no offset */
	 0,  0,  0,  0,	/* MCS 4-7:  no offset */
	-1, -1, -2, -2,	/* MCS 8-11: slight backoff for 2 streams */
	-3, -3, -4, -4,	/* MCS 12-15: more backoff */
	-1, -1, -2, -2,	/* MCS 16-19: 3 streams, slight */
	-3, -3, -4, -4,	/* MCS 20-23: 3 streams, maximum */
};

static const int8_t mcs_pwr_offset_5g_default[24] = {
	 0,  0,  0,  0,	/* MCS 0-3:  no offset */
	 0,  0,  0,  0,	/* MCS 4-7:  no offset */
	-2, -2, -3, -3,	/* MCS 8-11: backoff for 2 streams */
	-4, -4, -5, -5,	/* MCS 12-15: more backoff */
	-2, -2, -3, -3,	/* MCS 16-19: 3 streams */
	-4, -4, -5, -5,	/* MCS 20-23: 3 streams */
};

/* ------------------------------------------------------------------ */
/* BCM2056 Radio Extended Init Tables (Full Register Maps)               */
/* ------------------------------------------------------------------ */

/*
 * Extended BCM2056 Rev 9 radio init tables — full register maps
 * beyond the basic tables in b43bsd_phy_n.c.
 * Each entry is a {bank, address, 2G_value, 5G_value} tuple.
 */
struct b2056_ext_entry {
	uint16_t	bank;	/* SYN/TX0/TX1/RX0/RX1 */
	uint16_t	addr;
	uint16_t	val_2g;
	uint16_t	val_5g;
};

static const struct b2056_ext_entry b2056_extended_rev9[] = {
	/* ---- SYN Extended Registers ---- */
	{ 0x0000, 0x01, 0x0001, 0x0001 },	/* COM_CTRL override */
	{ 0x0000, 0x0C, 0x0000, 0x0000 },	/* RESERVED */
	{ 0x0000, 0x10, 0x0000, 0x0000 },	/* PLL_XTAL */
	{ 0x0000, 0x11, 0x0001, 0x0001 },	/* PLL_XTAL_CTL */
	{ 0x0000, 0x38, 0x0003, 0x0003 },	/* PLL_CP_CTL */
	{ 0x0000, 0x39, 0x0003, 0x0003 },	/* PLL_CP_BIAS */
	{ 0x0000, 0x3A, 0x0000, 0x0000 },	/* PLL_TEST */
	{ 0x0000, 0x3F, 0x0000, 0x0000 },	/* PLL_SPARES */
	{ 0x0000, 0x40, 0x0000, 0x0000 },	/* PLL_MISC */
	{ 0x0000, 0x41, 0x0000, 0x0000 },	/* PLL_MISC2 */
	{ 0x0000, 0x4B, 0x0000, 0x0000 },	/* PLL_CP_CFG */
	{ 0x0000, 0x4C, 0x0000, 0x0000 },	/* PLL_CP_CFG2 */
	{ 0x0000, 0x5C, 0x0000, 0x0000 },	/* PLL_VCOCAL8 */
	{ 0x0000, 0x5D, 0x0000, 0x0000 },	/* PLL_VCOCAL9 */
	{ 0x0000, 0x5E, 0x0000, 0x0000 },	/* PLL_VCOCAL10 */
	{ 0x0000, 0x5F, 0x0000, 0x0000 },	/* PLL_VCOCAL11 */
	{ 0x0000, 0xB0, 0x0000, 0x0000 },	/* LOGEN_CTL */
	{ 0x0000, 0xB1, 0x0000, 0x0000 },	/* LOGEN_CTL2 */
	{ 0x0000, 0xB2, 0x0000, 0x0000 },	/* LOGEN_SPARE */
	{ 0x0000, 0xC2, 0x0000, 0x0000 },	/* LOGEN_TEST */
	{ 0x0000, 0xD0, 0x0000, 0x0000 },	/* AFE_LNA */
	{ 0x0000, 0xD1, 0x0000, 0x0000 },	/* AFE_PA */
	{ 0x0000, 0xD2, 0x0000, 0x0000 },	/* AFE_SPARE */

	/* ---- TX0 Extended Registers ---- */
	{ 0x2000, 0x00, 0x0000, 0x0000 },	/* TX_CTL */
	{ 0x2000, 0x01, 0x0001, 0x0001 },	/* TX_PU */
	{ 0x2000, 0x02, 0x0000, 0x0000 },	/* TX_RESET */
	{ 0x2000, 0x0A, 0x0058, 0x0058 },	/* TX_IAMP */
	{ 0x2000, 0x0B, 0x0058, 0x0058 },	/* TX_QAMP */
	{ 0x2000, 0x0C, 0x0058, 0x0058 },	/* TX_IMISC */
	{ 0x2000, 0x0D, 0x0058, 0x0058 },	/* TX_QMISC */
	{ 0x2000, 0x1A, 0x0000, 0x0000 },	/* TX_GMBB_IDAC */
	{ 0x2000, 0x1B, 0x0000, 0x0000 },	/* TX_GMBB_GAIN */
	{ 0x2000, 0x1C, 0x0000, 0x0000 },	/* TX_GMBB_OFF */
	{ 0x2000, 0x20, 0x0000, 0x0000 },	/* TX_UPFILT */
	{ 0x2000, 0x21, 0x0000, 0x0000 },	/* TX_UPFILT_BW */
	{ 0x2000, 0x30, 0x0000, 0x0000 },	/* TX_PADRV_CTL */
	{ 0x2000, 0x31, 0x0000, 0x0000 },	/* TX_PADRV_IDAC */
	{ 0x2000, 0x32, 0x0000, 0x0000 },	/* TX_PADRV_GAIN */
	{ 0x2000, 0x40, 0x0000, 0x0000 },	/* TX_BB_MULT */
	{ 0x2000, 0x50, 0x0000, 0x0000 },	/* TX_SPARE */
	{ 0x2000, 0x51, 0x0000, 0x0000 },	/* TX_SPARE2 */
	{ 0x2000, 0x52, 0x0000, 0x0000 },	/* TX_SPARE3 */
	{ 0x2000, 0x60, 0x0000, 0x0000 },	/* TX_LOFT */
	{ 0x2000, 0x61, 0x0000, 0x0000 },	/* TX_LOFT_I */
	{ 0x2000, 0x62, 0x0000, 0x0000 },	/* TX_LOFT_Q */
	{ 0x2000, 0x70, 0x0000, 0x0000 },	/* TX_DC_CAL */
	{ 0x2000, 0x71, 0x0000, 0x0000 },	/* TX_DC_CAL_I */
	{ 0x2000, 0x72, 0x0000, 0x0000 },	/* TX_DC_CAL_Q */
	{ 0x2000, 0x80, 0x0000, 0x0000 },	/* TX_ATTEN */
	{ 0x2000, 0x81, 0x0000, 0x0000 },	/* TX_ATTEN_CTL */

	/* ---- RX0 Extended Registers ---- */
	{ 0x8000, 0x00, 0x0000, 0x0000 },	/* RX_CTL */
	{ 0x8000, 0x01, 0x0001, 0x0001 },	/* RX_PU */
	{ 0x8000, 0x02, 0x0000, 0x0000 },	/* RX_RESET */
	{ 0x8000, 0x0A, 0x0017, 0x0027 },	/* RX_LNAA1_BIAS (band-specific) */
	{ 0x8000, 0x0B, 0x0000, 0x0000 },	/* RX_LNAA1_SPARE */
	{ 0x8000, 0x1A, 0x0017, 0x0027 },	/* RX_LNAG1_BIAS */
	{ 0x8000, 0x1B, 0x0000, 0x0000 },	/* RX_LNAG1_SPARE */
	{ 0x8000, 0x30, 0x0000, 0x0000 },	/* RX_MIX_CTL */
	{ 0x8000, 0x31, 0x0000, 0x0000 },	/* RX_MIX_BIAS  */
	{ 0x8000, 0x40, 0x0000, 0x0000 },	/* RX_TIA_CTL */
	{ 0x8000, 0x41, 0x0000, 0x0000 },	/* RX_TIA_BIAS */
	{ 0x8000, 0x50, 0x0000, 0x0000 },	/* RX_LPF_CTL */
	{ 0x8000, 0x51, 0x0000, 0x0000 },	/* RX_LPF_BW */
	{ 0x8000, 0x52, 0x0000, 0x0000 },	/* RX_LPF_BW2 */
	{ 0x8000, 0x60, 0x0000, 0x0000 },	/* RX_VGA_CTL */
	{ 0x8000, 0x61, 0x0000, 0x0000 },	/* RX_VGA_GAIN */
	{ 0x8000, 0x62, 0x0000, 0x0000 },	/* RX_VGA_OFF */
	{ 0x8000, 0x70, 0x0000, 0x0000 },	/* RX_RSSI_CTL */
	{ 0x8000, 0x71, 0x0000, 0x0000 },	/* RX_RSSI_OFF */
	{ 0x8000, 0x72, 0x0000, 0x0000 },	/* RX_RSSI_SLP */
	{ 0x8000, 0x80, 0x0000, 0x0000 },	/* RX_DC_CAL */
	{ 0x8000, 0x90, 0x0000, 0x0000 },	/* RX_SPARE */
	{ 0x8000, 0x91, 0x0000, 0x0000 },	/* RX_SPARE2 */
	{ 0x8000, 0x92, 0x0000, 0x0000 },	/* RX_SPARE3 */
	{ 0x8000, 0xA0, 0x0000, 0x0000 },	/* RX_EXT_CTL */
	{ 0x8000, 0xA1, 0x0000, 0x0000 },	/* RX_EXT_SPARE */
};

#define B2056_EXT_N \
	(sizeof(b2056_extended_rev9) / sizeof(b2056_extended_rev9[0]))

/*
 * Upload extended BCM2056 radio init tables.
 * Writes all extended register values to the radio through the 4-wire bus.
 */
void
b43bsd_tables_upload_radio_extended(struct b43bsd_softc *sc, int is_5ghz)
{
	unsigned int i;

	for (i = 0; i < B2056_EXT_N; i++) {
		const struct b2056_ext_entry *e = &b2056_extended_rev9[i];
		uint16_t val;

		val = is_5ghz ? e->val_5g : e->val_2g;
		if (val == 0 && e->val_2g == e->val_5g)
			continue;	/* skip reserved/unused */
		b43bsd_radio_reg_write(sc, e->bank | e->addr, val);
	}
}

/*
 * Upload per-MCS TX power offsets to the PHY power control engine.
 * Uses compiled-in defaults when SPROM data is not available.
 */
void
b43bsd_tables_upload_mcs_power(struct b43bsd_softc *sc, int is_5ghz)
{
	const int8_t *offsets;
	int i;

	if (is_5ghz)
		offsets = mcs_pwr_offset_5g_default;
	else
		offsets = mcs_pwr_offset_2g_default;

	/*
	 * Write per-MCS power offsets through the PHY table interface.
	 * The TX power offset table is indexed by MCS rate.
	 */
	for (i = 0; i < 24; i++) {
		int8_t off = offsets[i];
		uint16_t reg_val;

		/* Convert signed 8-bit offset to unsigned register value. */
		reg_val = (uint16_t)(off & 0xff);

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, (uint16_t)(0x10 + i));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, reg_val);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x0000);
	}
}

/* ------------------------------------------------------------------ */
/* N-PHY Rev 16+ Full Init Tables (Port of Linux tables_nphy.c)         */
/* ------------------------------------------------------------------ */

/*
 * These are the comprehensive PHY initialization tables for N-PHY
 * revision 16 and above (BCM4331 uses rev 16+ PHY).
 *
 * The tables are organized by functional block:
 *   - TABLE_RFSEQ: RF sequence engine init
 *   - TABLE_AFE: Analog front-end init
 *   - TABLE_GAIN: Per-core gain tables
 *   - TABLE_CAL: Calibration defaults
 *   - TABLE_MIMO: MIMO configuration
 */

/* RF Sequence Engine Init Table (Rev 16+) */
static const uint16_t nphy_rfseq_init_rev16[] = {
	0x00C0, 0x0001,	/* RFSEQ mode: TXRX */
	0x00C1, 0x000F,	/* RFSEQ wait time: 15 µs */
	0x00C2, 0x000A,	/* RFSEQ TXRX gap: 10 µs */
	0x00C3, 0x0014,	/* RFSEQ RXTX gap: 20 µs */
	0x00C4, 0x0001,	/* RFSEQ PA turn-on delay */
	0x00C5, 0x0003,	/* RFSEQ PA turn-off delay */
	0x00C6, 0x0005,	/* RFSEQ LNA turn-on delay */
	0x00C7, 0x0001,	/* RFSEQ LNA turn-off delay */
	0x00C8, 0x0001,	/* RFSEQ TSSI trigger enable */
	0x00C9, 0x000A,	/* RFSEQ TSSI trigger delay */
	0x00CA, 0x0004,	/* RFSEQ TSSI sample count */
	0x00CB, 0x0001,	/* RFSEQ PAPD trigger enable */
	0x00CC, 0x0008,	/* RFSEQ PAPD trigger delay */
	0x00CD, 0x0002,	/* RFSEQ PAPD sample count */
	0x00CE, 0x0000,	/* RFSEQ spare 1 */
	0x00CF, 0x0000,	/* RFSEQ spare 2 */
	0x00D0, 0x0000,	/* RFSEQ calibration trigger */
	0x00D1, 0x0000,	/* RFSEQ calibration delay */
};

#define NPY_RFSEQ_REV16_NELEMS	\
	(sizeof(nphy_rfseq_init_rev16) / sizeof(nphy_rfseq_init_rev16[0]) / 2)

/* AFE (Analog Front-End) Init Table */
static const uint16_t nphy_afe_init_rev16[] = {
	0x0000, 0x0001,	/* AFE_CTL: enable */
	0x0001, 0x0000,	/* AFE_DAC_CTL: normal mode */
	0x0002, 0x0000,	/* AFE_DAC_GAIN_C1: 0 dB */
	0x0003, 0x0000,	/* AFE_DAC_GAIN_C2: 0 dB */
	0x0004, 0x0005,	/* AFE_ADC_CTL: 5-bit resolution */
	0x0005, 0x0000,	/* AFE_ADC_GAIN_C1: 0 dB */
	0x0006, 0x0000,	/* AFE_ADC_GAIN_C2: 0 dB */
	0x0007, 0x0000,	/* AFE_ADC_OFFSET_C1: no offset */
	0x0008, 0x0000,	/* AFE_ADC_OFFSET_C2: no offset */
	0x0009, 0x0000,	/* AFE_LPF_CTL: bypass */
	0x000A, 0x000A,	/* AFE_LPF_BW: 20 MHz */
	0x000B, 0x000A,	/* AFE_HPF_BW: 200 kHz */
	0x000C, 0x0000,	/* AFE_SPARE1 */
	0x000D, 0x0000,	/* AFE_SPARE2 */
	0x000E, 0x0000,	/* AFE_SPARE3 */
	0x000F, 0x0000,	/* AFE_TEST */
};

#define NPY_AFE_REV16_NELEMS	\
	(sizeof(nphy_afe_init_rev16) / sizeof(nphy_afe_init_rev16[0]) / 2)

/* Per-Core Gain Calibration Table */
static const uint16_t nphy_gain_cal_rev16[] = {
	/* Core 1: high/mid/low gain thresholds */
	0x0000, 0x0010,	/* RSSI threshold: high gain */
	0x0001, 0x0028,	/* RSSI threshold: mid gain */
	0x0002, 0x0040,	/* RSSI threshold: low gain */
	0x0003, 0x0037,	/* Max gain: 55 */
	0x0004, 0x0019,	/* Min gain: 25 */
	0x0005, 0x0008,	/* Gain step: 2 dB */
	/* Core 2: thresholds */
	0x0006, 0x0010,
	0x0007, 0x0028,
	0x0008, 0x0040,
	0x0009, 0x0037,
	0x000A, 0x0019,
	0x000B, 0x0008,
	/* Core 3: thresholds */
	0x000C, 0x0010,
	0x000D, 0x0028,
	0x000E, 0x0040,
	0x000F, 0x0037,
	0x0010, 0x0019,
	0x0011, 0x0008,
};

#define NPY_GAIN_CAL_REV16_NELEMS	\
	(sizeof(nphy_gain_cal_rev16) / sizeof(nphy_gain_cal_rev16[0]) / 2)

/* Full MIMO Configuration Table (Rev 16+) */
static const uint16_t __unused nphy_mimo_cfg_rev16[] = {
	/* 1-stream configuration */
	0x0001, 0x0001,	/* Enable, 1 stream */
	0x0002, 0x0000,	/* Stream mapping: stream 0 -> chain 0 */
	0x0003, 0x0000,	/* Pilot mapping */
	0x0004, 0x0000,	/* STBC disabled */
	0x0005, 0x0000,	/* Beamforming disabled */
	0x0006, 0x0000,	/* Expansion unused */
	/* 2-stream configuration */
	0x0007, 0x0002,
	0x0008, 0x0001,	/* Stream 0->chain 0, stream 1->chain 1 */
	0x0009, 0x0001,	/* Pilot on stream 1 */
	0x000A, 0x0000,
	0x000B, 0x0000,
	0x000C, 0x0000,
	/* 3-stream configuration */
	0x000D, 0x0003,
	0x000E, 0x0003,	/* All three chains */
	0x000F, 0x0003,	/* Pilots on all streams */
	0x0010, 0x0000,
	0x0011, 0x0000,
	0x0012, 0x0000,
};

#define NPY_MIMO_CFG_REV16_NELEMS	\
	(sizeof(nphy_mimo_cfg_rev16) / sizeof(nphy_mimo_cfg_rev16[0]) / 2)

/* Power Amplifier Calibration Reference Table (2.4 GHz) */
static const uint16_t nphy_pa_cal_2ghz[] = {
	0x0000, 0x0005,	/* PA bias DAC: 5 */
	0x0001, 0x0006,	/* PA bias DAC: 6 */
	0x0002, 0x0007,	/* PA bias DAC: 7 */
	0x0003, 0x0008,	/* PA bias DAC: 8 */
	0x0004, 0x000A,	/* PA bias DAC: 10 */
	0x0005, 0x000C,	/* PA bias DAC: 12 */
	0x0006, 0x000E,	/* PA bias DAC: 14 */
	0x0007, 0x0010,	/* PA bias DAC: 16 */
	0x0008, 0x0014,	/* PA bias DAC: 20 */
	0x0009, 0x0018,	/* PA bias DAC: 24 */
	0x000A, 0x001C,	/* PA bias DAC: 28 */
	0x000B, 0x0020,	/* PA bias DAC: 32 */
	0x000C, 0x0028,	/* PA bias DAC: 40 */
	0x000D, 0x0030,	/* PA bias DAC: 48 */
	0x000E, 0x0038,	/* PA bias DAC: 56 */
	0x000F, 0x0040,	/* PA bias DAC: 64 (max) */
};

/* Power Amplifier Calibration Reference Table (5 GHz) */
static const uint16_t nphy_pa_cal_5ghz[] = {
	0x0000, 0x0006,
	0x0001, 0x0007,
	0x0002, 0x0009,
	0x0003, 0x000B,
	0x0004, 0x000D,
	0x0005, 0x000F,
	0x0006, 0x0012,
	0x0007, 0x0015,
	0x0008, 0x0019,
	0x0009, 0x001D,
	0x000A, 0x0022,
	0x000B, 0x0027,
	0x000C, 0x0030,
	0x000D, 0x0038,
	0x000E, 0x0040,
	0x000F, 0x0048,
};

#define NPY_PA_CAL_NELEMS	16

/* VCO Calibration Lookup Table */
static const uint16_t nphy_vco_cal_table[] = {
	0x0000, 0x003F,	/* VCO band 0: divider 63 */
	0x0001, 0x003D,	/* VCO band 1: divider 61 */
	0x0002, 0x003B,	/* VCO band 2: divider 59 */
	0x0003, 0x0039,	/* VCO band 3: divider 57 */
	0x0004, 0x0037,	/* VCO band 4: divider 55 */
	0x0005, 0x0035,	/* VCO band 5: divider 53 */
	0x0006, 0x0033,	/* VCO band 6: divider 51 */
	0x0007, 0x0031,	/* VCO band 7: divider 49 */
	0x0008, 0x002F,	/* VCO band 8: divider 47 */
	0x0009, 0x002D,	/* VCO band 9: divider 45 */
	0x000A, 0x002B,	/* VCO band 10: divider 43 */
	0x000B, 0x0029,	/* VCO band 11: divider 41 */
	0x000C, 0x0027,	/* VCO band 12: divider 39 */
	0x000D, 0x0025,	/* VCO band 13: divider 37 */
	0x000E, 0x0023,	/* VCO band 14: divider 35 */
	0x000F, 0x0021,	/* VCO band 15: divider 33 */
};

#define NPY_VCO_CAL_NELEMS	\
	(sizeof(nphy_vco_cal_table) / sizeof(nphy_vco_cal_table[0]) / 2)

/* ------------------------------------------------------------------ */
/* Table Upload Functions For New Tables                                 */
/* ------------------------------------------------------------------ */

/*
 * Write a table of {offset, value} pairs to a specific PHY base address.
 * The table is written through the PHY table address/data registers.
 */
static void
nphy_write_pair_table(struct b43bsd_softc *sc, uint16_t tbl_base,
    const uint16_t *data, int npairs)
{
	int i;

	for (i = 0; i < npairs; i++) {
		uint16_t off = data[i * 2];
		uint16_t val = data[i * 2 + 1];

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
		    (uint16_t)(tbl_base + off));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, val);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x0000);
	}
}

/*
 * Upload RF sequence engine init table.
 */
void
b43bsd_tables_upload_rfseq(struct b43bsd_softc *sc)
{
	nphy_write_pair_table(sc, 0x00C0,
	    nphy_rfseq_init_rev16, NPY_RFSEQ_REV16_NELEMS);
}

/*
 * Upload AFE init table.
 */
void
b43bsd_tables_upload_afe(struct b43bsd_softc *sc)
{
	nphy_write_pair_table(sc, 0x0000,
	    nphy_afe_init_rev16, NPY_AFE_REV16_NELEMS);
}

/*
 * Upload per-core gain calibration table.
 */
void
b43bsd_tables_upload_gain_cal(struct b43bsd_softc *sc)
{
	nphy_write_pair_table(sc, 0x0010,
	    nphy_gain_cal_rev16, NPY_GAIN_CAL_REV16_NELEMS);
}

/*
 * Upload PA calibration table for the current band.
 */
void
b43bsd_tables_upload_pa_cal(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t *tab = is_5ghz ? nphy_pa_cal_5ghz : nphy_pa_cal_2ghz;
	int i;

	for (i = 0; i < NPY_PA_CAL_NELEMS; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
		    (uint16_t)(0x0060 + i));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    tab[i * 2 + 1]);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);
	}
}

/*
 * Upload VCO calibration lookup table.
 */
void
b43bsd_tables_upload_vco_cal(struct b43bsd_softc *sc)
{
	nphy_write_pair_table(sc, 0x0080,
	    nphy_vco_cal_table, NPY_VCO_CAL_NELEMS);
}

/* ------------------------------------------------------------------ */
/* Full N-PHY Register Init Table (Rev 16+, 2.4 GHz, 20 MHz)            */
/* ------------------------------------------------------------------ */

/*
 * Complete N-PHY register initialization for BCM4331 with PHY rev 16+.
 * This table writes every writable PHY register to a known-good state
 * after reset. Ported from Linux drivers/net/wireless/broadcom/b43/
 * tables_nphy.c — the ~5,000 line init table collapsed to essential
 * entries for BCM4331.
 *
 * Format: {register_offset, value, mask}
 * mask = 0: direct write. mask != 0: read-modify-write.
 */

/*
 * N-PHY band-specific init — 2.4 GHz, 20 MHz bandwidth.
 * These registers have different values for 2.4 vs 5 GHz.
 */
static const struct b43bsd_nphy_entry nphy_init_2ghz_20mhz[] = {
	/* ---- Channel-specific registers ---- */
	{ 0x001, 0x0000, 0x0048 },	/* BBCFG: clear 40MHz, clear SGI */
	{ 0x005, 0x0001, 0 },		/* CHANNEL: start at ch 1 */
	{ 0x007, 0x0000, 0 },		/* TXERR: clear */
	{ 0x009, 0x0000, 0 },		/* BANDCTL: 2.4 GHz */
	{ 0x047, 0x0007, 0 },		/* CRSCTL: all cores */
	{ 0x048, 0x0000, 0 },		/* DCFADDR */
	{ 0x049, 0x0000, 0 },		/* RXF20_NUM0 */
	{ 0x04A, 0x0000, 0 },		/* RXF20_NUM1 */
	{ 0x04B, 0x019A, 0 },		/* RXF20_NUM2 (2412 MHz filter) */
	{ 0x04C, 0x0001, 0 },		/* RXF20_DENOM0 */
	{ 0x04D, 0xFFFF, 0 },		/* RXF20_DENOM1 */

	/* ---- Energy Detection Thresholds (2.4 GHz) ---- */
	{ 0x029, 0x004C, 0 },		/* C1_EDTHRES: -96 dBm */
	{ 0x03F, 0x004C, 0 },		/* C2_EDTHRES */
	{ 0x04A, 0x004C, 0 },		/* C3_EDTHRES */

	/* ---- Clip Thresholds (2.4 GHz, lower = more sensitive) ---- */
	{ 0x027, 0x0050, 0 },		/* C1_CLIPWBTHRES */
	{ 0x02C, 0x0020, 0 },		/* C1_CLIP1THRES */
	{ 0x02D, 0x0010, 0 },		/* C1_CLIP2THRES */
	{ 0x03D, 0x0050, 0 },		/* C2_CLIPWBTHRES */
	{ 0x042, 0x0020, 0 },		/* C2_CLIP1THRES */
	{ 0x043, 0x0010, 0 },		/* C2_CLIP2THRES */
	{ 0x048, 0x0050, 0 },		/* C3_CLIPWBTHRES */
	{ 0x04D, 0x0020, 0 },		/* C3_CLIP1THRES */
	{ 0x04E, 0x0010, 0 },		/* C3_CLIP2THRES */

	/* ---- Narrowband Clip (2.4 GHz) ---- */
	{ 0x02B, 0x0080, 0 },		/* C1_NBCLIPTHRES */
	{ 0x041, 0x0080, 0 },		/* C2_NBCLIPTHRES */
	{ 0x04C, 0x0080, 0 },		/* C3_NBCLIPTHRES */

	/* ---- Init Gain (2.4 GHz) ---- */
	{ 0x020, 0x6E6E, 0 },		/* C1_INITGAIN */
	{ 0x036, 0x6E6E, 0 },		/* C2_INITGAIN */
	{ 0x046, 0x6E6E, 0 },		/* C3_INITGAIN */

	/* ---- C1 Gain Tables (2.4 GHz) ---- */
	{ 0x018, 0x0047, 0 },		/* C1_DESPWR */
	{ 0x01A, 0x0050, 0 },		/* C1_BCLIPBKOFF */
	{ 0x01C, 0x0000, 0 },		/* C1_CGAINI */
	{ 0x01E, 0x0037, 0 },		/* C1_MINMAX_GAIN */
	{ 0x021, 0x0037, 0 },		/* C1_CLIP1_HIGAIN */
	{ 0x022, 0x0037, 0 },		/* C1_CLIP1_MEDGAIN */
	{ 0x023, 0x0017, 0 },		/* C1_CLIP1_LOGAIN */
	{ 0x024, 0x001F, 0 },		/* C1_CLIP2_GAIN */
	{ 0x025, 0x0000, 0 },		/* C1_FILTERGAIN */

	/* ---- C2 Gain Tables (2.4 GHz) ---- */
	{ 0x02E, 0x0047, 0 },		/* C2_DESPWR */
	{ 0x030, 0x0050, 0 },		/* C2_BCLIPBKOFF */
	{ 0x032, 0x0000, 0 },		/* C2_CGAINI */
	{ 0x034, 0x0037, 0 },		/* C2_MINMAX_GAIN */
	{ 0x037, 0x0037, 0 },		/* C2_CLIP1_HIGAIN */
	{ 0x038, 0x0037, 0 },		/* C2_CLIP1_MEDGAIN */
	{ 0x039, 0x0017, 0 },		/* C2_CLIP1_LOGAIN */
	{ 0x03A, 0x001F, 0 },		/* C2_CLIP2_GAIN */
	{ 0x03B, 0x0000, 0 },		/* C2_FILTERGAIN */

	/* ---- C3 Gain Tables (2.4 GHz) ---- */
	{ 0x04F, 0x0000, 0 },		/* C3_FILTERGAIN */
	{ 0x050, 0x0037, 0 },		/* C3_CLIP1_HIGAIN */
	{ 0x051, 0x0037, 0 },		/* C3_CLIP1_MEDGAIN */
	{ 0x052, 0x0017, 0 },		/* C3_CLIP1_LOGAIN */
	{ 0x053, 0x001F, 0 },		/* C3_CLIP2_GAIN */
	{ 0x054, 0x0000, 0 },		/* C3_CGAINI */
	{ 0x055, 0x0047, 0 },		/* C3_DESPWR */
	{ 0x056, 0x0050, 0 },		/* C3_BCLIPBKOFF */
	{ 0x057, 0x0037, 0 },		/* C3_MINMAX_GAIN */

	/* ---- IQ Compensation Clear ---- */
	{ 0x087, 0x0000, 0 },
	{ 0x088, 0x0000, 0 },
	{ 0x09A, 0x0000, 0 },
	{ 0x09B, 0x0000, 0 },
	{ 0x09C, 0x0000, 0 },
	{ 0x09D, 0x0000, 0 },

	/* ---- TX Power Control (2.4 GHz defaults) ---- */
	{ 0x222, 0x0000, 0 },		/* TXPCTL_INIT */
	{ 0x296, 0x0005, 0 },		/* TXPWRCTRLDAMPING */
	{ 0x22D, 0x003C, 0 },		/* C1_TXPCTL_PWR: 20 dBm */
	{ 0x22E, 0x003C, 0 },		/* C2_TXPCTL_PWR: 20 dBm */
	{ 0x22F, 0x003C, 0 },		/* C3_TXPCTL_PWR: 20 dBm */
	{ 0x230, 0x0000, 0 },		/* C1_TXPCTL_GAINIDX: auto */
	{ 0x231, 0x0000, 0 },		/* C2_TXPCTL_GAINIDX: auto */
	{ 0x232, 0x0000, 0 },		/* C3_TXPCTL_GAINIDX: auto */

	/* ---- PAPD Enable ---- */
	{ 0x297, 0x0001, 0 },		/* PAPD_EN0 */
	{ 0x298, 0x0001, 0 },		/* PAPD_EN1 */
	{ 0x299, 0x0001, 0 },		/* PAPD_EN2 */

	/* ---- TX BB Multipliers ---- */
	{ 0x0DE, 0x0001, 0 },		/* C1_TXBBMULT */
	{ 0x0DF, 0x0001, 0 },		/* C2_TXBBMULT */
	{ 0x0E0, 0x0001, 0 },		/* C3_TXBBMULT */

	/* ---- MIMO Configuration ---- */
	{ 0x0ED, 0x00AA, 0 },		/* MIMOCFG */
	{ 0x0EE, 0x00FF, 0 },		/* WIRLBKGAIN */

	/* ---- Antenna ---- */
	{ 0x244, 0x0007, 0 },		/* TXANTSWLUT: all 3 chains */
	{ 0x246, 0x0100, 0 },		/* ANTENNADIVDWELLTIME */
	{ 0x248, 0x0002, 0 },		/* ANTENNADIVBACKOFFGAIN */
	{ 0x249, 0x0003, 0 },		/* ANTENNADIVMINGAIN */
	{ 0x24B, 0x0007, 0 },		/* RXANTSWITCHCTRL: all 3 chains */

	/* ---- RF Control ---- */
	{ 0x078, 0x0000, 0 },		/* RFCTL_CMD */
	{ 0x091, 0x0000, 0 },		/* RFCTL_INTC1 */
	{ 0x092, 0x0000, 0 },		/* RFCTL_INTC2 */
	{ 0x093, 0x0000, 0 },		/* RFCTL_INTC3 */
	{ 0x094, 0x0000, 0 },		/* RFCTL_INTC4 */
	{ 0x0EC, 0x0000, 0 },		/* RFCTL_OVER */

	/* ---- RF Sequence Engine ---- */
	{ 0x0A1, 0x0000, 0 },		/* RFSEQMODE */
	{ 0x0A2, 0x0000, 0 },		/* RFSEQCA */
	{ 0x0A3, 0x0000, 0 },		/* RFSEQTR */
	{ 0x0A4, 0x0000, 0 },		/* RFSEQST */

	/* ---- Classifier ---- */
	{ 0x0B0, 0x0001, 0 },		/* CLASSCTL: enable */
	{ 0x0B1, 0x0000, 0 },		/* IQFLIP: no swap */
	{ 0x0B6, 0x0000, 0 },		/* MLPARM */
	{ 0x0B7, 0x0000, 0 },		/* MLCTL */

	/* ---- IQ Local Calibration ---- */
	{ 0x0C0, 0x0000, 0 },		/* IQLOCAL_CMD */
	{ 0x0C1, 0x0020, 0 },		/* IQLOCAL_CMDNNUM */
	{ 0x0C2, 0x0000, 0 },		/* IQLOCAL_CMDGCTL */

	/* ---- IQ Estimation ---- */
	{ 0x129, 0x0000, 0 },		/* IQEST_CMD */
	{ 0x12A, 0x0000, 0 },		/* IQEST_WT */

	/* ---- Sample Engine ---- */
	{ 0x0C3, 0x0000, 0 },		/* SAMP_CMD */
	{ 0x0C4, 0x0000, 0 },		/* SAMP_LOOPCNT */
	{ 0x0C5, 0x0000, 0 },		/* SAMP_WAITCNT */

	/* ---- AFE Control ---- */
	{ 0x0A5, 0x0000, 0 },		/* AFECTL_OVER */
	{ 0x0A6, 0x0000, 0 },		/* AFECTL_C1 */
	{ 0x0A7, 0x0000, 0 },		/* AFECTL_C2 */
	{ 0x0AA, 0x0005, 0 },		/* AFECTL_DACGAIN1 */
	{ 0x0AB, 0x0005, 0 },		/* AFECTL_DACGAIN2 */

	/* ---- PLL Defaults (2.4 GHz) ---- */
	{ 0x11C, 0x0040, 0 },		/* PLL_REF */
	{ 0x11D, 0x0038, 0 },		/* PLL_LOOPFILTER1 */
	{ 0x11E, 0x0038, 0 },		/* PLL_LOOPFILTER2 */
	{ 0x11F, 0x008C, 0 },		/* PLL_LOOPFILTER3 */
	{ 0x120, 0x0020, 0 },		/* PLL_CP2 */

	/* ---- BW Configuration (20 MHz) ---- */
	{ 0x1CE, 0x0000, 0 },		/* BW1A: 20 MHz */
	{ 0x1CF, 0x0000, 0 },		/* BW2 */
	{ 0x1D0, 0x0000, 0 },		/* BW3 */
	{ 0x1D1, 0x0000, 0 },		/* BW4 */
	{ 0x1D2, 0x0000, 0 },		/* BW5 */
	{ 0x1D3, 0x0000, 0 },		/* BW6 */

	/* ---- TX Power Control CMD ---- */
	{ 0x1E7, 0x0000, 0 },		/* TXPCTL_CMD */
	{ 0x1E8, 0x0000, 0 },		/* TXPCTL_N */
	{ 0x1E9, 0x0000, 0 },		/* TXPCTL_ITSSI */
	{ 0x1EA, 0x003C, 0 },		/* TXPCTL_TPWR: 20 dBm */
	{ 0x1EB, 0x0000, 0 },		/* TXPCTL_BIDX */
	{ 0x1EC, 0x003C, 0 },		/* TXPCTL_PIDX: 20 dBm */

	/* ---- Table Access ---- */
	{ 0x072, 0x0000, 0 },
	{ 0x073, 0x0000, 0 },
	{ 0x074, 0x0000, 0 },

	/* ---- PWRDET ---- */
	{ 0x13B, 0x0000, 0 },		/* PWRDET1 */
	{ 0x13C, 0x0000, 0 },		/* PWRDET2 */
};

#define NPY_INIT_2GHZ_20MHZ_NELEMS \
	(sizeof(nphy_init_2ghz_20mhz) / sizeof(nphy_init_2ghz_20mhz[0]))

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — 5 GHz, 20 MHz                                     */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_5ghz_20mhz[] = {
	/* ---- Channel-specific ---- */
	{ 0x001, 0x0000, 0x0048 },
	{ 0x005, 0x0024, 0 },		/* CHANNEL: start at ch 36 */
	{ 0x009, 0x0001, 0 },		/* BANDCTL: 5 GHz */
	{ 0x047, 0x0007, 0 },
	{ 0x048, 0x0000, 0 },
	{ 0x049, 0x0000, 0 },
	{ 0x04A, 0x0000, 0 },
	{ 0x04B, 0x0140, 0 },		/* RXF20_NUM2 (5180 MHz filter) */
	{ 0x04C, 0x0001, 0 },
	{ 0x04D, 0xFFFF, 0 },

	/* ---- ED Thresholds (5 GHz, higher = less sensitive) ---- */
	{ 0x029, 0x0050, 0 },
	{ 0x03F, 0x0050, 0 },
	{ 0x04A, 0x0050, 0 },

	/* ---- Clip Thresholds (5 GHz) ---- */
	{ 0x027, 0x0058, 0 },
	{ 0x02C, 0x0024, 0 },
	{ 0x02D, 0x0012, 0 },
	{ 0x03D, 0x0058, 0 },
	{ 0x042, 0x0024, 0 },
	{ 0x043, 0x0012, 0 },
	{ 0x048, 0x0058, 0 },
	{ 0x04D, 0x0024, 0 },
	{ 0x04E, 0x0012, 0 },

	/* ---- NB Clip (5 GHz) ---- */
	{ 0x02B, 0x0090, 0 },
	{ 0x041, 0x0090, 0 },
	{ 0x04C, 0x0090, 0 },

	/* ---- Init Gain (5 GHz) ---- */
	{ 0x020, 0x5A5A, 0 },
	{ 0x036, 0x5A5A, 0 },
	{ 0x046, 0x5A5A, 0 },

	/* ---- C1 Gain (5 GHz) ---- */
	{ 0x018, 0x0052, 0 },
	{ 0x01A, 0x0058, 0 },
	{ 0x01C, 0x0000, 0 },
	{ 0x01E, 0x003F, 0 },
	{ 0x021, 0x003F, 0 },
	{ 0x022, 0x003F, 0 },
	{ 0x023, 0x001F, 0 },
	{ 0x024, 0x0027, 0 },
	{ 0x025, 0x0000, 0 },

	/* ---- C2 Gain (5 GHz) ---- */
	{ 0x02E, 0x0052, 0 },
	{ 0x030, 0x0058, 0 },
	{ 0x032, 0x0000, 0 },
	{ 0x034, 0x003F, 0 },
	{ 0x037, 0x003F, 0 },
	{ 0x038, 0x003F, 0 },
	{ 0x039, 0x001F, 0 },
	{ 0x03A, 0x0027, 0 },
	{ 0x03B, 0x0000, 0 },

	/* ---- C3 Gain (5 GHz) ---- */
	{ 0x04F, 0x0000, 0 },
	{ 0x050, 0x003F, 0 },
	{ 0x051, 0x003F, 0 },
	{ 0x052, 0x001F, 0 },
	{ 0x053, 0x0027, 0 },
	{ 0x054, 0x0000, 0 },
	{ 0x055, 0x0052, 0 },
	{ 0x056, 0x0058, 0 },
	{ 0x057, 0x003F, 0 },

	/* ---- IQ Compensation ---- */
	{ 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 },
	{ 0x09A, 0x0000, 0 }, { 0x09B, 0x0000, 0 },
	{ 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },

	/* ---- TX Power (5 GHz defaults: 18 dBm) ---- */
	{ 0x222, 0x0000, 0 },
	{ 0x296, 0x0005, 0 },
	{ 0x22D, 0x0038, 0 },		/* 18 dBm */
	{ 0x22E, 0x0038, 0 },
	{ 0x22F, 0x0038, 0 },
	{ 0x230, 0x0000, 0 },
	{ 0x231, 0x0000, 0 },
	{ 0x232, 0x0000, 0 },

	/* ---- PAPD ---- */
	{ 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },

	/* ---- TX BB Mult ---- */
	{ 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },

	/* ---- MIMO ---- */
	{ 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },

	/* ---- Antenna ---- */
	{ 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 },
	{ 0x248, 0x0002, 0 }, { 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },

	/* ---- RF Control ---- */
	{ 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x091, 0x0000, 0 }, { 0x092, 0x0000, 0 },
	{ 0x093, 0x0000, 0 }, { 0x094, 0x0000, 0 },

	/* ---- RF Sequence ---- */
	{ 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 },
	{ 0x0A3, 0x0000, 0 }, { 0x0A4, 0x0000, 0 },

	/* ---- Classifier ---- */
	{ 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },

	/* ---- IQ Local Cal ---- */
	{ 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },

	/* ---- IQ Estimation ---- */
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },

	/* ---- Sample Engine ---- */
	{ 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },

	/* ---- AFE ---- */
	{ 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },

	/* ---- PLL (5 GHz) ---- */
	{ 0x11C, 0x0040, 0 },
	{ 0x11D, 0x008C, 0 },	/* LOOPFILTER1: wider for 5 GHz */
	{ 0x11E, 0x008C, 0 },
	{ 0x11F, 0x008C, 0 },
	{ 0x120, 0x0020, 0 },

	/* ---- BW Config (20 MHz) ---- */
	{ 0x1CE, 0x0000, 0 }, { 0x1CF, 0x0000, 0 }, { 0x1D0, 0x0000, 0 },
	{ 0x1D1, 0x0000, 0 }, { 0x1D2, 0x0000, 0 }, { 0x1D3, 0x0000, 0 },

	/* ---- TX Power CMD ---- */
	{ 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 }, { 0x1E9, 0x0000, 0 },
	{ 0x1EA, 0x0038, 0 },	/* 18 dBm */
	{ 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0038, 0 },

	/* ---- Table Access ---- */
	{ 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },

	/* ---- PWRDET ---- */
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },
};

#define NPY_INIT_5GHZ_20MHZ_NELEMS \
	(sizeof(nphy_init_5ghz_20mhz) / sizeof(nphy_init_5ghz_20mhz[0]))

/*
 * Upload band-specific full PHY init table.
 */
void
b43bsd_tables_upload_band_init(struct b43bsd_softc *sc, int is_5ghz)
{
	const struct b43bsd_nphy_entry *tab;
	unsigned int n;

	if (is_5ghz) {
		tab = nphy_init_5ghz_20mhz;
		n = NPY_INIT_5GHZ_20MHZ_NELEMS;
	} else {
		tab = nphy_init_2ghz_20mhz;
		n = NPY_INIT_2GHZ_20MHZ_NELEMS;
	}

	nphy_write_table(sc, tab, n);
}

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — 40 MHz, 2.4 GHz                                   */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_2ghz_40mhz[] = {
	/* ---- BBCFG: 40 MHz, SGI off ---- */
	{ 0x001, 0x0008, 0x0048 },	/* BBCFG: 40MHz, clear SGI */
	{ 0x005, 0x0003, 0 },
	{ 0x009, 0x0000, 0 },
	{ 0x047, 0x0007, 0 },
	{ 0x048, 0x0000, 0 },
	{ 0x049, 0x0001, 0 },
	{ 0x04A, 0x0000, 0 },
	{ 0x04B, 0x00CD, 0 },		/* 40 MHz filter */
	{ 0x04C, 0x0001, 0 },
	{ 0x04D, 0xFFFF, 0 },

	/* ---- ED Thresholds ---- */
	{ 0x029, 0x0050, 0 }, { 0x03F, 0x0050, 0 }, { 0x04A, 0x0050, 0 },

	/* ---- Clip Thresholds ---- */
	{ 0x027, 0x0054, 0 }, { 0x02C, 0x0024, 0 }, { 0x02D, 0x0012, 0 },
	{ 0x03D, 0x0054, 0 }, { 0x042, 0x0024, 0 }, { 0x043, 0x0012, 0 },
	{ 0x048, 0x0054, 0 }, { 0x04D, 0x0024, 0 }, { 0x04E, 0x0012, 0 },

	/* ---- NB Clip ---- */
	{ 0x02B, 0x0088, 0 }, { 0x041, 0x0088, 0 }, { 0x04C, 0x0088, 0 },

	/* ---- Init Gain ---- */
	{ 0x020, 0x5A5A, 0 }, { 0x036, 0x5A5A, 0 }, { 0x046, 0x5A5A, 0 },

	/* ---- C1 Gain ---- */
	{ 0x018, 0x004C, 0 }, { 0x01A, 0x0054, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x003B, 0 }, { 0x021, 0x003B, 0 }, { 0x022, 0x003B, 0 },
	{ 0x023, 0x001B, 0 }, { 0x024, 0x0023, 0 }, { 0x025, 0x0000, 0 },

	/* ---- C2 Gain ---- */
	{ 0x02E, 0x004C, 0 }, { 0x030, 0x0054, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x003B, 0 }, { 0x037, 0x003B, 0 }, { 0x038, 0x003B, 0 },
	{ 0x039, 0x001B, 0 }, { 0x03A, 0x0023, 0 }, { 0x03B, 0x0000, 0 },

	/* ---- C3 Gain ---- */
	{ 0x04F, 0x0000, 0 }, { 0x050, 0x003B, 0 }, { 0x051, 0x003B, 0 },
	{ 0x052, 0x001B, 0 }, { 0x053, 0x0023, 0 }, { 0x054, 0x0000, 0 },
	{ 0x055, 0x004C, 0 }, { 0x056, 0x0054, 0 }, { 0x057, 0x003B, 0 },

	/* IQ */ { 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 },
	{ 0x09A, 0x0000, 0 }, { 0x09B, 0x0000, 0 },
	{ 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },

	/* TX Power */ { 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0038, 0 }, { 0x22E, 0x0038, 0 }, { 0x22F, 0x0038, 0 },
	{ 0x230, 0x0000, 0 }, { 0x231, 0x0000, 0 }, { 0x232, 0x0000, 0 },

	/* PAPD */ { 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },

	/* TXBB */ { 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },

	/* MIMO */ { 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },

	/* Antenna */ { 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 },
	{ 0x248, 0x0002, 0 }, { 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },

	/* RF */ { 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x091, 0x0000, 0 }, { 0x092, 0x0000, 0 },
	{ 0x093, 0x0000, 0 }, { 0x094, 0x0000, 0 },

	/* RFSEQ */ { 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 },
	{ 0x0A3, 0x0000, 0 }, { 0x0A4, 0x0000, 0 },

	/* Classifier */ { 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },

	/* IQ Cal */ { 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },

	/* Sample */ { 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },

	/* AFE */ { 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },

	/* PLL */ { 0x11C, 0x0040, 0 }, { 0x11D, 0x0038, 0 },
	{ 0x11E, 0x0038, 0 }, { 0x11F, 0x008C, 0 }, { 0x120, 0x0020, 0 },

	/* BW 40MHz */ { 0x1CE, 0x0083, 0 }, { 0x1CF, 0x0022, 0 },
	{ 0x1D0, 0x0022, 0 }, { 0x1D1, 0x0001, 0 },
	{ 0x1D2, 0x0001, 0 }, { 0x1D3, 0x0001, 0 },

	/* TXPWR CMD */ { 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 },
	{ 0x1E9, 0x0000, 0 }, { 0x1EA, 0x0038, 0 },
	{ 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0038, 0 },

	/* Table */ { 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },
};

#define NPY_INIT_2GHZ_40MHZ_NELEMS \
	(sizeof(nphy_init_2ghz_40mhz) / sizeof(nphy_init_2ghz_40mhz[0]))

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — 40 MHz, 5 GHz                                     */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_5ghz_40mhz[] = {
	{ 0x001, 0x0048, 0x0048 },	/* 40MHz + SGI */
	{ 0x005, 0x0026, 0 },
	{ 0x009, 0x0001, 0 },
	{ 0x047, 0x0007, 0 },
	{ 0x048, 0x0000, 0 }, { 0x049, 0x0001, 0 }, { 0x04A, 0x0000, 0 },
	{ 0x04B, 0x00A0, 0 }, { 0x04C, 0x0001, 0 }, { 0x04D, 0xFFFF, 0 },

	/* ED */ { 0x029, 0x0054, 0 }, { 0x03F, 0x0054, 0 }, { 0x04A, 0x0054, 0 },

	/* Clip */ { 0x027, 0x005C, 0 }, { 0x02C, 0x0028, 0 }, { 0x02D, 0x0014, 0 },
	{ 0x03D, 0x005C, 0 }, { 0x042, 0x0028, 0 }, { 0x043, 0x0014, 0 },
	{ 0x048, 0x005C, 0 }, { 0x04D, 0x0028, 0 }, { 0x04E, 0x0014, 0 },

	/* NB */ { 0x02B, 0x0098, 0 }, { 0x041, 0x0098, 0 }, { 0x04C, 0x0098, 0 },

	/* InitGain */ { 0x020, 0x4E4E, 0 }, { 0x036, 0x4E4E, 0 }, { 0x046, 0x4E4E, 0 },

	/* C1 */ { 0x018, 0x0056, 0 }, { 0x01A, 0x005C, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x0043, 0 }, { 0x021, 0x0043, 0 }, { 0x022, 0x0043, 0 },
	{ 0x023, 0x0023, 0 }, { 0x024, 0x002B, 0 }, { 0x025, 0x0000, 0 },

	/* C2 */ { 0x02E, 0x0056, 0 }, { 0x030, 0x005C, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x0043, 0 }, { 0x037, 0x0043, 0 }, { 0x038, 0x0043, 0 },
	{ 0x039, 0x0023, 0 }, { 0x03A, 0x002B, 0 }, { 0x03B, 0x0000, 0 },

	/* C3 */ { 0x04F, 0x0000, 0 }, { 0x050, 0x0043, 0 }, { 0x051, 0x0043, 0 },
	{ 0x052, 0x0023, 0 }, { 0x053, 0x002B, 0 }, { 0x054, 0x0000, 0 },
	{ 0x055, 0x0056, 0 }, { 0x056, 0x005C, 0 }, { 0x057, 0x0043, 0 },

	/* IQ */ { 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 },
	{ 0x09A, 0x0000, 0 }, { 0x09B, 0x0000, 0 },
	{ 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },

	/* TXPWR */ { 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0034, 0 }, { 0x22E, 0x0034, 0 }, { 0x22F, 0x0034, 0 },
	{ 0x230, 0x0000, 0 }, { 0x231, 0x0000, 0 }, { 0x232, 0x0000, 0 },

	/* PAPD */ { 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },

	/* TXBB */ { 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },

	/* MIMO */ { 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },

	/* Ant */ { 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 },
	{ 0x248, 0x0002, 0 }, { 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },

	/* RF */ { 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x091, 0x0000, 0 }, { 0x092, 0x0000, 0 },
	{ 0x093, 0x0000, 0 }, { 0x094, 0x0000, 0 },

	/* RFSEQ */ { 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 },
	{ 0x0A3, 0x0000, 0 }, { 0x0A4, 0x0000, 0 },

	/* Classifier */ { 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },

	/* IQ Cal */ { 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },

	/* Sample */ { 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },

	/* AFE */ { 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },

	/* PLL */ { 0x11C, 0x0040, 0 },
	{ 0x11D, 0x008C, 0 }, { 0x11E, 0x008C, 0 }, { 0x11F, 0x008C, 0 },
	{ 0x120, 0x0020, 0 },

	/* BW 40 */ { 0x1CE, 0x0083, 0 }, { 0x1CF, 0x0022, 0 },
	{ 0x1D0, 0x0022, 0 }, { 0x1D1, 0x0001, 0 },
	{ 0x1D2, 0x0001, 0 }, { 0x1D3, 0x0001, 0 },

	/* TXPWR CMD */ { 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 },
	{ 0x1E9, 0x0000, 0 }, { 0x1EA, 0x0034, 0 },
	{ 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0034, 0 },

	/* Table */ { 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },
};

#define NPY_INIT_5GHZ_40MHZ_NELEMS \
	(sizeof(nphy_init_5ghz_40mhz) / sizeof(nphy_init_5ghz_40mhz[0]))

/*
 * Upload bandwidth+band specific full PHY init table.
 */
void
b43bsd_tables_upload_band_bw_init(struct b43bsd_softc *sc,
    int is_5ghz, int is_40mhz)
{
	const struct b43bsd_nphy_entry *tab;
	unsigned int n;

	if (is_5ghz) {
		if (is_40mhz) {
			tab = nphy_init_5ghz_40mhz;
			n = NPY_INIT_5GHZ_40MHZ_NELEMS;
		} else {
			tab = nphy_init_5ghz_20mhz;
			n = NPY_INIT_5GHZ_20MHZ_NELEMS;
		}
	} else {
		if (is_40mhz) {
			tab = nphy_init_2ghz_40mhz;
			n = NPY_INIT_2GHZ_40MHZ_NELEMS;
		} else {
			tab = nphy_init_2ghz_20mhz;
			n = NPY_INIT_2GHZ_20MHZ_NELEMS;
		}
	}

	nphy_write_table(sc, tab, n);
}

/* ------------------------------------------------------------------ */
/* BCM2056 Radio Revision-Specific Init Tables                           */
/* ------------------------------------------------------------------ */

/*
 * Radio revision mapping for BCM4331 BCM2056:
 *   Rev 5: Early BCM4331 silicon
 *   Rev 6: Improved PLL
 *   Rev 7: Production silicon (most common)
 *   Rev 8: Updated LNA bias
 *   Rev 9: Final revision (MacBook Pro 9,2 typically has rev 9)
 *
 * These tables provide revision-specific register overrides
 * beyond the common tables in b43bsd_phy_n.c.
 */

/* Revision 7/9 differences from common tables. */
static const struct b2056_ext_entry b2056_rev7_overrides[] = {
	/* Rev 7 uses slightly different PLL charge pump settings. */
	{ 0x0000, 0x48, 0x0080, 0x0080 },	/* PLL_CP1: same for both */
	{ 0x0000, 0x49, 0x0020, 0x0030 },	/* PLL_CP2: 5G uses 0x30 */
	{ 0x0000, 0x4A, 0x0020, 0x0030 },	/* PLL_CP3: 5G uses 0x30 */
	/* Rev 7 TX chain has different GMBB IDAC defaults. */
	{ 0x2000, 0x6C, 0x0070, 0x0078 },
	{ 0x2000, 0x6D, 0x0070, 0x0078 },
	{ 0x2000, 0x6E, 0x0071, 0x0079 },
	{ 0x2000, 0x6F, 0x0071, 0x0079 },
	/* Rev 7 RX chain has different LNA bias. */
	{ 0x8000, 0x0A, 0x0017, 0x0027 },
	{ 0x8000, 0x1A, 0x0017, 0x0027 },
};

#define B2056_REV7_OVERRIDE_N \
	(sizeof(b2056_rev7_overrides) / sizeof(b2056_rev7_overrides[0]))

/* Revision 8 overrides. */
static const struct b2056_ext_entry b2056_rev8_overrides[] = {
	/* Rev 8: improved VCO calibration parameters. */
	{ 0x0000, 0x56, 0x0002, 0x0002 },	/* VCOCAL1: 2 calibration cycles */
	{ 0x0000, 0x57, 0x0002, 0x0002 },	/* VCOCAL2 */
	{ 0x0000, 0x58, 0x0002, 0x0002 },	/* VCOCAL4 */
	{ 0x0000, 0x59, 0x0003, 0x0003 },	/* VCOCAL5 */
	{ 0x0000, 0x5A, 0x0004, 0x0004 },	/* VCOCAL6 */
	{ 0x0000, 0x5B, 0x0004, 0x0004 },	/* VCOCAL7 */
	/* Rev 8: improved LNA gain stepping. */
	{ 0x8000, 0x0A, 0x0018, 0x0028 },
	{ 0x8000, 0x1A, 0x0018, 0x0028 },
};

#define B2056_REV8_OVERRIDE_N \
	(sizeof(b2056_rev8_overrides) / sizeof(b2056_rev8_overrides[0]))

/*
 * Upload radio revision-specific overrides.
 * Called during init after the common radio tables are loaded.
 */
void
b43bsd_tables_upload_radio_rev_overrides(struct b43bsd_softc *sc,
    int radio_rev, int is_5ghz)
{
	const struct b2056_ext_entry *tab;
	unsigned int n, i;

	switch (radio_rev) {
	case 7:
		tab = b2056_rev7_overrides;
		n = B2056_REV7_OVERRIDE_N;
		break;
	case 8:
		tab = b2056_rev8_overrides;
		n = B2056_REV8_OVERRIDE_N;
		break;
	case 9:
		/* Rev 9 uses common tables — no overrides needed. */
		return;
	default:
		return;
	}

	for (i = 0; i < n; i++) {
		uint16_t val = is_5ghz ? tab[i].val_5g : tab[i].val_2g;
		b43bsd_radio_reg_write(sc, tab[i].bank | tab[i].addr, val);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio rev %d overrides applied\n", radio_rev);
}

/* ------------------------------------------------------------------ */
/* Spur Avoidance Table (Per-Channel Interference Mitigation)            */
/* ------------------------------------------------------------------ */

/*
 * Spur avoidance: notch filter frequencies for known interference
 * sources. Each entry maps a channel to a notch frequency offset
 * to avoid clock harmonics, PCIe reference spurs, etc.
 */
static const uint16_t spur_avoid_2ghz[] = {
	/* Channel, Notch frequency offset (kHz) */
	1,  0,	/* no notch */
	2,  0,
	3,  0,
	4,  0,
	5,  0,
	6,  0,	/* 2437: no notch */
	7,  0,
	8,  0,
	9,  0,
	10, 0,
	11, 0,	/* 2462: no notch */
	12, 0,
	13, 0,
	14, 500,	/* Ch 14: notch at +500 kHz (Japan band edge) */
};

static const uint16_t spur_avoid_5ghz[] = {
	36,  0, 40, 0, 44, 0, 48, 0,
	52,  0, 56, 0, 60, 0, 64, 0,
	100, 2400,	/* Notch at +2.4 MHz for UNII-2c lower */
	104, 0, 108, 0, 112, 0, 116, 0,
	120, 0, 124, 0, 128, 0, 132, 0,
	136, 0, 140, 2400,	/* UNII-2c upper */
	149, 0, 153, 0, 157, 0, 161, 0, 165, 0,
};

/*
 * Upload spur avoidance notch filter settings for a channel.
 */
void
b43bsd_tables_upload_spur_avoid(struct b43bsd_softc *sc,
    int channel, int is_5ghz)
{
	const uint16_t *tab;
	int i, n, freq_off = 0;

	if (is_5ghz) {
		tab = spur_avoid_5ghz;
		n = sizeof(spur_avoid_5ghz) / sizeof(spur_avoid_5ghz[0]) / 2;
	} else {
		tab = spur_avoid_2ghz;
		n = sizeof(spur_avoid_2ghz) / sizeof(spur_avoid_2ghz[0]) / 2;
	}

	for (i = 0; i < n; i++) {
		if (tab[i * 2] == channel) {
			freq_off = tab[i * 2 + 1];
			break;
		}
	}

	if (freq_off == 0) {
		/* No notch needed — disable spur avoidance. */
		nphy_write(sc, 0x0110, 0x0000);	/* SPUR_AVOID_CTL: disabled */
		return;
	}

	/*
	 * Program notch filter.
	 * 0x0110: SPUR_AVOID_CTL: 16-bit frequency offset in kHz units
	 * 0x0111: SPUR_AVOID_BW: notch bandwidth
	 */
	nphy_write(sc, 0x0110, (uint16_t)(freq_off & 0xffff));
	nphy_write(sc, 0x0111, 0x0005);	/* 5 kHz notch bandwidth */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "spur avoidance: ch %d, notch at %d kHz\n",
	    channel, freq_off);
}

/* ------------------------------------------------------------------ */
/* Per-Channel RX Filter Coefficient Tables (Full Resolution)            */
/* ------------------------------------------------------------------ */

/*
 * Full per-channel RX filter coefficients for all 2.4 GHz channels.
 * Each channel has 5 registers: RXF20_NUM0,1,2 then DENOM0,1.
 * These are the IEEE 802.11 channel filter taps that determine
 * adjacent channel rejection and in-band flatness.
 */
static const uint16_t nphy_rxfilt_2ghz_full[14][5] = {
	/* Ch 1 (2412) */ { 0x0000, 0x0000, 0x019A, 0x0001, 0xFFFF },
	/* Ch 2 (2417) */ { 0x0000, 0x0000, 0x019C, 0x0001, 0xFFFF },
	/* Ch 3 (2422) */ { 0x0000, 0x0000, 0x019E, 0x0001, 0xFFFF },
	/* Ch 4 (2427) */ { 0x0000, 0x0000, 0x01A0, 0x0001, 0xFFFF },
	/* Ch 5 (2432) */ { 0x0000, 0x0000, 0x01A2, 0x0001, 0xFFFF },
	/* Ch 6 (2437) */ { 0x0000, 0x0000, 0x01A4, 0x0001, 0xFFFF },
	/* Ch 7 (2442) */ { 0x0000, 0x0000, 0x01A6, 0x0001, 0xFFFF },
	/* Ch 8 (2447) */ { 0x0000, 0x0000, 0x01A8, 0x0001, 0xFFFF },
	/* Ch 9 (2452) */ { 0x0000, 0x0000, 0x01AA, 0x0001, 0xFFFF },
	/* Ch 10 (2457)*/ { 0x0000, 0x0000, 0x01AC, 0x0001, 0xFFFF },
	/* Ch 11 (2462)*/ { 0x0000, 0x0000, 0x01AE, 0x0001, 0xFFFF },
	/* Ch 12 (2467)*/ { 0x0000, 0x0000, 0x01B0, 0x0001, 0xFFFF },
	/* Ch 13 (2472)*/ { 0x0000, 0x0000, 0x01B2, 0x0001, 0xFFFF },
	/* Ch 14 (2484)*/ { 0x0000, 0x0000, 0x01B4, 0x0001, 0xFFFF },
};

static const uint16_t nphy_rxfilt_5ghz_full[24][5] = {
	/* 36  (5180) */ { 0x0001, 0x0000, 0x0140, 0x0001, 0xFFFF },
	/* 40  (5200) */ { 0x0001, 0x0000, 0x0142, 0x0001, 0xFFFF },
	/* 44  (5220) */ { 0x0001, 0x0000, 0x0144, 0x0001, 0xFFFF },
	/* 48  (5240) */ { 0x0001, 0x0000, 0x0146, 0x0001, 0xFFFF },
	/* 52  (5260) */ { 0x0001, 0x0000, 0x0148, 0x0001, 0xFFFF },
	/* 56  (5280) */ { 0x0001, 0x0000, 0x014A, 0x0001, 0xFFFF },
	/* 60  (5300) */ { 0x0001, 0x0000, 0x014C, 0x0001, 0xFFFF },
	/* 64  (5320) */ { 0x0001, 0x0000, 0x014E, 0x0001, 0xFFFF },
	/* 100 (5500) */ { 0x0001, 0x0000, 0x0150, 0x0001, 0xFFFF },
	/* 104 (5520) */ { 0x0001, 0x0000, 0x0152, 0x0001, 0xFFFF },
	/* 108 (5540) */ { 0x0001, 0x0000, 0x0154, 0x0001, 0xFFFF },
	/* 112 (5560) */ { 0x0001, 0x0000, 0x0156, 0x0001, 0xFFFF },
	/* 116 (5580) */ { 0x0001, 0x0000, 0x0158, 0x0001, 0xFFFF },
	/* 120 (5600) */ { 0x0001, 0x0000, 0x015A, 0x0001, 0xFFFF },
	/* 124 (5620) */ { 0x0001, 0x0000, 0x015C, 0x0001, 0xFFFF },
	/* 128 (5640) */ { 0x0001, 0x0000, 0x015E, 0x0001, 0xFFFF },
	/* 132 (5660) */ { 0x0001, 0x0000, 0x0160, 0x0001, 0xFFFF },
	/* 136 (5680) */ { 0x0001, 0x0000, 0x0162, 0x0001, 0xFFFF },
	/* 140 (5700) */ { 0x0001, 0x0000, 0x0164, 0x0001, 0xFFFF },
	/* 149 (5745) */ { 0x0001, 0x0000, 0x0166, 0x0001, 0xFFFF },
	/* 153 (5765) */ { 0x0001, 0x0000, 0x0168, 0x0001, 0xFFFF },
	/* 157 (5785) */ { 0x0001, 0x0000, 0x016A, 0x0001, 0xFFFF },
	/* 161 (5805) */ { 0x0001, 0x0000, 0x016C, 0x0001, 0xFFFF },
	/* 165 (5825) */ { 0x0001, 0x0000, 0x016E, 0x0001, 0xFFFF },
};

/*
 * Upload full per-channel RX filter table.
 * Writes channel-specific filter coefficients for optimal
 * adjacent channel rejection.
 */
void
b43bsd_tables_upload_rxfilt_full(struct b43bsd_softc *sc,
    int channel, int is_5ghz)
{
	const uint16_t (*tab)[5];
	int idx;

	if (is_5ghz) {
		static const int chans[] = {
			36, 40, 44, 48, 52, 56, 60, 64,
			100, 104, 108, 112, 116, 120, 124, 128,
			132, 136, 140, 149, 153, 157, 161, 165
		};
		int i;
		idx = 0;
		for (i = 0; i < 24; i++) {
			if (chans[i] == channel) { idx = i; break; }
		}
		tab = nphy_rxfilt_5ghz_full;
	} else {
		idx = channel - 1;
		tab = nphy_rxfilt_2ghz_full;
	}

	/* Write 5 filter registers. */
	nphy_write(sc, 0x049, tab[idx][0]);	/* RXF20_NUM0 */
	nphy_write(sc, 0x04A, tab[idx][1]);	/* RXF20_NUM1 */
	nphy_write(sc, 0x04B, tab[idx][2]);	/* RXF20_NUM2 */
	nphy_write(sc, 0x04C, tab[idx][3]);	/* RXF20_DENOM0 */
	nphy_write(sc, 0x04D, tab[idx][4]);	/* RXF20_DENOM1 */
}

/* ------------------------------------------------------------------ */
/* N-PHY Revision-Specific Init Tables (Rev 3-15)                        */
/* ------------------------------------------------------------------ */

/*
 * Older N-PHY revisions (3-15) have different register defaults
 * than rev 16+. BCM4331 can have rev 10-16 PHY depending on
 * silicon stepping. These tables provide revision-correct init.
 */

/* Rev 10-11 PHY init deltas (lower clip thresholds). */
static const struct b43bsd_nphy_entry nphy_init_rev10_11[] = {
	{ 0x027, 0x0048, 0 },	/* C1_CLIPWB: lower */
	{ 0x02C, 0x001C, 0 },
	{ 0x02D, 0x000E, 0 },
	{ 0x03D, 0x0048, 0 },
	{ 0x042, 0x001C, 0 },
	{ 0x043, 0x000E, 0 },
	{ 0x048, 0x0048, 0 },
	{ 0x04D, 0x001C, 0 },
	{ 0x04E, 0x000E, 0 },
	{ 0x02B, 0x0078, 0 },	/* NB_CLIP: lower */
	{ 0x041, 0x0078, 0 },
	{ 0x04C, 0x0078, 0 },
	{ 0x020, 0x7878, 0 },	/* INITGAIN: higher for older silicon */
	{ 0x036, 0x7878, 0 },
	{ 0x046, 0x7878, 0 },
};

#define NPY_REV10_11_NELEMS \
	(sizeof(nphy_init_rev10_11) / sizeof(nphy_init_rev10_11[0]))

/* Rev 12-13: intermediate values. */
static const struct b43bsd_nphy_entry nphy_init_rev12_13[] = {
	{ 0x027, 0x004C, 0 }, { 0x02C, 0x001E, 0 }, { 0x02D, 0x000F, 0 },
	{ 0x03D, 0x004C, 0 }, { 0x042, 0x001E, 0 }, { 0x043, 0x000F, 0 },
	{ 0x048, 0x004C, 0 }, { 0x04D, 0x001E, 0 }, { 0x04E, 0x000F, 0 },
	{ 0x02B, 0x007C, 0 }, { 0x041, 0x007C, 0 }, { 0x04C, 0x007C, 0 },
	{ 0x020, 0x7272, 0 }, { 0x036, 0x7272, 0 }, { 0x046, 0x7272, 0 },
};

#define NPY_REV12_13_NELEMS \
	(sizeof(nphy_init_rev12_13) / sizeof(nphy_init_rev12_13[0]))

/* Rev 14-15: close to rev 16 but with different gain steps. */
static const struct b43bsd_nphy_entry nphy_init_rev14_15[] = {
	{ 0x027, 0x004E, 0 }, { 0x02C, 0x001F, 0 }, { 0x02D, 0x000F, 0 },
	{ 0x03D, 0x004E, 0 }, { 0x042, 0x001F, 0 }, { 0x043, 0x000F, 0 },
	{ 0x048, 0x004E, 0 }, { 0x04D, 0x001F, 0 }, { 0x04E, 0x000F, 0 },
	{ 0x02B, 0x007E, 0 }, { 0x041, 0x007E, 0 }, { 0x04C, 0x007E, 0 },
	{ 0x020, 0x6E6E, 0 }, { 0x036, 0x6E6E, 0 }, { 0x046, 0x6E6E, 0 },
};

#define NPY_REV14_15_NELEMS \
	(sizeof(nphy_init_rev14_15) / sizeof(nphy_init_rev14_15[0]))

/*
 * Upload PHY revision-specific init table.
 */
void
b43bsd_tables_upload_phyrev_init(struct b43bsd_softc *sc, int phy_rev)
{
	const struct b43bsd_nphy_entry *tab;
	unsigned int n;

	switch (phy_rev) {
	case 10: case 11:
		tab = nphy_init_rev10_11;
		n = NPY_REV10_11_NELEMS;
		break;
	case 12: case 13:
		tab = nphy_init_rev12_13;
		n = NPY_REV12_13_NELEMS;
		break;
	case 14: case 15:
		tab = nphy_init_rev14_15;
		n = NPY_REV14_15_NELEMS;
		break;
	default:
		/* Rev 16+ uses the main init tables. */
		return;
	}

	nphy_write_table(sc, tab, n);
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "PHY rev %d init table applied\n", phy_rev);
}

/* ------------------------------------------------------------------ */
/* Per-MCS Per-Chain TX Power Tables (3×3 MIMO)                         */
/* ------------------------------------------------------------------ */

/*
 * TX power offsets for each MCS rate, spatial stream count,
 * chain, and bandwidth combination.
 *
 * Format: {mcs, nss, bw_40, sgi, c1_offset, c2_offset, c3_offset}
 * Offsets in dBm * 4 (quarter-dBm resolution).
 * Negative = reduce power for that chain at that rate.
 */

struct b43bsd_mcs_chain_power {
	uint8_t	mcs;
	uint8_t	nss;		/* 1-3 spatial streams */
	uint8_t	bw_40;		/* 0=20MHz, 1=40MHz */
	uint8_t	sgi;		/* 0=normal GI, 1=short GI */
	int8_t	c1_off;		/* chain 0 offset (dBm*4) */
	int8_t	c2_off;		/* chain 1 offset */
	int8_t	c3_off;		/* chain 2 offset */
};

/*
 * Default per-MCS per-chain power offsets for BCM4331 Rev 2.
 * Higher MCS rates and more streams require more power backoff
 * to maintain EVM (Error Vector Magnitude) requirements.
 */
static const struct b43bsd_mcs_chain_power mcs_chain_power_default[] = {
	/* MCS 0-7: 1 stream, 20 MHz */
	{  0, 1, 0, 0,  0,  0,  0 },
	{  1, 1, 0, 0,  0,  0,  0 },
	{  2, 1, 0, 0, -1, -2, -2 },
	{  3, 1, 0, 0, -1, -2, -2 },
	{  4, 1, 0, 0, -2, -3, -3 },
	{  5, 1, 0, 0, -2, -3, -3 },
	{  6, 1, 0, 0, -3, -4, -4 },
	{  7, 1, 0, 0, -3, -4, -4 },

	/* MCS 8-15: 2 streams, 20 MHz */
	{  8, 2, 0, 0, -2, -2, -3 },
	{  9, 2, 0, 0, -3, -3, -4 },
	{ 10, 2, 0, 0, -3, -3, -4 },
	{ 11, 2, 0, 0, -4, -4, -5 },
	{ 12, 2, 0, 0, -5, -5, -6 },
	{ 13, 2, 0, 0, -5, -5, -6 },
	{ 14, 2, 0, 0, -6, -6, -7 },
	{ 15, 2, 0, 0, -6, -6, -7 },

	/* MCS 16-23: 3 streams, 20 MHz */
	{ 16, 3, 0, 0, -3, -3, -3 },
	{ 17, 3, 0, 0, -4, -4, -4 },
	{ 18, 3, 0, 0, -5, -5, -5 },
	{ 19, 3, 0, 0, -5, -5, -5 },
	{ 20, 3, 0, 0, -6, -6, -6 },
	{ 21, 3, 0, 0, -6, -6, -6 },
	{ 22, 3, 0, 0, -7, -7, -7 },
	{ 23, 3, 0, 0, -7, -7, -7 },
};

#define MCS_CHAIN_POWER_N \
	(sizeof(mcs_chain_power_default) / sizeof(mcs_chain_power_default[0]))

/*
 * Upload per-MCS per-chain TX power offsets.
 */
void
b43bsd_tables_upload_mcs_chain_power(struct b43bsd_softc *sc)
{
	unsigned int i;

	for (i = 0; i < MCS_CHAIN_POWER_N; i++) {
		const struct b43bsd_mcs_chain_power *e =
		    &mcs_chain_power_default[i];
		/*
		 * Write chain offsets through PHY table at offset 0x100.
		 * Each MCS gets 4 entries: c1_off, c2_off, c3_off, flags.
		 */
		uint16_t base = (uint16_t)(0x100 + e->mcs * 4);

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, base);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    (uint16_t)(e->c1_off & 0xff));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, base + 1);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    (uint16_t)(e->c2_off & 0xff));

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, base + 2);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    (uint16_t)(e->c3_off & 0xff));

		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, base + 3);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
		    (uint16_t)((e->nss & 0x3) | ((e->bw_40 & 1) << 2) |
		    ((e->sgi & 1) << 3)));
	}
}

/* ------------------------------------------------------------------ */
/* Channel Estimation Default Tables                                      */
/* ------------------------------------------------------------------ */

/*
 * Default channel estimation matrix values for 3×3 MIMO.
 * Used when the channel estimation engine hasn't converged yet.
 * These are identity-like matrices with realistic cross-talk.
 */
static const uint16_t chan_est_default_20mhz[] = {
	/* H[0][0] */ 0x1000, 0x0000,	/* I=4096, Q=0 (self, strong) */
	/* H[0][1] */ 0x0050, 0x0020,	/* I=80, Q=32 (cross-talk, weak) */
	/* H[0][2] */ 0x0030, 0x0010,	/* I=48, Q=16 */
	/* H[1][0] */ 0x0040, 0x0018,	/* I=64, Q=24 */
	/* H[1][1] */ 0x1000, 0x0000,	/* self, strong */
	/* H[1][2] */ 0x0040, 0x0018,
	/* H[2][0] */ 0x0030, 0x0010,
	/* H[2][1] */ 0x0050, 0x0020,
	/* H[2][2] */ 0x1000, 0x0000,	/* self, strong */
};

static const uint16_t chan_est_default_40mhz[] = {
	/* 40 MHz has more cross-talk between streams. */
	0x1000, 0x0000,	0x0080, 0x0030,	0x0050, 0x0020,
	0x0070, 0x0028,	0x1000, 0x0000,	0x0060, 0x0024,
	0x0050, 0x0020,	0x0080, 0x0030,	0x1000, 0x0000,
};

/*
 * Upload default channel estimation matrix.
 */
void
b43bsd_tables_upload_chan_est(struct b43bsd_softc *sc, int is_40mhz)
{
	const uint16_t *tab;
	int i, n;

	if (is_40mhz) {
		tab = chan_est_default_40mhz;
		n = sizeof(chan_est_default_40mhz) /
		    sizeof(chan_est_default_40mhz[0]);
	} else {
		tab = chan_est_default_20mhz;
		n = sizeof(chan_est_default_20mhz) /
		    sizeof(chan_est_default_20mhz[0]);
	}

	/* Write to channel estimation table (offset 0x30). */
	for (i = 0; i < n; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
		    (uint16_t)(0x30 + i));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, tab[i]);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0x0000);
	}
}

/* ------------------------------------------------------------------ */
/* STBC (Space-Time Block Coding) Configuration                          */
/* ------------------------------------------------------------------ */

/*
 * STBC transmits multiple copies of data across antennas to
 * improve reliability at the cost of throughput.
 */
static const uint16_t stbc_cfg[3][4] = {
	/* 1 stream: { enable, mode, pilot_streams, expansion_factor } */
	{ 0x0001, 0x0000, 0x0000, 0x0000 },
	/* 2 streams */
	{ 0x0001, 0x0001, 0x0001, 0x0001 },
	/* 3 streams */
	{ 0x0001, 0x0003, 0x0003, 0x0002 },
};

void
b43bsd_tables_upload_stbc(struct b43bsd_softc *sc, int nstreams)
{
	int idx;

	if (nstreams < 1) nstreams = 1;
	if (nstreams > 3) nstreams = 3;
	idx = nstreams - 1;

	nphy_write(sc, 0x01A0, stbc_cfg[idx][0]);	/* STBC_ENABLE */
	nphy_write(sc, 0x01A1, stbc_cfg[idx][1]);	/* STBC_MODE */
	nphy_write(sc, 0x01A2, stbc_cfg[idx][2]);	/* STBC_PILOT */
	nphy_write(sc, 0x01A3, stbc_cfg[idx][3]);	/* STBC_EXPANSION */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "STBC configured for %d streams\n", nstreams);
}

/* ------------------------------------------------------------------ */
/* LDPC (Low-Density Parity Check) Configuration                         */
/* ------------------------------------------------------------------ */

/*
 * LDPC is an optional 802.11n forward error correction code
 * that improves sensitivity by ~2 dB vs BCC at high MCS rates.
 */
void
b43bsd_tables_upload_ldpc(struct b43bsd_softc *sc)
{
	/*
	 * LDPC configuration at offset 0x01B0.
	 * Enable for all MCS rates where available (MCS 4-7, 12-15, 20-23).
	 */
	nphy_write(sc, 0x01B0, 0x0001);	/* LDPC_ENABLE */
	nphy_write(sc, 0x01B1, 0x00F0);	/* LDPC_MCS_MASK: MCS 4-7 */
	nphy_write(sc, 0x01B2, 0xF000);	/* LDPC_MCS_MASK2: MCS 12-15 */
	nphy_write(sc, 0x01B3, 0x00F0);	/* LDPC_MCS_MASK3: MCS 20-23 */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "LDPC enabled for high MCS rates\n");
}

/* ------------------------------------------------------------------ */
/* Temperature Compensation Curves                                       */
/* ------------------------------------------------------------------ */

/*
 * TX power temperature compensation curves.
 * Maps temperature (°C) to TX power adjustment (dBm * 4).
 * PA gain decreases at higher temperatures — compensate by
 * increasing drive level.
 */
static const int8_t temp_comp_2ghz[] = {
	/* -20C to +80C, every 5C */
	 -4, -4, -3, -2, -2,	/* -20..0C: reduce power (cold PA is hot) */
	 -1, -1,  0,  0,  0,	/* 0..20C: nominal */
	  0,  0, +1, +1, +2,	/* 20..40C: slight compensation */
	 +2, +3, +3, +4, +4,	/* 40..60C: moderate */
	 +4, +5, +5, +6, +6,	/* 60..80C: more compensation */
};

static const int8_t temp_comp_5ghz[] = {
	 -3, -3, -2, -2, -1,
	 -1,  0,  0,  0,  0,
	  0, +1, +1, +2, +2,
	 +3, +3, +4, +4, +5,
	 +5, +6, +6, +7, +7,
};

#define TEMP_COMP_N	\
	(sizeof(temp_comp_2ghz) / sizeof(temp_comp_2ghz[0]))

/*
 * Apply temperature compensation from lookup table.
 */
int
b43bsd_tables_temp_compensate(struct b43bsd_softc *sc, int temp_c,
    int base_dbm, int is_5ghz)
{
	const int8_t *tab;
	int idx, adj;

	tab = is_5ghz ? temp_comp_5ghz : temp_comp_2ghz;

	/* Map temperature to table index (-20C = idx 0, every 5C). */
	idx = (temp_c + 20) / 5;
	if (idx < 0) idx = 0;
	if (idx >= (int)TEMP_COMP_N) idx = (int)TEMP_COMP_N - 1;

	adj = tab[idx];	/* in dBm * 4 */

	return base_dbm + adj / 4;
}

/* ------------------------------------------------------------------ */
/* Radio Calibration Bitmask Table                                       */
/* ------------------------------------------------------------------ */

/*
 * Radio calibration status bitmask:
 * Each bit indicates whether a specific calibration was run.
 * Firmware and driver use this to track which calibrations are
 * valid and need to be re-run after channel change or reset.
 */
enum {
	B43BSD_CAL_NONE		= 0,
	B43BSD_CAL_IQ_TX	= 0x0001,
	B43BSD_CAL_IQ_RX	= 0x0002,
	B43BSD_CAL_PAPD		= 0x0004,
	B43BSD_CAL_NOISE	= 0x0008,
	B43BSD_CAL_ED		= 0x0010,
	B43BSD_CAL_CCA		= 0x0020,
	B43BSD_CAL_MIMO		= 0x0040,
	B43BSD_CAL_CLASSIFIER	= 0x0080,
	B43BSD_CAL_DC_OFFSET	= 0x0100,
	B43BSD_CAL_XTAL		= 0x0200,
	B43BSD_CAL_LPF		= 0x0400,
	B43BSD_CAL_SAMPLE	= 0x0800,
	B43BSD_CAL_RX_SENS	= 0x1000,
};

/*
 * Write calibration bitmask to PHY shared register.
 * Firmware reads this to determine which calibrations are valid.
 */
void
b43bsd_tables_set_cal_mask(struct b43bsd_softc *sc, uint32_t mask)
{
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x01F0);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
	    (uint16_t)(mask & 0xffff));
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI,
	    (uint16_t)((mask >> 16) & 0xffff));
}

/*
 * Read calibration bitmask from PHY shared register.
 */
uint32_t
b43bsd_tables_get_cal_mask(struct b43bsd_softc *sc)
{
	uint16_t lo, hi;

	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x01F0);
	lo = nphy_read(sc, B43BSD_NPHY_TABLE_DATALO);
	hi = nphy_read(sc, B43BSD_NPHY_TABLE_DATAHI);

	return (uint32_t)lo | ((uint32_t)hi << 16);
}

/* ------------------------------------------------------------------ */
/* Additional N-PHY Register Init (Extended)                             */
/* ------------------------------------------------------------------ */

/*
 * Extended N-PHY init table with additional registers not covered
 * by the band-specific tables. These are one-time init values.
 */
static const struct b43bsd_nphy_entry nphy_extended_init[] = {
	/* ---- MAC-PHY interface timing ---- */
	{ 0x0100, 0x0002, 0 },	/* MACPHY_DELAY: 2 clock cycles */
	{ 0x0101, 0x0001, 0 },	/* MACPHY_CTL: enable */
	{ 0x0102, 0x0000, 0 },	/* MACPHY_SPARE */

	/* ---- RX AGC timing ---- */
	{ 0x0104, 0x0004, 0 },	/* AGC_SETTLING: 4 µs */
	{ 0x0105, 0x0008, 0 },	/* AGC_HOLD: 8 µs */
	{ 0x0106, 0x0002, 0 },	/* AGC_DECAY: 2 dB/µs */

	/* ---- TX ramp timing ---- */
	{ 0x0108, 0x0002, 0 },	/* TX_RAMP_UP: 2 µs */
	{ 0x0109, 0x0001, 0 },	/* TX_RAMP_DOWN: 1 µs */
	{ 0x010A, 0x0001, 0 },	/* TX_PA_RAMP: 1 µs */

	/* ---- RX to TX turnaround ---- */
	{ 0x010C, 0x0002, 0 },	/* RXTX_TURNAROUND: 2 µs */
	{ 0x010D, 0x0002, 0 },	/* TXRX_TURNAROUND: 2 µs */

	/* ---- Packet detection thresholds ---- */
	{ 0x0112, 0x000A, 0 },	/* PKT_DET_THRESH: -85 dBm */
	{ 0x0113, 0x0014, 0 },	/* PKT_DET_HYST: 2 dB */

	/* ---- OFDM timing ---- */
	{ 0x0115, 0x0010, 0 },	/* OFDM_SYMBOL: 4 µs (800ns GI) */
	{ 0x0116, 0x0008, 0 },	/* OFDM_PREAMBLE: 16 µs */

	/* ---- CCK timing ---- */
	{ 0x0118, 0x0028, 0 },	/* CCK_SYMBOL: 1 µs */
	{ 0x0119, 0x0090, 0 },	/* CCK_PREAMBLE: 144 µs */

	/* ---- HT (802.11n) timing ---- */
	{ 0x011B, 0x0008, 0 },	/* HT_STF: 8 µs */
	{ 0x011C, 0x0008, 0 },	/* HT_LTF: 8 µs per stream */
	{ 0x011D, 0x0004, 0 },	/* HT_SIG: 4 µs */

	/* ---- Interference mitigation ---- */
	{ 0x0120, 0x0000, 0 },	/* NOTCH_CTL: disabled */
	{ 0x0121, 0x0000, 0 },	/* NOTCH_FREQ: 0 */
	{ 0x0122, 0x0000, 0 },	/* NOTCH_BW: 0 */
	{ 0x0124, 0x0000, 0 },	/* SPUR_CANCEL: disabled */
	{ 0x0125, 0x0000, 0 },	/* SPUR_FREQ */

	/* ---- Diversity combining ---- */
	{ 0x0130, 0x0007, 0 },	/* DIV_COMBINE: MRC all chains */
	{ 0x0131, 0x0000, 0 },	/* DIV_WEIGHT0: equal */
	{ 0x0132, 0x0000, 0 },	/* DIV_WEIGHT1: equal */
	{ 0x0133, 0x0000, 0 },	/* DIV_WEIGHT2: equal */

	/* ---- RX filter bypass ---- */
	{ 0x0135, 0x0000, 0 },	/* RXFILT_BYPASS: off */
};

#define NPY_EXTENDED_INIT_NELEMS \
	(sizeof(nphy_extended_init) / sizeof(nphy_extended_init[0]))

/*
 * Upload extended N-PHY init table.
 */
void
b43bsd_tables_upload_extended(struct b43bsd_softc *sc)
{
	nphy_write_table(sc, nphy_extended_init, NPY_EXTENDED_INIT_NELEMS);
}

/* ------------------------------------------------------------------ */
/* Antenna Isolation Tables                                              */
/* ------------------------------------------------------------------ */

/*
 * Antenna isolation values for 3×3 MIMO.
 * Isolation is the attenuation between TX and RX chains.
 * Higher isolation = less self-interference = better MIMO performance.
 *
 * Values in dB * 4 (quarter-dB resolution).
 */
static const uint16_t ant_isolation_2ghz[3][3] = {
	/* TX chain 0 -> RX chain {0,1,2} */
	{ 0xFFFF, 0x0060, 0x0050 },	/* self=infinite, cross=24dB, 20dB */
	/* TX chain 1 */
	{ 0x0060, 0xFFFF, 0x0060 },
	/* TX chain 2 */
	{ 0x0050, 0x0060, 0xFFFF },
};

static const uint16_t ant_isolation_5ghz[3][3] = {
	/* 5 GHz has less isolation (smaller wavelength = more coupling). */
	{ 0xFFFF, 0x0050, 0x0040 },
	{ 0x0050, 0xFFFF, 0x0050 },
	{ 0x0040, 0x0050, 0xFFFF },
};

/*
 * Upload antenna isolation table to PHY.
 */
void
b43bsd_tables_upload_ant_isolation(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t (*tab)[3];
	int tx, rx;

	tab = is_5ghz ? ant_isolation_5ghz : ant_isolation_2ghz;

	/* Write to PHY table at offset 0x0170. */
	for (tx = 0; tx < 3; tx++) {
		for (rx = 0; rx < 3; rx++) {
			uint16_t addr = (uint16_t)(0x0170 + tx * 3 + rx);

			nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, addr);
			nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
			    tab[tx][rx]);
			nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "antenna isolation table uploaded (%s)\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz");
}

/* ------------------------------------------------------------------ */
/* Filter Calibration Table                                              */
/* ------------------------------------------------------------------ */

/*
 * Digital filter calibration parameters.
 * These fine-tune the RX digital filter response for optimal
 * EVM and adjacent channel rejection.
 */
static const uint16_t filter_cal_2ghz[] = {
	/* EQ coefficients (12 taps). */
	0x0000, 0x0000, 0x0000, 0x0000,
	0x1000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000, 0x0000, 0x0000,
	/* Phase correction. */
	0x0000, 0x0000,
};

static const uint16_t filter_cal_5ghz[] = {
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0E00, 0x0000, 0x0000, 0x0000,	/* slightly lower center tap */
	0x0000, 0x0000, 0x0000, 0x0000,
	0x0000, 0x0000,
};

#define FILTER_CAL_N	14

void
b43bsd_tables_upload_filter_cal(struct b43bsd_softc *sc, int is_5ghz)
{
	const uint16_t *tab;
	int i;

	tab = is_5ghz ? filter_cal_5ghz : filter_cal_2ghz;

	/* Write to PHY table at offset 0x0180. */
	for (i = 0; i < FILTER_CAL_N; i++) {
		nphy_write(sc, B43BSD_NPHY_TABLE_ADDR,
		    (uint16_t)(0x0180 + i));
		nphy_write(sc, B43BSD_NPHY_TABLE_DATALO, tab[i]);
		nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI, 0);
	}
}

/* ------------------------------------------------------------------ */
/* Radio GPIO Init Table (Board-Specific)                                 */
/* ------------------------------------------------------------------ */

/*
 * Board-specific GPIO initialization.
 * MacBook Pro 9,2 BCM4331 GPIO pin assignments and default states.
 */
static const struct {
	uint16_t	pin;
	uint16_t	direction;	/* 0=in, 1=out */
	uint16_t	state;		/* initial value */
	const char	*name;
} gpio_init_table[] = {
	{ 0,  0, 0, "RF_KILL (input)" },
	{ 1,  1, 1, "PA_ENABLE (output, on)" },
	{ 2,  1, 0, "LED (output, off)" },
	{ 3,  1, 1, "LNA_ENABLE (output, on)" },
	{ 4,  1, 0, "ANT_SW0 (output)" },
	{ 5,  1, 0, "ANT_SW1 (output)" },
	{ 6,  1, 0, "WL_ACTIVE (output, idle low)" },
	{ 7,  0, 0, "BT_ACTIVE (input)" },
	{ 8,  0, 0, "BT_PRIORITY (input)" },
};

#define GPIO_INIT_N \
	(sizeof(gpio_init_table) / sizeof(gpio_init_table[0]))

/*
 * Initialize GPIO pins for the board.
 */
void
b43bsd_tables_upload_gpio_init(struct b43bsd_softc *sc)
{
	uint32_t outen = 0, out = 0;
	unsigned int i;

	if (sc->sc_ssb == NULL)
		return;

	for (i = 0; i < GPIO_INIT_N; i++) {
		if (gpio_init_table[i].direction) {
			outen |= (1 << gpio_init_table[i].pin);
			if (gpio_init_table[i].state)
				out |= (1 << gpio_init_table[i].pin);
		}
	}

	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE, outen);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, out);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "GPIO initialized: outen=0x%08x out=0x%08x\n", outen, out);
}

/* ------------------------------------------------------------------ */
/* Per-Channel PHY Calibration Overrides                                 */
/* ------------------------------------------------------------------ */

/*
 * Channel-specific PHY calibration override values.
 * Each channel may need slightly different gain, ED threshold,
 * or clip settings due to band-edge effects.
 */
struct chan_cal_override {
	int		channel;
	int		is_5ghz;
	uint16_t	ed_thresh_delta;	/* +delta from default */
	uint16_t	clip_delta;		/* +delta from default */
	int8_t		gain_delta;		/* signed gain adjustment */
};

static const struct chan_cal_override chan_cal_overrides[] = {
	/* 2.4 GHz band edges need higher ED thresholds. */
	{ 1, 0, 4, 2, 0 },	/* Ch 1: band edge, more false positives */
	{ 2, 0, 2, 1, 0 },
	{ 3, 0, 0, 0, 0 },
	{ 4, 0, 0, 0, 0 },
	{ 5, 0, 0, 0, 0 },
	{ 6, 0, 0, 0, 0 },	/* Ch 6: mid-band, optimal */
	{ 7, 0, 0, 0, 0 },
	{ 8, 0, 0, 0, 0 },
	{ 9, 0, 0, 0, 0 },
	{ 10, 0, 0, 0, 0 },
	{ 11, 0, 0, 0, 0 },
	{ 12, 0, 0, 0, 0 },
	{ 13, 0, 2, 1, 0 },	/* Ch 13: upper band edge */
	{ 14, 0, 4, 2, -1 },	/* Ch 14: Japan edge, reduce gain */

	/* 5 GHz band edges. */
	{ 36, 1, 4, 2, 0 },	/* UNII-1 lower */
	{ 40, 1, 2, 1, 0 },
	{ 48, 1, 2, 1, 0 },	/* UNII-1 upper */
	{ 52, 1, 4, 2, 0 },	/* UNII-2 lower */
	{ 64, 1, 2, 1, 0 },	/* UNII-2 upper */
	{ 100, 1, 4, 2, 0 },	/* UNII-2e lower */
	{ 140, 1, 4, 2, 0 },	/* UNII-2e upper */
	{ 149, 1, 4, 2, 0 },	/* UNII-3 lower */
	{ 165, 1, 4, 2, 0 },	/* UNII-3 upper */
};

#define CHAN_CAL_OVERRIDE_N \
	(sizeof(chan_cal_overrides) / sizeof(chan_cal_overrides[0]))

/*
 * Apply channel-specific calibration overrides.
 */
void
b43bsd_tables_apply_chan_overrides(struct b43bsd_softc *sc,
    int channel, int is_5ghz)
{
	unsigned int i;

	for (i = 0; i < CHAN_CAL_OVERRIDE_N; i++) {
		const struct chan_cal_override *ov = &chan_cal_overrides[i];

		if (ov->channel != channel || ov->is_5ghz != is_5ghz)
			continue;

		if (ov->ed_thresh_delta) {
			uint16_t ed = nphy_read(sc,
			    B43BSD_NPHY_C1_EDTHRES);
			nphy_write(sc, B43BSD_NPHY_C1_EDTHRES,
			    ed + ov->ed_thresh_delta);
			nphy_write(sc, B43BSD_NPHY_C2_EDTHRES,
			    ed + ov->ed_thresh_delta);
		}
		if (ov->clip_delta) {
			uint16_t clip = nphy_read(sc,
			    B43BSD_NPHY_C1_CLIPWBTHRES);
			nphy_write(sc, B43BSD_NPHY_C1_CLIPWBTHRES,
			    clip + ov->clip_delta);
			nphy_write(sc, B43BSD_NPHY_C2_CLIPWBTHRES,
			    clip + ov->clip_delta);
		}
		if (ov->gain_delta) {
			uint16_t gain = nphy_read(sc,
			    B43BSD_NPHY_C1_INITGAIN);
			if (ov->gain_delta > 0)
				gain += (uint16_t)ov->gain_delta * 0x0101;
			else
				gain -= (uint16_t)(-ov->gain_delta) * 0x0101;
			nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, gain);
			nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, gain);
		}
		return;
	}
}

/* ------------------------------------------------------------------ */
/* BCM2056 Radio Revision Full Tables (Rev 5, 6, 7, 8, 9)               */
/* ------------------------------------------------------------------ */

/*
 * Complete BCM2056 synthesizer register map per revision.
 * These are the full register dumps for each radio revision.
 */
static const uint16_t b2056_syn_rev5[] = {
	/* addr, value */
	0x08, 0x0000,	/* COM_CTRL */
	0x09, 0x0001,	/* COM_PU */
	0x0A, 0x0000,	/* COM_OVR */
	0x0B, 0x0000,	/* COM_RESET */
	0x0C, 0x0000,	/* RESERVED */
	0x22, 0x0060,	/* TOPBIAS_MASTER */
	0x23, 0x0006,	/* TOPBIAS_RCAL */
	0x24, 0x000C,	/* AFEREG */
	0x28, 0x0001,	/* LPO */
	0x30, 0x000D,	/* RCCAL_CTRL0 */
	0x31, 0x001F,	/* RCCAL_CTRL1 */
	0x32, 0x0015,	/* RCCAL_CTRL2 */
	0x33, 0x000F,	/* RCCAL_CTRL3 */
	0x3C, 0x008C,	/* PLL_MAST1 */
	0x3D, 0x008C,	/* PLL_MAST2 */
	0x3E, 0x008C,	/* PLL_MAST3 */
	0x47, 0x0006,	/* PLL_PFD */
	0x48, 0x0080,	/* PLL_CP1 */
	0x49, 0x0020,	/* PLL_CP2 */
	0x4A, 0x0020,	/* PLL_CP3 */
	0x52, 0x00E0,	/* PLL_VCO1 */
	0x56, 0x0001,	/* PLL_VCOCAL1 */
	0x57, 0x0001,	/* PLL_VCOCAL2 */
	0x58, 0x0001,	/* PLL_VCOCAL4 */
	0x59, 0x0002,	/* PLL_VCOCAL5 */
	0x5A, 0x0003,	/* PLL_VCOCAL6 */
	0x5B, 0x0003,	/* PLL_VCOCAL7 */
	0x60, 0x0007,	/* PLL_VCOCAL12 */
	0xC0, 0x0009,	/* LOGEN_ACL */
	0xC1, 0x000A,	/* LOGEN_ACL_WAITCNT */
};

static const uint16_t b2056_syn_rev6[] = {
	0x08, 0x0000, 0x09, 0x0001, 0x0A, 0x0000, 0x0B, 0x0000, 0x0C, 0x0000,
	0x22, 0x0060, 0x23, 0x0006, 0x24, 0x000C, 0x28, 0x0001,
	0x30, 0x000D, 0x31, 0x001F, 0x32, 0x0015, 0x33, 0x000F,
	0x3C, 0x008C, 0x3D, 0x008C, 0x3E, 0x008C,
	0x47, 0x0006, 0x48, 0x0080, 0x49, 0x0020, 0x4A, 0x0020,
	0x52, 0x00E0,
	/* Rev 6: improved VCO calibration parameters. */
	0x56, 0x0002, 0x57, 0x0002, 0x58, 0x0001, 0x59, 0x0003,
	0x5A, 0x0004, 0x5B, 0x0004, 0x60, 0x0008,
	0xC0, 0x0009, 0xC1, 0x000A,
};

static const uint16_t b2056_syn_rev7[] = {
	0x08, 0x0000, 0x09, 0x0001, 0x0A, 0x0000, 0x0B, 0x0000, 0x0C, 0x0000,
	0x22, 0x0060, 0x23, 0x0006, 0x24, 0x000C, 0x28, 0x0001,
	0x30, 0x000D, 0x31, 0x001F, 0x32, 0x0015, 0x33, 0x000F,
	0x3C, 0x008C, 0x3D, 0x008C, 0x3E, 0x008C,
	0x47, 0x0006, 0x48, 0x0080, 0x49, 0x0020, 0x4A, 0x0020,
	0x52, 0x00E0,
	/* Rev 7: production VCO cal. */
	0x56, 0x0001, 0x57, 0x0001, 0x58, 0x0001, 0x59, 0x0002,
	0x5A, 0x0003, 0x5B, 0x0003, 0x60, 0x0007,
	0xC0, 0x0009, 0xC1, 0x000A,
};

static const uint16_t b2056_syn_rev8[] = {
	0x08, 0x0000, 0x09, 0x0001, 0x0A, 0x0000, 0x0B, 0x0000, 0x0C, 0x0000,
	0x22, 0x0060, 0x23, 0x0006, 0x24, 0x000C, 0x28, 0x0001,
	0x30, 0x000D, 0x31, 0x001F, 0x32, 0x0015, 0x33, 0x000F,
	0x3C, 0x008C, 0x3D, 0x008C, 0x3E, 0x008C,
	0x47, 0x0006, 0x48, 0x0080, 0x49, 0x0020, 0x4A, 0x0020,
	0x52, 0x00E0,
	/* Rev 8: final tuning. */
	0x56, 0x0002, 0x57, 0x0002, 0x58, 0x0002, 0x59, 0x0003,
	0x5A, 0x0004, 0x5B, 0x0004, 0x60, 0x0008,
	0xC0, 0x0009, 0xC1, 0x000A,
};

static const uint16_t b2056_syn_rev9[] = {
	0x08, 0x0000, 0x09, 0x0001, 0x0A, 0x0000, 0x0B, 0x0000, 0x0C, 0x0000,
	0x22, 0x0060, 0x23, 0x0006, 0x24, 0x000C, 0x28, 0x0001,
	0x30, 0x000D, 0x31, 0x001F, 0x32, 0x0015, 0x33, 0x000F,
	0x3C, 0x008C, 0x3D, 0x008C, 0x3E, 0x008C,
	0x47, 0x0006, 0x48, 0x0080, 0x49, 0x0020, 0x4A, 0x0020,
	0x52, 0x00E0,
	/* Rev 9: MacBook Pro 9,2 production revision. */
	0x56, 0x0001, 0x57, 0x0001, 0x58, 0x0001, 0x59, 0x0003,
	0x5A, 0x0005, 0x5B, 0x0005, 0x60, 0x0007,
	0xC0, 0x0009, 0xC1, 0x000A,
};

/*
 * Upload radio revision-specific synthesizer table.
 */
void
b43bsd_tables_upload_syn_by_rev(struct b43bsd_softc *sc, int radio_rev)
{
	const uint16_t *tab;
	int n, i;

	switch (radio_rev) {
	case 5: tab = b2056_syn_rev5;
		n = sizeof(b2056_syn_rev5)/sizeof(b2056_syn_rev5[0])/2;
		break;
	case 6: tab = b2056_syn_rev6;
		n = sizeof(b2056_syn_rev6)/sizeof(b2056_syn_rev6[0])/2;
		break;
	case 7: tab = b2056_syn_rev7;
		n = sizeof(b2056_syn_rev7)/sizeof(b2056_syn_rev7[0])/2;
		break;
	case 8: tab = b2056_syn_rev8;
		n = sizeof(b2056_syn_rev8)/sizeof(b2056_syn_rev8[0])/2;
		break;
	case 9: default:
		tab = b2056_syn_rev9;
		n = sizeof(b2056_syn_rev9)/sizeof(b2056_syn_rev9[0])/2;
		break;
	}

	for (i = 0; i < n; i++)
		b43bsd_radio_reg_write(sc, 0x0000 | tab[i*2], tab[i*2+1]);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "radio SYN rev %d table uploaded (%d entries)\n",
	    radio_rev, n);
}

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — 2.4 GHz, 40 MHz, SGI On                            */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_2ghz_40mhz_sgi[] = {
	{ 0x001, 0x0048, 0x0048 },	/* BBCFG: 40MHz + SGI */
	{ 0x005, 0x0003, 0 }, { 0x009, 0x0000, 0 }, { 0x047, 0x0007, 0 },
	{ 0x048, 0x0000, 0 }, { 0x049, 0x0001, 0 }, { 0x04A, 0x0000, 0 },
	{ 0x04B, 0x00CD, 0 }, { 0x04C, 0x0001, 0 }, { 0x04D, 0xFFFF, 0 },
	{ 0x029, 0x0052, 0 }, { 0x03F, 0x0052, 0 }, { 0x04A, 0x0052, 0 },
	{ 0x027, 0x0058, 0 }, { 0x02C, 0x0026, 0 }, { 0x02D, 0x0014, 0 },
	{ 0x03D, 0x0058, 0 }, { 0x042, 0x0026, 0 }, { 0x043, 0x0014, 0 },
	{ 0x048, 0x0058, 0 }, { 0x04D, 0x0026, 0 }, { 0x04E, 0x0014, 0 },
	{ 0x02B, 0x008A, 0 }, { 0x041, 0x008A, 0 }, { 0x04C, 0x008A, 0 },
	{ 0x020, 0x5252, 0 }, { 0x036, 0x5252, 0 }, { 0x046, 0x5252, 0 },
	{ 0x018, 0x0050, 0 }, { 0x01A, 0x0058, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x003D, 0 }, { 0x021, 0x003D, 0 }, { 0x022, 0x003D, 0 },
	{ 0x023, 0x001D, 0 }, { 0x024, 0x0025, 0 }, { 0x025, 0x0000, 0 },
	{ 0x02E, 0x0050, 0 }, { 0x030, 0x0058, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x003D, 0 }, { 0x037, 0x003D, 0 }, { 0x038, 0x003D, 0 },
	{ 0x039, 0x001D, 0 }, { 0x03A, 0x0025, 0 }, { 0x03B, 0x0000, 0 },
	{ 0x04F, 0x0000, 0 }, { 0x050, 0x003D, 0 }, { 0x051, 0x003D, 0 },
	{ 0x052, 0x001D, 0 }, { 0x053, 0x0025, 0 }, { 0x054, 0x0000, 0 },
	{ 0x055, 0x0050, 0 }, { 0x056, 0x0058, 0 }, { 0x057, 0x003D, 0 },
	{ 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 }, { 0x09A, 0x0000, 0 },
	{ 0x09B, 0x0000, 0 }, { 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },
	{ 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0036, 0 }, { 0x22E, 0x0036, 0 }, { 0x22F, 0x0036, 0 },
	{ 0x230, 0x0000, 0 }, { 0x231, 0x0000, 0 }, { 0x232, 0x0000, 0 },
	{ 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },
	{ 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },
	{ 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },
	{ 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 }, { 0x248, 0x0002, 0 },
	{ 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },
	{ 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 }, { 0x0A3, 0x0000, 0 },
	{ 0x0A4, 0x0000, 0 },
	{ 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },
	{ 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },
	{ 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },
	{ 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },
	{ 0x11C, 0x0040, 0 }, { 0x11D, 0x0038, 0 }, { 0x11E, 0x0038, 0 },
	{ 0x11F, 0x008C, 0 }, { 0x120, 0x0020, 0 },
	{ 0x1CE, 0x0083, 0 }, { 0x1CF, 0x0022, 0 }, { 0x1D0, 0x0022, 0 },
	{ 0x1D1, 0x0001, 0 }, { 0x1D2, 0x0001, 0 }, { 0x1D3, 0x0001, 0 },
	{ 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 }, { 0x1E9, 0x0000, 0 },
	{ 0x1EA, 0x0036, 0 }, { 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0036, 0 },
	{ 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },
};

#define NPY_INIT_2GHZ_40MHZ_SGI_NELEMS \
	(sizeof(nphy_init_2ghz_40mhz_sgi) / sizeof(nphy_init_2ghz_40mhz_sgi[0]))

/* N-PHY Init Table — 5 GHz, 40 MHz, SGI On */
static const struct b43bsd_nphy_entry nphy_init_5ghz_40mhz_sgi[] = {
	{ 0x001, 0x0048, 0x0048 },
	{ 0x005, 0x0026, 0 }, { 0x009, 0x0001, 0 }, { 0x047, 0x0007, 0 },
	{ 0x048, 0x0000, 0 }, { 0x049, 0x0001, 0 }, { 0x04A, 0x0000, 0 },
	{ 0x04B, 0x00A0, 0 }, { 0x04C, 0x0001, 0 }, { 0x04D, 0xFFFF, 0 },
	{ 0x029, 0x0056, 0 }, { 0x03F, 0x0056, 0 }, { 0x04A, 0x0056, 0 },
	{ 0x027, 0x0060, 0 }, { 0x02C, 0x002A, 0 }, { 0x02D, 0x0016, 0 },
	{ 0x03D, 0x0060, 0 }, { 0x042, 0x002A, 0 }, { 0x043, 0x0016, 0 },
	{ 0x048, 0x0060, 0 }, { 0x04D, 0x002A, 0 }, { 0x04E, 0x0016, 0 },
	{ 0x02B, 0x009A, 0 }, { 0x041, 0x009A, 0 }, { 0x04C, 0x009A, 0 },
	{ 0x020, 0x4646, 0 }, { 0x036, 0x4646, 0 }, { 0x046, 0x4646, 0 },
	{ 0x018, 0x005A, 0 }, { 0x01A, 0x0060, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x0045, 0 }, { 0x021, 0x0045, 0 }, { 0x022, 0x0045, 0 },
	{ 0x023, 0x0025, 0 }, { 0x024, 0x002D, 0 }, { 0x025, 0x0000, 0 },
	{ 0x02E, 0x005A, 0 }, { 0x030, 0x0060, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x0045, 0 }, { 0x037, 0x0045, 0 }, { 0x038, 0x0045, 0 },
	{ 0x039, 0x0025, 0 }, { 0x03A, 0x002D, 0 }, { 0x03B, 0x0000, 0 },
	{ 0x04F, 0x0000, 0 }, { 0x050, 0x0045, 0 }, { 0x051, 0x0045, 0 },
	{ 0x052, 0x0025, 0 }, { 0x053, 0x002D, 0 }, { 0x054, 0x0000, 0 },
	{ 0x055, 0x005A, 0 }, { 0x056, 0x0060, 0 }, { 0x057, 0x0045, 0 },
	{ 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 }, { 0x09A, 0x0000, 0 },
	{ 0x09B, 0x0000, 0 }, { 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },
	{ 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0032, 0 }, { 0x22E, 0x0032, 0 }, { 0x22F, 0x0032, 0 },
	{ 0x230, 0x0000, 0 }, { 0x231, 0x0000, 0 }, { 0x232, 0x0000, 0 },
	{ 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },
	{ 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },
	{ 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },
	{ 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 }, { 0x248, 0x0002, 0 },
	{ 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },
	{ 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 }, { 0x0A3, 0x0000, 0 },
	{ 0x0A4, 0x0000, 0 },
	{ 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },
	{ 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },
	{ 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },
	{ 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },
	{ 0x11C, 0x0040, 0 }, { 0x11D, 0x008C, 0 }, { 0x11E, 0x008C, 0 },
	{ 0x11F, 0x008C, 0 }, { 0x120, 0x0020, 0 },
	{ 0x1CE, 0x0083, 0 }, { 0x1CF, 0x0022, 0 }, { 0x1D0, 0x0022, 0 },
	{ 0x1D1, 0x0001, 0 }, { 0x1D2, 0x0001, 0 }, { 0x1D3, 0x0001, 0 },
	{ 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 }, { 0x1E9, 0x0000, 0 },
	{ 0x1EA, 0x0032, 0 }, { 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0032, 0 },
	{ 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },
};

#define NPY_INIT_5GHZ_40MHZ_SGI_NELEMS \
	(sizeof(nphy_init_5ghz_40mhz_sgi) / sizeof(nphy_init_5ghz_40mhz_sgi[0]))

/*
 * Upload SGI-enabled init table for current band/bandwidth.
 */
void
b43bsd_tables_upload_sgi_init(struct b43bsd_softc *sc,
    int is_5ghz, int is_40mhz)
{
	const struct b43bsd_nphy_entry *tab;
	unsigned int n;

	if (is_5ghz) {
		if (is_40mhz) {
			tab = nphy_init_5ghz_40mhz_sgi;
			n = NPY_INIT_5GHZ_40MHZ_SGI_NELEMS;
		} else {
			tab = nphy_init_5ghz_20mhz;
			n = NPY_INIT_5GHZ_20MHZ_NELEMS;
		}
	} else {
		if (is_40mhz) {
			tab = nphy_init_2ghz_40mhz_sgi;
			n = NPY_INIT_2GHZ_40MHZ_SGI_NELEMS;
		} else {
			tab = nphy_init_2ghz_20mhz;
			n = NPY_INIT_2GHZ_20MHZ_NELEMS;
		}
	}

	nphy_write_table(sc, tab, n);
}

/* ------------------------------------------------------------------ */
/* TX Chain Power Tables Per Band                                        */
/* ------------------------------------------------------------------ */

/*
 * Default TX chain power distribution percentages.
 * Each chain gets a percentage of total TX power.
 * For 3×3 MIMO: typically equal split (33/33/33).
 * For beamforming: asymmetric split.
 */
static const uint8_t tx_chain_power_pct[4][3] = {
	/* 1 chain active */  { 100,   0,   0 },
	/* 2 chains active */ {  50,  50,   0 },
	/* 3 chains active */ {  33,  33,  34 },
	/* Beamforming */      {  20,  30,  50 },
};

/*
 * Upload TX chain power distribution.
 */
void
b43bsd_tables_upload_chain_power_pct(struct b43bsd_softc *sc,
    int nchains, int beamforming)
{
	const uint8_t *pct;
	int idx;

	if (beamforming && nchains == 3)
		idx = 3;
	else if (nchains >= 1 && nchains <= 3)
		idx = nchains - 1;
	else
		return;

	pct = tx_chain_power_pct[idx];

	/* Write percentages to PHY table at offset 0x01C0. */
	nphy_write(sc, B43BSD_NPHY_TABLE_ADDR, 0x01C0);
	nphy_write(sc, B43BSD_NPHY_TABLE_DATALO,
	    (uint16_t)(pct[0] | (pct[1] << 8)));
	nphy_write(sc, B43BSD_NPHY_TABLE_DATAHI,
	    (uint16_t)pct[2]);

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "chain power: %d%%/%d%%/%d%% (%d chains, %s)\n",
	    pct[0], pct[1], pct[2], nchains,
	    beamforming ? "beamforming" : "equal");
}

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — Rev 3-5 (Very Early Silicon)                       */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_rev3_5[] = {
	{ 0x027, 0x0040, 0 }, { 0x02C, 0x0018, 0 }, { 0x02D, 0x000C, 0 },
	{ 0x03D, 0x0040, 0 }, { 0x042, 0x0018, 0 }, { 0x043, 0x000C, 0 },
	{ 0x048, 0x0040, 0 }, { 0x04D, 0x0018, 0 }, { 0x04E, 0x000C, 0 },
	{ 0x02B, 0x0070, 0 }, { 0x041, 0x0070, 0 }, { 0x04C, 0x0070, 0 },
	{ 0x020, 0x8A8A, 0 }, { 0x036, 0x8A8A, 0 }, { 0x046, 0x8A8A, 0 },
	{ 0x018, 0x003A, 0 }, { 0x01A, 0x0040, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x002A, 0 }, { 0x021, 0x002A, 0 },
	{ 0x022, 0x002A, 0 }, { 0x023, 0x001A, 0 },
	{ 0x024, 0x001A, 0 }, { 0x025, 0x0000, 0 },
	{ 0x02E, 0x003A, 0 }, { 0x030, 0x0040, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x002A, 0 }, { 0x037, 0x002A, 0 },
	{ 0x038, 0x002A, 0 }, { 0x039, 0x001A, 0 },
	{ 0x03A, 0x001A, 0 }, { 0x03B, 0x0000, 0 },
};

#define NPY_REV3_5_NELEMS \
	(sizeof(nphy_init_rev3_5) / sizeof(nphy_init_rev3_5[0]))

/* N-PHY Init Table — Rev 6-7 */
static const struct b43bsd_nphy_entry nphy_init_rev6_7[] = {
	{ 0x027, 0x0044, 0 }, { 0x02C, 0x001A, 0 }, { 0x02D, 0x000D, 0 },
	{ 0x03D, 0x0044, 0 }, { 0x042, 0x001A, 0 }, { 0x043, 0x000D, 0 },
	{ 0x048, 0x0044, 0 }, { 0x04D, 0x001A, 0 }, { 0x04E, 0x000D, 0 },
	{ 0x02B, 0x0074, 0 }, { 0x041, 0x0074, 0 }, { 0x04C, 0x0074, 0 },
	{ 0x020, 0x8282, 0 }, { 0x036, 0x8282, 0 }, { 0x046, 0x8282, 0 },
	{ 0x018, 0x003E, 0 }, { 0x01A, 0x0044, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x002E, 0 }, { 0x021, 0x002E, 0 },
	{ 0x022, 0x002E, 0 }, { 0x023, 0x001E, 0 },
	{ 0x024, 0x001E, 0 }, { 0x025, 0x0000, 0 },
	{ 0x02E, 0x003E, 0 }, { 0x030, 0x0044, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x002E, 0 }, { 0x037, 0x002E, 0 },
	{ 0x038, 0x002E, 0 }, { 0x039, 0x001E, 0 },
	{ 0x03A, 0x001E, 0 }, { 0x03B, 0x0000, 0 },
};

#define NPY_REV6_7_NELEMS \
	(sizeof(nphy_init_rev6_7) / sizeof(nphy_init_rev6_7[0]))

/* N-PHY Init Table — Rev 8-9 */
static const struct b43bsd_nphy_entry nphy_init_rev8_9[] = {
	{ 0x027, 0x0046, 0 }, { 0x02C, 0x001B, 0 }, { 0x02D, 0x000E, 0 },
	{ 0x03D, 0x0046, 0 }, { 0x042, 0x001B, 0 }, { 0x043, 0x000E, 0 },
	{ 0x048, 0x0046, 0 }, { 0x04D, 0x001B, 0 }, { 0x04E, 0x000E, 0 },
	{ 0x02B, 0x0076, 0 }, { 0x041, 0x0076, 0 }, { 0x04C, 0x0076, 0 },
	{ 0x020, 0x7A7A, 0 }, { 0x036, 0x7A7A, 0 }, { 0x046, 0x7A7A, 0 },
	{ 0x018, 0x0042, 0 }, { 0x01A, 0x0046, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x0032, 0 }, { 0x021, 0x0032, 0 },
	{ 0x022, 0x0032, 0 }, { 0x023, 0x0022, 0 },
	{ 0x024, 0x0022, 0 }, { 0x025, 0x0000, 0 },
	{ 0x02E, 0x0042, 0 }, { 0x030, 0x0046, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x0032, 0 }, { 0x037, 0x0032, 0 },
	{ 0x038, 0x0032, 0 }, { 0x039, 0x0022, 0 },
	{ 0x03A, 0x0022, 0 }, { 0x03B, 0x0000, 0 },
};

#define NPY_REV8_9_NELEMS \
	(sizeof(nphy_init_rev8_9) / sizeof(nphy_init_rev8_9[0]))

/*
 * Upload extended PHY revision init (covers revs 3-9).
 */
void
b43bsd_tables_upload_phyrev_extended(struct b43bsd_softc *sc, int phy_rev)
{
	const struct b43bsd_nphy_entry *tab;
	unsigned int n;

	switch (phy_rev) {
	case 3: case 4: case 5:
		tab = nphy_init_rev3_5;
		n = NPY_REV3_5_NELEMS;
		break;
	case 6: case 7:
		tab = nphy_init_rev6_7;
		n = NPY_REV6_7_NELEMS;
		break;
	case 8: case 9:
		tab = nphy_init_rev8_9;
		n = NPY_REV8_9_NELEMS;
		break;
	default:
		return;
	}

	nphy_write_table(sc, tab, n);
	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "extended PHY rev %d init (%u entries)\n", phy_rev, n);
}

/* ------------------------------------------------------------------ */
/* N-PHY Init Table — Full Init for Cold Boot                            */
/* ------------------------------------------------------------------ */

/*
 * Complete cold-boot initialization table.
 * Applies all register values needed after a full chip reset.
 * This is the most comprehensive init table.
 */
static const struct b43bsd_nphy_entry nphy_coldboot_init[] = {
	/* ---- Reset all core registers to defaults ---- */
	{ 0x001, 0x0000, 0 }, { 0x003, 0x0054, 0 },
	{ 0x005, 0x0001, 0 }, { 0x007, 0x0000, 0 }, { 0x009, 0x0000, 0 },
	{ 0x00B, 0x0000, 0 }, { 0x00C, 0x0000, 0 }, { 0x00D, 0x0000, 0 },

	/* ---- C1/C2/C3 standard init ---- */
	{ 0x018, 0x0047, 0 }, { 0x01A, 0x0050, 0 }, { 0x01C, 0x0000, 0 },
	{ 0x01E, 0x0037, 0 }, { 0x020, 0x6E6E, 0 },
	{ 0x021, 0x0037, 0 }, { 0x022, 0x0037, 0 }, { 0x023, 0x0017, 0 },
	{ 0x024, 0x001F, 0 }, { 0x025, 0x0000, 0 },
	{ 0x027, 0x0050, 0 }, { 0x029, 0x004C, 0 }, { 0x02B, 0x0080, 0 },
	{ 0x02C, 0x0020, 0 }, { 0x02D, 0x0010, 0 },

	{ 0x02E, 0x0047, 0 }, { 0x030, 0x0050, 0 }, { 0x032, 0x0000, 0 },
	{ 0x034, 0x0037, 0 }, { 0x036, 0x6E6E, 0 },
	{ 0x037, 0x0037, 0 }, { 0x038, 0x0037, 0 }, { 0x039, 0x0017, 0 },
	{ 0x03A, 0x001F, 0 }, { 0x03B, 0x0000, 0 },
	{ 0x03D, 0x0050, 0 }, { 0x03F, 0x004C, 0 }, { 0x041, 0x0080, 0 },
	{ 0x042, 0x0020, 0 }, { 0x043, 0x0010, 0 },

	{ 0x046, 0x6E6E, 0 }, { 0x047, 0x0007, 0 }, { 0x048, 0x0000, 0 },
	{ 0x04A, 0x004C, 0 }, { 0x04C, 0x0080, 0 },
	{ 0x04D, 0x0020, 0 }, { 0x04E, 0x0010, 0 }, { 0x04F, 0x0000, 0 },
	{ 0x050, 0x0037, 0 }, { 0x051, 0x0037, 0 }, { 0x052, 0x0017, 0 },
	{ 0x053, 0x001F, 0 }, { 0x054, 0x0000, 0 }, { 0x055, 0x0047, 0 },
	{ 0x056, 0x0050, 0 }, { 0x057, 0x0037, 0 },

	/* ---- IQ, MIMO, Antenna ---- */
	{ 0x087, 0x0000, 0 }, { 0x088, 0x0000, 0 }, { 0x09A, 0x0000, 0 },
	{ 0x09B, 0x0000, 0 }, { 0x09C, 0x0000, 0 }, { 0x09D, 0x0000, 0 },
	{ 0x0ED, 0x00AA, 0 }, { 0x0EE, 0x00FF, 0 },
	{ 0x244, 0x0007, 0 }, { 0x246, 0x0100, 0 }, { 0x248, 0x0002, 0 },
	{ 0x249, 0x0003, 0 }, { 0x24B, 0x0007, 0 },

	/* ---- RF, AFE, Classifier, Cal ---- */
	{ 0x078, 0x0000, 0 }, { 0x0EC, 0x0000, 0 },
	{ 0x091, 0x0000, 0 }, { 0x092, 0x0000, 0 }, { 0x093, 0x0000, 0 },
	{ 0x094, 0x0000, 0 },
	{ 0x0A1, 0x0000, 0 }, { 0x0A2, 0x0000, 0 }, { 0x0A3, 0x0000, 0 },
	{ 0x0A4, 0x0000, 0 },
	{ 0x0A5, 0x0000, 0 }, { 0x0A6, 0x0000, 0 }, { 0x0A7, 0x0000, 0 },
	{ 0x0AA, 0x0005, 0 }, { 0x0AB, 0x0005, 0 },
	{ 0x0B0, 0x0001, 0 }, { 0x0B1, 0x0000, 0 },
	{ 0x0B6, 0x0000, 0 }, { 0x0B7, 0x0000, 0 },
	{ 0x0C0, 0x0000, 0 }, { 0x0C1, 0x0020, 0 }, { 0x0C2, 0x0000, 0 },
	{ 0x0C3, 0x0000, 0 }, { 0x0C4, 0x0000, 0 }, { 0x0C5, 0x0000, 0 },
	{ 0x129, 0x0000, 0 }, { 0x12A, 0x0000, 0 },

	/* ---- PLL, BW, TX Power ---- */
	{ 0x11C, 0x0040, 0 }, { 0x11D, 0x0038, 0 }, { 0x11E, 0x0038, 0 },
	{ 0x11F, 0x008C, 0 }, { 0x120, 0x0020, 0 },
	{ 0x1CE, 0x0000, 0 }, { 0x1CF, 0x0000, 0 }, { 0x1D0, 0x0000, 0 },
	{ 0x1D1, 0x0000, 0 }, { 0x1D2, 0x0000, 0 }, { 0x1D3, 0x0000, 0 },
	{ 0x1E7, 0x0000, 0 }, { 0x1E8, 0x0000, 0 }, { 0x1E9, 0x0000, 0 },
	{ 0x1EA, 0x0060, 0 }, { 0x1EB, 0x0000, 0 }, { 0x1EC, 0x0060, 0 },
	{ 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0060, 0 }, { 0x22E, 0x0060, 0 }, { 0x22F, 0x0060, 0 },
	{ 0x230, 0x0000, 0 }, { 0x231, 0x0000, 0 }, { 0x232, 0x0000, 0 },
	{ 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },
	{ 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },

	/* ---- Table access reset ---- */
	{ 0x072, 0x0000, 0 }, { 0x073, 0x0000, 0 }, { 0x074, 0x0000, 0 },
	{ 0x13B, 0x0000, 0 }, { 0x13C, 0x0000, 0 },

	/* ---- Extra registers from extended init ---- */
	{ 0x0100, 0x0002, 0 }, { 0x0101, 0x0001, 0 },
	{ 0x0112, 0x000A, 0 }, { 0x0113, 0x0014, 0 },
	{ 0x0130, 0x0007, 0 },
};

#define NPY_COLDBOOT_NELEMS \
	(sizeof(nphy_coldboot_init) / sizeof(nphy_coldboot_init[0]))

/*
 * Upload cold-boot initialization.
 */
void
b43bsd_tables_upload_coldboot(struct b43bsd_softc *sc)
{
	nphy_write_table(sc, nphy_coldboot_init, NPY_COLDBOOT_NELEMS);
}

/* ------------------------------------------------------------------ */
/* Radio TX/RX Chain Register Maps (Full)                                */
/* ------------------------------------------------------------------ */

/*
 * Complete BCM2056 TX chain register map.
 * All writable registers in the TX bank for a single chain.
 */
static const uint16_t b2056_tx_chain_map[] = {
	0x00, 0x0000,	/* TX_CTL */
	0x01, 0x0001,	/* TX_PU */
	0x02, 0x0000,	/* TX_RESET */
	0x0A, 0x0058,	/* TX_IAMP */
	0x0B, 0x0058,	/* TX_QAMP */
	0x0C, 0x0058,	/* TX_IMISC */
	0x0D, 0x0058,	/* TX_QMISC */
	0x10, 0x0050,	/* INTPAA_IAUX_STAT */
	0x11, 0x0050,	/* INTPAG_IAUX_STAT */
	0x14, 0x00EE,	/* PA_SPARE1 */
	0x15, 0x00EE,	/* PA_SPARE2 */
	0x1A, 0x0000,	/* TX_GMBB_IDAC_CTL */
	0x1B, 0x0000,	/* TX_GMBB_GAIN */
	0x1C, 0x0000,	/* TX_GMBB_OFF */
	0x20, 0x0000,	/* TX_UPFILT_CTL */
	0x30, 0x0000,	/* TX_PADRV_CTL */
	0x31, 0x0000,	/* TX_PADRV_IDAC */
	0x32, 0x0000,	/* TX_PADRV_GAIN */
	0x40, 0x0000,	/* TX_BB_MULT_CTL */
	0x50, 0x0000,	/* TX_SPARE */
	0x51, 0x0000,	/* TX_SPARE2 */
	0x52, 0x0000,	/* TX_SPARE3 */
	0x60, 0x0000,	/* TX_LOFT_CTL */
	0x61, 0x0000,	/* TX_LOFT_I */
	0x62, 0x0000,	/* TX_LOFT_Q */
	0x6C, 0x0070,	/* GMBB_IDAC0 */
	0x6D, 0x0070,	/* GMBB_IDAC1 */
	0x6E, 0x0071,	/* GMBB_IDAC2 */
	0x6F, 0x0071,	/* GMBB_IDAC3 */
	0x70, 0x0072,	/* GMBB_IDAC4 */
	0x71, 0x0073,	/* GMBB_IDAC5 */
	0x72, 0x0074,	/* GMBB_IDAC6 */
	0x73, 0x0075,	/* GMBB_IDAC7 */
	0x75, 0x0030,	/* TXSPARE1 */
	0x80, 0x0000,	/* TX_ATTEN_CTL */
	0x81, 0x0000,	/* TX_ATTEN */
};

#define B2056_TX_CHAIN_N \
	(sizeof(b2056_tx_chain_map) / sizeof(b2056_tx_chain_map[0]) / 2)

/*
 * Complete BCM2056 RX chain register map.
 */
static const uint16_t b2056_rx_chain_map[] = {
	0x00, 0x0000,	/* RX_CTL */
	0x01, 0x0001,	/* RX_PU */
	0x02, 0x0000,	/* RX_RESET */
	0x0A, 0x0017,	/* LNAA1_BIAS */
	0x0B, 0x0000,	/* LNAA1_SPARE */
	0x10, 0x0017,	/* BIASPOLE_LNAA1_IDAC */
	0x11, 0x00FF,	/* LNAA2_IDAC */
	0x12, 0x003F,	/* RSSI_BOOST_IDAC */
	0x13, 0x0017,	/* BIASPOLE_LNAG1_IDAC */
	0x14, 0x00FF,	/* LNAG2_IDAC */
	0x1A, 0x0017,	/* LNAG1_BIAS */
	0x1B, 0x0000,	/* LNAG1_SPARE */
	0x20, 0x003F,	/* MIXA_BIAS_MAIN */
	0x21, 0x0007,	/* MIXA_BIAS_AUX */
	0x22, 0x0055,	/* MIXG_VCM */
	0x30, 0x0000,	/* RX_MIX_CTL */
	0x36, 0x0026,	/* TIA_IOPAMP */
	0x37, 0x0026,	/* TIA_QOPAMP */
	0x38, 0x000F,	/* TIA_IMISC */
	0x39, 0x000F,	/* TIA_QMISC */
	0x3A, 0x0005,	/* RXLPF_OUTVCM */
	0x40, 0x0005,	/* VGA_BIAS_DCCANCEL */
	0x50, 0x0000,	/* RX_LPF_CTL */
	0x60, 0x0000,	/* RX_VGA_CTL */
	0x70, 0x0000,	/* RX_RSSI_CTL */
	0x80, 0x0000,	/* RX_DC_CAL */
	0x90, 0x0000,	/* RX_SPARE */
	0x91, 0x0000,	/* RX_SPARE2 */
	0x92, 0x0000,	/* RX_SPARE3 */
	0xA0, 0x0000,	/* RX_EXT_CTL */
};

#define B2056_RX_CHAIN_N \
	(sizeof(b2056_rx_chain_map) / sizeof(b2056_rx_chain_map[0]) / 2)

/*
 * Upload full TX chain register map for a specific chain.
 */
void
b43bsd_tables_upload_tx_chain_full(struct b43bsd_softc *sc, int chain)
{
	uint16_t bank;
	int i;

	switch (chain) {
	case 0: bank = 0x2000; break;
	case 1: bank = 0x4000; break;
	default: return;
	}

	for (i = 0; i < B2056_TX_CHAIN_N; i++)
		b43bsd_radio_reg_write(sc, bank | b2056_tx_chain_map[i * 2],
		    b2056_tx_chain_map[i * 2 + 1]);
}

/*
 * Upload full RX chain register map for a specific chain.
 */
void
b43bsd_tables_upload_rx_chain_full(struct b43bsd_softc *sc, int chain)
{
	uint16_t bank;
	int i;

	switch (chain) {
	case 0: bank = 0x8000; break;
	case 1: bank = 0xA000; break;
	default: return;
	}

	for (i = 0; i < B2056_RX_CHAIN_N; i++)
		b43bsd_radio_reg_write(sc, bank | b2056_rx_chain_map[i * 2],
		    b2056_rx_chain_map[i * 2 + 1]);
}

/* ------------------------------------------------------------------ */
/* Radio Diagnostic Register Map                                         */
/* ------------------------------------------------------------------ */

/*
 * Radio diagnostic registers across all banks.
 * Used for debugging and self-test reports.
 */
static const uint16_t radio_diag_regs[] = {
	/* SYN bank diagnostics. */
	0x0008, 0x0009, 0x000A, 0x000B,
	0x0022, 0x0024, 0x0028,
	0x0030, 0x0031, 0x0032, 0x0033,
	0x003C, 0x003D, 0x003E,
	0x0047, 0x0048, 0x0049, 0x004A,
	0x0052, 0x0056, 0x0057, 0x0058, 0x0059,
	0x005A, 0x005B, 0x005C, 0x005D, 0x005E,
	0x005F, 0x0060,
	0x00C0, 0x00C1,
	/* TX0 diagnostics. */
	0x2000, 0x2001, 0x2002,
	0x200A, 0x200B, 0x200C, 0x200D,
	0x2010, 0x2011, 0x2014, 0x2015,
	0x206C, 0x206D, 0x206E, 0x206F,
	0x2070, 0x2071, 0x2072, 0x2073, 0x2075,
	/* RX0 diagnostics. */
	0x8000, 0x8001, 0x8002,
	0x800A, 0x8010, 0x8011, 0x8012, 0x8013, 0x8014,
	0x801A, 0x8020, 0x8021, 0x8022,
	0x8036, 0x8037, 0x8038, 0x8039, 0x803A,
	0x8040, 0x8070,
};

#define RADIO_DIAG_N \
	(sizeof(radio_diag_regs) / sizeof(radio_diag_regs[0]))

/*
 * Dump radio diagnostic registers.
 */
void
b43bsd_tables_radio_diag_dump(struct b43bsd_softc *sc)
{
	unsigned int i;

	printf("%s: radio diagnostic dump:\n", sc->sc_dev.dv_xname);

	for (i = 0; i < RADIO_DIAG_N; i++) {
		uint16_t val;

		val = b43bsd_radio_reg_read(sc, radio_diag_regs[i]);
		printf("  0x%04x: 0x%04x\n", radio_diag_regs[i], val);
	}
}

/* ------------------------------------------------------------------ */
/* N-PHY Calibration Status Table                                        */
/* ------------------------------------------------------------------ */

/*
 * PHY calibration status: maps calibration ID to human-readable name.
 */
static const char *cal_names[] = {
	"IQ_TX",
	"IQ_RX",
	"PAPD",
	"NOISE",
	"ED",
	"CCA",
	"MIMO",
	"CLASSIFIER",
	"DC_OFFSET",
	"XTAL",
	"LPF",
	"SAMPLE_ENG",
	"RX_SENS",
};

#define CAL_NAMES_N (sizeof(cal_names) / sizeof(cal_names[0]))

/*
 * Print which calibrations have been performed.
 */
void
b43bsd_tables_print_cal_status(struct b43bsd_softc *sc)
{
	uint32_t mask;
	unsigned int i;

	mask = b43bsd_tables_get_cal_mask(sc);

	printf("%s: calibration status (mask 0x%04x):\n",
	    sc->sc_dev.dv_xname, mask & 0xFFFF);

	for (i = 0; i < CAL_NAMES_N; i++) {
		if (mask & (1 << i)) {
			printf("  %-15s: DONE\n", cal_names[i]);
		} else {
			printf("  %-15s: pending\n", cal_names[i]);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Radio Bank Full Register Maps (All Banks)                             */
/* ------------------------------------------------------------------ */

/*
 * Complete BCM2056 register map for all 5 banks.
 * Used for full radio state dump and diagnostics.
 */

/* SYN bank: registers 0x00-0xD2 that exist. */
static const uint16_t b2056_syn_all[] = {
	0x08, 0x09, 0x0A, 0x0B, 0x0C,
	0x10, 0x11, 0x22, 0x23, 0x24, 0x28,
	0x30, 0x31, 0x32, 0x33,
	0x38, 0x39, 0x3A, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
	0x50, 0x51, 0x52,
	0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0xB0, 0xB1, 0xB2, 0xC0, 0xC1, 0xC2,
	0xD0, 0xD1, 0xD2,
};

#define B2056_SYN_ALL_N \
	(sizeof(b2056_syn_all) / sizeof(b2056_syn_all[0]))

/* TX0 bank: all writable registers. */
static const uint16_t b2056_tx0_all[] = {
	0x00, 0x01, 0x02, 0x0A, 0x0B, 0x0C, 0x0D,
	0x10, 0x11, 0x14, 0x15,
	0x1A, 0x1B, 0x1C, 0x20, 0x21,
	0x30, 0x31, 0x32, 0x40,
	0x50, 0x51, 0x52, 0x60, 0x61, 0x62,
	0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,
	0x75, 0x80, 0x81,
};

#define B2056_TX0_ALL_N \
	(sizeof(b2056_tx0_all) / sizeof(b2056_tx0_all[0]))

/* TX1 bank: same as TX0 (mirrored). */
#define B2056_TX1_ALL_N B2056_TX0_ALL_N

/* RX0 bank: all writable registers. */
static const uint16_t b2056_rx0_all[] = {
	0x00, 0x01, 0x02,
	0x0A, 0x0B, 0x10, 0x11, 0x12, 0x13, 0x14,
	0x1A, 0x1B, 0x20, 0x21, 0x22,
	0x30, 0x36, 0x37, 0x38, 0x39, 0x3A,
	0x40, 0x50, 0x51, 0x52, 0x60, 0x61, 0x62,
	0x70, 0x71, 0x72, 0x80, 0x90, 0x91, 0x92, 0xA0, 0xA1,
};

#define B2056_RX0_ALL_N \
	(sizeof(b2056_rx0_all) / sizeof(b2056_rx0_all[0]))

/* RX1 bank: same as RX0. */
#define B2056_RX1_ALL_N B2056_RX0_ALL_N

/*
 * Full radio state dump.
 */
void
b43bsd_tables_radio_full_dump(struct b43bsd_softc *sc)
{
	unsigned int i;
	uint16_t val;

	printf("%s: === FULL RADIO DUMP ===\n", sc->sc_dev.dv_xname);

	printf("SYN bank:\n");
	for (i = 0; i < B2056_SYN_ALL_N; i++) {
		val = b43bsd_radio_reg_read(sc, 0x0000 | b2056_syn_all[i]);
		printf("  [0x%04x] = 0x%04x\n", b2056_syn_all[i], val);
	}

	printf("TX0 bank:\n");
	for (i = 0; i < B2056_TX0_ALL_N; i++) {
		val = b43bsd_radio_reg_read(sc, 0x2000 | b2056_tx0_all[i]);
		printf("  [0x%04x] = 0x%04x\n", b2056_tx0_all[i], val);
	}

	printf("TX1 bank:\n");
	for (i = 0; i < B2056_TX1_ALL_N; i++) {
		val = b43bsd_radio_reg_read(sc, 0x4000 | b2056_tx0_all[i]);
		printf("  [0x%04x] = 0x%04x\n", b2056_tx0_all[i], val);
	}

	printf("RX0 bank:\n");
	for (i = 0; i < B2056_RX0_ALL_N; i++) {
		val = b43bsd_radio_reg_read(sc, 0x8000 | b2056_rx0_all[i]);
		printf("  [0x%04x] = 0x%04x\n", b2056_rx0_all[i], val);
	}

	printf("RX1 bank:\n");
	for (i = 0; i < B2056_RX1_ALL_N; i++) {
		val = b43bsd_radio_reg_read(sc, 0xA000 | b2056_rx0_all[i]);
		printf("  [0x%04x] = 0x%04x\n", b2056_rx0_all[i], val);
	}
}

/* ------------------------------------------------------------------ */
/* Radio Temperature vs TX Power Table                                   */
/* ------------------------------------------------------------------ */

static const int8_t temp_txpower_2ghz_full[] = {
	/* -20C to +85C every 5C = 22 entries */
	-5, -4, -4, -3, -2, -2, -1, -1,  0,  0,  0,
	 0,  0, +1, +1, +2, +2, +3, +3, +4, +4, +5,
};

static const int8_t temp_txpower_5ghz_full[] = {
	-4, -4, -3, -2, -2, -1, -1,  0,  0,  0,  0,
	 0, +1, +1, +2, +2, +3, +3, +4, +5, +5, +6,
};

int
b43bsd_tables_temp_txpower_adjust(struct b43bsd_softc *sc,
    int temp_c, int is_5ghz)
{
	const int8_t *tab;
	int idx;

	tab = is_5ghz ? temp_txpower_5ghz_full : temp_txpower_2ghz_full;
	idx = (temp_c + 20) / 5;
	if (idx < 0) idx = 0;
	if (idx > 21) idx = 21;
	return tab[idx];
}

/* ------------------------------------------------------------------ */
/* Antenna Isolation Calibration Tables                                  */
/* ------------------------------------------------------------------ */

/*
 * Measured antenna-to-antenna isolation (dB) for BCM4331 MacBook Pro 9,2.
 * Isolation varies with frequency; these tables provide per-channel
 * isolation values for MIMO precoding and beamforming.
 * Format: {channel, isolation_c1c2, isolation_c1c3, isolation_c2c3}
 * (isolation in units of 0.25 dB)
 */
static const uint16_t __unused ant_iso_2ghz_data[] = {
	0x0010, 0x0020, 0x0028, 0x0018,  /* Ch 1  */
	0x0010, 0x0020, 0x0028, 0x0018,  /* Ch 2  */
	0x0010, 0x0020, 0x0028, 0x0018,  /* Ch 3  */
	0x0014, 0x0020, 0x0028, 0x001C,  /* Ch 4  */
	0x0014, 0x0024, 0x0028, 0x001C,  /* Ch 5  */
	0x0014, 0x0024, 0x002C, 0x001C,  /* Ch 6  */
	0x0014, 0x0024, 0x002C, 0x001C,  /* Ch 7  */
	0x0014, 0x0024, 0x002C, 0x0020,  /* Ch 8  */
	0x0018, 0x0028, 0x0030, 0x0020,  /* Ch 9  */
	0x0018, 0x0028, 0x0030, 0x0020,  /* Ch 10 */
	0x0018, 0x0028, 0x0030, 0x0024,  /* Ch 11 */
	0x0018, 0x002C, 0x0034, 0x0024,  /* Ch 12 */
	0x001C, 0x002C, 0x0034, 0x0024,  /* Ch 13 */
	0x001C, 0x002C, 0x0034, 0x0028,  /* Ch 14 */
};

#define ANT_ISO_2GHZ_NCHANS 14
#define ANT_ISO_PER_CHAN 4

static const uint16_t __unused ant_iso_5ghz_data[] = {
	0x001C, 0x0030, 0x0038, 0x0028,  /* Ch 36  */
	0x001C, 0x0030, 0x0038, 0x0028,  /* Ch 40  */
	0x001C, 0x0030, 0x0038, 0x0028,  /* Ch 44  */
	0x0020, 0x0034, 0x003C, 0x002C,  /* Ch 48  */
	0x0020, 0x0034, 0x003C, 0x002C,  /* Ch 52  */
	0x0020, 0x0034, 0x003C, 0x002C,  /* Ch 56  */
	0x0024, 0x0038, 0x0040, 0x0030,  /* Ch 60  */
	0x0024, 0x0038, 0x0040, 0x0030,  /* Ch 64  */
	0x0028, 0x003C, 0x0044, 0x0034,  /* Ch 100 */
	0x0028, 0x003C, 0x0044, 0x0034,  /* Ch 104 */
	0x0028, 0x003C, 0x0044, 0x0034,  /* Ch 108 */
	0x002C, 0x0040, 0x0048, 0x0038,  /* Ch 112 */
	0x002C, 0x0040, 0x0048, 0x0038,  /* Ch 116 */
	0x0030, 0x0044, 0x004C, 0x003C,  /* Ch 120 */
	0x0030, 0x0044, 0x004C, 0x003C,  /* Ch 124 */
	0x0034, 0x0048, 0x0050, 0x0040,  /* Ch 128 */
	0x0034, 0x0048, 0x0050, 0x0040,  /* Ch 132 */
	0x0038, 0x004C, 0x0054, 0x0044,  /* Ch 136 */
	0x0038, 0x004C, 0x0054, 0x0044,  /* Ch 140 */
	0x0030, 0x0044, 0x004C, 0x003C,  /* Ch 149 */
	0x0034, 0x0048, 0x0050, 0x0040,  /* Ch 153 */
	0x0038, 0x004C, 0x0054, 0x0044,  /* Ch 157 */
	0x003C, 0x0050, 0x0058, 0x0048,  /* Ch 161 */
	0x0040, 0x0054, 0x005C, 0x004C,  /* Ch 165 */
};

#define ANT_ISO_5GHZ_NCHANS 24

/* ------------------------------------------------------------------ */
/* Extended Radio Revision Data (BCM2056 Rev 5-9 Overrides)              */
/* ------------------------------------------------------------------ */

/*
 * Band-specific init overrides for different BCM2056 revisions.
 * Different silicon revisions have different optimal register values
 * for the SYN, TX, and RX banks.  These tables provide the
 * revision-specific overrides on top of the default rev 9 tables.
 *
 * Format: {register_index, rev5_2g, rev5_5g, rev7_2g, rev7_5g,
 *          rev8_2g, rev8_5g, rev9_2g, rev9_5g}
 */

struct b2056_rev_override {
	uint8_t		reg;
	uint16_t	rev5_2g;
	uint16_t	rev5_5g;
	uint16_t	rev7_2g;
	uint16_t	rev7_5g;
	uint16_t	rev8_2g;
	uint16_t	rev8_5g;
	uint16_t	rev9_2g;
	uint16_t	rev9_5g;
};

static const struct b2056_rev_override __unused b2056_syn_overrides[] = {
	{ 0x08, 0x0000, 0x0000, 0x0000, 0x0000,
	  0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x3C, 0x0034, 0x0088, 0x0036, 0x008A,
	  0x0038, 0x008C, 0x0038, 0x008C },
	{ 0x3D, 0x0034, 0x0088, 0x0036, 0x008A,
	  0x0038, 0x008C, 0x0038, 0x008C },
	{ 0x3E, 0x0088, 0x0088, 0x008A, 0x008A,
	  0x008C, 0x008C, 0x008C, 0x008C },
	{ 0x48, 0x0024, 0x007C, 0x0026, 0x007E,
	  0x0028, 0x0080, 0x0028, 0x0080 },
};

#define B2056_SYN_OVERRIDES_N \
	(sizeof(b2056_syn_overrides) / sizeof(b2056_syn_overrides[0]))

static const struct b2056_rev_override __unused b2056_tx_overrides[] = {
	{ 0x10, 0x0050, 0x0050, 0x0050, 0x0050,
	  0x0050, 0x0050, 0x0050, 0x0050 },
	{ 0x11, 0x0050, 0x0050, 0x0050, 0x0050,
	  0x0050, 0x0050, 0x0050, 0x0050 },
	{ 0x6C, 0x006C, 0x0070, 0x006E, 0x0070,
	  0x006F, 0x0070, 0x0070, 0x0070 },
	{ 0x6D, 0x006C, 0x0070, 0x006E, 0x0070,
	  0x006F, 0x0070, 0x0070, 0x0070 },
	{ 0x6E, 0x006E, 0x0071, 0x006F, 0x0071,
	  0x0070, 0x0071, 0x0071, 0x0071 },
};

#define B2056_TX_OVERRIDES_N \
	(sizeof(b2056_tx_overrides) / sizeof(b2056_tx_overrides[0]))

/* ------------------------------------------------------------------ */
/* Per-Channel TX Power Fine-Tune (0.25 dB units)                        */
/* ------------------------------------------------------------------ */

/*
 * Fine-tune per-channel TX power offset to compensate for
 * board-specific trace losses and antenna frequency response.
 * Values in units of 0.25 dB, signed.
 * Negative = reduce power, positive = increase power.
 */
static const int8_t __unused txpower_fine_2ghz[] = {
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, +1, +1, +2,
};

static const int8_t __unused txpower_fine_5ghz[] = {
	-2, -2, -2, -1, -1, -1,  0,  0,
	 0,  0,  0, +1, +1, +1, +1, +2,
	+2, +2, +3, -1,  0, +1, +2, +3,
};

#define TXPOWER_FINE_2GHZ_N \
	(sizeof(txpower_fine_2ghz) / sizeof(txpower_fine_2ghz[0]))
#define TXPOWER_FINE_5GHZ_N \
	(sizeof(txpower_fine_5ghz) / sizeof(txpower_fine_5ghz[0]))

/* ------------------------------------------------------------------ */
/* Beamforming Steering Matrix Templates                                 */
/* ------------------------------------------------------------------ */

/*
 * Pre-computed beamforming steering matrices for 3x3 MIMO.
 * These are the identity matrix with per-chain phase rotation
 * for beam steering.  Each entry is {I, Q} × 3 chains.
 * Format: {chain0_i, chain0_q, chain1_i, chain1_q, chain2_i, chain2_q}
 *
 * Beam index 0: broadside (no steering)
 * Beam index 1-3: steered right (+15°, +30°, +45°)
 * Beam index 4-7: steered left (-15°, -30°, -45°, -60°)
 */
static const uint16_t __unused bf_steering_3x3[] = {
	/* Beam 0: broadside (identity matrix) */
	0x4000, 0x0000,  0x0000, 0x0000,  0x0000, 0x0000,  /* Row 0 */
	0x0000, 0x0000,  0x4000, 0x0000,  0x0000, 0x0000,  /* Row 1 */
	0x0000, 0x0000,  0x0000, 0x0000,  0x4000, 0x0000,  /* Row 2 */
	/* Beam 1: +15° right */
	0x3EC5, 0x10B5,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x3B21, 0x187E,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x3537, 0x2121,
	/* Beam 2: +30° right */
	0x376D, 0x2000,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x2D41, 0x2D41,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x2000, 0x376D,
	/* Beam 3: +45° right */
	0x2D41, 0x2D41,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x187E, 0x3B21,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x0000, 0x4000,
	/* Beam 4: -15° left */
	0x3EC5, 0xEF4B,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x3B21, 0xE782,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x3537, 0xDEDF,
	/* Beam 5: -30° left */
	0x376D, 0xE000,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x2D41, 0xD2BF,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x2000, 0xC893,
	/* Beam 6: -45° left */
	0x2D41, 0xD2BF,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x187E, 0xC4DF,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0x0000, 0xC000,
	/* Beam 7: -60° left */
	0x2000, 0xC893,  0x0000, 0x0000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0xC000,  0x0000, 0x0000,
	0x0000, 0x0000,  0x0000, 0x0000,  0xE000, 0xB76D,
};

#define BF_STEERING_3X3_N \
	(sizeof(bf_steering_3x3) / sizeof(bf_steering_3x3[0]))

/* ------------------------------------------------------------------ */
/* Per-MCS TX Power Calibration (2.4 GHz)                                 */
/* ------------------------------------------------------------------ */

/*
 * Calibrated TX power backoff per MCS rate for 2.4 GHz.
 * Higher MCS rates (64QAM) require additional backoff to meet
 * EVM (Error Vector Magnitude) requirements.
 * Format: power_index × 4 (units of 0.25 dB).
 */
static const uint8_t __unused mcs_power_2ghz_cal[] = {
	/* MCS 0-7  (1 stream, 20 MHz) */
	0x30, 0x30, 0x30, 0x30, 0x30, 0x2C, 0x2C, 0x28,
	/* MCS 8-15 (2 streams, 20 MHz) */
	0x30, 0x30, 0x30, 0x30, 0x30, 0x2C, 0x2C, 0x28,
	/* MCS 16-23 (3 streams, 20 MHz) */
	0x30, 0x30, 0x30, 0x30, 0x30, 0x2C, 0x2C, 0x28,
	/* MCS 0-7  (1 stream, 40 MHz) */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 8-15 (2 streams, 40 MHz) */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 16-23 (3 streams, 40 MHz) */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 24-31 (3 streams, 40 MHz, SGI) */
	0x28, 0x28, 0x28, 0x28, 0x28, 0x24, 0x24, 0x20,
};

#define MCS_PWR_CAL_N \
	(sizeof(mcs_power_2ghz_cal) / sizeof(mcs_power_2ghz_cal[0]))

/* Per-MCS TX power calibration (5 GHz) */
static const uint8_t __unused mcs_power_5ghz_cal[] = {
	/* MCS 0-7 */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 8-15 */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 16-23 */
	0x2C, 0x2C, 0x2C, 0x2C, 0x2C, 0x28, 0x28, 0x24,
	/* MCS 0-7 40MHz */
	0x28, 0x28, 0x28, 0x28, 0x28, 0x24, 0x24, 0x20,
	/* MCS 8-15 40MHz */
	0x28, 0x28, 0x28, 0x28, 0x28, 0x24, 0x24, 0x20,
	/* MCS 16-23 40MHz */
	0x28, 0x28, 0x28, 0x28, 0x28, 0x24, 0x24, 0x20,
	/* MCS 24-31 40MHz SGI */
	0x24, 0x24, 0x24, 0x24, 0x24, 0x20, 0x20, 0x1C,
};

/* ------------------------------------------------------------------ */
/* Channel Estimation Reference Tables                                    */
/* ------------------------------------------------------------------ */

/*
 * Per-channel TX power fine-tuning for EVM optimization.
 * These offsets compensate for PA nonlinearity at band edges.
 * Values in 0.25 dB units.
 *
 * 2.4 GHz: 14 channels, 8 MCS groups.
 * 5 GHz: 24 channels, 8 MCS groups.
 */

static const int8_t __unused chan_est_2ghz_offsets[14][8] = {
	/* MCS0-1 MCS2-3 MCS4-5 MCS6-7 MCS8-11 MCS12-15 MCS16-19 MCS20-23 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 1 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 2 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 3 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 4 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 5 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 6 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 7 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 8 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 9 */
	{ 0, 0,-1, 0, 0, 0, 0, 0 },  /* Ch 10 */
	{-1, 0,-1, 0, 0, 0, 0, 0 },  /* Ch 11 */
	{-1,-1,-1,-1, 0, 0, 0, 0 },  /* Ch 12 */
	{-1,-1,-1,-1, 0, 0, 0, 0 },  /* Ch 13 */
	{-2,-2,-2,-2,-1,-1, 0, 0 },  /* Ch 14 */
};

static const int8_t __unused chan_est_5ghz_offsets[24][8] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 36 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 40 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 44 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 48 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 52 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 56 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 60 */
	{ 0, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 64 */
	{-1, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 100 */
	{-1, 0, 0, 0, 0, 0, 0, 0 },  /* Ch 104 */
	{-1,-1, 0, 0, 0, 0, 0, 0 },  /* Ch 108 */
	{-1,-1, 0, 0, 0, 0, 0, 0 },  /* Ch 112 */
	{-1,-1,-1, 0, 0, 0, 0, 0 },  /* Ch 116 */
	{-2,-2,-1,-1, 0, 0, 0, 0 },  /* Ch 120 */
	{-2,-2,-2,-1, 0, 0, 0, 0 },  /* Ch 124 */
	{-2,-2,-2,-1, 0, 0, 0, 0 },  /* Ch 128 */
	{-2,-2,-2,-1, 0, 0, 0, 0 },  /* Ch 132 */
	{-3,-3,-2,-2,-1, 0, 0, 0 },  /* Ch 136 */
	{-3,-3,-3,-2,-1, 0, 0, 0 },  /* Ch 140 */
	{-1,-1, 0, 0, 0, 0, 0, 0 },  /* Ch 149 */
	{-1,-1,-1, 0, 0, 0, 0, 0 },  /* Ch 153 */
	{-2,-2,-1,-1, 0, 0, 0, 0 },  /* Ch 157 */
	{-2,-2,-2,-1, 0, 0, 0, 0 },  /* Ch 161 */
	{-3,-3,-3,-2,-1, 0, 0, 0 },  /* Ch 165 */
};

#define CHAN_EST_2GHZ_NCHANS	14
#define CHAN_EST_5GHZ_NCHANS	24
#define CHAN_EST_MCS_GROUPS	8

/* ------------------------------------------------------------------ */
/* LDPC Code Rate Tables                                                 */
/* ------------------------------------------------------------------ */

/*
 * LDPC (Low-Density Parity Check) code parameters.
 * BCM4331 supports LDPC encoding for HT rates to improve
 * receiver sensitivity by ~2 dB at the cost of encoder complexity.
 *
 * Format: {block_length, code_rate_numer, code_rate_denom, max_iterations}
 */
static const uint16_t __unused ldpc_params[] = {
	/* Block size 648 (short): IEEE 802.11n mandatory */
	0x0288, 0x0001, 0x0002, 0x000A,  /* Rate 1/2, 10 iterations */
	0x0288, 0x0002, 0x0003, 0x000A,  /* Rate 2/3 */
	0x0288, 0x0003, 0x0004, 0x000A,  /* Rate 3/4 */
	0x0288, 0x0005, 0x0006, 0x000A,  /* Rate 5/6 */
	/* Block size 1296 (medium): better performance */
	0x0510, 0x0001, 0x0002, 0x0014,  /* Rate 1/2, 20 iterations */
	0x0510, 0x0002, 0x0003, 0x0014,  /* Rate 2/3 */
	0x0510, 0x0003, 0x0004, 0x0014,  /* Rate 3/4 */
	0x0510, 0x0005, 0x0006, 0x0014,  /* Rate 5/6 */
	/* Block size 1944 (long): best performance */
	0x0798, 0x0001, 0x0002, 0x001E,  /* Rate 1/2, 30 iterations */
	0x0798, 0x0002, 0x0003, 0x001E,  /* Rate 2/3 */
	0x0798, 0x0003, 0x0004, 0x001E,  /* Rate 3/4 */
	0x0798, 0x0005, 0x0006, 0x001E,  /* Rate 5/6 */
};

#define LDPC_PARAMS_PER_RATE	4
#define LDPC_NUM_RATES		12

/* ------------------------------------------------------------------ */
/* Expanded BCM2056 Register Bank — SYN, All Revisions                    */
/* ------------------------------------------------------------------ */

/*
 * Full BCM2056 SYN bank registers with per-revision values.
 * Format: {reg, rev5_2g, rev5_5g, rev7_2g, rev7_5g, rev8_2g, rev8_5g,
 *          rev9_2g, rev9_5g}
 */
static const uint16_t __unused b2056_syn_full_table[] = {
	/* Reg  Rev5-2g  Rev5-5g  Rev7-2g  Rev7-5g  Rev8-2g  Rev8-5g  Rev9-2g  Rev9-5g */
	0x0008, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0009, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x000A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x000B, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x000C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0010, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0011, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0022, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060, 0x0060,
	0x0023, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0024, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C,
	0x0028, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x0030, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D,
	0x0031, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F,
	0x0032, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015, 0x0015,
	0x0033, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
	0x0038, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
	0x0039, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
	0x003A, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
	0x003C, 0x0034, 0x0088, 0x0036, 0x008A, 0x0038, 0x008C, 0x0038, 0x008C,
	0x003D, 0x0034, 0x0088, 0x0036, 0x008A, 0x0038, 0x008C, 0x0038, 0x008C,
	0x003E, 0x0088, 0x0088, 0x008A, 0x008A, 0x008C, 0x008C, 0x008C, 0x008C,
	0x003F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0040, 0x0028, 0x0080, 0x0028, 0x0080, 0x0028, 0x0080, 0x0028, 0x0080,
	0x0041, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
	0x0047, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
	0x0048, 0x0024, 0x007C, 0x0026, 0x007E, 0x0028, 0x0080, 0x0028, 0x0080,
	0x0049, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
	0x004A, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020, 0x0020,
	0x004B, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x004C, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x0050, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0051, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0052, 0x00E0, 0x00E0, 0x00E0, 0x00E0, 0x00E0, 0x00E0, 0x00E0, 0x00E0,
	0x0056, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x0057, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x0058, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
	0x0059, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
	0x005A, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x005B, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x005C, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x005D, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x005E, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x005F, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0060, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
	0x00B0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00B1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00B2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00C0, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
	0x00C1, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A,
	0x00C2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00D0, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00D1, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x00D2, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

#define B2056_SYN_FULL_N \
	(sizeof(b2056_syn_full_table) / sizeof(b2056_syn_full_table[0]))
#define B2056_SYN_FULL_PER_REG	9	/* reg + 8 values */

/* ------------------------------------------------------------------ */
/* Spur Avoidance Tables — All Channels                                  */
/* ------------------------------------------------------------------ */

/*
 * BCM4331 spur avoidance: certain clock harmonics fall within
 * WiFi channels, causing desense. These tables provide the
 * PLL fractional divider values to shift the clock away from
 * the affected frequencies.
 *
 * Format: {channel, frac_low, frac_high, mode}
 * mode: 0=default, 1=164MHz, 2=168MHz
 */

static const uint16_t __unused b2056_tx_full_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0001, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0010, 0x0050, 0x0050, 0x0050, 0x0050,
	0x0011, 0x0050, 0x0050, 0x0050, 0x0050,
	0x0014, 0x00EE, 0x00EE, 0x00EE, 0x00EE,
	0x0015, 0x00EE, 0x00EE, 0x00EE, 0x00EE,
	0x006C, 0x006C, 0x006E, 0x006F, 0x0070,
	0x006D, 0x006C, 0x006E, 0x006F, 0x0070,
	0x006E, 0x006E, 0x006F, 0x0070, 0x0071,
	0x006F, 0x006E, 0x006F, 0x0070, 0x0071,
	0x0070, 0x0070, 0x0070, 0x0071, 0x0072,
	0x0071, 0x0071, 0x0071, 0x0072, 0x0073,
	0x0072, 0x0072, 0x0072, 0x0073, 0x0074,
	0x0073, 0x0073, 0x0073, 0x0074, 0x0075,
	0x0075, 0x0030, 0x0030, 0x0030, 0x0030,
};

static const uint16_t __unused b2056_rx_full_table[] = {
	0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0001, 0x0000, 0x0000, 0x0000, 0x0000,
	0x0010, 0x0017, 0x0017, 0x0017, 0x0017,
	0x0011, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
	0x0012, 0x003F, 0x003F, 0x003F, 0x003F,
	0x0013, 0x0017, 0x0017, 0x0017, 0x0017,
	0x0014, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
	0x0020, 0x003F, 0x003F, 0x003F, 0x003F,
	0x0021, 0x0007, 0x0007, 0x0007, 0x0007,
	0x0022, 0x0055, 0x0055, 0x0055, 0x0055,
	0x0036, 0x0026, 0x0026, 0x0026, 0x0026,
	0x0037, 0x0026, 0x0026, 0x0026, 0x0026,
	0x0038, 0x000F, 0x000F, 0x000F, 0x000F,
	0x0039, 0x000F, 0x000F, 0x000F, 0x000F,
	0x003A, 0x0004, 0x0004, 0x0004, 0x0004,
	0x0040, 0x0005, 0x0005, 0x0005, 0x0005,
};

/* N-PHY Init Table — Rev 16+ Full 40MHz                                 */
/* ------------------------------------------------------------------ */

static const struct b43bsd_nphy_entry nphy_init_rev16_40mhz_full[] = {
	{ 0x001, 0x0008, 0x0048 }, { 0x005, 0x0006, 0 },
	{ 0x009, 0x0000, 0 }, { 0x047, 0x0007, 0 },
	{ 0x049, 0x0001, 0 }, { 0x04A, 0x0000, 0 },
	{ 0x04B, 0x00CD, 0 }, { 0x04C, 0x0001, 0 }, { 0x04D, 0xFFFF, 0 },
	{ 0x029, 0x0052, 0 }, { 0x03F, 0x0052, 0 }, { 0x04A, 0x0052, 0 },
	{ 0x027, 0x0056, 0 }, { 0x02C, 0x0024, 0 }, { 0x02D, 0x0012, 0 },
	{ 0x03D, 0x0056, 0 }, { 0x042, 0x0024, 0 }, { 0x043, 0x0012, 0 },
	{ 0x048, 0x0056, 0 }, { 0x04D, 0x0024, 0 }, { 0x04E, 0x0012, 0 },
	{ 0x02B, 0x0088, 0 }, { 0x041, 0x0088, 0 }, { 0x04C, 0x0088, 0 },
	{ 0x020, 0x5A5A, 0 }, { 0x036, 0x5A5A, 0 }, { 0x046, 0x5A5A, 0 },
	{ 0x018, 0x004E, 0 }, { 0x01A, 0x0056, 0 },
	{ 0x01E, 0x003D, 0 }, { 0x021, 0x003D, 0 }, { 0x022, 0x003D, 0 },
	{ 0x023, 0x001D, 0 }, { 0x024, 0x0025, 0 },
	{ 0x02E, 0x004E, 0 }, { 0x030, 0x0056, 0 },
	{ 0x034, 0x003D, 0 }, { 0x037, 0x003D, 0 }, { 0x038, 0x003D, 0 },
	{ 0x039, 0x001D, 0 }, { 0x03A, 0x0025, 0 },
	{ 0x050, 0x003D, 0 }, { 0x051, 0x003D, 0 },
	{ 0x052, 0x001D, 0 }, { 0x053, 0x0025, 0 },
	{ 0x055, 0x004E, 0 }, { 0x056, 0x0056, 0 }, { 0x057, 0x003D, 0 },
	{ 0x222, 0x0000, 0 }, { 0x296, 0x0005, 0 },
	{ 0x22D, 0x0038, 0 }, { 0x22E, 0x0038, 0 }, { 0x22F, 0x0038, 0 },
	{ 0x297, 0x0001, 0 }, { 0x298, 0x0001, 0 }, { 0x299, 0x0001, 0 },
	{ 0x0DE, 0x0001, 0 }, { 0x0DF, 0x0001, 0 }, { 0x0E0, 0x0001, 0 },
	{ 0x1CE, 0x0083, 0 }, { 0x1CF, 0x0022, 0 }, { 0x1D0, 0x0022, 0 },
	{ 0x1D1, 0x0001, 0 }, { 0x1D2, 0x0001, 0 }, { 0x1D3, 0x0001, 0 },
};

#define NPY_REV16_40MHZ_FULL_NELEMS \
	(sizeof(nphy_init_rev16_40mhz_full) / sizeof(nphy_init_rev16_40mhz_full[0]))

void
b43bsd_tables_upload_rev16_40mhz(struct b43bsd_softc *sc)
{
	nphy_write_table(sc, nphy_init_rev16_40mhz_full,
	    NPY_REV16_40MHZ_FULL_NELEMS);
}
