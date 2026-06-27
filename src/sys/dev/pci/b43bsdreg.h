/*	$OpenBSD: b43bsdreg.h,v 1.1 2026/06/24 xirtus Exp $	*/

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

#ifndef _DEV_PCI_B43BSDREG_H_
#define _DEV_PCI_B43BSDREG_H_

/*
 * BCM4331 MMIO register offsets and bitfield definitions.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/b43.h
 * and drivers/net/wireless/broadcom/b43/main.c.
 */

/* ------------------------------------------------------------------ */
/* SSB (Sonics Silicon Backplane) Core-Specific Registers              */
/* ------------------------------------------------------------------ */

/* ChipCommon core registers */
#define B43_CC_CHIPID			0x0000	/* Chip ID */
#define  B43_CC_CHIPID_ID		0x0000ffff
#define  B43_CC_CHIPID_REV		0x000f0000
#define  B43_CC_CHIPID_PKG		0x00f00000
#define B43_CC_CAPABILITIES		0x0004	/* Capabilities */
#define B43_CC_CTL			0x0008	/* Control */
#define B43_CC_PMU_CTL			0x0600	/* PMU Control */
#define B43_CC_PMU_STAT			0x0604	/* PMU Status */
#define B43_CC_PMU_CAP			0x0608	/* PMU Capabilities */

/* SPROM / OTP registers */
#define B43_SPROM_BASE			0x0800
#define B43_SPROM_REVISION		0x0800
#define B43_SPROM_BOARD_FLAGS2		0x0810
#define B43_SPROM_BOARDFLAGS2_2GHZ_GPIO	0x0002
#define B43_SPROM_BOARDFLAGS2_5GHZ_GPIO	0x0004
#define B43_SPROM_MACADDR_OFFSET	0x01c0
#define B43_SPROM_PA_PARAMS		0x0400

/* PCIe core registers */
#define B43_PCIE_TLP_WORKAROUND		0x0100

/* ------------------------------------------------------------------ */
/* 802.11 MAC Core Registers                                           */
/* ------------------------------------------------------------------ */

/* Interrupt registers */
#define B43_MMIO_GEN_IRQ_REASON		0x0020
#define B43_MMIO_GEN_IRQ_MASK		0x0024
#define  B43_IRQ_READY			(1 << 0)
#define  B43_IRQ_BEACON			(1 << 1)
#define  B43_IRQ_TBTT			(1 << 2)
#define  B43_IRQ_TX_OK			(1 << 3)
#define  B43_IRQ_RF_DISABLED		(1 << 4)
#define  B43_IRQ_TX_ERROR		(1 << 5)
#define  B43_IRQ_RF_ENABLE		(1 << 6)
#define  B43_IRQ_RX_OK			(1 << 7)
#define  B43_IRQ_NOISE			(1 << 8)
#define  B43_IRQ_TIMER0			(1 << 9)
#define  B43_IRQ_TIMER1			(1 << 10)
#define  B43_IRQ_TIMER2			(1 << 11)
#define  B43_IRQ_PS			(1 << 12)
#define  B43_IRQ_UCODE_DEBUG		(1 << 14)
#define  B43_IRQ_PHY_TX_ERR		(1 << 15)
#define  B43_IRQ_RX_PLCP_ERR		(1 << 20)
#define  B43_IRQ_RX_FIFOFLOW_CTL	(1 << 22)
#define  B43_IRQ_RX_BUFFER_FULL		(1 << 24)
#define  B43_IRQ_PIO_WORKAROUND		(1 << 25)
#define  B43_IRQ_WL			(1 << 26)
#define  B43_IRQ_PMQ			(1 << 27)
#define  B43_IRQ_MAC_TXERR		(1 << 28)
#define  B43_IRQ_DMA			(1 << 29)
#define  B43_IRQ_TXFIFO_FLUSH		(1 << 30)
#define  B43_IRQ_ATIM_END		(1U << 31)

#define B43_IRQ_ALL			0xffffffff
#define B43_IRQ_MASK_NORMAL		\
	(B43_IRQ_READY | B43_IRQ_BEACON | B43_IRQ_TBTT | \
	 B43_IRQ_TX_OK | B43_IRQ_TX_ERROR | \
	 B43_IRQ_RX_OK | B43_IRQ_NOISE | B43_IRQ_PS | \
	 B43_IRQ_PHY_TX_ERR | B43_IRQ_RX_PLCP_ERR | \
	 B43_IRQ_DMA | B43_IRQ_WL)

/* DMA registers */
#define B43_DMA0_TX_CTL			0x0200
#define B43_DMA0_TX_DESC_RING		0x0204
#define B43_DMA0_TX_INDEX		0x0208
#define B43_DMA0_TX_STATUS		0x020c
#define B43_DMA0_RX_CTL			0x0210
#define B43_DMA0_RX_DESC_RING		0x0214
#define B43_DMA0_RX_INDEX		0x0218
#define B43_DMA0_RX_STATUS		0x021c

/* DMA control bits */
#define B43_DMA_CTL_ENABLE		0x00000001
#define B43_DMA_CTL_SUSPEND		0x00000002
#define B43_DMA_CTL_LOOPBACK		0x00000004
#define B43_DMA_CTL_FLUSH		0x00000008
#define B43_DMA_CTL_PARITY		0x00000010
#define B43_DMA_CTL_ADDREXT_MASK	0x00030000
#define B43_DMA_CTL_ADDREXT_SHIFT	16

/* DMA descriptor control bits */
#define B43_DMA_DESC_CTL_LEN		0x00001fff
#define B43_DMA_DESC_CTL_DTABLE		0x00002000
#define B43_DMA_DESC_CTL_FRAMESTART	0x00004000
#define B43_DMA_DESC_CTL_FRAMEEND	0x00008000
#define B43_DMA_DESC_CTL_TX_LAST	0x00010000
#define B43_DMA_DESC_CTL_IRQ		0x00020000
#define B43_DMA_DESC_CTL_SOP		0x00040000
#define B43_DMA_DESC_CTL_EOP		0x00080000

/* DMA status bits */
#define B43_DMA_DESC_STATUS_ACTIVE	0x00000001
#define B43_DMA_DESC_STATUS_DISABLED	0x00000002
#define B43_DMA_DESC_STATUS_STOPPED	0x00000004
#define B43_DMA_DESC_STATUS_IDLEWAIT	0x00000008
#define B43_DMA_DESC_STATUS_ERROR	0x00000010

/* DMA RX status bits (in status field of descriptor) */
#define B43_DMA_RX_STATUS_FRAME		0x00000031
#define B43_DMA_RX_STATUS_OVERSIZE	0x00000004
#define B43_DMA_RX_STATUS_UNDERSIZE	0x00000008
#define B43_DMA_RX_STATUS_RXERR		0x00000010

/* DMA TX status bits */
#define B43_DMA_TX_STATUS_STAT_MASK	0x000f0000
#define B43_DMA_TX_STATUS_STAT_SHIFT	16
#define B43_DMA_TX_STATUS_AMPDU		0x00100000
#define B43_DMA_TX_STATUS_INTERMEDIATE	0x00200000

/* ------------------------------------------------------------------ */
/* 802.11 Core Control Registers                                       */
/* ------------------------------------------------------------------ */

#define B43_MMIO_MACCTL			0x0120
#define  B43_MACCTL_ENABLED		0x00000001
#define  B43_MACCTL_PSM_RUN		0x00000002
#define  B43_MACCTL_PSM_JMP0		0x00000004
#define  B43_MACCTL_SHM_ENABLED		0x00000100
#define  B43_MACCTL_IHR_ENABLED		0x00000400
#define  B43_MACCTL_SHM_UPPER		0x00000800
#define  B43_MACCTL_INFRA		0x00001000
#define  B43_MACCTL_GMODE		0x00002000
#define  B43_MACCTL_HW_PS		0x00004000
#define  B43_MACCTL_AWAKE		0x00008000
#define  B43_MACCTL_BEACON_PROMISC	0x00010000
#define  B43_MACCTL_HW_BEACON_TIMER	0x00020000

/* Reset control */
#define B43_MMIO_XMITSTAT_0		0x0170
#define B43_MMIO_XMITSTAT_1		0x0174

/* MAC IFS timing registers */
#define B43_MMIO_IFS_SLOT		0x0684
#define B43_MMIO_IFS_SIFS		0x0688
#define B43_MMIO_IFS_EIFS		0x068C
#define B43_MMIO_IFS_CTL		0x0680
#define  B43_IFSCTL_USE_EDCF		0x0004

/* MAC address filter registers */
#define B43_MMIO_MACFILTER_CONTROL	0x0420
#define B43_MMIO_MACFILTER_DATA		0x0424

/* TSF (Timing Synchronization Function) registers. */
#define B43_MMIO_TSF_LOW		0x0630
#define B43_MMIO_TSF_HIGH		0x0632
#define B43_MMIO_TSF_CFP_START		0x0634
#define B43_MMIO_TSF_CFP_PRETBTT	0x0636
#define B43_MMIO_TSF_CFP_MAX_DUR		0x0638
#define  B43_MACFILTER_SELF		0x0001
#define  B43_MACFILTER_BSSID		0x0002

/* Ucode registers */
#define B43_SHM_CONTROL			0x0016
#define  B43_SHM_CONTROL_BYTE		0x0000
#define  B43_SHM_CONTROL_WORD		0x0001

/* Ucode shared memory */
#define B43_UCODE_REVISION		0x0000
#define B43_UCODE_PATCHLEVEL		0x0002
#define B43_UCODE_DATE			0x0004
#define B43_UCODE_TIME			0x0006

/* ------------------------------------------------------------------ */
/* PHY-N Registers (BCM4331 / BCM2056)                                 */
/* ------------------------------------------------------------------ */

#define B43_PHY_N_BBCFG			0x0001
#define B43_PHY_N_RFCTL_CMD		0x003a
#define B43_PHY_N_RFCTL_DATA		0x003e
#define B43_PHY_N_TABLE_ADDR		0x0072
#define B43_PHY_N_TABLE_DATALO		0x0074
#define B43_PHY_N_TABLE_DATAHI		0x0076
#define B43_PHY_N_TABLECTL		0x0078
#define B43_PHY_N_BW			0x007c
#define B43_PHY_N_BW_20MHZ		0x0000
#define B43_PHY_N_BW_40MHZ		0x0001
#define B43_PHY_N_CHANNEL		0x0050
#define B43_PHY_N_ANTENNA_SELECT	0x007a
#define B43_PHY_N_TX_GAIN_CTL		0x00a0

/* PHY versioning */
#define B43_PHYTYPE_A			0x00
#define B43_PHYTYPE_B			0x01
#define B43_PHYTYPE_G			0x02
#define B43_PHYTYPE_N			0x04
#define B43_PHYTYPE_LP			0x05

/* ------------------------------------------------------------------ */
/* Hardware Key Table (MAC offset 0x03E0)                               */
/* ------------------------------------------------------------------ */

#define B43BSD_KEYCTL_ALGO_WEP		0x00000000
#define B43BSD_KEYCTL_ALGO_TKIP		0x00000001
#define B43BSD_KEYCTL_ALGO_CCMP		0x00000002
#define B43BSD_KEYCTL_VALID		0x00000004
#define B43BSD_KEYCTL_GROUP		0x00000008
#define B43BSD_KEYCTL_TX		0x00000010

/* ------------------------------------------------------------------ */
/* MAC Control Bits (BCM4331 firmware)                                  */
/* ------------------------------------------------------------------ */

#define B43BSD_MACCTL_PSM_RUN		0x00000002
#define B43BSD_MACCTL_PSM_JMP0		0x00000004
#define B43_PHYTYPE_HT			0x07

/* PHY-N TX/RX chain masks */
#define B43_PHY_N_TXCHAIN_0		0x0001
#define B43_PHY_N_TXCHAIN_1		0x0002
#define B43_PHY_N_TXCHAIN_2		0x0004
#define B43_PHY_N_RXCHAIN_0		0x0001
#define B43_PHY_N_RXCHAIN_1		0x0002
#define B43_PHY_N_RXCHAIN_2		0x0004

/* Radio registers (BCM2056) */
#define B43_RADIO_BCM2056		0x2056

#define B43_RADIO_ID			0x0001
#define B43_RADIO_CTL			0x0002
#define B43_RADIO_TX_GAIN_2GHZ		0x0010
#define B43_RADIO_TX_GAIN_5GHZ		0x0014

/* Radio init values (from firmware n0initvals16.fw) */
struct b43_initval {
	uint16_t	offset;
	uint16_t	value;
	uint16_t	mask;
};

/* ------------------------------------------------------------------ */
/* PCI Config Space Helpers                                            */
/* ------------------------------------------------------------------ */

#define B43_PCI_BAR0			0x10

/* Interrupt storm thresholds */
#define B43BSD_IRQ_STORM_THRESH		10000
#define B43BSD_IRQ_STORM_INTERVAL	1	/* seconds */

#endif /* _DEV_PCI_B43BSDREG_H_ */
