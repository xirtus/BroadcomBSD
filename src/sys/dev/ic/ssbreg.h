/*	$OpenBSD: ssbreg.h,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * Sonics Silicon Backplane register definitions.
 *
 * Ported from Linux include/linux/ssb/ssb_regs.h (GPLv2).
 */

#ifndef _DEV_IC_SSBREG_H_
#define _DEV_IC_SSBREG_H_

/* ------------------------------------------------------------------ */
/* Backplane Address Map                                               */
/* ------------------------------------------------------------------ */

#define SSB_SDRAM_BASE		0x00000000U
#define SSB_PCI_MEM		0x08000000U
#define SSB_PCI_CFG		0x0c000000U
#define SSB_SDRAM_SWAPPED	0x10000000U
#define SSB_ENUM_BASE		0x18000000U
#define SSB_ENUM_LIMIT		0x18010000U
#define SSB_FLASH2		0x1c000000U
#define SSB_FLASH2_SZ		0x02000000U
#define SSB_EXTIF_BASE		0x1f000000U
#define SSB_FLASH1		0x1fc00000U
#define SSB_FLASH1_SZ		0x00400000U
#define SSB_PCI_DMA		0x40000000U
#define SSB_PCI_DMA_SZ		0x40000000U
#define SSB_PCIE_DMA_L32	0x00000000U
#define SSB_PCIE_DMA_H32	0x80000000U
#define SSB_EUART		0x1f008000U
#define SSB_LED			0x1f009000U
#define SSB_EJTAG		0xff200000U

#define SSB_CORE_SIZE		0x1000
#define SSB_MAX_NR_CORES	16

/* ------------------------------------------------------------------ */
/* PCI Config Space Registers                                          */
/* ------------------------------------------------------------------ */

#define SSB_BAR0_WIN		0x80
#define SSB_BAR1_WIN		0x84
#define SSB_SPROMCTL		0x88
#define  SSB_SPROMCTL_WE	0x10
#define SSB_BAR1_CONTROL	0x8c
#define SSB_PCI_IRQS		0x90
#define SSB_PCI_IRQMASK		0x94
#define SSB_BACKPLANE_IRQS	0x98
#define SSB_GPIO_IN		0xB0
#define SSB_GPIO_OUT		0xB4
#define SSB_GPIO_OUT_ENABLE	0xB8

#define SSB_GPIO_SCS		0x10
#define SSB_GPIO_HWRAD		0x20
#define SSB_GPIO_XTAL		0x40
#define SSB_GPIO_PLL		0x80

#define SSB_BAR0_MAX_RETRIES	50

/* ------------------------------------------------------------------ */
/* Core Identification (top of each core's 0x1000 region)              */
/* ------------------------------------------------------------------ */

#define SSB_IDLOW		0x0FF8
#define SSB_IDHIGH		0x0FFC

/* IDHIGH bitfields */
#define SSB_IDHIGH_RCLO		0x0000000F
#define SSB_IDHIGH_CC		0x00008FF0
#define  SSB_IDHIGH_CC_SHIFT	4
#define SSB_IDHIGH_RCHI		0x00007000
#define SSB_IDHIGH_VC		0xFFFF0000

/* IDLOW bitfields */
#define SSB_IDLOW_CFGSP		0x00000003
#define SSB_IDLOW_ADDRNGE	0x00000038
#define SSB_IDLOW_SYNC		0x00000040
#define SSB_IDLOW_INITIATOR	0x00000080
#define SSB_IDLOW_MIBL		0x00000F00
#define SSB_IDLOW_MABL		0x0000F000
#define SSB_IDLOW_TIF		0x00010000
#define SSB_IDLOW_CCW		0x000C0000
#define SSB_IDLOW_TPT		0x00F00000
#define SSB_IDLOW_INITP		0x0F000000
#define SSB_IDLOW_SSBREV		0xF0000000

#define SSB_IDLOW_SSBREV_22	0x00000000
#define SSB_IDLOW_SSBREV_23	0x10000000
#define SSB_IDLOW_SSBREV_24	0x40000000
#define SSB_IDLOW_SSBREV_25	0x50000000
#define SSB_IDLOW_SSBREV_26	0x60000000
#define SSB_IDLOW_SSBREV_27	0x70000000

/* ------------------------------------------------------------------ */
/* Core Control Registers                                              */
/* ------------------------------------------------------------------ */

#define SSB_IPSFLAG		0x0F08
#define SSB_TPSFLAG		0x0F18
#define SSB_TMERRLOGA		0x0F48
#define SSB_TMERRLOG		0x0F50
#define SSB_ADMATCH3		0x0F60
#define SSB_ADMATCH2		0x0F68
#define SSB_ADMATCH1		0x0F70
#define SSB_IMSTATE		0x0F90

/* Inititator State bits */
#define SSB_IMSTATE_PC		0x0000000f
#define SSB_IMSTATE_AP_MASK	0x00000030
#define SSB_IMSTATE_IBE		0x00020000
#define SSB_IMSTATE_TO		0x00040000
#define SSB_IMSTATE_BUSY	0x01800000
#define SSB_IMSTATE_REJECT	0x02000000

#define SSB_INTVEC		0x0F94

#define SSB_INTVEC_PCI		0x00000001
#define SSB_INTVEC_ENET0	0x00000002
#define SSB_INTVEC_ILINE20	0x00000004
#define SSB_INTVEC_CODEC		0x00000008
#define SSB_INTVEC_USB		0x00000010
#define SSB_INTVEC_EXTIF	0x00000020
#define SSB_INTVEC_ENET1	0x00000040

#define SSB_TMSLOW		0x0F98
#define  SSB_TMSLOW_RESET	0x00000001
#define  SSB_TMSLOW_REJECT	0x00000002
#define  SSB_TMSLOW_REJECT_23	0x00000004
#define  SSB_TMSLOW_CLOCK	0x00010000
#define  SSB_TMSLOW_FGC		0x00020000
#define  SSB_TMSLOW_PE		0x40000000
#define  SSB_TMSLOW_BE		0x80000000

#define SSB_TMSHIGH		0x0F9C
#define  SSB_TMSHIGH_SERR	0x00000001
#define  SSB_TMSHIGH_INT	0x00000002
#define  SSB_TMSHIGH_BUSY	0x00000004
#define  SSB_TMSHIGH_TO		0x00000020
#define  SSB_TMSHIGH_COREFL	0x0FFF0000
#define  SSB_TMSHIGH_DMA64	0x10000000
#define  SSB_TMSHIGH_GCR	0x20000000
#define  SSB_TMSHIGH_BISTF	0x40000000
#define  SSB_TMSHIGH_BISTD	0x80000000

#define SSB_BWA0		0x0FA0
#define SSB_IMCFGLO		0x0FA8

#define SSB_IMCFGLO_SERTO	0x00000007
#define SSB_IMCFGLO_REQTO	0x00000070
#define SSB_IMCFGLO_CONNID	0x00FF0000

#define SSB_IMCFGHI		0x0FAC

/* Address Match 0 — core base address */
#define SSB_ADMATCH0		0x0FB0
#define  SSB_ADM_TYPE		0x00000003
#define  SSB_ADM_AD64		0x00000004
#define  SSB_ADM_SZ0		0x000000F8
#define  SSB_ADM_SZ1		0x000001F8
#define  SSB_ADM_SZ2		0x000001F8
#define  SSB_ADM_EN		0x00000400
#define  SSB_ADM_NEG		0x00000800
#define  SSB_ADM_BASE0		0xFFFFFF00
#define  SSB_ADM_BASE1		0xFFFFF000
#define  SSB_ADM_BASE2		0xFFFF0000

#define SSB_TMCFGLO		0x0FB8
#define SSB_TMCFGHI		0x0FBC
#define SSB_BCONFIG		0x0FC0
#define SSB_BSTATE		0x0FC8
#define SSB_ACTCFG		0x0FD8
#define SSB_FLAGST		0x0FE8

/* ------------------------------------------------------------------ */
/* ChipCommon PMU Registers                                            */
/* ------------------------------------------------------------------ */

#define SSB_CHIPCO_PMU_CTL		0x0600
#define  SSB_CHIPCO_PMU_CTL_ILP_DIV	0x00000001
#define  SSB_CHIPCO_PMU_CTL_ILP_DIV_SHIFT	0
#define  SSB_CHIPCO_PMU_CTL_PLL_DIV	0x00000002
#define  SSB_CHIPCO_PMU_CTL_XTALFREQ	0x0000003c
#define  SSB_CHIPCO_PMU_CTL_XTALFREQ_SHIFT	2
#define  SSB_CHIPCO_PMU_CTL_PLLPLDEN	0x00000040
#define  SSB_CHIPCO_PMU_CTL_NOISELVL	0x00000180
#define  SSB_CHIPCO_PMU_CTL_NOILPONW	0x00000200

#define SSB_CHIPCO_PMU_STAT		0x0604
#define  SSB_CHIPCO_PMU_STAT_XTALFREQ	0x0000003c

#define SSB_CHIPCO_PMU_CAP		0x0608
#define  SSB_CHIPCO_PMU_CAP_REV		0x000000ff
#define  SSB_CHIPCO_PMU_CAP_NCAP	0x0000ff00
#define SSB_CHIPCO_CLKCTLST		0x01E0
#define SSB_CHIPCO_CLKCTL		0x01E4

/* ChipCommon capabilities */
#define SSB_CHIPCO_CAP_PLL		0x00000010
#define SSB_CHIPCO_CAP_PMU		0x00000020
#define SSB_CHIPCO_CAP_OTP		0x00000040
#define SSB_CHIPCO_CAP_JTAGM		0x00000200
#define SSB_CHIPCO_CAP_PCTL		0x00800000
#define SSB_CHIPCO_CAP_PMU_ALP		0x01000000

#define SSB_CHIPCO_CTL			0x0008
#define SSB_CHIPCO_CAPABILITIES		0x0004
#define SSB_CHIPCO_CHIPID		0x0000

/* ChipCommon Clock Control */
#define SSB_CHIPCO_CLOCKCTL		0x0010
#define SSB_CHIPCO_CLOCKCTL_N		0x0014
#define SSB_CHIPCO_CLOCKCTL_M2		0x0018
#define SSB_CHIPCO_CLOCKCTL_M3		0x001C
#define SSB_CHIPCO_CLOCKCTL_SB		0x0020
#define  SSB_CHIPCO_CLOCKCTL_FORCEHT	0x00010000
#define  SSB_CHIPCO_CLOCKCTL_ALPREQ	0x00020000
#define  SSB_CHIPCO_CLOCKCTL_HTREQ	0x00040000
#define  SSB_CHIPCO_CLOCKCTL_FASTPWROFF	0x00080000

/* ChipCommon PLL control */
#define SSB_CHIPCO_PLLCTL_ADDR		0x0660
#define SSB_CHIPCO_PLLCTL_DATA		0x0664

/* ChipCommon ChipCtrl (PHY PLL reset) */
#define SSB_CHIPCO_CHIPCTL_ADDR		0x0658
#define SSB_CHIPCO_CHIPCTL_DATA		0x065C

/* BCM4331 external PA lines control via ChipCtrl. */
#define SSB_CHIPCTL_4331_EXTPA_EN	0x00001000
#define SSB_CHIPCTL_4331_EXTPA_EN2	0x00002000
#define SSB_CHIPCTL_4331_EXTPA_ON_GPIO2_5 0x00004000

/* ChipCommon PMU resource control */
#define SSB_CHIPCO_PMU_MINRES		0x0618
#define SSB_CHIPCO_PMU_MAXRES		0x061C
#define SSB_CHIPCO_PMU_RES_REQ		0x0650
#define  SSB_CHIPCO_PMU_RES_REQ_UP	0x00000001
#define  SSB_CHIPCO_PMU_RES_REQ_DOWN	0x00000002
#define  SSB_CHIPCO_PMU_RES_MASK	0x0000FFFF

/* PMU resource up/down timers */
#define SSB_CHIPCO_PMU_RES_UP_TIMER	0x0620
#define SSB_CHIPCO_PMU_RES_DOWN_TIMER	0x0624

/* PMU resource IDs */
#define SSB_PMU_RES_80211		0x0001
#define SSB_PMU_RES_PCIE		0x0004
#define SSB_PMU_RES_CHIPCOMMON		0x0010
#define SSB_PMU_RES_XTAL		0x0020

/* BCM4331 specific PMU resources (from Linux pmu.c) */
#define SSB_PMURES_4331_XTAL_PU		0
#define SSB_PMURES_4331_CBUCK_PU	2
#define SSB_PMURES_4331_CLDO_PU		3
#define SSB_PMURES_4331_LNLDO1_PU	4
#define SSB_PMURES_4331_LNLDO2_PU	5
#define SSB_PMURES_4331_BBPLL_PU	8
#define SSB_PMURES_4331_RFPLL_PU	10
#define SSB_PMURES_4331_RX_PWRSW_PU	12
#define SSB_PMURES_4331_TX_PWRSW_PU	13
#define SSB_PMURES_4331_LOGEN_PWRSW_PU	14
#define SSB_PMURES_4331_AFE_PWRSW_PU	15
#define SSB_PMURES_4331_BBPLL_PWRSW_PU	17

/* ChipCommon watchdogs */
#define SSB_CHIPCO_WATCHDOG		0x0400

/* ------------------------------------------------------------------ */
/* PCIe core registers                                                 */
/* ------------------------------------------------------------------ */

#define SSB_PCIE_CTL			0x0100
#define  SSB_PCIE_CTL_CLKREQ		0x00000004

#define SSB_PCIE_DLLP_LINKCTL		0x0500
#define  SSB_PCIE_DLLP_LINKCTL_UPTIME	0x0000ffff
#define  SSB_PCIE_DLLP_LINKCTL_ATTIME	0xffff0000

/* PCIe core SPROM shadow area (BCM4331+).
 * WORD(offset) = 0x0800 + (offset * 2). */
#define SSB_PCIE_SPROM(wordoffset)	(0x0800 + ((wordoffset) * 2))
#define  SSB_PCIE_SPROM_MISC_CONFIG	5	/* word 5 */
#define   SSB_PCIE_SPROM_L23READY_EXIT_NOPERST	0x8000	/* bit 15 */

/* ChipCommon SPROM Control (BCM4331+ PCIe SPROM shadow) */
#define SSB_CHIPCO_SPROMCTL		0x0830
#define  SSB_CHIPCO_SPROMCTL_OFFSET	0x0000ffff
#define  SSB_CHIPCO_SPROMCTL_BUSY	0x80000000

/* ------------------------------------------------------------------ */
/* SPROM Definitions                                                   */
/* ------------------------------------------------------------------ */

#define SSB_SPROMSIZE_WORDS_R4		220
#define SSB_SPROMSIZE_WORDS_R10		230
#define SSB_SPROMSIZE_WORDS		64
#define SSB_SPROMSIZE_BYTES		(SSB_SPROMSIZE_WORDS * 2)
#define SSB_SPROM_BASE31		0x0800
#define SSB_SPROM_REVISION		0x007E
#define SSB_SPROM_BASE			0x1000

/* SPROM Board Flags (low) */
#define SSB_BFL_BTCOEXIST	0x0001
#define SSB_BFL_PACTRL		0x0002
#define SSB_BFL_RSSI		0x0008
#define SSB_BFL_CCKHIPWR	0x0040
#define SSB_BFL_AFTERBURNER	0x0200
#define SSB_BFL_FEM		0x0800
#define SSB_BFL_EXTLNA		0x1000
#define SSB_BFL_HGPA		0x2000

/* SPROM Board Flags (high) */
#define SSB_BFH_NOPA		0x0001
#define SSB_BFH_RSSIINV		0x0002
#define SSB_BFH_PAREF		0x0004

/* SPROM Board Flags 2 */
#define SSB_BFL2_RXBB_INT_REG_DIS	0x0001
#define SSB_BFL2_TXPWRCTRL_EN		0x0004
#define SSB_BFL2_5G_PWRGAIN		0x0010
#define SSB_BFL2_PCIEWAR_OVR		0x0020

/* SPROM Rev 8 layout (BCM4331) */
#define SSB_SPROM8_MACADDR_OFFSET	0x0018
#define SSB_SPROM8_BOARDREV		0x0000
#define SSB_SPROM8_BOARDFLAGS2		0x0004
#define SSB_SPROM8_BF_LO		0x000A
#define SSB_SPROM8_BF_HI		0x000C
#define SSB_SPROM8_IL0MACADDR		0x0018
#define SSB_SPROM8_ET0MACADDR		0x001E
#define SSB_SPROM8_ET1MACADDR		0x0024
#define SSB_SPROM8_ETHPHY		0x002A
#define SSB_SPROM8_CCK2GPO		0x0140
#define SSB_SPROM8_OFDM2GPO		0x0142
#define SSB_SPROM8_OFDM5GPO		0x0146
#define SSB_SPROM8_OFDM5GLPO		0x014A
#define SSB_SPROM8_OFDM5GHPO		0x014E
#define SSB_SPROM8_2G_MCSPO		0x0152
#define SSB_SPROM8_5G_MCSPO		0x0162
#define SSB_SPROM8_5GL_MCSPO		0x0172
#define SSB_SPROM8_5GH_MCSPO		0x0182
#define SSB_SPROM8_CDDPO		0x0192
#define SSB_SPROM8_STBCPO		0x0194
#define SSB_SPROM8_BW40PO		0x0196
#define SSB_SPROM8_BWDUPPO		0x0198

#define SSB_SROM8_PWR_INFO_CORE0	0x00C0
#define SSB_SROM8_PWR_INFO_CORE1	0x00E0
#define SSB_SROM8_PWR_INFO_CORE2	0x0100
#define SSB_SROM8_PWR_INFO_CORE3	0x0120

/* ------------------------------------------------------------------ */
/* Core ID table for BCM4331                                           */
/* ------------------------------------------------------------------ */

#define SSB_COREID_CHIPCOMMON		0x800
#define SSB_COREID_PCIE			0x820
#define SSB_COREID_80211		0x812
#define SSB_COREID_ARM_CM3		0x82A
#define SSB_COREID_MIMO_PHY		0x821

/* PMU resource up/down table entry (for power sequencing) */
struct ssb_pmu_res_updown {
	uint8_t		resource;
	uint16_t	updown;	/* up in bits 11:8, down in bits 3:0 */
};

/* PMU crystal frequency table (kHz) */
struct ssb_pmu_xtalfreq {
	uint8_t		xf;		/* XTALFREQ register value */
	uint32_t	freq;		/* frequency in kHz */
};

#endif /* _DEV_IC_SSBREG_H_ */