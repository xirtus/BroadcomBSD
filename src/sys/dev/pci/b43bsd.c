/*	$OpenBSD: b43bsd.c,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * Broadcom BCM4331 SoftMAC WiFi driver for OpenBSD.
 *
 * Phase 1: PCI attachment, chip identification, autoconf glue.
 *
 * References:
 *   Linux drivers/net/wireless/broadcom/b43/ (GPLv2)
 *   Linux drivers/ssb/ (GPLv2)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/if_arp.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/* ------------------------------------------------------------------ */
/* PCI match table                                                      */
/* ------------------------------------------------------------------ */
const struct pci_matchid b43bsd_devices[] = {
	{ PCI_VENDOR_BROADCOM, PCI_PRODUCT_BROADCOM_BCM4331 },
};

/* ------------------------------------------------------------------ */
/* Debug sysctl nodes                                                   */
/* ------------------------------------------------------------------ */
#ifdef B43BSD_DEBUG
int b43bsd_debug = 0
    | B43BSD_DBG_ATTACH
    ;
#endif

/* Interrupt storm auto-recovery (1 = enabled, 0 = disabled). */
int b43bsd_storm_reset = 1;

/*
 * Attach staging level:
 *   0 = print PCI IDs only, return (no hardware touch)
 *   1 = + BAR0 mapping + size print
 *   2 = + chip ID read from MMIO
 *   3 = + SSB bus enumeration (core discovery)
 *   4 = + PMU init + core enable + 802.11 offset calculation
 *   5 = + PHY rev + SPROM parse + flags
 *   6 = + interrupt + chip reset + net80211 + LED (full attach)
 */
int b43bsd_attach_level = 0;

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

int	b43bsd_match(struct device *, void *, void *);
void	b43bsd_attach(struct device *, struct device *, void *);
int	b43bsd_detach(struct device *, int);
int	b43bsd_activate(struct device *, int);

int	b43bsd_intr(void *);
int	b43bsd_chip_init(struct b43bsd_softc *);
void	b43bsd_chip_reset(struct b43bsd_softc *);
void	b43bsd_tsf_clk_setup(struct b43bsd_softc *);

/* net80211 glue */
int	b43bsd_newstate(struct ieee80211com *, enum ieee80211_state, int);
int	b43bsd_ioctl(struct ifnet *, u_long, caddr_t);
int	b43bsd_init(struct b43bsd_softc *);
void	b43bsd_stop(struct b43bsd_softc *);
void	b43bsd_start(struct ifnet *);
void	b43bsd_watchdog(struct ifnet *);
void	b43bsd_set_channel(struct ieee80211com *);
void	b43bsd_setup_channels(struct b43bsd_softc *);
int	b43bsd_raw_xmit(struct ieee80211com *, struct mbuf *,
	    struct ieee80211_node *);
void	b43bsd_storm_recover(void *);
void	b43bsd_beacon_check(void *);
void	b43bsd_updateslot(struct ieee80211com *);
void	b43bsd_set_bssid(struct b43bsd_softc *, const uint8_t *);
void	b43bsd_set_beacon_interval(struct b43bsd_softc *, int);
void	b43bsd_periodic_cal(void *);
int	b43bsd_rfkill_check(struct b43bsd_softc *);
void	b43bsd_stats_collect(struct b43bsd_softc *);
int	b43bsd_set_key(struct ieee80211com *, struct ieee80211_node *,
	    struct ieee80211_key *);
void	b43bsd_delete_key(struct ieee80211com *, struct ieee80211_node *,
	    struct ieee80211_key *);
void	b43bsd_update_promisc(struct ieee80211com *);
int	b43bsd_ifmedia_change(struct ifnet *);
void	b43bsd_ifmedia_status(struct ifnet *, struct ifmediareq *);

int	b43bsd_ampdu_rx_start(struct ieee80211com *,
	    struct ieee80211_node *, u_int8_t);
void	b43bsd_ampdu_rx_stop(struct ieee80211com *,
	    struct ieee80211_node *, u_int8_t);
int	b43bsd_ampdu_tx_start(struct ieee80211com *,
	    struct ieee80211_node *, u_int8_t);
void	b43bsd_ampdu_tx_stop(struct ieee80211com *,
	    struct ieee80211_node *, u_int8_t);

/* ------------------------------------------------------------------ */
/* Autoconf attachment structure                                        */
/* ------------------------------------------------------------------ */

struct cfattach b43bsd_pci_ca = {
	sizeof(struct b43bsd_softc),
	b43bsd_match,
	b43bsd_attach,
	b43bsd_detach,
	b43bsd_activate,
};

struct cfdriver b43bsd_cd = {
	NULL,
	"b43bsd",
	DV_IFNET,
};

/* ------------------------------------------------------------------ */
/* Register I/O helpers                                                 */
/* ------------------------------------------------------------------ */

/*
 * Register I/O helpers — all 802.11 core register access adds
 * the core's BAR0 offset to reach the correct MMIO window.
 */
static inline uint32_t
b43bsd_read32(struct b43bsd_softc *sc, uint16_t offset)
{
	return bus_space_read_4(sc->sc_st, sc->sc_sh,
	    sc->sc_11core_offset + offset);
}

static inline void
b43bsd_write32(struct b43bsd_softc *sc, uint16_t offset, uint32_t val)
{
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    sc->sc_11core_offset + offset, val);
}

/* ------------------------------------------------------------------ */
/* PCI Match                                                            */
/* ------------------------------------------------------------------ */

int
b43bsd_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_BROADCOM)
		return 0;

	if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_BCM4331)
		return 1;

	return 0;
}

/* ------------------------------------------------------------------ */
/* PCI Attach                                                           */
/* ------------------------------------------------------------------ */

void
b43bsd_attach(struct device *parent, struct device *self, void *aux)
{
	struct b43bsd_softc *sc = (struct b43bsd_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	pcireg_t memtype;
	const char *intrstr;
	int error;
	int level = b43bsd_attach_level;

	/*
	 * STAGE 0: Print PCI IDs only, no hardware access.
	 * This verifies the autoconf framework calls our attach function.
	 */
	printf(": BCM%04x rev %d (attach level %d)",
	    PCI_PRODUCT(pa->pa_id), PCI_REVISION(pa->pa_class), level);
	if (level <= 0) {
		printf(" stub-ok\n");
		return;
	}

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag;
	sc->sc_dmat = pa->pa_dmat;

	timeout_set(&sc->sc_storm_to, b43bsd_storm_recover, sc);
	timeout_set(&sc->sc_beacon_to, b43bsd_beacon_check, sc);
	timeout_set(&sc->sc_cal_to, b43bsd_periodic_cal, sc);

#ifdef B43BSD_DEBUG
	sc->sc_debug = b43bsd_debug;
#endif

	/*
	 * STAGE 1: Map PCI BAR0 + print size.
	 */
	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, B43_PCI_BAR0);
	error = pci_mapreg_map(pa, B43_PCI_BAR0, memtype, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &sc->sc_sz, 0);
	if (error != 0) {
		printf(": cannot map BAR0 (error %d)\n", error);
		return;
	}
	printf(": BAR0 0x%lx", (unsigned long)sc->sc_sz);
	if (level <= 1) {
		printf(" level1-ok\n");
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);
		sc->sc_sz = 0;
		return;
	}

	/* Fix BAR0_WIN (PCI config 0x80) so ChipCommon is at BAR0 offset 0. */
	{
		pcireg_t bar0win;

		bar0win = pci_conf_read(pa->pa_pc, pa->pa_tag, 0x80);
		printf(" win=0x%08x", bar0win);
		if (bar0win != SSB_ENUM_BASE) {
			pci_conf_write(pa->pa_pc, pa->pa_tag, 0x80,
			    SSB_ENUM_BASE);
			(void)pci_conf_read(pa->pa_pc, pa->pa_tag, 0x80);
			B43BSD_DPRINTF(sc, B43BSD_DBG_ATTACH,
			    "BAR0_WIN fixed to 0x%08x\n", SSB_ENUM_BASE);
		}
	}

	/* Disable ASPM on BCM4331 Apple boards BEFORE any MMIO. */
	{
		int pcie_cap;

		pcie_cap = pci_get_capability(pa->pa_pc, pa->pa_tag,
		    PCI_CAP_PCIEXPRESS, NULL, NULL);
		if (pcie_cap) {
			pcireg_t lnkctl;

			lnkctl = pci_conf_read(pa->pa_pc, pa->pa_tag,
			    pcie_cap + PCI_PCIE_LCAP);
			lnkctl &= ~(PCI_PCIE_LCAP_ASPM_L0S |
			    PCI_PCIE_LCAP_ASPM_L1);
			pci_conf_write(pa->pa_pc, pa->pa_tag,
			    pcie_cap + PCI_PCIE_LCAP, lnkctl);
		}
	}

	/* Mask interrupts BEFORE enabling bus mastering. */
	{
		pcireg_t cmd;

		cmd = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG);
		cmd |= PCI_COMMAND_INTERRUPT_DISABLE;
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG, cmd);
	}

	/* Enable bus mastering and memory space. */
	{
		pcireg_t cmd;

		cmd = pci_conf_read(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG);
		cmd |= PCI_COMMAND_MASTER_ENABLE;
		cmd |= PCI_COMMAND_MEM_ENABLE;
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG, cmd);
	}

	/*
	 * STAGE 2: Read chip identification from MMIO.
	 */
	sc->sc_chipid = bus_space_read_4(sc->sc_st, sc->sc_sh, 0);
	if (sc->sc_chipid == 0xffffffff || sc->sc_chipid == 0) {
		/* Chip might be in reset. Toggle watchdog to reset. */
		bus_space_write_4(sc->sc_st, sc->sc_sh, 0x0400, 1);
		delay(1000);
		bus_space_write_4(sc->sc_st, sc->sc_sh, 0x0400, 0);
		delay(10000);
		sc->sc_chipid = bus_space_read_4(sc->sc_st, sc->sc_sh, 0);
	}
	sc->sc_chiprev = (sc->sc_chipid & B43_CHIPID_MASK) >>
	    B43_CHIPID_SHIFT;

	printf(": BCM%x rev %d", sc->sc_chipid & 0xffff,
	    (sc->sc_chipid >> 16) & 0xf);
	if (level <= 2) {
		printf(" level2-ok\n");
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);
		sc->sc_sz = 0;
		return;
	}

	/*
	 * STAGE 3: Allocate SSB bus and enumerate cores.
	 */
	sc->sc_ssb = malloc(sizeof(*sc->sc_ssb), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_ssb == NULL) {
		printf(": cannot allocate SSB bus\n");
		goto unmap;
	}
	sc->sc_ssb->dev = sc->sc_dev;
	if (ssb_attach(sc->sc_ssb, sc->sc_st, sc->sc_sh, sc->sc_sz) != 0) {
		printf(": SSB bus attach failed\n");
		goto free_ssb;
	}
	if (level <= 3) {
		printf(" level3-ok\n");
		goto free_ssb;
	}

	/*
	 * STAGE 4: Initialize PMU, enable cores, calculate 802.11 offset.
	 */
	ssb_pmu_init(sc->sc_ssb);

	if (sc->sc_ssb->ieee80211_idx >= 0)
		ssb_core_enable(sc->sc_ssb, sc->sc_ssb->ieee80211_idx);
	if (sc->sc_ssb->mimo_phy_idx >= 0)
		ssb_core_enable(sc->sc_ssb, sc->sc_ssb->mimo_phy_idx);
	if (sc->sc_ssb->arm_cm3_idx >= 0)
		ssb_core_enable(sc->sc_ssb, sc->sc_ssb->arm_cm3_idx);

	{
		uint32_t cc_base;

		if (sc->sc_ssb->chipcommon_idx >= 0)
			cc_base = sc->sc_ssb->cores[
			    sc->sc_ssb->chipcommon_idx].base;
		else
			cc_base = 0;

		if (sc->sc_ssb->ieee80211_idx >= 0)
			sc->sc_11core_offset =
			    sc->sc_ssb->cores[
			    sc->sc_ssb->ieee80211_idx].base - cc_base;
		else
			sc->sc_11core_offset = 0;

		B43BSD_DPRINTF(sc, B43BSD_DBG_ATTACH,
		    "802.11 core at BAR0+0x%x\n", sc->sc_11core_offset);
	}
	if (level <= 4) {
		printf(" level4-ok\n");
		goto free_ssb;
	}

	/*
	 * STAGE 5: Read SPROM, set flags, attach net80211 (no interrupts).
	 */
	sc->sc_flags = B43BSD_FLAG_11N | B43BSD_FLAG_40MHZ;

	if (sc->sc_ssb->mimo_phy_idx >= 0) {
		uint16_t bbgcfg;

		bbgcfg = ssb_core_read16(sc->sc_ssb,
		    sc->sc_ssb->mimo_phy_idx, B43_PHY_N_BBCFG);
		sc->sc_radio.phy_rev = bbgcfg & 0xf;
		B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
		    "N-PHY revision %d\n", sc->sc_radio.phy_rev);
	} else {
		sc->sc_radio.phy_rev = 16;
	}

	b43bsd_sprom_parse(sc);

	if (sc->sc_ssb->pcie_idx >= 0) {
		uint32_t pciectl;

		pciectl = ssb_core_read32(sc->sc_ssb,
		    sc->sc_ssb->pcie_idx, SSB_PCIE_CTL);
		pciectl &= ~SSB_PCIE_CTL_CLKREQ;
		ssb_core_write32(sc->sc_ssb,
		    sc->sc_ssb->pcie_idx, SSB_PCIE_CTL, pciectl);
	}

	sc->sc_flags |= B43BSD_FLAG_5GHZ;

	if ((sc->sc_flags & B43BSD_FLAG_5GHZ) == 0) {
		printf("%s: 5 GHz disabled by SPROM\n",
		    sc->sc_dev.dv_xname);
	}

	/* Attach net80211 interface (without interrupt). */
	{
		struct ifnet *ifp = &sc->sc_ic.ic_if;

		sc->sc_ic.ic_phytype = IEEE80211_T_OFDM;
		sc->sc_ic.ic_opmode = IEEE80211_M_STA;
		sc->sc_ic.ic_state = IEEE80211_S_INIT;
		sc->sc_ic.ic_softc = sc;
		sc->sc_ic.ic_caps =
		    IEEE80211_C_WEP |
		    IEEE80211_C_SHPREAMBLE |
		    IEEE80211_C_SHSLOT |
		    IEEE80211_C_MONITOR |
		    IEEE80211_C_RSN |
		    IEEE80211_C_QOS |
		    IEEE80211_C_TX_AMPDU;
		sc->sc_ic.ic_htcaps =
		    IEEE80211_HTCAP_SGI20 |
		    IEEE80211_HTCAP_SGI40 |
		    IEEE80211_HTCAP_CBW20_40 |
		    IEEE80211_HTCAP_GF;
		sc->sc_ic.ic_htxcaps = 0;
		sc->sc_ic.ic_txbfcaps = 0;
		sc->sc_ic.ic_aselcaps = 0;
		sc->sc_ic.ic_ampdu_params =
		    (IEEE80211_AMPDU_PARAM_SS_4 | 0x3);

		sc->sc_newstate = sc->sc_ic.ic_newstate;
		sc->sc_ic.ic_newstate = b43bsd_newstate;
		b43bsd_setup_channels(sc);
		sc->sc_ic.ic_updatechan = b43bsd_set_channel;
		sc->sc_ic.ic_updateslot = b43bsd_updateslot;
		sc->sc_ic.ic_set_key = b43bsd_set_key;
		sc->sc_ic.ic_delete_key = b43bsd_delete_key;
		sc->sc_ic.ic_updateprot = b43bsd_update_promisc;
		sc->sc_ic.ic_ampdu_rx_start = b43bsd_ampdu_rx_start;
		sc->sc_ic.ic_ampdu_rx_stop = b43bsd_ampdu_rx_stop;
		sc->sc_ic.ic_ampdu_tx_start = b43bsd_ampdu_tx_start;
		sc->sc_ic.ic_ampdu_tx_stop = b43bsd_ampdu_tx_stop;

		IEEE80211_ADDR_COPY(sc->sc_ic.ic_myaddr, sc->sc_macaddr);

		{
			uint8_t *a = sc->sc_macaddr;
			uint32_t v;

			v = a[0] | (a[1] << 8) | (a[2] << 16) | (a[3] << 24);
			b43bsd_write32(sc, 0x03ec, v);
			v = a[4] | (a[5] << 8);
			b43bsd_write32(sc, 0x03f0, v);
			b43bsd_write32(sc, 0x03e8, 0x00000001);
		}

		ifp->if_softc = sc;
		ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
		ifp->if_ioctl = b43bsd_ioctl;
		ifp->if_start = b43bsd_start;
		ifp->if_watchdog = b43bsd_watchdog;
		ifq_init_maxlen(&ifp->if_snd, IFQ_MAXLEN);
		strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

		if_attach(ifp);
		ether_ifattach(ifp);
		ieee80211_ifattach(ifp);
		ieee80211_media_init(ifp, b43bsd_ifmedia_change,
		    b43bsd_ifmedia_status);
	}

	if (level <= 5) {
		printf(" level5-ok\n");
		goto free_ssb;
	}

	/*
	 * STAGE 6: Map interrupt, chip reset, LED, sysctl (full attach).
	 */
	if (pci_intr_map_msi(pa, &ih) == 0) {
		sc->sc_msi = 1;
	} else if (pci_intr_map(pa, &ih) != 0) {
		printf(": cannot map interrupt\n");
		goto free_ssb;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET,
	    b43bsd_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": cannot establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto free_ssb;
	}
	printf(": %s", intrstr != NULL ? intrstr : "?");

	b43bsd_chip_reset(sc);

#ifdef B43BSD_DEBUG
	b43bsd_sysctl_attach(sc);
#endif

	b43bsd_led_attach(sc);

	printf(" level6-ok\n");
	return;

free_ssb:
	free(sc->sc_ssb, M_DEVBUF, sizeof(*sc->sc_ssb));
	sc->sc_ssb = NULL;
unmap:
	if (sc->sc_sz != 0) {
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);
		sc->sc_sz = 0;
	}
}

/* ------------------------------------------------------------------ */
/* PCI Detach                                                           */
/* ------------------------------------------------------------------ */

int
b43bsd_detach(struct device *self, int flags)
{
	struct b43bsd_softc *sc = (struct b43bsd_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	timeout_del(&sc->sc_storm_to);
	timeout_del(&sc->sc_beacon_to);
	timeout_del(&sc->sc_cal_to);
	b43bsd_stop(sc);

	b43bsd_fw_free(sc);
	ieee80211_ifdetach(ifp);
	ether_ifdetach(ifp);
	if_detach(ifp);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	if (sc->sc_sz != 0) {
		bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);
		sc->sc_sz = 0;
	}

	if (sc->sc_ssb != NULL) {
		free(sc->sc_ssb, M_DEVBUF, sizeof(*sc->sc_ssb));
		sc->sc_ssb = NULL;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Activate (suspend/resume)                                            */
/* ------------------------------------------------------------------ */

int
b43bsd_activate(struct device *self, int act)
{
	struct b43bsd_softc *sc = (struct b43bsd_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	switch (act) {
	case DVACT_DEACTIVATE:
	case DVACT_SUSPEND:
		/*
		 * Pre-suspend: stop DMA, mask interrupts,
		 * save firmware state, reset chip.
		 */
		if (sc->sc_flags & B43BSD_FLAG_RUNNING) {
			/* Mask all interrupts. */
			b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, 0);
			/* Disarm timeouts. */
			timeout_del(&sc->sc_beacon_to);
			timeout_del(&sc->sc_storm_to);
			/* Disable and free DMA. */
			b43bsd_dma_free(sc);
		}
		b43bsd_chip_reset(sc);
		break;
	case DVACT_RESUME:
	case DVACT_WAKEUP:
		/*
		 * Post-resume: re-enable PCI, re-init chip,
		 * re-load firmware if needed, re-init DMA,
		 * and trigger re-association.
		 */
		{
			pcireg_t cmd;

			/* Re-enable bus mastering. */
			cmd = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
			    PCI_COMMAND_STATUS_REG);
			cmd |= PCI_COMMAND_MASTER_ENABLE |
			    PCI_COMMAND_MEM_ENABLE;
			pci_conf_write(sc->sc_pct, sc->sc_pcitag,
			    PCI_COMMAND_STATUS_REG, cmd);
		}

		/* Reset and re-init chip. */
		b43bsd_chip_init(sc);

		/* Re-init PHY PLL. */
		ssb_pmu_pll_reset(sc->sc_ssb);

		/* If firmware was loaded, check if it survived suspend. */
		if (sc->sc_fw.running) {
			uint16_t fwrev;

			fwrev = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
			    B43BSD_SHM_UCODEREV);
			if (fwrev != sc->sc_fw.rev) {
				/* Firmware lost — full re-init needed. */
				printf("%s: firmware lost during "
				    "suspend, re-initializing\n",
				    sc->sc_dev.dv_xname);
				b43bsd_fw_init(sc);
				b43bsd_phy_n_init(sc);
			}
		}

		/* If interface was running, bring it back up. */
		if (ifp->if_flags & IFF_UP) {
			b43bsd_init(sc);
			/* Trigger re-association. */
			ieee80211_new_state(&sc->sc_ic,
			    IEEE80211_S_SCAN, -1);
		}
		break;
	default:
		break;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Interrupt Handler                                                    */
/* ------------------------------------------------------------------ */

int
b43bsd_intr(void *arg)
{
	struct b43bsd_softc *sc = arg;
	uint32_t reason;

	/*
	 * Check if the interrupt is ours.  This is REQUIRED for shared
	 * interrupt lines (common on MacBooks where PCIe WiFi shares
	 * the interrupt line with FireWire, SD card reader, etc.).
	 */
	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0) {
		/* Device is not running — mask and ignore. */
		b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, 0x00000000);
		return 0;
	}

	reason = b43bsd_read32(sc, B43_MMIO_GEN_IRQ_REASON);
	if (reason == 0) {
		/* Not ours — return 0 for shared interrupt handling. */
		return 0;
	}

	/*
	 * Interrupt storm detection (EFI spurious interrupt bug).
	 * If interrupts arrive too fast, mask and log.
	 */
	sc->sc_intr_count++;
	if (sc->sc_intr_count > B43BSD_IRQ_STORM_THRESH) {
		if (!sc->sc_irq_storm) {
			sc->sc_irq_storm = 1;
			printf("%s: spurious interrupt storm, "
			    "masking until reset\n",
			    sc->sc_dev.dv_xname);
			if (b43bsd_storm_reset)
				timeout_add_sec(&sc->sc_storm_to, 1);
		}
		/* Mask all interrupts at the device. */
		b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, 0x00000000);
		return 1;
	}

	/* Acknowledge interrupts by writing the reason back. */
	b43bsd_write32(sc, B43_MMIO_GEN_IRQ_REASON, reason);

	/* Handle TBTT (pre-beacon timing for PS sync). */
	if (reason & B43_IRQ_TBTT) {
		/*
		 * In STA mode, TBTT fires before the AP's expected
		 * beacon. Wake the chip from PS so we can receive
		 * the beacon's TIM element.
		 */
		if (sc->sc_ps.enabled && sc->sc_ps.sleeping)
			b43bsd_ps_dtim_wake(sc);
	}

	/* Handle beacon (beacon received or missed). */
	if (reason & B43_IRQ_BEACON) {
		/*
		 * Beacon interrupt fires when an AP beacon is received.
		 * Reset the beacon miss counter and TSF sync is maintained.
		 */
		sc->sc_beacon_miss = 0;
	}

	/* Handle TX completion. */
	if (reason & B43_IRQ_TX_OK) {
		b43bsd_dma_tx_done(sc);
		b43bsd_led_on(sc, B43BSD_LED_GPIO);
		sc->sc_led_blink = 1;
		/* Allow chip to sleep again after TX. */
		b43bsd_ps_tx_done(sc);
	}

	/* Handle RX. */
	if (reason & B43_IRQ_RX_OK) {
		b43bsd_dma_rx_process(sc);
		b43bsd_led_on(sc, B43BSD_LED_GPIO);
		sc->sc_led_blink = 1;
	}

	/* Handle DMA error interrupts. */
	if (reason & B43_IRQ_DMA) {
		if (b43bsd_dma_error_recover(sc) != 0) {
			printf("%s: DMA error recovery failed, "
			    "full reinit\n", sc->sc_dev.dv_xname);
			b43bsd_init(sc);
		}
	}

	/* Handle RX overflow. */
	if (reason & B43_IRQ_RX_FIFOFLOW_CTL)
		b43bsd_dma_rx_overflow(sc);

	/* Clear reason bits we handled. */
	reason &= ~(B43_IRQ_READY | B43_IRQ_BEACON |
	    B43_IRQ_TBTT | B43_IRQ_PS |
	    B43_IRQ_NOISE | B43_IRQ_TX_OK | B43_IRQ_TX_ERROR |
	    B43_IRQ_RX_OK | B43_IRQ_WL | B43_IRQ_DMA |
	    B43_IRQ_RX_FIFOFLOW_CTL | B43_IRQ_PHY_TX_ERR |
	    B43_IRQ_RX_PLCP_ERR);

	if (reason != 0) {
		B43BSD_DPRINTF(sc, B43BSD_DBG_INTR,
		    "unhandled interrupt reason 0x%08x\n", reason);
	}

	return 1;
}

/* ------------------------------------------------------------------ */
/* net80211 State Machine                                               */
/* ------------------------------------------------------------------ */

int
b43bsd_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	enum ieee80211_state ostate = ic->ic_state;

	if (ostate == nstate)
		return 0;

	switch (nstate) {
	case IEEE80211_S_INIT:
		if (ic->ic_bss != NULL)
			b43bsd_rc_deinit(sc, ic->ic_bss);
		b43bsd_stop(sc);
		break;
	case IEEE80211_S_SCAN:
		if (ostate == IEEE80211_S_INIT)
			b43bsd_init(sc);
		break;
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		/*
		 * Set BSSID filter so we only receive frames from
		 * our target AP during association.
		 */
		if (ic->ic_bss != NULL)
			b43bsd_set_bssid(sc, ic->ic_bss->ni_bssid);
		break;
	case IEEE80211_S_RUN:
		/*
		 * Association complete — configure the chip for
		 * normal operation in the BSS.
		 */
		if (ic->ic_bss != NULL) {
			b43bsd_set_bssid(sc, ic->ic_bss->ni_bssid);
			b43bsd_set_beacon_interval(sc,
			    ic->ic_bss->ni_intval);
			b43bsd_updateslot(ic);

			/*
			 * Configure MACCTL for infrastructure STA mode:
			 * INFRA = infrastructure (not ad-hoc)
			 * GMODE = enable 802.11g/n rates (set via slot)
			 * ENABLED = MAC enabled
			 */
			{
				uint32_t macctl;

				macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
				macctl |= B43_MACCTL_INFRA;
				macctl |= B43_MACCTL_ENABLED;
				macctl &= ~B43_MACCTL_BEACON_PROMISC;
				b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
			}

	/* Initialize rate control for the BSS. */
			b43bsd_rc_init(sc, ic->ic_bss);
			sc->sc_cur_rateidx =
			    b43bsd_rc_rateidx(sc, ic->ic_bss, 1);

			/* Apply PS listen interval from BSS. */
			sc->sc_ps.listen_interval =
			    ic->ic_bss->ni_intval > 0 ?
			    ic->ic_bss->ni_intval : 10;
			b43bsd_ps_set_listen_interval(sc,
			    sc->sc_ps.listen_interval);
		}
		/* Allow chip to enter power save between beacons. */
		if (sc->sc_ps.enabled)
			b43bsd_ps_enter(sc);
		break;
	}

	return sc->sc_newstate(ic, nstate, arg);
}

/* ------------------------------------------------------------------ */
/* ifnet ioctl handler                                                  */
/* ------------------------------------------------------------------ */

int
b43bsd_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct b43bsd_softc *sc = ifp->if_softc;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				b43bsd_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				b43bsd_stop(sc);
		}
		/* Handle promiscuous mode changes. */
		if ((ifp->if_flags ^ sc->sc_promisc) & IFF_PROMISC)
			b43bsd_update_promisc(&sc->sc_ic);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	{
		struct ifreq *ifr = (struct ifreq *)data;
		struct arpcom *ac = &sc->sc_ic.ic_ac;

		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, ac) :
		    ether_delmulti(ifr, ac);
		if (error == ENETRESET) {
			/*
			 * Multicast list changed — set hash filter
			 * to accept all multicast frames.
			 */
			b43bsd_write32(sc, 0x0410, 0xffffffff);
			b43bsd_write32(sc, 0x0414, 0xffffffff);
			error = 0;
		}
		break;
	}
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (struct ifreq *)data,
		    &sc->sc_ic.ic_media, cmd);
		break;
	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		error = 0;
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			b43bsd_init(sc);
	}

	splx(s);
	return error;
}

/* ------------------------------------------------------------------ */
/* Full Driver Initialization                                           */
/* ------------------------------------------------------------------ */

int
b43bsd_init(struct b43bsd_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int error;

	if (sc->sc_flags & B43BSD_FLAG_RUNNING)
		return 0;

	/* Check RF kill switch. */
	if (b43bsd_rfkill_check(sc)) {
		printf("%s: radio disabled by hardware switch\n",
		    sc->sc_dev.dv_xname);
		return EPERM;
	}

	/* Refuse to init if no 802.11 core was found (BAR0 too small). */
	if (sc->sc_ssb->ieee80211_idx < 0) {
		printf("%s: no 802.11 core found (BAR0 too small?)\n",
		    sc->sc_dev.dv_xname);
		return ENXIO;
	}

	/* Reset chip to known state. */
	b43bsd_chip_reset(sc);

	/* Initialize MACCTL, IFS timing, and RTS/CTS. */
	b43bsd_chip_init(sc);

	/* Apply BCM4331 hardware workarounds. */
	b43bsd_wa_apply_all(sc);

	/* Load firmware and initvals. */
	error = b43bsd_fw_init(sc);
	if (error != 0) {
		printf("%s: firmware init failed (error %d), "
		    "using compiled-in tables\n",
		    sc->sc_dev.dv_xname, error);
	}
	/* Upload compiled-in calibration tables to supplement or replace firmware. */
	b43bsd_tables_upload_all(sc, 0);
	b43bsd_tables_upload_radio_extended(sc, 0);
	b43bsd_tables_upload_mcs_power(sc, 0);
	b43bsd_tables_upload_rfseq(sc);
	b43bsd_tables_upload_afe(sc);
	b43bsd_tables_upload_gain_cal(sc);
	b43bsd_tables_upload_pa_cal(sc, 0);
	b43bsd_tables_upload_vco_cal(sc);

	/* Reset and initialize the BCM2056 radio. */
	b43bsd_radio_reset(sc);

	/* Initialize PHY-N and radio. */
	error = b43bsd_phy_n_attach(sc);
	if (error != 0) {
		printf("%s: PHY-N attach failed (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return error;
	}
	b43bsd_phy_n_init(sc);

	/* Run full PHY calibration sequence. */
	if (b43bsd_phy_n_full_calibration(sc) != 0) {
		printf("%s: PHY calibration had errors, continuing\n",
		    sc->sc_dev.dv_xname);
	}

	/* Initialize DMA engine. */
	error = b43bsd_dma_init(sc);
	if (error != 0) {
		printf("%s: DMA init failed (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return error;
	}

	sc->sc_flags |= B43BSD_FLAG_RUNNING;
	ifp->if_flags |= IFF_RUNNING;

	/*
	 * Clear any pending interrupts before enabling.
	 * MacBook EFI bug can leave the chip with asserted interrupts.
	 * Read and ack IRQ_REASON 3 times with 1ms delays to clear
	 * any latched spurious interrupts.
	 */
	{
		int s;

		for (s = 0; s < 3; s++) {
			uint32_t r;

			r = b43bsd_read32(sc, B43_MMIO_GEN_IRQ_REASON);
			b43bsd_write32(sc, B43_MMIO_GEN_IRQ_REASON, r);
			delay(1000);	/* 1ms */
		}
	}

	/* Enable device interrupts. */
	b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, B43_IRQ_MASK_NORMAL);

	/* Arm beacon miss check. */
	sc->sc_beacon_miss = 0;
	timeout_add_msec(&sc->sc_beacon_to, 100 * 1024 / 1000);

	/* Arm periodic calibration (every 30 seconds). */
	timeout_add_sec(&sc->sc_cal_to, 30);

	/* Initialize power save mode. */
	b43bsd_ps_init(sc);

	/* Initialize BT coexistence. */
	b43bsd_btcoex_init(sc);

	/* Turn on LED. */
	b43bsd_led_update(sc);

	/*
	 * Re-enable PCI interrupts LAST.
	 * b43bsd_attach disabled them to prevent EFI-spurious IRQs.
	 * Now that the device is fully initialized, the interrupt
	 * handler is ready, and IRQ sources are configured,
	 * clear PCI_COMMAND_INTERRUPT_DISABLE so interrupts are
	 * delivered.  This MUST come after IRQ_MASK is programmed
	 * to avoid processing interrupts on a partially-setup device.
	 */
	{
		pcireg_t cmd;

		cmd = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
		    PCI_COMMAND_STATUS_REG);
		cmd &= ~PCI_COMMAND_INTERRUPT_DISABLE;
		pci_conf_write(sc->sc_pct, sc->sc_pcitag,
		    PCI_COMMAND_STATUS_REG, cmd);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE, "initialized\n");
	return 0;
}

/* ------------------------------------------------------------------ */
/* Stop / Shutdown                                                      */
/* ------------------------------------------------------------------ */

void
b43bsd_stop(struct b43bsd_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return;

	/* Mask all interrupts. */
	b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, 0x00000000);

	/* Disable power save and force chip awake. */
	b43bsd_ps_deinit(sc);

	/* Disable BT coexistence. */
	b43bsd_btcoex_deinit(sc);

	/* Disarm beacon check. */
	timeout_del(&sc->sc_beacon_to);

	/* Disarm periodic calibration. */
	timeout_del(&sc->sc_cal_to);

	/* Disable and free DMA. */
	b43bsd_dma_free(sc);

	/* Reset chip. */
	b43bsd_chip_reset(sc);

	sc->sc_flags &= ~B43BSD_FLAG_RUNNING;
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;

	/* Turn off LED. */
	b43bsd_led_update(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE, "stopped\n");
}

/* ------------------------------------------------------------------ */
/* if_start — Transmit                                                  */
/* ------------------------------------------------------------------ */

void
b43bsd_start(struct ifnet *ifp)
{
	struct b43bsd_softc *sc = ifp->if_softc;
	struct mbuf *m;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	for (;;) {
		struct ieee80211_node *ni;

		/* Wake chip from PS before transmitting. */
		b43bsd_ps_tx_wake(sc);

		m = ifq_dequeue(&ifp->if_snd);
		if (m == NULL)
			break;

		/* Use current BSS node for rate control feedback. */
		ni = sc->sc_ic.ic_bss;

		if (b43bsd_dma_tx_start(sc, m, ni,
		    b43bsd_rc_rateidx(sc, ni, 1)) != 0) {
			m_freem(m);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		ifp->if_opackets++;
	}
}

/* ------------------------------------------------------------------ */
/* Watchdog                                                             */
/* ------------------------------------------------------------------ */

void
b43bsd_watchdog(struct ifnet *ifp)
{
	struct b43bsd_softc *sc = ifp->if_softc;

	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;
	b43bsd_init(sc);
}

/* ------------------------------------------------------------------ */
/* Raw Transmit (Monitor Mode Injection)                                */
/* ------------------------------------------------------------------ */

int
b43bsd_raw_xmit(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0) {
		m_freem(m);
		return ENETDOWN;
	}

	if (b43bsd_dma_tx_start(sc, m, ni,
	    sc->sc_cur_rateidx) != 0) {
		m_freem(m);
		return ENOBUFS;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Slot Time Update                                                     */
/* ------------------------------------------------------------------ */

void
b43bsd_updateslot(struct ieee80211com *ic)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	uint32_t macctl;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return;

	/*
	 * Set short slot time (9 µs) if the BSS permits it;
	 * otherwise use long slot time (20 µs).
	 * BCM4331: GMODE bit in MACCTL controls short slot.
	 */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	if (ic->ic_flags & IEEE80211_F_SHSLOT)
		macctl |= B43_MACCTL_GMODE;
	else
		macctl &= ~B43_MACCTL_GMODE;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "slot time %s\n",
	    (ic->ic_flags & IEEE80211_F_SHSLOT) ? "short" : "long");
}

/* ------------------------------------------------------------------ */
/* BSSID Filter                                                         */
/* ------------------------------------------------------------------ */

/*
 * Program the BSSID into the hardware MAC address filter.
 * When associated, the chip should only accept frames addressed
 * to our BSSID (or broadcast/multicast).
 */
void
b43bsd_set_bssid(struct b43bsd_softc *sc, const uint8_t *bssid)
{
	uint32_t lo, hi;

	lo = bssid[0] | (bssid[1] << 8) |
	    (bssid[2] << 16) | (bssid[3] << 24);
	hi = bssid[4] | (bssid[5] << 8);

	/* BSSID filter registers at MAC offset 0x01A0 / 0x01A4. */
	b43bsd_write32(sc, 0x01a0, lo);
	b43bsd_write32(sc, 0x01a4, hi);

	/* Notify firmware of BSSID change. */
	b43bsd_fw_set_bssid(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "BSSID set\n");
}

/* ------------------------------------------------------------------ */
/* Beacon Interval                                                      */
/* ------------------------------------------------------------------ */

void
b43bsd_set_beacon_interval(struct b43bsd_softc *sc, int interval)
{
	/*
	 * Program the beacon interval in TU (1024 µs).
	 * BCM4331: beacon interval register at MAC offset 0x0190.
	 */
	b43bsd_write32(sc, 0x0190, interval);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "beacon interval %d TU\n", interval);
}

/* ------------------------------------------------------------------ */
/* Interrupt Storm Recovery                                             */
/* ------------------------------------------------------------------ */

void
b43bsd_storm_recover(void *arg)
{
	struct b43bsd_softc *sc = arg;

	printf("%s: attempting storm recovery reset\n",
	    sc->sc_dev.dv_xname);

	b43bsd_init(sc);
}

/* ------------------------------------------------------------------ */
/* Beacon Loss Detection                                                 */
/* ------------------------------------------------------------------ */

/*
 * Periodic beacon check: called every beacon interval.
 * Increments miss counter; reports loss to net80211 if threshold exceeded.
 */
void
b43bsd_beacon_check(void *arg)
{
	struct b43bsd_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return;

	/* LED blink — turn off if no recent activity. */
	if (sc->sc_led_blink) {
		sc->sc_led_blink = 0;
	} else {
		b43bsd_led_off(sc, B43BSD_LED_GPIO);
	}

	sc->sc_beacon_miss++;

	/* Poll BT coexistence state. */
	b43bsd_btcoex_poll(sc);

	if (sc->sc_beacon_miss >= B43BSD_BEACON_MISS_MAX &&
	    ic->ic_state == IEEE80211_S_RUN) {
		printf("%s: beacon miss (%d consecutive), "
		    "triggering scan\n",
		    sc->sc_dev.dv_xname, sc->sc_beacon_miss);
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		return;
	}

	/* Re-arm if still running. */
	if (sc->sc_flags & B43BSD_FLAG_RUNNING) {
		int iv = ic->ic_bss ? ic->ic_bss->ni_intval : 100;
		timeout_add_msec(&sc->sc_beacon_to, iv * 1024 / 1000);
	}
}

/*
 * Periodic PHY calibration: runs noise floor, CCA, and antenna diversity
 * maintenance every 30 seconds to adapt to changing RF conditions.
 */
void
b43bsd_periodic_cal(void *arg)
{
	struct b43bsd_softc *sc = arg;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return;

	/* Run noise floor calibration. */
	b43bsd_phy_n_noise_cal(sc);

	/* Auto-tune CCA thresholds. */
	b43bsd_phy_n_cca_autotune(sc);

	/* Run antenna diversity cycle. */
	b43bsd_phy_n_antdiv(sc);

	/* Check radio temperature and compensate TX power. */
	b43bsd_radio_temp_compensation(sc, 16);

	/* Re-arm (every 30 seconds). */
	if (sc->sc_flags & B43BSD_FLAG_RUNNING)
		timeout_add_sec(&sc->sc_cal_to, 30);
}

/* ------------------------------------------------------------------ */
/* Channel Setup & Switching                                            */
/* ------------------------------------------------------------------ */

void
b43bsd_setup_channels(struct b43bsd_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	/* 2.4 GHz channels 1–14. */
	for (i = 1; i <= 14; i++) {
		uint8_t chan = i;

		ic->ic_channels[chan].ic_freq =
		    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[chan].ic_flags =
		    IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK |
		    IEEE80211_CHAN_OFDM;
		setbit(ic->ic_chan_avail, chan);
		setbit(ic->ic_chan_active, chan);
	}

	/* 5 GHz channels (if supported). */
	if (sc->sc_flags & B43BSD_FLAG_5GHZ) {
		static const int chans_5ghz[] = {
			36, 40, 44, 48, 52, 56, 60, 64,
			100, 104, 108, 112, 116, 120, 124, 128,
			132, 136, 140, 149, 153, 157, 161, 165
		};
		int nchans_5g = sizeof(chans_5ghz) / sizeof(chans_5ghz[0]);

		for (i = 0; i < nchans_5g; i++) {
			int chan = chans_5ghz[i];
			uint16_t flags;

			ic->ic_channels[chan].ic_freq =
			    ieee80211_ieee2mhz(chan, IEEE80211_CHAN_5GHZ);
			flags = IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM;

			/*
			 * DFS channels (52-64, 100-140) require
			 * radar detection before transmission.
			 * Mark as passive-scan only for safety.
			 */
			if ((chan >= 52 && chan <= 64) ||
			    (chan >= 100 && chan <= 140))
				flags |= IEEE80211_CHAN_PASSIVE;

			ic->ic_channels[chan].ic_flags = flags;
			setbit(ic->ic_chan_avail, chan);
			setbit(ic->ic_chan_active, chan);
		}
	}
}

void
b43bsd_set_channel(struct ieee80211com *ic)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	struct ieee80211_channel *c;
	int channel;

	/*
	 * During scan or init, the desired channel is in ic_des_chan.
	 * Once associated, ic_bss->ni_chan holds the BSS channel.
	 */
	c = ic->ic_bss ? ic->ic_bss->ni_chan : ic->ic_des_chan;
	if (c == NULL)
		return;
	channel = ieee80211_mhz2ieee(c->ic_freq, 0);
	b43bsd_phy_n_switch_channel(sc, channel);
	b43bsd_phy_n_set_bw(sc, 0, 0);
}

/* ------------------------------------------------------------------ */
/* Chip Reset                                                           */
/* ------------------------------------------------------------------ */

void
b43bsd_chip_reset(struct b43bsd_softc *sc)
{
	uint32_t tmp;

	/* Disable all interrupts at the 802.11 core. */
	b43bsd_write32(sc, B43_MMIO_GEN_IRQ_MASK, 0x00000000);

	/* Clear any pending interrupts. */
	tmp = b43bsd_read32(sc, B43_MMIO_GEN_IRQ_REASON);
	b43bsd_write32(sc, B43_MMIO_GEN_IRQ_REASON, tmp);

	/* Disable MAC. */
	tmp = b43bsd_read32(sc, B43_MMIO_MACCTL);
	tmp &= ~B43_MACCTL_ENABLED;
	b43bsd_write32(sc, B43_MMIO_MACCTL, tmp);
}

/* ------------------------------------------------------------------ */
/* TSF Clock Setup (BCM4331)                                            */
/* ------------------------------------------------------------------ */

/*
 * Program the TSF (Timing Synchronization Function) clock frequency.
 * BCM4331 uses a fractional divider derived from the crystal.
 *
 * Default: 160 MHz from 20 MHz crystal. Fraction = 2^26 / 160 = 0x66666.
 * Alternative spur-avoidance modes: 164 MHz (0x63E70), 168 MHz (0x61862).
 */
void
b43bsd_tsf_clk_setup(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;

	if (chip_id != 0x4331)
		return;

	/*
	 * MacBook Pro 9,2 BCM4331: 160 MHz TSF clock (default).
	 * Write fractional divider to TSF_CLK_FRAC registers.
	 * Registers at MAC offset 0x062C (low), 0x062E (high).
	 */
	b43bsd_write32(sc, 0x062c,
	    (0x6 << 16) | 0x6666);  /* 160 MHz: frac = 0x66666 */
}

/* ------------------------------------------------------------------ */
/* Chip Init (stub — expanded in Phase 2)                               */
/* ------------------------------------------------------------------ */

int
b43bsd_chip_init(struct b43bsd_softc *sc)
{
	/* Reset the chip to a known state. */
	b43bsd_chip_reset(sc);

	/* Reset PHY PLL to ensure it's locked. */
	ssb_pmu_pll_reset(sc->sc_ssb);

	/* Program TSF clock frequency for BCM4331 (~160 MHz from 20 MHz xtal). */
	b43bsd_tsf_clk_setup(sc);

	/* Enable MAC control register infrastructure mode. */
	{
		uint32_t macctl;

		macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
		macctl |= B43_MACCTL_INFRA;
		macctl |= B43_MACCTL_GMODE;
		macctl |= B43_MACCTL_SHM_ENABLED;
		macctl |= B43_MACCTL_IHR_ENABLED;
		b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
	}

	/* Configure IFS timing for 802.11g/n (short slot = 9µs capable). */
	b43bsd_write32(sc, B43_MMIO_IFS_SLOT, 0x00000200);  /* 512 * 0.5µs / 28 = ~9µs */
	b43bsd_write32(sc, B43_MMIO_IFS_SIFS, 0x0000000a);   /* 10µs */
	b43bsd_write32(sc, B43_MMIO_IFS_EIFS, 0x000003e0);   /* ~55µs */

	/* RTS/CTS threshold: 2346 (disable, use max). */
	b43bsd_write32(sc, 0x0690, 2346);

	sc->sc_intr_count = 0;
	sc->sc_irq_storm = 0;

	return 0;
}

/* ------------------------------------------------------------------ */
/* Key Management                                                       */
/* ------------------------------------------------------------------ */

int
b43bsd_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	uint32_t keyctl;
	int i, slot;

	if (k->k_id >= B43BSD_MAX_KEYS)
		return EINVAL;

	/* Find a free or matching hardware key slot. */
	slot = -1;
	for (i = 0; i < B43BSD_MAX_KEYS; i++) {
		if (sc->sc_keys[i].keyidx == k->k_id &&
		    sc->sc_keys[i].keylen > 0) {
			slot = i;
			break;
		}
	}
	if (slot < 0) {
		for (i = 0; i < B43BSD_MAX_KEYS; i++) {
			if (sc->sc_keys[i].keylen == 0) {
				slot = i;
				break;
			}
		}
	}
	if (slot < 0)
		return ENOSPC;

	/* Store key in softc. */
	sc->sc_keys[slot].keyidx = k->k_id;
	sc->sc_keys[slot].keylen = k->k_len;
	sc->sc_keys[slot].cipher = k->k_cipher;
	memcpy(sc->sc_keys[slot].key, k->k_key, k->k_len);

	/*
	 * Program hardware key table.
	 * BCM4331 key table at MAC offset 0x03E0, 4 entries × 32 bytes.
	 * Each entry: 16 bytes key data + 4 bytes key control.
	 */
	{
		uint32_t base = 0x03e0 + (slot * 8); /* 8 × 32-bit words */

		/* Write key data (first 4 32-bit words = 16 bytes). */
		for (i = 0; i < 4 && i * 4 < k->k_len; i++) {
			uint32_t w;

			w = (uint32_t)k->k_key[i * 4] |
			    ((uint32_t)k->k_key[i * 4 + 1] << 8) |
			    ((uint32_t)k->k_key[i * 4 + 2] << 16) |
			    ((uint32_t)k->k_key[i * 4 + 3] << 24);
			b43bsd_write32(sc, base + (i * 4), w);
		}
		/* Zero remaining key data words. */
		for (; i < 4; i++)
			b43bsd_write32(sc, base + (i * 4), 0);

		/* Build key control word. */
		keyctl = 0;
		switch (k->k_cipher) {
		case IEEE80211_CIPHER_WEP40:
		case IEEE80211_CIPHER_WEP104:
			keyctl |= B43BSD_KEYCTL_ALGO_WEP;
			break;
		case IEEE80211_CIPHER_TKIP:
			keyctl |= B43BSD_KEYCTL_ALGO_TKIP;
			break;
		case IEEE80211_CIPHER_CCMP:
			keyctl |= B43BSD_KEYCTL_ALGO_CCMP;
			break;
		default:
			break;
		}
		keyctl |= B43BSD_KEYCTL_VALID;
		if (k->k_flags & IEEE80211_KEY_GROUP)
			keyctl |= B43BSD_KEYCTL_GROUP;
		if (k->k_flags & IEEE80211_KEY_TX)
			keyctl |= B43BSD_KEYCTL_TX;

		/* Write key control at offset 16 (word 4). */
		b43bsd_write32(sc, base + 16, keyctl);
	}

	if (sc->sc_keys[slot].keylen == 0)
		sc->sc_nkeys++;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "key set idx %d slot %d cipher %d\n", k->k_id, slot, k->k_cipher);

	return 0;
}

void
b43bsd_delete_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	int i, slot;

	slot = -1;
	for (i = 0; i < B43BSD_MAX_KEYS; i++) {
		if (sc->sc_keys[i].keyidx == k->k_id &&
		    sc->sc_keys[i].keylen > 0) {
			slot = i;
			break;
		}
	}
	if (slot < 0)
		return;

	/* Clear hardware key slot. */
	{
		uint32_t base = 0x03e0 + (slot * 8);

		b43bsd_write32(sc, base + 16, 0); /* Invalidate key. */
		for (i = 0; i < 4; i++)
			b43bsd_write32(sc, base + (i * 4), 0);
	}

	explicit_bzero(sc->sc_keys[slot].key,
	    sizeof(sc->sc_keys[slot].key));
	sc->sc_keys[slot].keylen = 0;
	sc->sc_nkeys--;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "key deleted idx %d slot %d\n", k->k_id, slot);
}

/* ------------------------------------------------------------------ */
/* Promiscuous Mode                                                      */
/* ------------------------------------------------------------------ */

void
b43bsd_update_promisc(struct ieee80211com *ic)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	sc->sc_promisc = (ifp->if_flags & IFF_PROMISC) ? 1 : 0;

	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return;

	{
		uint32_t macctl;

		macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
		if (sc->sc_promisc) {
			/* Accept all frames: clear MAC filter, promisc beacons. */
			macctl |= B43_MACCTL_BEACON_PROMISC;
			b43bsd_write32(sc, 0x03e8, 0);  /* disable MAC filter */
		} else {
			macctl &= ~B43_MACCTL_BEACON_PROMISC;
			b43bsd_write32(sc, 0x03e8, 1);  /* re-enable MAC filter */
		}
		b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
	}
}

/* ------------------------------------------------------------------ */
/* ifmedia Change / Status                                              */
/* ------------------------------------------------------------------ */

int
b43bsd_ifmedia_change(struct ifnet *ifp)
{
	struct b43bsd_softc *sc = ifp->if_softc;
	int error;

	error = ieee80211_media_change(ifp);
	if (error != 0)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING))
		b43bsd_init(sc);

	return 0;
}

void
b43bsd_ifmedia_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	ieee80211_media_status(ifp, imr);
}

/* ------------------------------------------------------------------ */
/* TSF Timer Access (for beacon sync)                                    */
/* ------------------------------------------------------------------ */

/*
 * Read the 64-bit TSF (Timing Synchronization Function) timer.
 * The TSF runs at 1 MHz and is used for beacon timestamp comparison.
 * BCM4331: TSF low at MAC offset 0x0630, TSF high at 0x0634.
 */
uint64_t
b43bsd_tsf_read(struct b43bsd_softc *sc)
{
	uint32_t lo, hi;

	lo = b43bsd_read32(sc, 0x0630);
	hi = b43bsd_read32(sc, 0x0634);
	return ((uint64_t)hi << 32) | lo;
}

/*
 * Power-save listen interval: program how often the STA wakes
 * to listen for beacons (in units of the beacon interval).
 * Called during association when PS is enabled.
 */
void
b43bsd_set_listen_interval(struct b43bsd_softc *sc, int interval)
{
	/*
	 * BCM4331 listen interval register at MAC offset 0x0198.
	 * Set to 0 to disable power save, or 1–65535 for PS.
	 */
	if (interval <= 0) {
		/* Disable power save. */
		b43bsd_write32(sc, 0x0198, 0);
	} else {
		b43bsd_write32(sc, 0x0198, (uint32_t)interval);
		B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
		    "listen interval %d\n", interval);
	}
}

/* ------------------------------------------------------------------ */
/* A-MPDU Aggregation Callbacks                                         */
/* ------------------------------------------------------------------ */

/*
 * Start an RX Block ACK session for the given TID.
 * Configures the hardware RX filter to accept A-MPDU frames
 * and allocates a reorder buffer.
 */
int
b43bsd_ampdu_rx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	struct ieee80211_rx_ba *ba = &ni->ni_rx_ba[tid];

	if (tid >= B43BSD_MAX_TID)
		return EINVAL;

	if (sc->sc_ampdu_rx[tid].active)
		return 0;

	sc->sc_ampdu_rx[tid].active = 1;
	sc->sc_ampdu_rx[tid].ssn = ba->ba_winstart;
	sc->sc_ampdu_rx[tid].buf_size = ba->ba_winsize;
	sc->sc_ampdu_rx[tid].ba_timeout = 0;

	ba->ba_state = IEEE80211_BA_AGREED;

	/*
	 * Program the RX DMA engine for A-MPDU reception:
	 * Set the RX frame offset to account for the 4-byte
	 * A-MPDU delimiter between subframes.
	 */
	{
		uint32_t rxc;

		rxc = b43bsd_read32(sc, B43BSD_DMA64_RX_CTL);
		rxc |= ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
		    B43BSD_DMA64_RXFRAMEOFF);
		b43bsd_write32(sc, B43BSD_DMA64_RX_CTL, rxc);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "ampdu rx start TID %d winsize %d\n", tid, ba->ba_winsize);

	return 0;
}

/*
 * Stop an RX Block ACK session.
 */
void
b43bsd_ampdu_rx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if (tid >= B43BSD_MAX_TID)
		return;

	sc->sc_ampdu_rx[tid].active = 0;
	ni->ni_rx_ba[tid].ba_state = IEEE80211_BA_INIT;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "ampdu rx stop TID %d\n", tid);
}

/*
 * Start a TX Block ACK session.
 * Configures the TX DMA engine to aggregate frames.
 */
int
b43bsd_ampdu_tx_start(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	struct ieee80211_tx_ba *ba = &ni->ni_tx_ba[tid];

	if (tid >= B43BSD_MAX_TID)
		return EINVAL;

	if (sc->sc_ampdu_tx[tid].active)
		return 0;

	sc->sc_ampdu_tx[tid].active = 1;
	sc->sc_ampdu_tx[tid].ssn = ba->ba_winstart;
	sc->sc_ampdu_tx[tid].agg_limit = 64; /* max 64 subframes */

	ba->ba_state = IEEE80211_BA_AGREED;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "ampdu tx start TID %d ssn %d\n", tid, ba->ba_winstart);

	return 0;
}

/*
 * Stop a TX Block ACK session.
 */
void
b43bsd_ampdu_tx_stop(struct ieee80211com *ic, struct ieee80211_node *ni,
    uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if (tid >= B43BSD_MAX_TID)
		return;

	sc->sc_ampdu_tx[tid].active = 0;
	ni->ni_tx_ba[tid].ba_state = IEEE80211_BA_INIT;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "ampdu tx stop TID %d\n", tid);
}

/* ------------------------------------------------------------------ */
/* RF Kill Switch                                                        */
/* ------------------------------------------------------------------ */

/*
 * Check if the hardware RF kill switch is active.
 * MacBook Pro 9,2 has the RF kill on GPIO 0 of ChipCommon.
 * GPIO low = radio enabled, GPIO high = radio killed.
 *
 * Returns 1 if radio is killed, 0 if enabled.
 */
int
b43bsd_rfkill_check(struct b43bsd_softc *sc)
{
	uint32_t gpio_in;

	if (sc->sc_ssb == NULL || sc->sc_ssb->chipcommon_idx < 0)
		return 0;

	/*
	 * Read GPIO input register. GPIO 0 is typically the RF kill line
	 * on MacBook boards. Active high = radio disabled.
	 */
	gpio_in = ssb_read32(sc->sc_ssb, SSB_GPIO_IN);

	/*
	 * BCM4331 on MacBook: GPIO bit 0 is RF kill.
	 * Some boards use inverted logic — check SPROM boardflags.
	 */
	if (gpio_in & (1 << B43BSD_RFKILL_GPIO)) {
		sc->sc_rfkill = 1;
		return 1;
	}

	sc->sc_rfkill = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* MAC Statistics Collection                                             */
/* ------------------------------------------------------------------ */

/*
 * Collect hardware MAC statistics.
 * Reads counters from the 802.11 core's statistics registers.
 */
void
b43bsd_stats_collect(struct b43bsd_softc *sc)
{
	/*
	 * MAC statistics registers are at offset 0x0400-0x0500 in the
	 * 802.11 core register space. These counters are 32-bit and
	 * wrap at 2^32.
	 *
	 * Register map:
	 *   0x0400: TX frame count
	 *   0x0404: TX byte count (lower 32)
	 *   0x0408: TX error count
	 *   0x040C: TX retry count
	 *   0x0410: RX frame count
	 *   0x0414: RX byte count (lower 32)
	 *   0x0418: RX error count
	 *   0x041C: RX CRC error count
	 *   0x0420: RX PHY error count
	 *   0x0424: RX FIFO overflow count
	 *   0x0428: RX decrypt error count
	 */

	sc->sc_stats.tx_frames = b43bsd_read32(sc, 0x0400);
	sc->sc_stats.tx_bytes = b43bsd_read32(sc, 0x0404);
	sc->sc_stats.tx_errors = b43bsd_read32(sc, 0x0408);
	sc->sc_stats.tx_retries = b43bsd_read32(sc, 0x040C);
	sc->sc_stats.rx_frames = b43bsd_read32(sc, 0x0410);
	sc->sc_stats.rx_bytes = b43bsd_read32(sc, 0x0414);
	sc->sc_stats.rx_errors = b43bsd_read32(sc, 0x0418);
	sc->sc_stats.rx_crc_errors = b43bsd_read32(sc, 0x041C);
	sc->sc_stats.rx_phy_errors = b43bsd_read32(sc, 0x0420);
	sc->sc_stats.rx_fifo_overflow = b43bsd_read32(sc, 0x0424);
	sc->sc_stats.rx_decrypt_errors = b43bsd_read32(sc, 0x0428);
	sc->sc_stats.beacon_missed = b43bsd_read32(sc, 0x0430);
	sc->sc_stats.tsf_sync_lost = b43bsd_read32(sc, 0x0434);
}

/* ------------------------------------------------------------------ */
/* LED GPIO Control (MacBook WiFi LED)                                   */
/* ------------------------------------------------------------------ */

/*
 * MacBook WiFi LED GPIO pins:
 *   GPIO 2 = activity LED (on chassis)
 *   GPIO 3 = radio LED
 * Control via SSB GPIO registers in the PCI config space window.
 */

void
b43bsd_led_on(struct b43bsd_softc *sc, int gpio)
{
	uint32_t val;

	/* Enable GPIO output. */
	val = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE);
	val |= (1 << gpio);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE, val);

	/* Set GPIO high (LED on). */
	val = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT);
	val |= (1 << gpio);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, val);

	sc->sc_led_active = 1;
}

void
b43bsd_led_off(struct b43bsd_softc *sc, int gpio)
{
	uint32_t val;

	val = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT);
	val &= ~(1 << gpio);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, val);

	sc->sc_led_active = 0;
}

/*
 * Configure the WiFi LED based on driver state.
 */
void
b43bsd_led_update(struct b43bsd_softc *sc)
{
	if (sc->sc_ssb == NULL)
		return;

	if (sc->sc_flags & B43BSD_FLAG_RUNNING)
		b43bsd_led_on(sc, B43BSD_LED_GPIO);
	else
		b43bsd_led_off(sc, B43BSD_LED_GPIO);
}

/* ------------------------------------------------------------------ */
/* LED Trigger (called from state transitions)                           */
/* ------------------------------------------------------------------ */

void
b43bsd_led_attach(struct b43bsd_softc *sc)
{
	/* Initialize LED — turn on briefly at attach, then off. */
	if (sc->sc_ssb != NULL) {
		b43bsd_led_on(sc, B43BSD_LED_GPIO);
		delay(200000);	/* 200 ms */
		b43bsd_led_off(sc, B43BSD_LED_GPIO);
	}
}

/* ------------------------------------------------------------------ */
/* Debug Sysctl Setup                                                   */
/* ------------------------------------------------------------------ */

#ifdef B43BSD_DEBUG
#include <sys/sysctl.h>

void
b43bsd_sysctl_attach(struct b43bsd_softc *sc)
{
	/* Debug sysctl nodes registered in b43bsd_debug.c */
}
#else
void
b43bsd_sysctl_attach(struct b43bsd_softc *sc)
{
}
#endif
/* ------------------------------------------------------------------ */
/* MAC-Level Controls                                                    */
/* ------------------------------------------------------------------ */

/*
 * Set RTS/CTS threshold.
 * Frames larger than this threshold use RTS/CTS handshake.
 * 2346 = disabled (max frame size). 0 = always use RTS/CTS.
 */
void
b43bsd_set_rts_threshold(struct b43bsd_softc *sc, int threshold)
{
	if (threshold < 0) threshold = 2346;
	if (threshold > 2346) threshold = 2346;

	b43bsd_write32(sc, 0x0690, (uint32_t)threshold);
	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "RTS threshold set to %d\n", threshold);
}

/*
 * Set fragmentation threshold.
 * Frames larger than this threshold are fragmented.
 * 2346 = disabled. 256-2346 = valid range.
 */
void
b43bsd_set_frag_threshold(struct b43bsd_softc *sc, int threshold)
{
	if (threshold < 256) threshold = 256;
	if (threshold > 2346) threshold = 2346;

	b43bsd_write32(sc, 0x0694, (uint32_t)threshold);
	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "frag threshold set to %d\n", threshold);
}

/*
 * Set short/long retry limits.
 * Short retry: for frames shorter than RTS threshold.
 * Long retry: for frames longer than RTS threshold.
 */
void
b43bsd_set_retry_limits(struct b43bsd_softc *sc, int short_retry,
    int long_retry)
{
	if (short_retry < 0) short_retry = 7;
	if (long_retry < 0) long_retry = 4;

	b43bsd_write32(sc, 0x06A0, (uint32_t)short_retry);
	b43bsd_write32(sc, 0x06A4, (uint32_t)long_retry);
}

/* ------------------------------------------------------------------ */
/* Diagnostic Interfaces                                                 */
/* ------------------------------------------------------------------ */

/*
 * Dump firmware information.
 */
void
b43bsd_diag_fwinfo(struct b43bsd_softc *sc)
{
	printf("%s: firmware info:\n", sc->sc_dev.dv_xname);
	printf("  ucode rev %d patch %d\n", sc->sc_fw.rev, sc->sc_fw.patch);
	printf("  ucode size %zu initvals %zu band %zu pcm %zu\n",
	    sc->sc_fw.ucode_size, sc->sc_fw.initvals_size,
	    sc->sc_fw.initvals_band_size, sc->sc_fw.pcm_size);
	printf("  fw_cached=%d running=%d loaded=%d\n",
	    sc->sc_fw.fw_cached, sc->sc_fw.running, sc->sc_fw.loaded);

	if (sc->sc_fw.running) {
		uint16_t stat, capa;
		stat = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
		    B43BSD_SHM_UCODESTAT);
		capa = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
		    B43BSD_SHM_FWCAPA);
		printf("  ucode status 0x%04x capabilities 0x%04x\n",
		    stat, capa);
	}
}

/*
 * Dump PHY/radio information.
 */
void
b43bsd_diag_phyinfo(struct b43bsd_softc *sc)
{
	printf("%s: PHY/radio info:\n", sc->sc_dev.dv_xname);
	printf("  radio: manuf 0x%02x ver %d rev %d\n",
	    sc->sc_radio.manuf, sc->sc_radio.version,
	    sc->sc_radio.revision);
	printf("  PHY rev %d, TX ant %d RX ant %d\n",
	    sc->sc_radio.phy_rev, sc->sc_radio.txant,
	    sc->sc_radio.rxant);
	printf("  temperature: %d C\n", sc->sc_radio.radio_temp);
	printf("  RSSI offsets: C1=0x%02x C2=0x%02x\n",
	    sc->sc_radio.rssi_offset_c1, sc->sc_radio.rssi_offset_c2);
	printf("  TX power: 2.4 GHz %d dBm, 5 GHz %d dBm\n",
	    sc->sc_maxpwr_2ghz, sc->sc_maxpwr_5ghz);
}

/*
 * Dump DMA ring information.
 */
void
b43bsd_diag_dmainfo(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *tx = &sc->sc_txring;
	struct b43bsd_dma_ring *rx = &sc->sc_rxring;

	printf("%s: DMA info:\n", sc->sc_dev.dv_xname);
	printf("  TX ring: %d slots, cur_tx=%d used=%d paddr=0x%llx\n",
	    tx->nslots, tx->cur_tx, tx->used,
	    (unsigned long long)tx->ring_paddr);
	printf("  RX ring: %d slots, cur_rx=%d paddr=0x%llx\n",
	    rx->nslots, rx->cur_rx,
	    (unsigned long long)rx->ring_paddr);
}

/*
 * Dump MAC statistics.
 */
void
b43bsd_diag_macstats(struct b43bsd_softc *sc)
{
	b43bsd_stats_collect(sc);

	printf("%s: MAC statistics:\n", sc->sc_dev.dv_xname);
	printf("  TX: %u frames %u bytes %u errors %u retries\n",
	    sc->sc_stats.tx_frames, sc->sc_stats.tx_bytes,
	    sc->sc_stats.tx_errors, sc->sc_stats.tx_retries);
	printf("  RX: %u frames %u bytes %u errors %u CRC %u PHY %u "
	    "fifo_ovfl %u decrypt_err\n",
	    sc->sc_stats.rx_frames, sc->sc_stats.rx_bytes,
	    sc->sc_stats.rx_errors, sc->sc_stats.rx_crc_errors,
	    sc->sc_stats.rx_phy_errors, sc->sc_stats.rx_fifo_overflow,
	    sc->sc_stats.rx_decrypt_errors);
	printf("  beacon_missed=%u tsf_sync_lost=%u "
	    "dma_tx_stalls=%u dma_rx_stalls=%u\n",
	    sc->sc_stats.beacon_missed, sc->sc_stats.tsf_sync_lost,
	    sc->sc_stats.dma_tx_stalls, sc->sc_stats.dma_rx_stalls);
}

/*
 * Full diagnostic dump (for debugging).
 */
void
b43bsd_diag_dump_all(struct b43bsd_softc *sc)
{
	printf("=== B43BSD Diagnostic Dump ===\n");
	printf("Device: %s\n", sc->sc_dev.dv_xname);
	printf("Chip: BCM%x rev %d, CHIPID 0x%08x\n",
	    sc->sc_chipid & 0xffff, (sc->sc_chipid >> 16) & 0xf,
	    sc->sc_chipid);
	printf("Flags: 0x%08x Quirks: 0x%08x\n",
	    sc->sc_flags, sc->sc_quirks);
	printf("802.11 core offset: 0x%08x\n", sc->sc_11core_offset);
	printf("TSF mode: %d, RF kill: %d, Promisc: %d\n",
	    sc->sc_tsf_mode, sc->sc_rfkill, sc->sc_promisc);

	b43bsd_diag_fwinfo(sc);
	b43bsd_diag_phyinfo(sc);
	b43bsd_diag_dmainfo(sc);
	b43bsd_diag_macstats(sc);
	b43bsd_intr_stats(sc);

	printf("PS: enabled=%d sleeping=%d listen_interval=%d "
	    "wake_count=%u pspoll_count=%u\n",
	    sc->sc_ps.enabled, sc->sc_ps.sleeping,
	    sc->sc_ps.listen_interval,
	    sc->sc_ps.wake_count, sc->sc_ps.pspoll_count);

	printf("=== End Diagnostic Dump ===\n");
}

/* ------------------------------------------------------------------ */
/* GPIO Control (Reset, External PA, LNA)                                */
/* ------------------------------------------------------------------ */

/*
 * Control GPIO pins for external components.
 * BCM4331 on MacBook Pro 9,2 uses GPIO for:
 *   GPIO 0: RF kill switch (input)
 *   GPIO 1: Power amplifier enable
 *   GPIO 2: WiFi activity LED
 *   GPIO 3: External LNA enable
 *   GPIO 4: Antenna switch 0
 *   GPIO 5: Antenna switch 1
 *   GPIO 6: WL_ACTIVE (BT coexistence)
 *   GPIO 7: BT_ACTIVE (BT coexistence, input)
 */

/*
 * Enable external power amplifier.
 */
void
b43bsd_gpio_pa_enable(struct b43bsd_softc *sc, int enable)
{
	uint32_t out, outen;

	if (sc->sc_ssb == NULL)
		return;

	out = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT);
	outen = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE);

	/* GPIO 1 = PA enable. Configure as output. */
	outen |= (1 << 1);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE, outen);

	if (enable)
		out |= (1 << 1);
	else
		out &= ~(1 << 1);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, out);
}

/*
 * Enable external LNA.
 */
void
b43bsd_gpio_lna_enable(struct b43bsd_softc *sc, int enable)
{
	uint32_t out, outen;

	if (sc->sc_ssb == NULL)
		return;

	out = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT);
	outen = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE);

	/* GPIO 3 = LNA enable. Configure as output. */
	outen |= (1 << 3);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE, outen);

	if (enable)
		out |= (1 << 3);
	else
		out &= ~(1 << 3);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, out);
}

/*
 * Select antenna (0, 1, or auto).
 */
void
b43bsd_gpio_ant_select(struct b43bsd_softc *sc, int ant)
{
	uint32_t out, outen;

	if (sc->sc_ssb == NULL)
		return;

	out = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT);
	outen = ssb_read32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE);

	/* GPIO 4-5 = antenna switch. Configure as outputs. */
	outen |= (1 << 4) | (1 << 5);
	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT_ENABLE, outen);

	/* Clear antenna bits, then set. */
	out &= ~((1 << 4) | (1 << 5));

	switch (ant) {
	case 0:
		/* Antenna 0: GPIO4=0 GPIO5=0 */
		break;
	case 1:
		/* Antenna 1: GPIO4=1 GPIO5=0 */
		out |= (1 << 4);
		break;
	case 2:
		/* Antenna 2: GPIO4=0 GPIO5=1 */
		out |= (1 << 5);
		break;
	default:
		/* Auto: GPIO4=1 GPIO5=1 */
		out |= (1 << 4) | (1 << 5);
		break;
	}

	ssb_write32(sc->sc_ssb, SSB_GPIO_OUT, out);
}

/* ------------------------------------------------------------------ */
/* Beacon / Probe Frame Templates                                        */
/* ------------------------------------------------------------------ */

/*
 * IEEE 802.11 beacon frame template.
 * Used by firmware for automatic beacon generation in AP mode.
 * For STA mode, this is informational — the hardware doesn't
 * transmit beacons, but the template is used for TBTT timing.
 */
static const uint8_t beacon_template[] = {
	0x80, 0x00,				/* Frame Control: Beacon */
	0x00, 0x00,				/* Duration */
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	/* DA: Broadcast */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* SA (filled at runtime) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* BSSID (filled at runtime) */
	0x00, 0x00,				/* Sequence Control */
	/* Timestamp (8 bytes) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* Beacon Interval (2 bytes) */
	0x64, 0x00,				/* 100 TU */
	/* Capability Info (2 bytes) */
	0x01, 0x00,				/* ESS */
};

/*
 * IEEE 802.11 probe request template.
 * Used for active scanning.
 */
static const uint8_t __unused probe_req_template[] = {
	0x40, 0x00,				/* Frame Control: Probe Req */
	0x00, 0x00,				/* Duration */
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	/* DA: Broadcast */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* SA (filled) */
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,	/* BSSID: Wildcard */
	0x00, 0x00,				/* Sequence Control */
	/* SSID IE: tag=0, len=0 (wildcard) */
	0x00, 0x00,
	/* Supported Rates IE: tag=1, len=8 */
	0x01, 0x08,
	0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24,
	/* Extended Supported Rates IE */
	0x32, 0x04, 0x30, 0x48, 0x60, 0x6C,
	/* HT Capabilities IE (placeholder for net80211) */
};

/*
 * Upload beacon template to shared memory for firmware use.
 */
void
b43bsd_beacon_template_upload(struct b43bsd_softc *sc)
{
	unsigned int i;

	/*
	 * Write beacon template to SHM at offset 0x0100.
	 * Firmware uses this for beacon timing and, in AP mode,
	 * for automatic beacon generation.
	 */
	for (i = 0; i < sizeof(beacon_template); i += 2) {
		uint16_t w;

		w = (uint16_t)beacon_template[i] |
		    ((uint16_t)beacon_template[i + 1] << 8);
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    (uint16_t)(0x0100 + i / 2), w);
	}
}

/*
 * Expanded radio self-test with detailed results.
 */
void
b43bsd_radio_selftest_expanded(struct b43bsd_softc *sc)
{
	int result, temp;
	uint16_t vco, pll, rssi;

	printf("%s: expanded radio self-test:\n", sc->sc_dev.dv_xname);

	/* Basic self-test. */
	result = b43bsd_radio_selftest(sc);

	/* Read PLL lock status in detail. */
	pll = b43bsd_radio_reg_read(sc, 0x0008);
	printf("  PLL COM_CTL: 0x%04x (locked=%s)\n",
	    pll, (pll & 0x0002) ? "yes" : "NO");

	/* Read VCO calibration value. */
	vco = b43bsd_radio_reg_read(sc, 0x0060);
	printf("  VCO cal value: 0x%04x\n", vco);

	/* Read temperature. */
	temp = b43bsd_radio_read_temp(sc);
	printf("  Temperature: %d C\n", temp);

	/* Read RSSI. */
	rssi = b43bsd_radio_reg_read(sc, 0x8070);
	printf("  RX0 RSSI: 0x%02x (~%d dBm)\n",
	    rssi & 0xff, ((int)(rssi & 0xff) / 2) - 95);

	/* Run TX loopback test if self-test passed all 4 tests. */
	if (result == 0x0F) {
		printf("  Loopback test: ");
		if (b43bsd_radio_loopback_test(sc) == 0)
			printf("PASSED\n");
		else
			printf("FAILED\n");
	}

	printf("  Self-test summary: 0x%x (%s)\n", result,
	    result == 0x0F ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
}

/* ------------------------------------------------------------------ */
/* EDCA (Enhanced Distributed Channel Access) Parameters                 */
/* ------------------------------------------------------------------ */

/*
 * Configure EDCA parameters for each access category.
 * EDCA provides QoS by giving different channel access priorities:
 *   AC_VO (Voice): highest priority, shortest AIFS, smallest CW
 *   AC_VI (Video): high priority
 *   AC_BE (Best Effort): default
 *   AC_BK (Background): lowest priority
 */
void
b43bsd_edca_setup(struct b43bsd_softc *sc)
{
	/*
	 * EDCA parameter registers at MAC offset 0x0600-0x0630.
	 * Each AC has: AIFSN, CWmin, CWmax, TXOP limit.
	 *
	 * Values from 802.11e default EDCA parameter set.
	 */
	static const struct {
		uint16_t	aifsn;		/* Arbitration IFS Number */
		uint16_t	cwmin;		/* Contention Window min */
		uint16_t	cwmax;		/* Contention Window max */
		uint16_t	txop;		/* TX Opportunity (32µs units) */
	} edca_params[4] = {
		/* AC_BK */ { 7, 15, 1023, 0 },
		/* AC_BE */ { 3, 15, 63,   0 },
		/* AC_VI */ { 2, 7,  15,   94 },	/* 3.008 ms */
		/* AC_VO */ { 2, 3,  7,    47 },	/* 1.504 ms */
	};
	int ac;

	for (ac = 0; ac < 4; ac++) {
		uint32_t base = 0x0600 + ac * 16;

		b43bsd_write32(sc, base,
		    (edca_params[ac].aifsn & 0xF) |
		    ((edca_params[ac].cwmin & 0xF) << 4) |
		    ((edca_params[ac].cwmax & 0xF) << 8));

		b43bsd_write32(sc, base + 4, edca_params[ac].txop);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "EDCA parameters configured\n");
}

/* ------------------------------------------------------------------ */
/* QoS Null Frame Template                                               */
/* ------------------------------------------------------------------ */

/*
 * IEEE 802.11 QoS Null frame template.
 * Used for power-save signaling: STA sends QoS Null with PM bit
 * set to notify AP it's entering power save mode.
 */
static const uint8_t qos_null_template[] = {
	0x48, 0x01,	/* Frame Control: QoS Data + To DS */
	0x00, 0x00,	/* Duration */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* DA (AP BSSID, filled) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* SA (our MAC, filled) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* BSSID (filled) */
	0x00, 0x00,	/* Sequence Control */
	/* QoS Control: TID=0, EOSP=0, ACK Policy=Normal */
	0x00, 0x00,
};

/*
 * Send a QoS Null frame to the AP.
 * Used for power-save signaling.
 */
int
b43bsd_send_qos_null(struct b43bsd_softc *sc, int pm_bit)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;
	uint8_t *frm;

	if (ic->ic_bss == NULL)
		return EINVAL;

	ni = ic->ic_bss;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return ENOBUFS;

	m->m_pkthdr.len = m->m_len = sizeof(qos_null_template);
	memcpy(mtod(m, uint8_t *), qos_null_template,
	    sizeof(qos_null_template));

	frm = mtod(m, uint8_t *);

	/* Set PM bit if entering power save. */
	if (pm_bit)
		frm[1] |= 0x10;	/* Power Management bit in Frame Control */

	/* Fill addresses. */
	IEEE80211_ADDR_COPY(&frm[4], ni->ni_bssid);	/* DA */
	IEEE80211_ADDR_COPY(&frm[10], ic->ic_myaddr);	/* SA */
	IEEE80211_ADDR_COPY(&frm[16], ni->ni_bssid);	/* BSSID */

	return b43bsd_dma_tx_start(sc, m, ni, 0);
}

/* ------------------------------------------------------------------ */
/* Background Scan Support                                               */
/* ------------------------------------------------------------------ */

/*
 * Configure the hardware for background scanning.
 * BCM4331 can scan off-channel briefly (dwell time ~30ms) while
 * maintaining association. This allows periodic background scans
 * for roaming without disrupting data traffic.
 */
void
b43bsd_bgscan_setup(struct b43bsd_softc *sc)
{
	/*
	 * Enable off-channel dwell capability.
	 * The chip can tune to a different channel for brief periods
	 * and return to the BSS channel without disassociating.
	 */
	{
		uint32_t macctl;

		macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
		macctl |= B43_MACCTL_BEACON_PROMISC;	/* accept all beacons */
		b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
	}

	/*
	 * Set off-channel dwell time: 30ms.
	 * This is enough to hear a beacon on another channel.
	 */
	b43bsd_write32(sc, 0x0198, 30);	/* OFFCHAN_DWELL_MS */

	/*
	 * Set home channel dwell time: 200ms.
	 * We stay on our BSS channel most of the time.
	 */
	b43bsd_write32(sc, 0x019C, 200);	/* HOMECHAN_DWELL_MS */

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "background scan configured (off-chan 30ms, home 200ms)\n");
}

/* ------------------------------------------------------------------ */
/* Roam Trigger Configuration                                            */
/* ------------------------------------------------------------------ */

/*
 * Configure roam triggers based on signal quality.
 * When RSSI drops below the roam threshold, the driver should
 * initiate a scan to find a better AP.
 */
void
b43bsd_roam_setup(struct b43bsd_softc *sc, int rssi_threshold_dbm)
{
	if (rssi_threshold_dbm > 0) rssi_threshold_dbm = -75;
	if (rssi_threshold_dbm < -95) rssi_threshold_dbm = -95;

	/*
	 * Store roam threshold in hardware register.
	 * When RX RSSI consistently drops below this, the hardware
	 * can generate a roam interrupt.
	 */
	b43bsd_write32(sc, 0x01A8, (uint32_t)(rssi_threshold_dbm & 0xff));

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "roam threshold set to %d dBm\n", rssi_threshold_dbm);
}

/* ------------------------------------------------------------------ */
/* TX Ring Management                                                    */
/* ------------------------------------------------------------------ */

/*
 * Check if the TX ring has available slots.
 * Returns number of free slots.
 */
int
b43bsd_tx_ring_available(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;

	return ring->nslots - ring->used;
}

/*
 * Check if the TX ring is empty (all frames completed).
 */
int
b43bsd_tx_ring_empty(struct b43bsd_softc *sc)
{
	return (b43bsd_tx_ring_available(sc) ==
	    (int)sc->sc_txring.nslots);
}

/*
 * Get TX ring occupancy for flow control.
 * Returns 0-100 percentage.
 */
int
b43bsd_tx_ring_occupancy(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;

	if (ring->nslots == 0)
		return 0;

	return (ring->used * 100) / ring->nslots;
}

/*
 * Pause TX when ring is nearly full.
 */
void
b43bsd_tx_pause(struct b43bsd_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	ifp->if_flags |= IFF_OACTIVE;
	B43BSD_DPRINTF(sc, B43BSD_DBG_TX,
	    "TX paused (occupancy %d%%)\n",
	    b43bsd_tx_ring_occupancy(sc));
}

/*
 * Resume TX when ring has space.
 */
void
b43bsd_tx_resume(struct b43bsd_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_start(ifp);
}

/* ------------------------------------------------------------------ */
/* MAC Hardware Address Filter                                           */
/* ------------------------------------------------------------------ */

/*
 * Configure the MAC address filter.
 * BCM4331 has a programmable filter that can match:
 *   - Unicast (our MAC)
 *   - Multicast (hash-based)
 *   - Broadcast
 *   - Promiscuous (all frames)
 *   - Control frames
 */
void
b43bsd_mac_filter_setup(struct b43bsd_softc *sc)
{
	uint32_t ctl;

	/*
	 * MAC filter control at offset 0x03E8.
	 * Bit 0: Enable filter
	 * Bit 1: Accept broadcast
	 * Bit 2: Accept multicast (hash-based)
	 * Bit 3: Accept unicast to our MAC
	 * Bit 4: Accept control frames
	 */
	ctl = 0x00000001;	/* Enable filter */
	ctl |= 0x00000002;	/* Accept broadcast */
	ctl |= 0x00000004;	/* Accept multicast */
	ctl |= 0x00000008;	/* Accept unicast */

	if (sc->sc_promisc)
		ctl |= 0x00000010;	/* Promiscuous */

	b43bsd_write32(sc, 0x03E8, ctl);

	/*
	 * Program our MAC address into the filter.
	 * Registers at 0x03EC (low 4 bytes) and 0x03F0 (high 2 bytes).
	 */
	{
		uint8_t *mac = sc->sc_macaddr;
		uint32_t lo, hi;

		lo = mac[0] | (mac[1] << 8) | (mac[2] << 16) |
		    (mac[3] << 24);
		hi = mac[4] | (mac[5] << 8);

		b43bsd_write32(sc, 0x03EC, lo);
		b43bsd_write32(sc, 0x03F0, hi);
	}

	/*
	 * Program BSSID filter.
	 * Registers at 0x03F4 (low) and 0x03F8 (high).
	 * When associated, only accept frames with matching BSSID.
	 */
	if (sc->sc_ic.ic_bss != NULL) {
		uint8_t *bssid = sc->sc_ic.ic_bss->ni_bssid;
		uint32_t lo, hi;

		lo = bssid[0] | (bssid[1] << 8) | (bssid[2] << 16) |
		    (bssid[3] << 24);
		hi = bssid[4] | (bssid[5] << 8);

		b43bsd_write32(sc, 0x03F4, lo);
		b43bsd_write32(sc, 0x03F8, hi);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "MAC filter configured (promisc=%d)\n", sc->sc_promisc);
}

/* ------------------------------------------------------------------ */
/* Multicast Hash Filter                                                 */
/* ------------------------------------------------------------------ */

/*
 * Compute multicast hash from MAC address.
 */
static uint32_t
b43bsd_mcast_hash(const uint8_t *addr)
{
	uint32_t hash = 0;
	int i;

	/* Simple XOR-based hash of all 6 bytes. */
	for (i = 0; i < 6; i++)
		hash = (hash << 1) ^ addr[i];

	return hash & 0x3F;	/* 6-bit hash */
}

/*
 * Program multicast hash filter.
 * Sets bits in the 64-bit filter mask for each multicast address.
 */
void
b43bsd_mcast_filter_set(struct b43bsd_softc *sc)
{
	uint32_t hash_lo = 0, hash_hi = 0;
	struct arpcom *ac = &sc->sc_ic.ic_ac;
	struct ether_multi *enm;

	/*
	 * Build hash from multicast list.
	 * For each multicast address, compute hash and set bit.
	 */
	LIST_FOREACH(enm, &ac->ac_multiaddrs, enm_list) {
		uint32_t bit;

		bit = b43bsd_mcast_hash(enm->enm_addrlo);
		if (bit < 32)
			hash_lo |= (1 << bit);
		else
			hash_hi |= (1 << (bit - 32));
	}

	/* Also accept broadcast and all-nodes multicast. */
	hash_lo |= (1 << b43bsd_mcast_hash(etherbroadcastaddr));

	/* Write hash filter registers. */
	b43bsd_write32(sc, 0x0410, hash_lo);
	b43bsd_write32(sc, 0x0414, hash_hi);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "multicast filter: lo=0x%08x hi=0x%08x\n", hash_lo, hash_hi);
}

/* ------------------------------------------------------------------ */
/* Hardware Cryptographic Acceleration Setup                             */
/* ------------------------------------------------------------------ */

/*
 * Configure the hardware crypto engine for WEP/TKIP/CCMP.
 * BCM4331 has a dedicated crypto accelerator that handles
 * encryption/decryption in hardware, offloading the CPU.
 */
void
b43bsd_crypto_setup(struct b43bsd_softc *sc)
{
	uint32_t ctl;

	/*
	 * Crypto control register at MAC offset 0x03D0.
	 * Bit 0: Enable crypto engine
	 * Bit 1: Enable WEP
	 * Bit 2: Enable TKIP
	 * Bit 3: Enable CCMP (AES-CCM)
	 * Bit 4: Enable hardware MIC (TKIP Michael)
	 */
	ctl = 0x00000001;	/* Enable */
	ctl |= 0x00000002;	/* WEP */
	ctl |= 0x00000004;	/* TKIP */
	ctl |= 0x00000008;	/* CCMP */
	ctl |= 0x00000010;	/* HW MIC */

	b43bsd_write32(sc, 0x03D0, ctl);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "hardware crypto engine enabled (WEP/TKIP/CCMP)\n");
}

/*
 * Program a key into the hardware key table.
 * The key table has 4 entries at MAC offset 0x03E0, each 8×32-bit words.
 *
 * Entry layout:
 *   Word 0-3: Key data (16 bytes)
 *   Word 4:   Key control (algorithm, valid, group, TX)
 *   Word 5-6: TKIP MIC key (8 bytes, TKIP only)
 *   Word 7:   Reserved
 */
void
b43bsd_crypto_write_key(struct b43bsd_softc *sc, int slot,
    const uint8_t *key, int keylen, int cipher, int flags)
{
	uint32_t base = 0x03E0 + slot * 32;	/* 8 words per entry */
	uint32_t kctl = 0;
	int i;

	if (slot < 0 || slot >= 4)
		return;

	/* Write key data (first 16 bytes, zero-padded). */
	for (i = 0; i < 4; i++) {
		uint32_t w = 0;

		if (i * 4 < keylen)
			w |= key[i * 4];
		if (i * 4 + 1 < keylen)
			w |= (uint32_t)key[i * 4 + 1] << 8;
		if (i * 4 + 2 < keylen)
			w |= (uint32_t)key[i * 4 + 2] << 16;
		if (i * 4 + 3 < keylen)
			w |= (uint32_t)key[i * 4 + 3] << 24;

		b43bsd_write32(sc, base + i * 4, w);
	}

	/* Build key control word. */
	switch (cipher) {
	case IEEE80211_CIPHER_WEP40:
	case IEEE80211_CIPHER_WEP104:
		kctl |= 0x00000001;	/* WEP */
		break;
	case IEEE80211_CIPHER_TKIP:
		kctl |= 0x00000002;	/* TKIP */
		break;
	case IEEE80211_CIPHER_CCMP:
		kctl |= 0x00000003;	/* CCMP */
		break;
	}

	kctl |= 0x00000004;	/* Key valid */

	if (flags & IEEE80211_KEY_GROUP)
		kctl |= 0x00000008;	/* Group key */

	if (flags & IEEE80211_KEY_TX)
		kctl |= 0x00000010;	/* TX key */

	b43bsd_write32(sc, base + 16, kctl);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "crypto key slot %d: cipher=%d flags=0x%x len=%d\n",
	    slot, cipher, flags, keylen);
}

/*
 * Clear a key from the hardware key table.
 */
void
b43bsd_crypto_clear_key(struct b43bsd_softc *sc, int slot)
{
	uint32_t base = 0x03E0 + slot * 32;
	int i;

	if (slot < 0 || slot >= 4)
		return;

	/* Zero all 8 words. */
	for (i = 0; i < 8; i++)
		b43bsd_write32(sc, base + i * 4, 0);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "crypto key slot %d cleared\n", slot);
}

/*
 * Set TKIP Michael MIC key.
 * TKIP uses a separate 8-byte MIC key for message integrity.
 */
void
b43bsd_crypto_write_tkip_mic(struct b43bsd_softc *sc, int slot,
    const uint8_t *rx_mic, const uint8_t *tx_mic)
{
	uint32_t base = 0x03E0 + slot * 32;
	uint32_t lo, hi;

	if (slot < 0 || slot >= 4)
		return;

	/* RX MIC key at word 5. */
	if (rx_mic) {
		lo = rx_mic[0] | (rx_mic[1] << 8) |
		    (rx_mic[2] << 16) | (rx_mic[3] << 24);
		hi = rx_mic[4] | (rx_mic[5] << 8) |
		    (rx_mic[6] << 16) | (rx_mic[7] << 24);
		b43bsd_write32(sc, base + 20, lo);
		b43bsd_write32(sc, base + 24, hi);
	}

	/* TX MIC key at word 6. */
	if (tx_mic) {
		lo = tx_mic[0] | (tx_mic[1] << 8) |
		    (tx_mic[2] << 16) | (tx_mic[3] << 24);
		hi = tx_mic[4] | (tx_mic[5] << 8) |
		    (tx_mic[6] << 16) | (tx_mic[7] << 24);
		b43bsd_write32(sc, base + 28, lo);
		b43bsd_write32(sc, base + 32, hi);
	}
}

/* ------------------------------------------------------------------ */
/* Firmware Capabilities Query                                           */
/* ------------------------------------------------------------------ */

/*
 * Query firmware capabilities.
 * The firmware reports its supported features in shared memory.
 */
void
b43bsd_fw_caps_query(struct b43bsd_softc *sc)
{
	uint16_t capa;

	if (!sc->sc_fw.running)
		return;

	capa = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_FWCAPA);

	printf("%s: firmware capabilities: 0x%04x\n",
	    sc->sc_dev.dv_xname, capa);

	/* Decode capabilities. */
	if (capa & 0x0001) printf("  WME/QoS supported\n");
	if (capa & 0x0002) printf("  Hardware crypto supported\n");
	if (capa & 0x0004) printf("  Power save supported\n");
	if (capa & 0x0008) printf("  A-MPDU aggregation supported\n");
	if (capa & 0x0010) printf("  Block ACK supported\n");
	if (capa & 0x0020) printf("  TSF sync supported\n");
	if (capa & 0x0040) printf("  Beacon processing supported\n");
	if (capa & 0x0080) printf("  Probe response offload\n");
	if (capa & 0x0100) printf("  BT coexistence supported\n");
	if (capa & 0x0200) printf("  MIMO PS supported\n");
}

/* ------------------------------------------------------------------ */
/* MAC RX Control Register Setup                                         */
/* ------------------------------------------------------------------ */

/*
 * Configure RX control registers.
 * These control the MAC's receive behavior: frame types accepted,
 * RX filter mode, and hardware assist features.
 */
void
b43bsd_rx_control_setup(struct b43bsd_softc *sc)
{
	uint32_t rxctl;

	/*
	 * RX control register at offset 0x0420.
	 * Bit 0: Accept data frames
	 * Bit 1: Accept control frames (RTS/CTS/ACK)
	 * Bit 2: Accept management frames (beacon/probe/auth/assoc)
	 * Bit 3: Strip FCS (CRC) from received frames
	 * Bit 4: Accept frames with errors (for monitor mode)
	 * Bit 5: Enable RX overflow detection
	 * Bit 6: Auto-decrypt received frames (hardware crypto)
	 * Bit 7: Pass decrypted frames to host
	 */
	rxctl = 0x00000001;	/* Accept data */
	rxctl |= 0x00000002;	/* Accept control */
	rxctl |= 0x00000004;	/* Accept management */
	rxctl |= 0x00000008;	/* Strip FCS */
	rxctl |= 0x00000020;	/* RX overflow detection */
	rxctl |= 0x00000040;	/* Auto-decrypt */
	rxctl |= 0x00000080;	/* Pass decrypted */

	b43bsd_write32(sc, 0x0420, rxctl);

	/*
	 * RX filter control at offset 0x0424.
	 * Bit 0: Enable MAC address filter
	 * Bit 1: Accept broadcast
	 * Bit 2: Accept multicast (hash-based)
	 * Bit 3: Accept unicast to our MAC
	 * Bit 4: Accept beacons to any BSSID (for scanning)
	 * Bit 5: Accept probe responses
	 */
	{
		uint32_t flt;

		flt = 0x00000001;	/* Enable filter */
		flt |= 0x00000002;	/* Broadcast */
		flt |= 0x00000004;	/* Multicast */
		flt |= 0x00000008;	/* Unicast */
		flt |= 0x00000010;	/* Beacons */
		flt |= 0x00000020;	/* Probe responses */

		b43bsd_write32(sc, 0x0424, flt);
	}

	/*
	 * RX control 2 at offset 0x0428.
	 * Bit 0: Enable RX DMA
	 * Bit 1: Enable RX status header (30 bytes)
	 * Bit 2: Enable RX radiotap-style info
	 */
	b43bsd_write32(sc, 0x0428, 0x00000007);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "RX control registers configured\n");
}

/* ------------------------------------------------------------------ */
/* DMA Ring Diagnostics                                                  */
/* ------------------------------------------------------------------ */

/*
 * Walk the TX descriptor ring and report status.
 */
void
b43bsd_dma_tx_diag(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	int pending = 0, completed = 0, free_slots = 0;
	int i;

	printf("%s: TX ring diagnostics:\n", sc->sc_dev.dv_xname);

	for (i = 0; i < ring->nslots; i++) {
		struct b43bsd_dma_desc64 *desc = &ring->ring_desc[i];

		if (ring->tx_slot[i].m == NULL) {
			free_slots++;
			continue;
		}

		/* Check if hardware consumed this descriptor. */
		if (letoh32(desc->ctrl0) & (B43BSD_DMA64_DCTL0_FRAMESTART |
		    B43BSD_DMA64_DCTL0_FRAMEEND)) {
			pending++;
		} else {
			completed++;

			/* Report rate info. */
			if (ring->tx_slot[i].ni != NULL) {
				completed++;
			}
		}
	}

	printf("  slots=%d free=%d pending=%d completed=%d used=%d\n",
	    ring->nslots, free_slots, pending, completed, ring->used);
	printf("  cur_tx=%d occupancy=%d%%\n",
	    ring->cur_tx, b43bsd_tx_ring_occupancy(sc));
}

/*
 * Walk the RX descriptor ring and report status.
 */
void
b43bsd_dma_rx_diag(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_rxring;
	int empty = 0, filled = 0;
	int i;

	printf("%s: RX ring diagnostics:\n", sc->sc_dev.dv_xname);

	for (i = 0; i < ring->nslots; i++) {
		if (ring->rx_slot[i].m == NULL)
			empty++;
		else
			filled++;
	}

	printf("  slots=%d empty=%d filled=%d cur_rx=%d\n",
	    ring->nslots, empty, filled, ring->cur_rx);

	/* Read RX status register. */
	{
		uint32_t rx_status;

		rx_status = b43bsd_read32(sc, B43BSD_DMA64_RX_STATUS);
		printf("  RX status: 0x%08x (stopped=%d error=%d)\n",
		    rx_status,
		    (rx_status & B43BSD_DMA64_STAT_STOPPED) ? 1 : 0,
		    (rx_status & B43BSD_DMA64_STAT_ERROR) ? 1 : 0);
	}
}

/* ------------------------------------------------------------------ */
/* MAC Power Management                                                  */
/* ------------------------------------------------------------------ */

/*
 * Configure MAC-level power management.
 * Controls when the MAC can enter low-power states.
 */
void
b43bsd_mac_power_mgmt_setup(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);

	/*
	 * Enable hardware PS mode.
	 * MACCTL_HW_PS: Hardware controls power state transitions.
	 * MACCTL_AWAKE: Keep MAC awake initially.
	 */
	macctl |= B43_MACCTL_HW_PS;
	macctl |= B43_MACCTL_AWAKE;

	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "MAC power management configured\n");
}

/*
 * Force MAC awake (override PS).
 */
void
b43bsd_mac_force_awake(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_AWAKE;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
}

/*
 * Allow MAC to sleep.
 */
void
b43bsd_mac_allow_sleep(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_AWAKE;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);
}

/* ------------------------------------------------------------------ */
/* MAC Scan State Machine                                                */
/* ------------------------------------------------------------------ */

/*
 * Configure MAC for active scanning.
 * Sets the MAC to accept beacons and probe responses from any BSSID.
 */
void
b43bsd_mac_scan_start(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	/* Enable promiscuous beacon reception. */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_BEACON_PROMISC;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/* Disable BSSID filter to accept all frames. */
	b43bsd_write32(sc, 0x03E8, 0x00000000);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "MAC scan mode enabled\n");
}

/*
 * Restore MAC to associated mode after scan.
 */
void
b43bsd_mac_scan_stop(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	/* Disable promiscuous beacons. */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_BEACON_PROMISC;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/* Re-enable MAC filter. */
	b43bsd_mac_filter_setup(sc);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "MAC scan mode disabled\n");
}

/* ------------------------------------------------------------------ */
/* Firmware Error Counters                                               */
/* ------------------------------------------------------------------ */

/*
 * Read firmware error counters from shared memory.
 */
void
b43bsd_fw_error_counters(struct b43bsd_softc *sc)
{
	uint16_t fatal, nonfatal, cmd_timeout, bad_frames;

	if (!sc->sc_fw.running)
		return;

	/*
	 * Firmware error counters in SHM_SHARED:
	 *   0x0044: Fatal errors
	 *   0x0046: Non-fatal errors
	 *   0x0048: Command timeouts
	 *   0x004A: Bad frame count
	 */
	fatal = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, 0x0044);
	nonfatal = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, 0x0046);
	cmd_timeout = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, 0x0048);
	bad_frames = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED, 0x004A);

	if (fatal || nonfatal || cmd_timeout || bad_frames) {
		printf("%s: firmware errors: fatal=%u nonfatal=%u "
		    "cmd_timeout=%u bad_frames=%u\n",
		    sc->sc_dev.dv_xname,
		    fatal, nonfatal, cmd_timeout, bad_frames);
	}
}

/*
 * Clear firmware error counters.
 */
void
b43bsd_fw_error_clear(struct b43bsd_softc *sc)
{
	if (!sc->sc_fw.running)
		return;

	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, 0x0044, 0);
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, 0x0046, 0);
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, 0x0048, 0);
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, 0x004A, 0);
}

/* ------------------------------------------------------------------ */
/* Rate Adaptation Hooks                                                 */
/* ------------------------------------------------------------------ */

/*
 * Notify the hardware of the current rate selection.
 * Updates the MAC's transmit rate registers for the current MCS.
 */
void
b43bsd_rate_notify_hw(struct b43bsd_softc *sc, int mcs, int nss,
    int is_ht40, int is_sgi)
{
	uint32_t rate_reg;

	/*
	 * BCM4331 TX rate control register at offset 0x0698.
	 * Bits [3:0]: MCS index
	 * Bit 4: 40 MHz bandwidth
	 * Bit 5: Short guard interval
	 * Bits [7:6]: Number of spatial streams - 1
	 */
	rate_reg = (mcs & 0xF);
	if (is_ht40) rate_reg |= (1 << 4);
	if (is_sgi)  rate_reg |= (1 << 5);
	rate_reg |= ((nss - 1) & 0x3) << 6;

	b43bsd_write32(sc, 0x0698, rate_reg);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "rate notified: MCS %d, Nss %d, HT40=%d, SGI=%d\n",
	    mcs, nss, is_ht40, is_sgi);
}

/*
 * Read the current TX rate from hardware.
 */
void
b43bsd_rate_read_hw(struct b43bsd_softc *sc, int *mcs, int *nss,
    int *is_ht40, int *is_sgi)
{
	uint32_t rate_reg;

	rate_reg = b43bsd_read32(sc, 0x0698);

	*mcs = rate_reg & 0xF;
	*is_ht40 = (rate_reg >> 4) & 1;
	*is_sgi = (rate_reg >> 5) & 1;
	*nss = ((rate_reg >> 6) & 0x3) + 1;
}

/* ------------------------------------------------------------------ */
/* net80211 State Machine Extended Hooks                                 */
/* ------------------------------------------------------------------ */

/*
 * Extended newstate handler with additional MAC configuration
 * for each 802.11 state transition.
 */
void
b43bsd_newstate_extended(struct b43bsd_softc *sc,
    enum ieee80211_state ostate, enum ieee80211_state nstate)
{
	struct ieee80211com *ic = &sc->sc_ic;

	switch (nstate) {
	case IEEE80211_S_INIT:
		/* Full stop — disable everything. */
		b43bsd_mac_force_awake(sc);
		b43bsd_stop(sc);
		b43bsd_radio_sleep(sc);
		b43bsd_fw_error_clear(sc);
		break;

	case IEEE80211_S_SCAN:
		/* Enable scan mode. */
		b43bsd_mac_force_awake(sc);
		if (ostate == IEEE80211_S_INIT) {
			b43bsd_init(sc);
			b43bsd_radio_wake(sc);
		}
		b43bsd_mac_scan_start(sc);
		b43bsd_bgscan_setup(sc);
		break;

	case IEEE80211_S_AUTH:
		/* Authentication: keep scan mode for retries. */
		b43bsd_mac_force_awake(sc);
		b43bsd_edca_setup(sc);
		break;

	case IEEE80211_S_ASSOC:
		/* Association: switch to BSSID filter. */
		b43bsd_mac_force_awake(sc);
		if (ic->ic_bss != NULL) {
			b43bsd_set_bssid(sc, ic->ic_bss->ni_bssid);
		}
		break;

	case IEEE80211_S_RUN:
		/* Associated: full operational mode. */
		b43bsd_mac_scan_stop(sc);
		b43bsd_mac_filter_setup(sc);
		b43bsd_mcast_filter_set(sc);
		b43bsd_crypto_setup(sc);
		b43bsd_mac_power_mgmt_setup(sc);
		if (ic->ic_bss != NULL) {
			b43bsd_set_beacon_interval(sc,
			    ic->ic_bss->ni_intval);
			b43bsd_beacon_timer_setup(sc,
			    ic->ic_bss->ni_intval);
			b43bsd_roam_setup(sc, -75);
			b43bsd_fw_beacon_enable(sc, 1);
		}
		/* Allow PS after association. */
		b43bsd_mac_allow_sleep(sc);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* net80211 Key Management Extended                                      */
/* ------------------------------------------------------------------ */

/*
 * Extended set_key that uses hardware crypto.
 */
int
b43bsd_set_key_hw(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	int slot = k->k_id;

	if (slot >= 4)
		return EINVAL;

	/* Program key into hardware crypto engine. */
	b43bsd_crypto_write_key(sc, slot, k->k_key, k->k_len,
	    k->k_cipher, k->k_flags);

	/* For TKIP, also program MIC keys. */
	if (k->k_cipher == IEEE80211_CIPHER_TKIP && k->k_flags & IEEE80211_KEY_TX) {
		b43bsd_crypto_write_tkip_mic(sc, slot,
		    k->k_key + 16, k->k_key + 24);
	}

	return 0;
}

/*
 * Extended delete_key.
 */
void
b43bsd_delete_key_hw(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	b43bsd_crypto_clear_key(sc, k->k_id);
}

/* ------------------------------------------------------------------ */
/* A-MPDU Extended Session Management                                    */
/* ------------------------------------------------------------------ */

/*
 * Start RX Block ACK session with hardware setup.
 */
int
b43bsd_ampdu_rx_start_hw(struct ieee80211com *ic,
    struct ieee80211_node *ni, uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;
	struct ieee80211_rx_ba *ba;

	if (tid >= 8)
		return EINVAL;

	ba = &ni->ni_rx_ba[tid];

	/* Initialize reorder buffer. */
	b43bsd_ampdu_reorder_init(&sc->sc_rxring, tid);
	sc->sc_rxring.reorder[tid].active = 1;
	sc->sc_rxring.reorder[tid].win_start = ba->ba_winstart;
	sc->sc_rxring.reorder[tid].win_size = ba->ba_winsize;

	ba->ba_state = IEEE80211_BA_AGREED;

	/* Enable A-MPDU in RX DMA engine. */
	{
		uint32_t rxc;
		rxc = b43bsd_read32(sc, B43BSD_DMA64_RX_CTL);
		rxc |= 0x00010000;	/* A-MPDU enable */
		b43bsd_write32(sc, B43BSD_DMA64_RX_CTL, rxc);
	}

	return 0;
}

/*
 * Stop RX Block ACK session.
 */
void
b43bsd_ampdu_rx_stop_hw(struct ieee80211com *ic,
    struct ieee80211_node *ni, uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if (tid >= 8)
		return;

	/* Flush and deactivate reorder buffer. */
	b43bsd_ampdu_reorder_flush(sc, &sc->sc_rxring, tid);
	sc->sc_rxring.reorder[tid].active = 0;
	ni->ni_rx_ba[tid].ba_state = IEEE80211_BA_INIT;
}

/*
 * Start TX Block ACK session.
 */
int
b43bsd_ampdu_tx_start_hw(struct ieee80211com *ic,
    struct ieee80211_node *ni, uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if (tid >= 8)
		return EINVAL;

	sc->sc_ampdu_tx[tid].active = 1;
	ni->ni_tx_ba[tid].ba_state = IEEE80211_BA_AGREED;

	/* Enable A-MPDU in TX DMA engine. */
	{
		uint32_t txc;
		txc = b43bsd_read32(sc, B43BSD_DMA64_TX_CTL);
		txc |= 0x00010000;	/* A-MPDU enable */
		b43bsd_write32(sc, B43BSD_DMA64_TX_CTL, txc);
	}

	return 0;
}

/*
 * Stop TX Block ACK session.
 */
void
b43bsd_ampdu_tx_stop_hw(struct ieee80211com *ic,
    struct ieee80211_node *ni, uint8_t tid)
{
	struct b43bsd_softc *sc = ic->ic_softc;

	if (tid >= 8)
		return;

	sc->sc_ampdu_tx[tid].active = 0;
	ni->ni_tx_ba[tid].ba_state = IEEE80211_BA_INIT;
}

/* ------------------------------------------------------------------ */
/* Frame Type Filter Setup                                              */
/* ------------------------------------------------------------------ */

void
b43bsd_frame_filter_setup(struct b43bsd_softc *sc)
{
	uint32_t flt;

	flt = b43bsd_read32(sc, 0x0420);

	/* Accept all management subtypes. */
	flt |= 0x00000004;
	/* Accept all control subtypes. */
	flt |= 0x00000002;
	/* Accept all data subtypes. */
	flt |= 0x00000001;

	b43bsd_write32(sc, 0x0420, flt);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "frame filter: mgmt+ctrl+data accepted\n");
}

/* ------------------------------------------------------------------ */
/* Short/Long Preamble Control                                          */
/* ------------------------------------------------------------------ */

void
b43bsd_preamble_setup(struct b43bsd_softc *sc, int short_preamble)
{
	uint32_t macctl;

	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	if (short_preamble)
		macctl |= B43_MACCTL_GMODE;
	else
		macctl &= ~B43_MACCTL_GMODE;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "preamble: %s\n", short_preamble ? "short" : "long");
}

/* ------------------------------------------------------------------ */
/* ACK Timeout Control                                                  */
/* ------------------------------------------------------------------ */

void
b43bsd_ack_timeout_setup(struct b43bsd_softc *sc)
{
	/* ACK timeout = SIFS + slot_time + 2*propagation_delay */
	uint32_t ackto;

	/* 9µs SIFS + 9µs slot + 2µs prop = 20µs */
	ackto = 20;

	b43bsd_write32(sc, 0x0680, ackto);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "ACK timeout: %u us\n", ackto);
}

/* ------------------------------------------------------------------ */
/* NAV (Network Allocation Vector) Control                              */
/* ------------------------------------------------------------------ */

void
b43bsd_nav_setup(struct b43bsd_softc *sc)
{
	/*
	 * NAV registers at 0x0684.
	 * Bit 0: Enable NAV
	 * Bit 1: Reset NAV on TBTT
	 * Bit 2: Use long NAV for CTS-to-self
	 */
	b43bsd_write32(sc, 0x0684, 0x00000007);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE, "NAV enabled\n");
}

/* ------------------------------------------------------------------ */
/* Probe Response Template Upload                                       */
/* ------------------------------------------------------------------ */

static const uint8_t probe_resp_template[] = {
	0x50, 0x00,	/* Frame Control: Probe Response */
	0x00, 0x00,	/* Duration */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* DA (filled) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* SA (filled) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* BSSID (filled) */
	0x00, 0x00,	/* Sequence */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Timestamp */
	0x64, 0x00,	/* Beacon Interval: 100 TU */
	0x01, 0x00,	/* Capability: ESS */
};

void
b43bsd_probe_resp_template_upload(struct b43bsd_softc *sc)
{
	unsigned int i;

	for (i = 0; i < sizeof(probe_resp_template); i += 2) {
		uint16_t w = (uint16_t)probe_resp_template[i] |
		    ((uint16_t)probe_resp_template[i + 1] << 8);
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED,
		    (uint16_t)(0x0200 + i / 2), w);
	}
}

/* ------------------------------------------------------------------ */
/* AP Mode Support                                                      */
/* ------------------------------------------------------------------ */

/*
 * Configure the chip for Access Point (AP) mode.
 * AP mode requires:
 * - Beacon generation (hardware or software)
 * - Multiple station support (MAC address filter list)
 * - Power management response (DTIM beacon delivery)
 * - Protection mechanism (RTS/CTS, CTS-to-self)
 */
void
b43bsd_ap_mode_setup(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "configuring AP mode\n");

	/*
	 * 1. Configure MACCTL for AP mode.
	 *    - Clear INFRA (we are the infrastructure)
	 *    - Set HW_BEACON_TIMER for auto-beacon generation
	 *    - Enable beacon promiscuous for client tracking
	 */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_INFRA;
	macctl |= B43_MACCTL_HW_BEACON_TIMER;
	macctl |= B43_MACCTL_BEACON_PROMISC;
	macctl |= B43_MACCTL_GMODE;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/*
	 * 2. Upload beacon template to hardware.
	 *    The hardware will auto-transmit beacons at TBTT.
	 */
	b43bsd_beacon_template_upload(sc);

	/*
	 * 3. Set DTIM period (every 3 beacons) and
	 *    configure TIM (Traffic Indication Map).
	 */
	b43bsd_write32(sc, 0x0504, 3);		/* DTIM period */
	b43bsd_write32(sc, 0x0508, 0);		/* TIM bitmap offset */

	/*
	 * 4. Enable protection: RTS/CTS for OFDM rates,
	 *    CTS-to-self for HT rates.
	 */
	b43bsd_set_rts_threshold(sc, 2346);	/* default */
	b43bsd_write32(sc, 0x0690, 0x00000001); /* Protection enabled */

	/*
	 * 5. Enable MAC address filter for client list.
	 *    Initially allow all stations (promiscuous),
	 *    tighten as stations associate.
	 */
	b43bsd_write32(sc, 0x0420, 0x00000007); /* accept all */

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "AP mode configured\n");
}

/*
 * Handle a new station associating in AP mode.
 * Add station MAC to hardware filter for directed frame delivery.
 */
void
b43bsd_ap_station_add(struct b43bsd_softc *sc, const uint8_t *mac)
{
	uint32_t v;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "AP station add: %02x:%02x:%02x:%02x:%02x:%02x\n",
	    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	/*
	 * Write station MAC to the next available filter slot.
	 * Filter slots start at register 0x03F2, each 8 bytes apart.
	 * Slot 0 is the BSSID, slots 1-15 are station addresses.
	 */
	v = mac[0] | ((uint32_t)mac[1] << 8) |
	    ((uint32_t)mac[2] << 16) | ((uint32_t)mac[3] << 24);
	b43bsd_write32(sc, 0x03F2, v);
	v = ((uint32_t)mac[4] | ((uint32_t)mac[5] << 8));
	b43bsd_write32(sc, 0x03F6, v);

	/* Enable this filter slot. */
	{
		uint32_t ctl;

		ctl = b43bsd_read32(sc, 0x03E8);
		ctl |= 0x00000002;	/* Enable slot 1 */
		b43bsd_write32(sc, 0x03E8, ctl);
	}
}

/*
 * Remove a station from the hardware filter in AP mode.
 */
void
b43bsd_ap_station_remove(struct b43bsd_softc *sc)
{
	uint32_t ctl;

	ctl = b43bsd_read32(sc, 0x03E8);
	ctl &= ~0x00000002;
	b43bsd_write32(sc, 0x03E8, ctl);
}

/* ------------------------------------------------------------------ */
/* IBSS (Ad-Hoc) Mode Support                                           */
/* ------------------------------------------------------------------ */

/*
 * Configure the chip for IBSS (ad-hoc) mode.
 * IBSS requires:
 * - No INFRA bit (peer-to-peer, no AP)
 * - Beacon generation with randomized TBTT offset
 * - ATIM window for power saving
 * - BSSID = IBSS random BSSID
 */
void
b43bsd_ibss_mode_setup(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "configuring IBSS mode\n");

	/* Clear INFRA, set beacon timer and promisc. */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl &= ~B43_MACCTL_INFRA;
	macctl |= B43_MACCTL_HW_BEACON_TIMER;
	macctl |= B43_MACCTL_BEACON_PROMISC;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/*
	 * Set ATIM window (Ad-hoc Traffic Indication Map).
	 * ATIM window = 0 means no power saving in IBSS.
	 */
	b43bsd_write32(sc, 0x050C, 0);	/* ATIM window = 0 (no PS) */

	/*
	 * Randomize initial TBTT offset to avoid beacon collisions
	 * when multiple IBSS peers start simultaneously.
	 */
	{
		uint32_t tsf_low;
		int offset;

		tsf_low = b43bsd_read32(sc, B43_MMIO_TSF_LOW);
		offset = (tsf_low & 0x3FF) * 1024;  /* 0-1024 TU random */
		b43bsd_write32(sc, 0x0510, offset);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "IBSS mode configured\n");
}

/* ------------------------------------------------------------------ */
/* WDS (Wireless Distribution System) Support                           */
/* ------------------------------------------------------------------ */

/*
 * Configure WDS mode — 4-address frame format for wireless bridging.
 * WDS uses 4 MAC addresses (RA, TA, DA, SA) in each frame header,
 * allowing transparent Ethernet bridging over WiFi.
 */
void
b43bsd_wds_mode_setup(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "configuring WDS mode\n");

	/*
	 * WDS mode: set INFRA (like STA) but enable
	 * 4-address frame format in the MAC.
	 */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_INFRA;	/* STA-like mode */
	macctl &= ~B43_MACCTL_BEACON_PROMISC;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/* Enable 4-address frame format. */
	b43bsd_write32(sc, 0x0418, 0x00000001);	/* 4-addr enable */

	/* Accept all frames (bridge mode). */
	b43bsd_write32(sc, 0x0420, 0x00000007);

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "WDS mode configured\n");
}

/* ------------------------------------------------------------------ */
/* Monitor Mode (Full)                                                   */
/* ------------------------------------------------------------------ */

/*
 * Full monitor mode setup — capture all frames including
 * control frames, PLCP headers, and radiotap headers.
 */
void
b43bsd_monitor_full_setup(struct b43bsd_softc *sc)
{
	uint32_t macctl;

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "configuring full monitor mode\n");

	/* Enable promiscuous mode + beacon promisc. */
	macctl = b43bsd_read32(sc, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_BEACON_PROMISC;
	b43bsd_write32(sc, B43_MMIO_MACCTL, macctl);

	/* Accept ALL frames including control and bad frames. */
	b43bsd_write32(sc, 0x0420, 0x00000007);

	/* Disable hardware decryption (capture raw encrypted frames). */
	{
		int i;

		for (i = 0; i < B43BSD_MAX_KEYS; i++)
			b43bsd_crypto_clear_key(sc, i);
	}

	/* Enable PLCP header capture. */
	b43bsd_write32(sc, 0x0500, 0x00000001);	/* PLCP capture */

	/* Enable FCS (Frame Check Sequence) pass-through. */
	b43bsd_write32(sc, 0x0428, 0x00000001);	/* FCS passthrough */

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "full monitor mode configured\n");
}

/* ------------------------------------------------------------------ */
/* Extended Statistics                                                  */
/* ------------------------------------------------------------------ */

/*
 * Collect extended MAC statistics from hardware counters.
 * BCM4331 provides detailed per-queue and per-MCS statistics.
 */
void
b43bsd_stats_extended(struct b43bsd_softc *sc)
{
	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "collecting extended statistics\n");

	/* Read MAC-level stats registers. */
	sc->sc_stats.tx_frames += b43bsd_read32(sc, 0x0170);
	sc->sc_stats.rx_frames += b43bsd_read32(sc, 0x0174);
	sc->sc_stats.tx_errors += b43bsd_read32(sc, 0x0178);
	sc->sc_stats.rx_errors += b43bsd_read32(sc, 0x017C);
	sc->sc_stats.rx_crc_errors += b43bsd_read32(sc, 0x0180);
	sc->sc_stats.rx_phy_errors += b43bsd_read32(sc, 0x0184);
	sc->sc_stats.rx_fifo_overflow += b43bsd_read32(sc, 0x0188);
	sc->sc_stats.rx_decrypt_errors += b43bsd_read32(sc, 0x018C);

	/* Read per-queue TX stats. */
	{
		int q;

		for (q = 0; q < 4; q++) {
			uint32_t pending, completed;

			pending = b43bsd_read32(sc,
			    (uint16_t)(0x0190 + q * 8));
			completed = b43bsd_read32(sc,
			    (uint16_t)(0x0194 + q * 8));

			B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
			    "   Q%d: pending=%u completed=%u\n",
			    q, pending, completed);
		}
	}

	/* Read DMA engine statistics. */
	{
		uint32_t tx_stalls, rx_stalls;

		tx_stalls = b43bsd_read32(sc, B43BSD_DMA64_TX_STATUS);
		rx_stalls = b43bsd_read32(sc, B43BSD_DMA64_RX_STATUS);

		sc->sc_stats.dma_tx_stalls +=
		    (tx_stalls >> 16) & 0xFF;
		sc->sc_stats.dma_rx_stalls +=
		    (rx_stalls >> 16) & 0xFF;
	}

	/* Read TSF sync statistics. */
	sc->sc_stats.tsf_sync_lost +=
	    b43bsd_read32(sc, 0x0160);

	/* Reset hardware counters after reading. */
	b43bsd_write32(sc, 0x0150, 0x00000001);
}

/*
 * Print a summary of all collected statistics.
 */
void
b43bsd_stats_print(struct b43bsd_softc *sc)
{
	printf("%s: statistics:\n", sc->sc_dev.dv_xname);
	printf("  TX: %u frames, %u bytes, %u errors, %u retries\n",
	    sc->sc_stats.tx_frames, sc->sc_stats.tx_bytes,
	    sc->sc_stats.tx_errors, sc->sc_stats.tx_retries);
	printf("  RX: %u frames, %u bytes, %u errors\n",
	    sc->sc_stats.rx_frames, sc->sc_stats.rx_bytes,
	    sc->sc_stats.rx_errors);
	printf("  RX CRC errors: %u, PHY errors: %u\n",
	    sc->sc_stats.rx_crc_errors, sc->sc_stats.rx_phy_errors);
	printf("  RX FIFO overflow: %u, decrypt errors: %u\n",
	    sc->sc_stats.rx_fifo_overflow,
	    sc->sc_stats.rx_decrypt_errors);
	printf("  Beacons missed: %u, TSF sync lost: %u\n",
	    sc->sc_stats.beacon_missed, sc->sc_stats.tsf_sync_lost);
	printf("  DMA stalls: TX=%u RX=%u\n",
	    sc->sc_stats.dma_tx_stalls, sc->sc_stats.dma_rx_stalls);
}

/* ------------------------------------------------------------------ */
/* Radio Temperature Sensor                                             */
/* ------------------------------------------------------------------ */

/*
 * Read the BCM2056 temperature sensor.
 * Returns temperature in degrees Celsius.
 * The BCM2056 radio has an on-die temperature sensor accessible
 * via the SYN bank register 0x60 (PLL_VCOCAL12).
 */
int
b43bsd_radio_temp(struct b43bsd_softc *sc)
{
	uint16_t raw;
	int temp;

	raw = b43bsd_radio_reg_read(sc, 0x0060);

	/*
	 * BCM2056 temperature sensor formula:
	 *   temp_C = (raw - 312) * 50 / 128
	 */
	temp = ((int)(raw & 0x3FF) - 312) * 50 / 128;

	/* Clamp to reasonable range. */
	if (temp < -20) temp = -20;
	if (temp > 85) temp = 85;

	sc->sc_radio.radio_temp = temp;

	return temp;
}

/* ------------------------------------------------------------------ */
/* Adaptive Noise Immunity                                               */
/* ------------------------------------------------------------------ */

/*
 * Adaptive noise immunity: periodically monitor noise floor
 * and adjust sensitivity parameters to maintain performance
 * in noisy environments.
 */
void
b43bsd_adaptive_noise_immunity(struct b43bsd_softc *sc)
{
	int16_t noise_c1, noise_c2;
	int avg_noise;

	/*
	 * Read noise floor from both cores.
	 * Higher values → higher noise → worse conditions.
	 */
	noise_c1 = (int16_t)(nphy_read(sc, B43BSD_NPHY_RSSI1) & 0xFF);
	noise_c2 = (int16_t)(nphy_read(sc, B43BSD_NPHY_RSSI2) & 0xFF);
	avg_noise = ((int)noise_c1 + (int)noise_c2) / 2;

	/*
	 * Classification:
	 *   < 0xB0: quiet environment — maximize sensitivity
	 *   0xB0-0xD0: moderate noise — normal operation
	 *   > 0xD0: noisy environment — raise thresholds
	 */
	if (avg_noise < 0xB0) {
		/* Quiet — lower ED thresholds for better sensitivity. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x0044);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x0044);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x0044);

		/* Increase RX gain for weak signal detection. */
		nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, 0x7E7E);
		nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, 0x7E7E);
		nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, 0x7E7E);
	} else if (avg_noise > 0xD0) {
		/* Noisy — raise ED thresholds to avoid false CCA. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x0058);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x0058);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x0058);

		/* Reduce gain to avoid ADC saturation. */
		nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, 0x5A5A);
		nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, 0x5A5A);
		nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, 0x5A5A);

		/* Enable narrowband interference filter. */
		nphy_maskset(sc, 0x0B9, 0, 0x0001);
	} else {
		/* Moderate — defaults. */
		nphy_write(sc, B43BSD_NPHY_C1_EDTHRES, 0x004C);
		nphy_write(sc, B43BSD_NPHY_C2_EDTHRES, 0x004C);
		nphy_write(sc, B43BSD_NPHY_C3_EDTHRES, 0x004C);

		nphy_write(sc, B43BSD_NPHY_C1_INITGAIN, 0x6E6E);
		nphy_write(sc, B43BSD_NPHY_C2_INITGAIN, 0x6E6E);
		nphy_write(sc, B43BSD_NPHY_C3_INITGAIN, 0x6E6E);

		nphy_maskset(sc, 0x0B9, ~0x0001, 0);
	}

	sc->sc_radio.rssi_offset_c1 = noise_c1;
	sc->sc_radio.rssi_offset_c2 = noise_c2;

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "adaptive noise: avg=0x%02x\n", avg_noise);
}

/* ------------------------------------------------------------------ */
/* Link Quality Monitoring                                              */
/* ------------------------------------------------------------------ */

/*
 * Monitor link quality and report to upper layers.
 * Called periodically (every 5 seconds) when associated.
 */
void
b43bsd_link_quality_monitor(struct b43bsd_softc *sc)
{
	int rssi, noise, cqi;
	uint32_t tx_retries;

	rssi = b43bsd_phy_n_get_rssi(sc, 0);
	noise = (sc->sc_radio.rssi_offset_c1 & 0xFF) - 0xC0;
	cqi = b43bsd_phy_n_cqi_measure(sc);

	/* Read TX retry count. */
	tx_retries = b43bsd_read32(sc, 0x0178);

	/*
	 * If CQI drops below 30 for 3 consecutive samples,
	 * trigger a background scan for a better AP.
	 */
	if (cqi < 30) {
		static int low_cqi_count = 0;

		if (++low_cqi_count >= 3) {
			B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
			    "link quality poor (CQI=%d), "
			    "triggering roam\n", cqi);
			b43bsd_roam_setup(sc, -70);
			low_cqi_count = 0;
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_STATE,
	    "link quality: RSSI=%d dBm noise=%d CQI=%d "
	    "TX retries=%u\n",
	    rssi, noise, cqi, tx_retries);
}

/* ------------------------------------------------------------------ */
/* TX Power Per MCS Calibration                                          */
/* ------------------------------------------------------------------ */

/*
 * Calibrate TX power per MCS rate using closed-loop TSSI.
 * Different MCS rates have different PAR (Peak-to-Average Ratio),
 * requiring per-MCS power backoff to meet EVM requirements.
 */
void
b43bsd_txpower_per_mcs_cal(struct b43bsd_softc *sc, int is_5ghz)
{
	int mcs, nss;			/* nss: number of spatial streams */

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX power per-MCS calibration (%s)\n",
	    is_5ghz ? "5 GHz" : "2.4 GHz");

	for (nss = 1; nss <= 3; nss++) {
		int mcs_max;

		/* MCS range for this Nss. */
		if (nss == 1)      mcs_max = 7;   /* MCS 0-7 */
		else if (nss == 2) mcs_max = 15;  /* MCS 8-15 */
		else               mcs_max = 23;  /* MCS 16-23 */

		for (mcs = (nss - 1) * 8; mcs <= mcs_max; mcs++) {
			int base_dbm;
			int8_t *pwr_tab;

			base_dbm = is_5ghz ? sc->sc_maxpwr_5ghz :
			    sc->sc_maxpwr_2ghz;
			pwr_tab = is_5ghz ? sc->sc_mcs_pwr_5g :
			    sc->sc_mcs_pwr_2g;

			/* Apply per-MCS power offset from SPROM. */
			if (mcs < 24) {
				base_dbm += pwr_tab[mcs] / 4;
			}

			/*
			 * Higher MCS rates (64QAM, 5/6 coding)
			 * need more backoff for EVM compliance.
			 */
			if (mcs >= 5 && mcs <= 7)	/* 64QAM */
				base_dbm -= 1;
			if (mcs >= 13 && mcs <= 15)	/* 64QAM 2ss */
				base_dbm -= 2;
			if (mcs >= 21 && mcs <= 23)	/* 64QAM 3ss */
				base_dbm -= 3;

			/* Set TX power via TSSI loop. */
			b43bsd_phy_n_tssi_loop(sc, base_dbm);

			/* Store calibrated TSSI value. */
			{
				uint16_t tssi;

				tssi = nphy_read(sc,
				    B43BSD_NPHY_C1_TXPCTL_STAT);
				nphy_write(sc,
				    0x300 + (uint16_t)(mcs * 2), tssi);
			}
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_PHY,
	    "TX power per-MCS calibration complete\n");
}
