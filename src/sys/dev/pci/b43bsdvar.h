/*	$OpenBSD: b43bsdvar.h,v 1.1 2026/06/24 xirtus Exp $	*/

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

#ifndef _DEV_PCI_B43BSDVAR_H_
#define _DEV_PCI_B43BSDVAR_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/task.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <machine/bus.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/b43bsd_dma.h>
#include <dev/ic/b43bsd_phy_n.h>
#include <dev/ic/b43bsd_fw.h>
#include <dev/ic/b43bsd_rate.h>
#include <dev/ic/b43bsd_ps.h>
#include <dev/ic/b43bsd_wa.h>
#include <dev/ic/b43bsd_btcoex.h>
#include <dev/ic/b43bsd_tables.h>
#include <dev/ic/b43bsd_radio.h>
#include <dev/ic/b43bsd_xmit.h>

#include <dev/pci/pcivar.h>

#define B43BSD_TX_RING_SIZE	256
#define B43BSD_RX_RING_SIZE	256

/* SSB core identifiers — from Linux include/linux/ssb/ssb.h */
#define SSB_DEV_CHIPCOMMON	0x800	/* ChipCommon */
#define SSB_DEV_PCIE		0x820	/* PCI Express bridge */
#define SSB_DEV_80211		0x812	/* IEEE 802.11 MAC */
#define SSB_DEV_ARM_CM3		0x82A	/* ARM Cortex-M3 (ucode engine) */
#define SSB_DEV_MIMO_PHY	0x821	/* MIMO PHY (802.11n radio) */

/* PCI vendor/product IDs */
#define PCI_VENDOR_BROADCOM		0x14e4
#define PCI_PRODUCT_BROADCOM_BCM4331	0x4331

/* Chip ID register */
#define B43_CHIPID		0x0000
#define B43_CHIPID_MASK		0x000f0000
#define B43_CHIPID_SHIFT	16

/* b43 device quirks */
#define B43BSD_QUIRK_EFI_SPURIOUS	0x0001
#define B43BSD_QUIRK_PCIE_WAR		0x0002

/*
 * Forward declaration for SSB bus.
 */
struct ssb_bus;

/*
 * Radiotap headers.
 */
struct b43bsd_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_antsignal;
	int8_t		wr_antnoise;
	uint8_t		wr_antenna;
} __packed __aligned(8);

#define B43BSD_RX_RADIOTAP_PRESENT \
	((1 << IEEE80211_RADIOTAP_FLAGS) | \
	 (1 << IEEE80211_RADIOTAP_RATE) | \
	 (1 << IEEE80211_RADIOTAP_CHANNEL) | \
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) | \
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) | \
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct b43bsd_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed __aligned(8);

#define B43BSD_TX_RADIOTAP_PRESENT \
	((1 << IEEE80211_RADIOTAP_FLAGS) | \
	 (1 << IEEE80211_RADIOTAP_RATE) | \
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

/*
 * Firmware state.
 */
struct b43bsd_fw {
	u_char		*ucode;
	size_t		ucode_size;
	u_char		*initvals;
	size_t		initvals_size;
	u_char		*initvals_band;
	size_t		initvals_band_size;
	u_char		*pcm;
	size_t		pcm_size;
	int		loaded;
	int		running;
	int		fw_cached;
	uint16_t	rev;
	uint16_t	patch;
};

/*
 * Radio info (BCM2056 for BCM4331).
 */
struct b43bsd_radio {
	uint16_t	manuf;
	uint16_t	version;
	uint16_t	revision;
	int		txant;
	int		rxant;
	int		phy_rev;	/* N-PHY revision for initvals */
	int		radio_temp;	/* last temperature reading (C) */
	int		rssi_offset_c1;	/* RSSI idle offset, core 1 */
	int		rssi_offset_c2;	/* RSSI idle offset, core 2 */
};

/*
 * Per-device softc.
 */
struct b43bsd_softc {
	struct device		sc_dev;
	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_size_t		sc_sz;
	bus_dma_tag_t		sc_dmat;
	pci_chipset_tag_t	sc_pct;
	pcitag_t		sc_pcitag;

	void			*sc_ih;
	int			sc_msi;

	uint32_t		sc_chipid;
	uint16_t		sc_chiprev;
	uint32_t		sc_11core_offset; /* 802.11 core BAR0 offset */
	uint32_t		sc_flags;
#define B43BSD_FLAG_5GHZ	(1 << 0)
#define B43BSD_FLAG_11N		(1 << 1)
#define B43BSD_FLAG_40MHZ	(1 << 2)
#define B43BSD_FLAG_RUNNING	(1 << 3)
#define B43BSD_FLAG_FW_LOADED	(1 << 4)

	uint32_t		sc_quirks;
	char			sc_fwpath[64];

	/* SSB bus handle */
	struct ssb_bus		*sc_ssb;

	/* Firmware */
	struct b43bsd_fw	sc_fw;

	/* Radio */
	struct b43bsd_radio	sc_radio;

	/* Regulatory TX power limits (dBm, from SPROM) */
	int			sc_maxpwr_2ghz;
	int			sc_maxpwr_5ghz;

	/* Per-MCS TX power offsets (dBm * 4, from SPROM) */
	int8_t			sc_mcs_pwr_2g[24];
	int8_t			sc_mcs_pwr_5g[24];

	/* DMA rings */
	struct b43bsd_dma_ring	sc_txring;
	struct b43bsd_dma_ring	sc_rxring;

	/* Interrupt storm detection */
	int			sc_intr_count;
	int			sc_irq_storm;
	struct timeout		sc_storm_to;

	/* Beacon loss tracking */
	int			sc_beacon_miss;	/* consecutive missed beacons */
#define B43BSD_BEACON_MISS_MAX	5	/* max before reporting loss */
	struct timeout		sc_beacon_to;	/* periodic beacon check */
	struct timeout		sc_cal_to;	/* periodic PHY calibration */

	/* LED control */
	int			sc_led_blink;	/* LED activity indicator */

	/* MAC address from SPROM */
	uint8_t			sc_macaddr[IEEE80211_ADDR_LEN];

	/* Debug level */
	int			sc_debug;
#define B43BSD_DBG_ATTACH	0x0001
#define B43BSD_DBG_INTR		0x0002
#define B43BSD_DBG_TX		0x0004
#define B43BSD_DBG_RX		0x0008
#define B43BSD_DBG_FW		0x0010
#define B43BSD_DBG_PHY		0x0020
#define B43BSD_DBG_SSB		0x0040
#define B43BSD_DBG_DMA		0x0080
#define B43BSD_DBG_STATE	0x0100

	/* net80211 radio-tap */
	struct b43bsd_rx_radiotap_header	sc_rxtap;
	struct b43bsd_tx_radiotap_header	sc_txtap;

	/* Rate control (Minstrel-HT). */
	uint8_t			sc_cur_rateidx;	/* current best rate */

	/* Encryption key management. */
	int			sc_nkeys;
#define B43BSD_MAX_KEYS		4
	struct {
		uint8_t		key[IEEE80211_KEYBUF_SIZE];
		uint8_t		keylen;
		uint8_t		keyidx;
		uint16_t	cipher;
	} sc_keys[B43BSD_MAX_KEYS];

	/* Promiscuous mode flag. */
	int			sc_promisc;

	/* LED GPIO state. */
	int			sc_led_active;

	/* RF kill switch state. */
	int			sc_rfkill;	/* 0=radio on, 1=killed */
#define B43BSD_RFKILL_GPIO	0		/* GPIO pin for RF kill */

	/* MAC statistics (since last query). */
	struct {
		uint32_t	tx_frames;
		uint32_t	tx_bytes;
		uint32_t	tx_errors;
		uint32_t	tx_retries;
		uint32_t	rx_frames;
		uint32_t	rx_bytes;
		uint32_t	rx_errors;
		uint32_t	rx_crc_errors;
		uint32_t	rx_phy_errors;
		uint32_t	rx_fifo_overflow;
		uint32_t	rx_decrypt_errors;
		uint32_t	beacon_missed;
		uint32_t	tsf_sync_lost;
		uint32_t	dma_tx_stalls;
		uint32_t	dma_rx_stalls;
	} sc_stats;

	/* A-MPDU aggregation state. */
#define B43BSD_MAX_TID		8
#define B43BSD_MAX_AGG_SIZE	65535	/* max aggregate frame size */
	struct {
		int		active;		/* BA session active */
		uint16_t	ssn;		/* starting sequence number */
		uint16_t	buf_size;	/* reorder buffer size */
		int		ba_timeout;	/* block ack timeout (TU) */
	} sc_ampdu_rx[B43BSD_MAX_TID];
	struct {
		int		active;
		uint16_t	ssn;
		int		agg_limit;	/* max subframes per AMPDU */
	} sc_ampdu_tx[B43BSD_MAX_TID];

	/* Power save state. */
	struct b43bsd_ps_state	sc_ps;

	/* TSF clock spur avoidance mode (0=default, 1=164MHz, 2=168MHz). */
	int			sc_tsf_mode;

	/* BT coexistence state. */
	struct {
		int		enabled;	/* BT coex active */
		uint8_t		bt_state;	/* current BT state */
		uint16_t	bt_flags;	/* BT coex flags */
#define B43BSD_BTCOEX_FLAG_ACTIVE	0x0001
#define B43BSD_BTCOEX_FLAG_BT_PRIO	0x0002
#define B43BSD_BTCOEX_FLAG_WLAN_PRIO	0x0004
		uint32_t	bt_time;	/* last BT activity time */
	} sc_btcoex;
};

#define B43BSD_LED_GPIO		2	/* GPIO pin for WiFi activity LED */

int	b43bsd_sprom_parse(struct b43bsd_softc *);

void	b43bsd_led_on(struct b43bsd_softc *, int);
void	b43bsd_led_off(struct b43bsd_softc *, int);
void	b43bsd_led_update(struct b43bsd_softc *);
void	b43bsd_led_attach(struct b43bsd_softc *);

#ifdef B43BSD_DEBUG
void	b43bsd_sysctl_attach(struct b43bsd_softc *);
#else
#define b43bsd_sysctl_attach(sc)	do { } while (0)
#endif

#define B43BSD_DPRINTF(sc, flag, fmt, ...) \
	do { \
		if ((sc)->sc_debug & (flag)) \
			printf("%s: " fmt, (sc)->sc_dev.dv_xname, ##__VA_ARGS__); \
	} while (0)

/*
 * PCI match table — defined in b43bsd.c.
 */
extern const struct pci_matchid b43bsd_devices[];

#endif /* _DEV_PCI_B43BSDVAR_H_ */
