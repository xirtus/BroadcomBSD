/*	$OpenBSD: b43bsd_phy_n.h,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * BCM4331 PHY-N (N-PHY) register definitions and data structures.
 * Ported from Linux drivers/net/wireless/broadcom/b43/phy_n.h (GPLv2).
 */

#ifndef _DEV_IC_B43BSD_PHY_N_H_
#define _DEV_IC_B43BSD_PHY_N_H_

#include <sys/param.h>

struct b43bsd_softc;

/*
 * PHY-N calibration and state data.
 */
struct b43bsd_phy_n {
	/* TX gain tables (per-core, per-chain). */
	uint16_t	tx_gain[2][4];		/* [core0/1][chain] */

	/* RX gain tables. */
	uint16_t	rx_gain[2][4];

	/* Filter coefficients for RX path. */
	uint16_t	filter_coeff[32];

	/* Calibration state. */
	int		calibrated;
	int		tx_iq_cal_done;
	int		rx_iq_cal_done;

	/* Antenna selection state. */
	int		tx_chain_mask;		/* active TX chains */
	int		rx_chain_mask;		/* active RX chains */

	/* Current channel parameters. */
	int		current_channel;
	int		current_bw_40mhz;
	int		current_is_5ghz;

	/* TSSI (Transmit Signal Strength Indication). */
	uint16_t	tssi[2][4];

	/* Noise floor (per core). */
	int16_t		noise[2];

	/* Energy detect thresholds. */
	uint16_t	ed_thresh[2];

	/* CCA (Clear Channel Assessment) state. */
	int		cca_override;
	int		cca_override_value;

	/* Band-specific init state. */
	int		band_init_done;
};

/* ------------------------------------------------------------------ */
/* N-PHY Complete Register Address Map (0x000-0x2FF)                     */
/*                                                                      */
/* This covers every register in the N-PHY address space needed by      */
/* the calibration, channel switching, and MIMO control functions.      */
/*                                                                      */
/* Ported from Linux drivers/net/wireless/broadcom/b43/phy_n.h         */
/* ------------------------------------------------------------------ */

/* ---- PHY Core Identification (0x000-0x00F) ---- */
#define B43BSD_NPHY_REVISION		B43BSD_PHY_N(0x000)
#define B43BSD_NPHY_BBCFG		B43BSD_PHY_N(0x001)
#define B43BSD_NPHY_CORECTL		B43BSD_PHY_N(0x002)
#define B43BSD_NPHY_BANDCFG		B43BSD_PHY_N(0x003)
#define B43BSD_NPHY_ANTCFG		B43BSD_PHY_N(0x004)
#define B43BSD_NPHY_CHANNEL		B43BSD_PHY_N(0x005)
#define B43BSD_NPHY_CLKCTL		B43BSD_PHY_N(0x006)
#define B43BSD_NPHY_TXERR		B43BSD_PHY_N(0x007)
#define B43BSD_NPHY_PWRDET		B43BSD_PHY_N(0x008)
#define B43BSD_NPHY_BANDCTL		B43BSD_PHY_N(0x009)
#define B43BSD_NPHY_4WI_ADDR		B43BSD_PHY_N(0x00B)
#define B43BSD_NPHY_4WI_DATAHI		B43BSD_PHY_N(0x00C)
#define B43BSD_NPHY_4WI_DATALO		B43BSD_PHY_N(0x00D)

/* ---- Core 1 RX/TX (0x010-0x02D) ---- */
#define B43BSD_NPHY_C1_CRSGAIN		B43BSD_PHY_N(0x010)
#define B43BSD_NPHY_C1_FREQEST		B43BSD_PHY_N(0x011)
#define B43BSD_NPHY_C1_SYNTHCTL		B43BSD_PHY_N(0x012)
#define B43BSD_NPHY_C1_CFOSET		B43BSD_PHY_N(0x013)
#define B43BSD_NPHY_C1_TIMING		B43BSD_PHY_N(0x014)
#define B43BSD_NPHY_C1_DESPWR		B43BSD_PHY_N(0x018)
#define B43BSD_NPHY_C1_BCLIPBKOFF	B43BSD_PHY_N(0x01A)
#define B43BSD_NPHY_C1_CGAINI		B43BSD_PHY_N(0x01C)
#define B43BSD_NPHY_C1_MINMAX_GAIN	B43BSD_PHY_N(0x01E)
#define B43BSD_NPHY_C1_INITGAIN		B43BSD_PHY_N(0x020)
#define B43BSD_NPHY_C1_CLIP1_HIGAIN	B43BSD_PHY_N(0x021)
#define B43BSD_NPHY_C1_CLIP1_MEDGAIN	B43BSD_PHY_N(0x022)
#define B43BSD_NPHY_C1_CLIP1_LOGAIN	B43BSD_PHY_N(0x023)
#define B43BSD_NPHY_C1_CLIP2_GAIN	B43BSD_PHY_N(0x024)
#define B43BSD_NPHY_C1_FILTERGAIN	B43BSD_PHY_N(0x025)
#define B43BSD_NPHY_C1_CLIPWBTHRES	B43BSD_PHY_N(0x027)
#define B43BSD_NPHY_C1_EDTHRES		B43BSD_PHY_N(0x029)
#define B43BSD_NPHY_C1_NBCLIPTHRES	B43BSD_PHY_N(0x02B)
#define B43BSD_NPHY_C1_CLIP1THRES	B43BSD_PHY_N(0x02C)
#define B43BSD_NPHY_C1_CLIP2THRES	B43BSD_PHY_N(0x02D)

/* ---- Core 2 RX/TX (0x02E-0x045) ---- */
#define B43BSD_NPHY_C2_DESPWR		B43BSD_PHY_N(0x02E)
#define B43BSD_NPHY_C2_BCLIPBKOFF	B43BSD_PHY_N(0x030)
#define B43BSD_NPHY_C2_CGAINI		B43BSD_PHY_N(0x032)
#define B43BSD_NPHY_C2_MINMAX_GAIN	B43BSD_PHY_N(0x034)
#define B43BSD_NPHY_C2_INITGAIN		B43BSD_PHY_N(0x036)
#define B43BSD_NPHY_C2_CLIP1_HIGAIN	B43BSD_PHY_N(0x037)
#define B43BSD_NPHY_C2_CLIP1_MEDGAIN	B43BSD_PHY_N(0x038)
#define B43BSD_NPHY_C2_CLIP1_LOGAIN	B43BSD_PHY_N(0x039)
#define B43BSD_NPHY_C2_CLIP2_GAIN	B43BSD_PHY_N(0x03A)
#define B43BSD_NPHY_C2_FILTERGAIN	B43BSD_PHY_N(0x03B)
#define B43BSD_NPHY_C2_CLIPWBTHRES	B43BSD_PHY_N(0x03D)
#define B43BSD_NPHY_C2_EDTHRES		B43BSD_PHY_N(0x03F)
#define B43BSD_NPHY_C2_NBCLIPTHRES	B43BSD_PHY_N(0x041)
#define B43BSD_NPHY_C2_CLIP1THRES	B43BSD_PHY_N(0x042)
#define B43BSD_NPHY_C2_CLIP2THRES	B43BSD_PHY_N(0x043)

/* ---- Core 3 RX/TX (0x046-0x063) ---- */
#define B43BSD_NPHY_C3_INITGAIN		B43BSD_PHY_N(0x046)
#define B43BSD_NPHY_CRSCTL		B43BSD_PHY_N(0x047)
#define B43BSD_NPHY_RSSIMULT		B43BSD_PHY_N(0x049)
#define B43BSD_NPHY_C3_CLIPWBTHRES	B43BSD_PHY_N(0x04A)
#define B43BSD_NPHY_C3_EDTHRES		B43BSD_PHY_N(0x04C)
#define B43BSD_NPHY_C3_NBCLIPTHRES	B43BSD_PHY_N(0x04E)
#define B43BSD_NPHY_C3_CLIP1THRES	B43BSD_PHY_N(0x050)
#define B43BSD_NPHY_C3_CLIP2THRES	B43BSD_PHY_N(0x052)
#define B43BSD_NPHY_C3_FILTERGAIN	B43BSD_PHY_N(0x054)
#define B43BSD_NPHY_C3_CLIP1_HIGAIN	B43BSD_PHY_N(0x056)
#define B43BSD_NPHY_C3_CLIP1_MEDGAIN	B43BSD_PHY_N(0x058)
#define B43BSD_NPHY_C3_CLIP2_GAIN	B43BSD_PHY_N(0x05A)
#define B43BSD_NPHY_C3_CGAINI		B43BSD_PHY_N(0x05C)
#define B43BSD_NPHY_C3_DESPWR		B43BSD_PHY_N(0x05E)

/* ---- Filter Coefficients (0x064-0x070) ---- */
#define B43BSD_NPHY_RXF20_NUM0		B43BSD_PHY_N(0x064)
#define B43BSD_NPHY_RXF20_NUM1		B43BSD_PHY_N(0x065)
#define B43BSD_NPHY_RXF20_NUM2		B43BSD_PHY_N(0x066)
#define B43BSD_NPHY_RXF20_DENOM0	B43BSD_PHY_N(0x067)
#define B43BSD_NPHY_RXF20_DENOM1	B43BSD_PHY_N(0x068)

/* ---- Table Access (0x072-0x076) ---- */
#define B43BSD_NPHY_TABLE_ADDR		B43BSD_PHY_N(0x072)
#define B43BSD_NPHY_TABLE_DATALO	B43BSD_PHY_N(0x073)
#define B43BSD_NPHY_TABLE_DATAHI	B43BSD_PHY_N(0x074)

/* ---- RF Control (0x078-0x09F) ---- */
#define B43BSD_NPHY_RFCTL_CMD		B43BSD_PHY_N(0x078)
#define B43BSD_NPHY_RFCTL_RSSIO1	B43BSD_PHY_N(0x07A)
#define B43BSD_NPHY_RFCTL_RXG1		B43BSD_PHY_N(0x07B)
#define B43BSD_NPHY_RFCTL_TXG1		B43BSD_PHY_N(0x07C)
#define B43BSD_NPHY_RFCTL_RSSIO2	B43BSD_PHY_N(0x07D)
#define B43BSD_NPHY_RFCTL_RXG2		B43BSD_PHY_N(0x07E)
#define B43BSD_NPHY_RFCTL_TXG2		B43BSD_PHY_N(0x07F)
#define B43BSD_NPHY_RFCTL_RSSIO3	B43BSD_PHY_N(0x080)
#define B43BSD_NPHY_RFCTL_RXG3		B43BSD_PHY_N(0x081)
#define B43BSD_NPHY_RFCTL_TXG3		B43BSD_PHY_N(0x082)
#define B43BSD_NPHY_RFCTL_RSSIO4	B43BSD_PHY_N(0x083)
#define B43BSD_NPHY_RFCTL_RXG4		B43BSD_PHY_N(0x084)
#define B43BSD_NPHY_RFCTL_TXG4		B43BSD_PHY_N(0x085)
#define B43BSD_NPHY_C1_TXIQ_COMP_OFF	B43BSD_PHY_N(0x087)
#define B43BSD_NPHY_C2_TXIQ_COMP_OFF	B43BSD_PHY_N(0x088)
#define B43BSD_NPHY_C1_TXCTL		B43BSD_PHY_N(0x08B)
#define B43BSD_NPHY_C2_TXCTL		B43BSD_PHY_N(0x08C)
#define B43BSD_NPHY_RFCTL_INTC1		B43BSD_PHY_N(0x091)
#define B43BSD_NPHY_RFCTL_INTC2		B43BSD_PHY_N(0x092)
#define B43BSD_NPHY_RFCTL_INTC3		B43BSD_PHY_N(0x093)
#define B43BSD_NPHY_RFCTL_INTC4		B43BSD_PHY_N(0x094)
#define B43BSD_NPHY_C1_RXIQ_COMPA0	B43BSD_PHY_N(0x09A)
#define B43BSD_NPHY_C1_RXIQ_COMPB0	B43BSD_PHY_N(0x09B)
#define B43BSD_NPHY_C2_RXIQ_COMPA1	B43BSD_PHY_N(0x09C)
#define B43BSD_NPHY_C2_RXIQ_COMPB1	B43BSD_PHY_N(0x09D)

/* ---- RX/TX/Classifier (0x0A0-0x0BF) ---- */
#define B43BSD_NPHY_RXCTL		B43BSD_PHY_N(0x0A0)
#define B43BSD_NPHY_RFSEQMODE		B43BSD_PHY_N(0x0A1)
#define B43BSD_NPHY_RFSEQCA		B43BSD_PHY_N(0x0A2)
#define B43BSD_NPHY_RFSEQTR		B43BSD_PHY_N(0x0A3)
#define B43BSD_NPHY_RFSEQST		B43BSD_PHY_N(0x0A4)
#define B43BSD_NPHY_AFECTL_OVER		B43BSD_PHY_N(0x0A5)
#define B43BSD_NPHY_AFECTL_C1		B43BSD_PHY_N(0x0A6)
#define B43BSD_NPHY_AFECTL_C2		B43BSD_PHY_N(0x0A7)
#define B43BSD_NPHY_AFECTL_DACGAIN1	B43BSD_PHY_N(0x0AA)
#define B43BSD_NPHY_AFECTL_DACGAIN2	B43BSD_PHY_N(0x0AB)
#define B43BSD_NPHY_CLASSCTL		B43BSD_PHY_N(0x0B0)
#define B43BSD_NPHY_IQFLIP		B43BSD_PHY_N(0x0B1)
#define B43BSD_NPHY_SMPSMODE		B43BSD_PHY_N(0x0B2)
#define B43BSD_NPHY_MLPARM		B43BSD_PHY_N(0x0B6)
#define B43BSD_NPHY_MLCTL		B43BSD_PHY_N(0x0B7)
#define B43BSD_NPHY_NOTCH1		B43BSD_PHY_N(0x0B8)
#define B43BSD_NPHY_NOTCH2		B43BSD_PHY_N(0x0B9)

/* ---- IQ Calibration Engine (0x0C0-0x0CF) ---- */
#define B43BSD_NPHY_IQLOCAL_CMD		B43BSD_PHY_N(0x0C0)
#define B43BSD_NPHY_IQLOCAL_CMDNNUM	B43BSD_PHY_N(0x0C1)
#define B43BSD_NPHY_IQLOCAL_CMDGCTL	B43BSD_PHY_N(0x0C2)
#define B43BSD_NPHY_SAMP_CMD		B43BSD_PHY_N(0x0C3)
#define B43BSD_NPHY_SAMP_LOOPCNT	B43BSD_PHY_N(0x0C4)
#define B43BSD_NPHY_SAMP_WAITCNT	B43BSD_PHY_N(0x0C5)
#define B43BSD_NPHY_SAMP_STAT		B43BSD_PHY_N(0x0C7)

/* ---- TX BB Multipliers (0x0DE-0x0E1) ---- */
#define B43BSD_NPHY_C1_TXBBMULT		B43BSD_PHY_N(0x0DE)
#define B43BSD_NPHY_C2_TXBBMULT		B43BSD_PHY_N(0x0DF)
#define B43BSD_NPHY_C3_TXBBMULT		B43BSD_PHY_N(0x0E0)

/* ---- MIMO Configuration (0x0ED-0x0EF) ---- */
#define B43BSD_NPHY_MIMOCFG		B43BSD_PHY_N(0x0ED)
#define B43BSD_NPHY_WIRLBKGAIN		B43BSD_PHY_N(0x0EE)
#define B43BSD_NPHY_RFCTL_OVER		B43BSD_PHY_N(0x0EC)

/* ---- PLL / Synthesizer (0x11C-0x128) ---- */
#define B43BSD_NPHY_PLL_REF		B43BSD_PHY_N(0x11C)
#define B43BSD_NPHY_PLL_LOOPFILTER1	B43BSD_PHY_N(0x11D)
#define B43BSD_NPHY_PLL_LOOPFILTER2	B43BSD_PHY_N(0x11E)
#define B43BSD_NPHY_PLL_LOOPFILTER3	B43BSD_PHY_N(0x11F)
#define B43BSD_NPHY_PLL_CP2		B43BSD_PHY_N(0x120)
#define B43BSD_NPHY_PLL_VCO1		B43BSD_PHY_N(0x124)
#define B43BSD_NPHY_PLL_VCO2		B43BSD_PHY_N(0x125)
#define B43BSD_NPHY_PLL_DIV_INT		B43BSD_PHY_N(0x126)
#define B43BSD_NPHY_PLL_DIV_FRAC	B43BSD_PHY_N(0x127)
#define B43BSD_NPHY_PLL_MMD_DIV		B43BSD_PHY_N(0x128)

/* ---- IQ Estimation (0x129-0x12C) ---- */
#define B43BSD_NPHY_IQEST_CMD		B43BSD_PHY_N(0x129)
#define B43BSD_NPHY_IQEST_WT		B43BSD_PHY_N(0x12A)
#define B43BSD_NPHY_IQEST_SAMPLES	B43BSD_PHY_N(0x12B)

/* ---- Power Detection (0x13B-0x13C) ---- */
#define B43BSD_NPHY_PWRDET1		B43BSD_PHY_N(0x13B)
#define B43BSD_NPHY_PWRDET2		B43BSD_PHY_N(0x13C)

/* ---- Frequency/Phase Tracking (0x150-0x15F) ---- */
#define B43BSD_NPHY_FREQ_EST_C1		B43BSD_PHY_N(0x150)
#define B43BSD_NPHY_PHASE_TRACK_C1	B43BSD_PHY_N(0x154)
#define B43BSD_NPHY_CFO_EST_C1		B43BSD_PHY_N(0x158)
#define B43BSD_NPHY_EVM_EST_C1		B43BSD_PHY_N(0x15A)
#define B43BSD_NPHY_FOFFSET_C1		B43BSD_PHY_N(0x15C)
#define B43BSD_NPHY_XTAL_TRIM		B43BSD_PHY_N(0x15E)

/* ---- Bandwidth Control (0x1CE-0x1D6) ---- */
#define B43BSD_NPHY_BW1A		B43BSD_PHY_N(0x1CE)
#define B43BSD_NPHY_BW2			B43BSD_PHY_N(0x1CF)
#define B43BSD_NPHY_BW3			B43BSD_PHY_N(0x1D0)
#define B43BSD_NPHY_BW4			B43BSD_PHY_N(0x1D1)
#define B43BSD_NPHY_BW5			B43BSD_PHY_N(0x1D2)
#define B43BSD_NPHY_BW6			B43BSD_PHY_N(0x1D3)
#define B43BSD_NPHY_BW7			B43BSD_PHY_N(0x1D4)
#define B43BSD_NPHY_BW40_CTL		B43BSD_PHY_N(0x1D7)

/* ---- TX Power Control (0x1E7-0x1EE, 0x222-0x232) ---- */
#define B43BSD_NPHY_TXPCTL_CMD		B43BSD_PHY_N(0x1E7)
#define B43BSD_NPHY_TXPCTL_N		B43BSD_PHY_N(0x1E8)
#define B43BSD_NPHY_TXPCTL_ITSSI	B43BSD_PHY_N(0x1E9)
#define B43BSD_NPHY_TXPCTL_TPWR		B43BSD_PHY_N(0x1EA)
#define B43BSD_NPHY_TXPCTL_PIDX		B43BSD_PHY_N(0x1EC)
#define B43BSD_NPHY_C1_TXPCTL_STAT	B43BSD_PHY_N(0x1ED)
#define B43BSD_NPHY_TXPCTL_INIT		B43BSD_PHY_N(0x222)
#define B43BSD_NPHY_TXPWRCTRLDAMPING	B43BSD_PHY_N(0x296)
#define B43BSD_NPHY_C1_TXPCTL_PWR	B43BSD_PHY_N(0x22D)
#define B43BSD_NPHY_C2_TXPCTL_PWR	B43BSD_PHY_N(0x22E)
#define B43BSD_NPHY_C3_TXPCTL_PWR	B43BSD_PHY_N(0x22F)
#define B43BSD_NPHY_C1_TXPCTL_GAINIDX	B43BSD_PHY_N(0x230)
#define B43BSD_NPHY_C2_TXPCTL_GAINIDX	B43BSD_PHY_N(0x231)
#define B43BSD_NPHY_C3_TXPCTL_GAINIDX	B43BSD_PHY_N(0x232)

/* ---- RSSI (0x219-0x21B) ---- */
#define B43BSD_NPHY_RSSI1		B43BSD_PHY_N(0x219)
#define B43BSD_NPHY_RSSI2		B43BSD_PHY_N(0x21A)

/* ---- Antenna Selection (0x244-0x24C) ---- */
#define B43BSD_NPHY_TXANTSWLUT		B43BSD_PHY_N(0x244)
#define B43BSD_NPHY_ANTENNADIVDWELLTIME	B43BSD_PHY_N(0x246)
#define B43BSD_NPHY_ANTENNADIVBACKOFFGAIN B43BSD_PHY_N(0x248)
#define B43BSD_NPHY_ANTENNADIVMINGAIN	B43BSD_PHY_N(0x249)
#define B43BSD_NPHY_RXANTSWITCHCTRL	B43BSD_PHY_N(0x24B)

/* ---- MIMO Channel Matrix (0x250-0x265) ---- */
#define B43BSD_NPHY_CHANMAT_BASE	B43BSD_PHY_N(0x250)
#define B43BSD_NPHY_CHANMAT_COND	B43BSD_PHY_N(0x262)
#define B43BSD_NPHY_CHANMAT_CONDTHRESH	B43BSD_PHY_N(0x263)

/* ---- MRC Weights (0x270-0x27F) ---- */
#define B43BSD_NPHY_MRC_WEIGHT0		B43BSD_PHY_N(0x270)
#define B43BSD_NPHY_MRC_WEIGHT1		B43BSD_PHY_N(0x271)
#define B43BSD_NPHY_MRC_WEIGHT2		B43BSD_PHY_N(0x272)

/* ---- PAPD (0x297-0x29A, 0x2A0+) ---- */
#define B43BSD_NPHY_PAPD_EN0		B43BSD_PHY_N(0x297)
#define B43BSD_NPHY_PAPD_EN1		B43BSD_PHY_N(0x298)
#define B43BSD_NPHY_PAPD_EN2		B43BSD_PHY_N(0x299)
#define B43BSD_NPHY_PAPD_TRAIN		B43BSD_PHY_N(0x29A)
#define B43BSD_NPHY_PAPD_MCS_BASE	B43BSD_PHY_N(0x2A0)

/* ---- Digital Pre-Distortion (0x2B0-0x2BF) ---- */
#define B43BSD_NPHY_DPD_CTL0		B43BSD_PHY_N(0x2B0)
#define B43BSD_NPHY_DPD_CTL1		B43BSD_PHY_N(0x2B1)
#define B43BSD_NPHY_DPD_CTL2		B43BSD_PHY_N(0x2B2)
#define B43BSD_NPHY_DPD_TRAIN		B43BSD_PHY_N(0x2B4)
#define B43BSD_NPHY_DPD_ERR_I		B43BSD_PHY_N(0x2B6)
#define B43BSD_NPHY_DPD_ERR_Q		B43BSD_PHY_N(0x2B7)
#define B43BSD_NPHY_DPD_COEFF_I0	B43BSD_PHY_N(0x2B8)
#define B43BSD_NPHY_DPD_COEFF_Q0	B43BSD_PHY_N(0x2B9)

/* ---- PA Protection (0x2D0-0x2DF) ---- */
#define B43BSD_NPHY_PA_PWR_THRESH0	B43BSD_PHY_N(0x2D0)
#define B43BSD_NPHY_PA_TEMP_THRESH0	B43BSD_PHY_N(0x2D1)
#define B43BSD_NPHY_PA_PWR_THRESH1	B43BSD_PHY_N(0x2D2)
#define B43BSD_NPHY_PA_TEMP_THRESH1	B43BSD_PHY_N(0x2D3)
#define B43BSD_NPHY_PA_PWR_THRESH2	B43BSD_PHY_N(0x2D4)
#define B43BSD_NPHY_PA_TEMP_THRESH2	B43BSD_PHY_N(0x2D5)
#define B43BSD_NPHY_PA_PROT_EN		B43BSD_PHY_N(0x2D6)
#define B43BSD_NPHY_PA_BIAS_OVERRIDE	B43BSD_PHY_N(0x2D8)

/* ---- Beamforming (0x1E0-0x1E5, 0x200-0x23F) ---- */
#define B43BSD_NPHY_BF_SOUND_CHAIN	B43BSD_PHY_N(0x1E0)
#define B43BSD_NPHY_BF_SOUND_EN		B43BSD_PHY_N(0x1E1)
#define B43BSD_NPHY_BF_SOUND_STAT	B43BSD_PHY_N(0x1E2)
#define B43BSD_NPHY_BF_SV_I		B43BSD_PHY_N(0x1E4)
#define B43BSD_NPHY_BF_SV_Q		B43BSD_PHY_N(0x1E5)
#define B43BSD_NPHY_BF_MATRIX_BASE	B43BSD_PHY_N(0x200)

/* ---- Interference Detection (0x148-0x14B) ---- */
#define B43BSD_NPHY_WB_ENERGY		B43BSD_PHY_N(0x148)
#define B43BSD_NPHY_NB_ENERGY		B43BSD_PHY_N(0x149)
#define B43BSD_NPHY_BT_ENERGY		B43BSD_PHY_N(0x14A)
#define B43BSD_NPHY_RADAR_ENERGY	B43BSD_PHY_N(0x14B)

/* ---- Band Control bits ---- */
#define B43BSD_NPHY_BANDCTL_5GHZ	0x0001
#define B43BSD_NPHY_BANDCTL_DACLPF	0x0002

/* ---- RF Sequence States ---- */
#define B43BSD_NPHY_RFSEQST_DISABLED	0
#define B43BSD_NPHY_RFSEQST_TX2RX	1
#define B43BSD_NPHY_RFSEQST_RX2TX	2
#define B43BSD_NPHY_RFSEQST_RXWAIT	3

/* ---- BCM2056 Synthesizer Bank Addresses ---- */
#define B2056_SYN		0x0000
#define B2056_TX0		0x2000
#define B2056_TX1		0x4000
#define B2056_RX0		0x8000
#define B2056_RX1		0xA000

/* ------------------------------------------------------------------ */
/* PHY-N API                                                           */
/* ------------------------------------------------------------------ */

int	b43bsd_phy_n_attach(struct b43bsd_softc *);
void	b43bsd_phy_n_init(struct b43bsd_softc *);
int	b43bsd_phy_n_switch_channel(struct b43bsd_softc *, int);
void	b43bsd_phy_n_set_bw(struct b43bsd_softc *, int, int);
int	b43bsd_phy_n_set_antenna(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_txpower(struct b43bsd_softc *, int);
int	b43bsd_phy_n_get_rssi(struct b43bsd_softc *, int);
int	b43bsd_phy_n_tx_iq_cal(struct b43bsd_softc *);
int	b43bsd_phy_n_rx_iq_cal(struct b43bsd_softc *);
int	b43bsd_phy_n_cca(struct b43bsd_softc *);

/* ------------------------------------------------------------------ */
/* PHY Register Address Space                                          */
/* ------------------------------------------------------------------ */

/* PHY register address space — mapped through SSB core base. */
#define B43BSD_PHY_N(offset)	((offset) & 0x0fff)

/* ------------------------------------------------------------------ */
/* Core Configuration & Global Control                                 */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_BBCFG		B43BSD_PHY_N(0x001)
#define B43BSD_NPHY_CHANNEL		B43BSD_PHY_N(0x005)
#define B43BSD_NPHY_TXERR		B43BSD_PHY_N(0x007)
#define B43BSD_NPHY_BANDCTL		B43BSD_PHY_N(0x009)
#define B43BSD_NPHY_CRSCTL		B43BSD_PHY_N(0x047)
#define B43BSD_NPHY_MIMOCFG		B43BSD_PHY_N(0x0ED)
#define B43BSD_NPHY_RXCTL		B43BSD_PHY_N(0x0A0)
#define B43BSD_NPHY_CLASSCTL		B43BSD_PHY_N(0x0B0)
#define B43BSD_NPHY_IQFLIP		B43BSD_PHY_N(0x0B1)
#define B43BSD_NPHY_MLCTL		B43BSD_PHY_N(0x0B7)
#define B43BSD_NPHY_MLPARM		B43BSD_PHY_N(0x0B6)

/* ------------------------------------------------------------------ */
/* Four-Wire Bus (Radio Register Access)                               */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_4WI_ADDR		B43BSD_PHY_N(0x00B)
#define B43BSD_NPHY_4WI_DATAHI		B43BSD_PHY_N(0x00C)
#define B43BSD_NPHY_4WI_DATALO		B43BSD_PHY_N(0x00D)

/* ------------------------------------------------------------------ */
/* RF Control Registers                                                */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_RFCTL_CMD		B43BSD_PHY_N(0x078)
#define B43BSD_NPHY_RFCTL_RSSIO1	B43BSD_PHY_N(0x07A)
#define B43BSD_NPHY_RFCTL_RXG1		B43BSD_PHY_N(0x07B)
#define B43BSD_NPHY_RFCTL_TXG1		B43BSD_PHY_N(0x07C)
#define B43BSD_NPHY_RFCTL_RSSIO2	B43BSD_PHY_N(0x07D)
#define B43BSD_NPHY_RFCTL_RXG2		B43BSD_PHY_N(0x07E)
#define B43BSD_NPHY_RFCTL_TXG2		B43BSD_PHY_N(0x07F)
#define B43BSD_NPHY_RFCTL_RSSIO3	B43BSD_PHY_N(0x080)
#define B43BSD_NPHY_RFCTL_RXG3		B43BSD_PHY_N(0x081)
#define B43BSD_NPHY_RFCTL_TXG3		B43BSD_PHY_N(0x082)
#define B43BSD_NPHY_RFCTL_RSSIO4	B43BSD_PHY_N(0x083)
#define B43BSD_NPHY_RFCTL_RXG4		B43BSD_PHY_N(0x084)
#define B43BSD_NPHY_RFCTL_TXG4		B43BSD_PHY_N(0x085)
#define B43BSD_NPHY_RFCTL_INTC1		B43BSD_PHY_N(0x091)
#define B43BSD_NPHY_RFCTL_INTC2		B43BSD_PHY_N(0x092)
#define B43BSD_NPHY_RFCTL_INTC3		B43BSD_PHY_N(0x093)
#define B43BSD_NPHY_RFCTL_INTC4		B43BSD_PHY_N(0x094)
#define B43BSD_NPHY_RFCTL_OVER		B43BSD_PHY_N(0x0EC)
#define B43BSD_NPHY_RFSEQMODE		B43BSD_PHY_N(0x0A1)
#define B43BSD_NPHY_RFSEQCA		B43BSD_PHY_N(0x0A2)
#define B43BSD_NPHY_RFSEQTR		B43BSD_PHY_N(0x0A3)
#define B43BSD_NPHY_RFSEQST		B43BSD_PHY_N(0x0A4)

/* RF sequence state */
#define B43BSD_NPHY_RFSEQST_DISABLED	0
#define B43BSD_NPHY_RFSEQST_TX2RX	1
#define B43BSD_NPHY_RFSEQST_RX2TX	2
#define B43BSD_NPHY_RFSEQST_RXWAIT	3

/* ------------------------------------------------------------------ */
/* TX/RX Gain Control — Core 1                                         */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_C1_DESPWR		B43BSD_PHY_N(0x018)
#define B43BSD_NPHY_C1_BCLIPBKOFF	B43BSD_PHY_N(0x01A)
#define B43BSD_NPHY_C1_CGAINI		B43BSD_PHY_N(0x01C)
#define B43BSD_NPHY_C1_MINMAX_GAIN	B43BSD_PHY_N(0x01E)
#define B43BSD_NPHY_C1_INITGAIN		B43BSD_PHY_N(0x020)
#define B43BSD_NPHY_C1_CLIP1_HIGAIN	B43BSD_PHY_N(0x021)
#define B43BSD_NPHY_C1_CLIP1_MEDGAIN	B43BSD_PHY_N(0x022)
#define B43BSD_NPHY_C1_CLIP1_LOGAIN	B43BSD_PHY_N(0x023)
#define B43BSD_NPHY_C1_CLIP2_GAIN	B43BSD_PHY_N(0x024)
#define B43BSD_NPHY_C1_FILTERGAIN	B43BSD_PHY_N(0x025)
#define B43BSD_NPHY_C1_CLIPWBTHRES	B43BSD_PHY_N(0x027)
#define B43BSD_NPHY_C1_EDTHRES		B43BSD_PHY_N(0x029)
#define B43BSD_NPHY_C1_NBCLIPTHRES	B43BSD_PHY_N(0x02B)
#define B43BSD_NPHY_C1_CLIP1THRES	B43BSD_PHY_N(0x02C)
#define B43BSD_NPHY_C1_CLIP2THRES	B43BSD_PHY_N(0x02D)
#define B43BSD_NPHY_C1_TXBBMULT		B43BSD_PHY_N(0x0DE)

/* ------------------------------------------------------------------ */
/* TX/RX Gain Control — Core 2                                         */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_C2_DESPWR		B43BSD_PHY_N(0x02E)
#define B43BSD_NPHY_C2_BCLIPBKOFF	B43BSD_PHY_N(0x030)
#define B43BSD_NPHY_C2_CGAINI		B43BSD_PHY_N(0x032)
#define B43BSD_NPHY_C2_MINMAX_GAIN	B43BSD_PHY_N(0x034)
#define B43BSD_NPHY_C2_INITGAIN		B43BSD_PHY_N(0x036)
#define B43BSD_NPHY_C2_CLIP1_HIGAIN	B43BSD_PHY_N(0x037)
#define B43BSD_NPHY_C2_CLIP1_MEDGAIN	B43BSD_PHY_N(0x038)
#define B43BSD_NPHY_C2_CLIP1_LOGAIN	B43BSD_PHY_N(0x039)
#define B43BSD_NPHY_C2_CLIP2_GAIN	B43BSD_PHY_N(0x03A)
#define B43BSD_NPHY_C2_FILTERGAIN	B43BSD_PHY_N(0x03B)
#define B43BSD_NPHY_C2_CLIPWBTHRES	B43BSD_PHY_N(0x03D)
#define B43BSD_NPHY_C2_EDTHRES		B43BSD_PHY_N(0x03F)
#define B43BSD_NPHY_C2_NBCLIPTHRES	B43BSD_PHY_N(0x041)
#define B43BSD_NPHY_C2_CLIP1THRES	B43BSD_PHY_N(0x042)
#define B43BSD_NPHY_C2_CLIP2THRES	B43BSD_PHY_N(0x043)
#define B43BSD_NPHY_C2_TXBBMULT		B43BSD_PHY_N(0x0DF)

/* ------------------------------------------------------------------ */
/* TX/RX Gain Control — Core 3                                         */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_C2_TXIQ_COMP_OFF	B43BSD_PHY_N(0x088)
#define B43BSD_NPHY_C1_RXIQ_COMPA0	B43BSD_PHY_N(0x09A)
#define B43BSD_NPHY_C1_RXIQ_COMPB0	B43BSD_PHY_N(0x09B)
#define B43BSD_NPHY_C2_RXIQ_COMPA1	B43BSD_PHY_N(0x09C)
#define B43BSD_NPHY_C2_RXIQ_COMPB1	B43BSD_PHY_N(0x09D)
#define B43BSD_NPHY_IQLOCAL_CMD		B43BSD_PHY_N(0x0C0)
#define B43BSD_NPHY_IQLOCAL_CMDNNUM	B43BSD_PHY_N(0x0C1)
#define B43BSD_NPHY_IQLOCAL_CMDGCTL	B43BSD_PHY_N(0x0C2)
#define B43BSD_NPHY_IQEST_CMD		B43BSD_PHY_N(0x129)
#define B43BSD_NPHY_IQEST_WT		B43BSD_PHY_N(0x12A)

/* ------------------------------------------------------------------ */
/* Bandwidth Control                                                   */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_BW1A		B43BSD_PHY_N(0x1CE)
#define B43BSD_NPHY_BW2			B43BSD_PHY_N(0x1CF)
#define B43BSD_NPHY_BW3			B43BSD_PHY_N(0x1D0)
#define B43BSD_NPHY_BW4			B43BSD_PHY_N(0x1D1)
#define B43BSD_NPHY_BW5			B43BSD_PHY_N(0x1D2)
#define B43BSD_NPHY_BW6			B43BSD_PHY_N(0x1D3)

/* ------------------------------------------------------------------ */
/* Antenna Selection                                                   */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_ANTENNADIVDWELLTIME	B43BSD_PHY_N(0x246)
#define B43BSD_NPHY_ANTENNADIVBACKOFFGAIN B43BSD_PHY_N(0x248)
#define B43BSD_NPHY_ANTENNADIVMINGAIN	B43BSD_PHY_N(0x249)
#define B43BSD_NPHY_RXANTSWITCHCTRL	B43BSD_PHY_N(0x24B)
#define B43BSD_NPHY_TXANTSWLUT		B43BSD_PHY_N(0x244)

/* ------------------------------------------------------------------ */
/* TX Power Control                                                    */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_TXPCTL_CMD		B43BSD_PHY_N(0x1E7)
#define B43BSD_NPHY_TXPCTL_N		B43BSD_PHY_N(0x1E8)
#define B43BSD_NPHY_TXPCTL_ITSSI	B43BSD_PHY_N(0x1E9)
#define B43BSD_NPHY_TXPCTL_TPWR		B43BSD_PHY_N(0x1EA)
#define B43BSD_NPHY_TXPCTL_BIDX		B43BSD_PHY_N(0x1EB)
#define B43BSD_NPHY_TXPCTL_PIDX		B43BSD_PHY_N(0x1EC)
#define B43BSD_NPHY_C1_TXPCTL_STAT	B43BSD_PHY_N(0x1ED)
#define B43BSD_NPHY_C2_TXPCTL_STAT	B43BSD_PHY_N(0x1EE)
#define B43BSD_NPHY_TXPCTL_INIT		B43BSD_PHY_N(0x222)
#define B43BSD_NPHY_TXPWRCTRLDAMPING	B43BSD_PHY_N(0x296)

/* ------------------------------------------------------------------ */
/* RSSI & Power Detection                                              */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_RSSI1		B43BSD_PHY_N(0x219)
#define B43BSD_NPHY_RSSI2		B43BSD_PHY_N(0x21A)
#define B43BSD_NPHY_PWRDET1		B43BSD_PHY_N(0x13B)
#define B43BSD_NPHY_PWRDET2		B43BSD_PHY_N(0x13C)

/* ------------------------------------------------------------------ */
/* Table Access                                                        */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_TABLE_ADDR		B43BSD_PHY_N(0x072)
#define B43BSD_NPHY_TABLE_DATALO	B43BSD_PHY_N(0x073)
#define B43BSD_NPHY_TABLE_DATAHI	B43BSD_PHY_N(0x074)
#define B43BSD_NPHY_DCFADDR		B43BSD_PHY_N(0x048)

/* ------------------------------------------------------------------ */
/* AFE Control                                                         */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_AFECTL_OVER		B43BSD_PHY_N(0x0A5)
#define B43BSD_NPHY_AFECTL_C1		B43BSD_PHY_N(0x0A6)
#define B43BSD_NPHY_AFECTL_C2		B43BSD_PHY_N(0x0A7)
#define B43BSD_NPHY_AFECTL_DACGAIN1	B43BSD_PHY_N(0x0AA)
#define B43BSD_NPHY_AFECTL_DACGAIN2	B43BSD_PHY_N(0x0AB)

/* ------------------------------------------------------------------ */
/* Sample Engine                                                       */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_SAMP_CMD		B43BSD_PHY_N(0x0C3)
#define B43BSD_NPHY_SAMP_LOOPCNT	B43BSD_PHY_N(0x0C4)
#define B43BSD_NPHY_SAMP_WAITCNT	B43BSD_PHY_N(0x0C5)
#define B43BSD_NPHY_SAMP_STAT		B43BSD_PHY_N(0x0C7)

/* ------------------------------------------------------------------ */
/* RX Control                                                          */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_RXCTL		B43BSD_PHY_N(0x0A0)

/* ------------------------------------------------------------------ */
/* Band Control bits                                                   */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_BANDCTL_5GHZ	0x0001
#define B43BSD_NPHY_BANDCTL_DACLPF	0x0002

/* ------------------------------------------------------------------ */
/* PHY-N Public API (declared at top of file)                          */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* BCM2056 Synthesizer (PLL) Registers                                  */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_PLL_REF			B43BSD_PHY_N(0x11C)
#define B43BSD_NPHY_PLL_LOOPFILTER1		B43BSD_PHY_N(0x11D)
#define B43BSD_NPHY_PLL_LOOPFILTER2		B43BSD_PHY_N(0x11E)
#define B43BSD_NPHY_PLL_LOOPFILTER3		B43BSD_PHY_N(0x11F)
#define B43BSD_NPHY_PLL_CP2			B43BSD_PHY_N(0x120)
#define B43BSD_NPHY_PLL_VCO1			B43BSD_PHY_N(0x124)
#define B43BSD_NPHY_PLL_VCO2			B43BSD_PHY_N(0x125)
#define B43BSD_NPHY_PLL_DIV_INT			B43BSD_PHY_N(0x126)
#define B43BSD_NPHY_PLL_DIV_FRAC		B43BSD_PHY_N(0x127)
#define B43BSD_NPHY_PLL_MMD_DIV			B43BSD_PHY_N(0x128)

/* ------------------------------------------------------------------ */
/* Per-Chain TX Power Control                                           */
/* ------------------------------------------------------------------ */
#define B43BSD_NPHY_C1_TXPCTL_PWR		B43BSD_PHY_N(0x22D)
#define B43BSD_NPHY_C2_TXPCTL_PWR		B43BSD_PHY_N(0x22E)
#define B43BSD_NPHY_C3_TXPCTL_PWR		B43BSD_PHY_N(0x22F)
#define B43BSD_NPHY_C1_TXPCTL_GAINIDX		B43BSD_PHY_N(0x230)
#define B43BSD_NPHY_C2_TXPCTL_GAINIDX		B43BSD_PHY_N(0x231)
#define B43BSD_NPHY_C3_TXPCTL_GAINIDX		B43BSD_PHY_N(0x232)
#define B43BSD_NPHY_PAPD_EN			B43BSD_PHY_N(0x297)
void	b43bsd_phy_n_tx_power_per_chain_cal(struct b43bsd_softc *, int);
int	b43bsd_phy_n_channel_switch_full(struct b43bsd_softc *,
	    int channel, int is_5ghz, int bw_40mhz);
void	b43bsd_phy_n_tx_chain_enable(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_rx_chain_enable(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_set_active_chains(struct b43bsd_softc *, int);
void	b43bsd_phy_n_rssi_per_chain_cal(struct b43bsd_softc *);
void	b43bsd_phy_n_evm_estimate(struct b43bsd_softc *);
void	b43bsd_phy_n_freq_offset_estimate(struct b43bsd_softc *);
void	b43bsd_phy_n_tx_power_mcs_chain_cal(struct b43bsd_softc *, int);
void	b43bsd_phy_n_rx_gain_per_chain_optimize(struct b43bsd_softc *);
void	b43bsd_phy_n_mimo_condition_number(struct b43bsd_softc *);
int	b43bsd_phy_n_full_selftest(struct b43bsd_softc *);
void	b43bsd_phy_n_state_save(struct b43bsd_softc *, uint16_t *);
void	b43bsd_phy_n_state_restore(struct b43bsd_softc *, const uint16_t *);
void	b43bsd_phy_n_band_reinit(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_apply_per_channel(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_regdump(struct b43bsd_softc *);
void	b43bsd_phy_n_warm_reset(struct b43bsd_softc *);
void	b43bsd_phy_n_loop_gain_cal(struct b43bsd_softc *);
void	b43bsd_phy_n_rx_timestamp_cal(struct b43bsd_softc *);
int	b43bsd_phy_n_init_sequence(struct b43bsd_softc *, int);

/* PHY register access (exported for use from main driver). */
uint16_t nphy_read(struct b43bsd_softc *, uint16_t);
void	 nphy_write(struct b43bsd_softc *, uint16_t, uint16_t);
void	 nphy_maskset(struct b43bsd_softc *, uint16_t, uint16_t, uint16_t);

/* Internal PHY functions used by the main driver. */
void	b43bsd_phy_n_noise_cal(struct b43bsd_softc *);
void	b43bsd_phy_n_antdiv(struct b43bsd_softc *);
void	b43bsd_adaptive_noise_immunity(struct b43bsd_softc *);
void	b43bsd_txpower_per_mcs_cal(struct b43bsd_softc *, int);
void	b43bsd_link_quality_monitor(struct b43bsd_softc *);

/* Extended calibration (added R19). */
void	b43bsd_phy_n_bf_tx_train(struct b43bsd_softc *);
void	b43bsd_phy_n_agc_advanced(struct b43bsd_softc *);
void	b43bsd_phy_n_rssi_temp_cal(struct b43bsd_softc *);
int	b43bsd_phy_n_loopback_test(struct b43bsd_softc *, int);
void	b43bsd_phy_n_papd_mcs_train(struct b43bsd_softc *);
void	b43bsd_phy_n_freq_offset_advanced(struct b43bsd_softc *);
void	b43bsd_phy_n_state_save_full(struct b43bsd_softc *, uint16_t *);
void	b43bsd_phy_n_state_restore_full(struct b43bsd_softc *,
	    const uint16_t *);
void	b43bsd_phy_n_warm_init(struct b43bsd_softc *, int);
void	b43bsd_phy_n_interference_avoid(struct b43bsd_softc *);
void	b43bsd_phy_n_afe_optimize(struct b43bsd_softc *,
	    int, int, int);
int	b43bsd_phy_n_cqi_measure(struct b43bsd_softc *);
void	b43bsd_phy_n_evm_optimize(struct b43bsd_softc *);
void	b43bsd_phy_n_channel_matrix(struct b43bsd_softc *);
void	b43bsd_phy_n_rx_mrc_cal(struct b43bsd_softc *);
void	b43bsd_phy_n_pa_protection_cal(struct b43bsd_softc *);
void	b43bsd_phy_n_rx_sensitivity_optimize(struct b43bsd_softc *);
int	b43bsd_phy_n_pll_tune(struct b43bsd_softc *, int);
int	b43bsd_phy_n_full_calibration(struct b43bsd_softc *);
void	b43bsd_phy_n_cca_autotune(struct b43bsd_softc *);
int	b43bsd_phy_n_tssi_loop(struct b43bsd_softc *, int);
int	b43bsd_phy_n_full_calibration_extended(struct b43bsd_softc *);
int	b43bsd_phy_n_channel_switch_full(struct b43bsd_softc *,
	    int channel, int is_5ghz, int bw_40mhz);
void	b43bsd_phy_n_tx_chain_enable(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_rx_chain_enable(struct b43bsd_softc *, int, int);
void	b43bsd_phy_n_set_active_chains(struct b43bsd_softc *, int);
int	b43bsd_phy_n_init_sequence(struct b43bsd_softc *, int);

#endif /* _DEV_IC_B43BSD_PHY_N_H_ */
