/*	$OpenBSD: ssb.c,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * Sonics Silicon Backplane (SSB) bus driver.
 *
 * Ported from Linux drivers/ssb/ (GPLv2).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>

#include <dev/ic/ssbvar.h>

/* ------------------------------------------------------------------ */
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */

int	ssb_enum_cores(struct ssb_bus *);

/* ------------------------------------------------------------------ */
/* Core name lookup table (from Linux drivers/ssb/scan.c)               */
/* ------------------------------------------------------------------ */

static const struct ssb_core_name ssb_core_names[] = {
	{ 0x800, "ChipCommon" },
	{ 0x801, "ILine20" },
	{ 0x803, "SDRAM" },
	{ 0x804, "PCI" },
	{ 0x805, "MIPS" },
	{ 0x806, "Ethernet" },
	{ 0x807, "V90" },
	{ 0x808, "USB 1.1 Host/Dev" },
	{ 0x809, "ADSL" },
	{ 0x80A, "ILine100" },
	{ 0x80B, "IPSEC" },
	{ 0x80D, "PCMCIA" },
	{ 0x80E, "Internal Mem" },
	{ 0x80F, "MEMC SDRAM" },
	{ 0x811, "EXTIF" },
	{ 0x812, "IEEE 802.11" },
	{ 0x816, "MIPS 3302" },
	{ 0x817, "USB 1.1 Host" },
	{ 0x818, "USB 1.1 Dev" },
	{ 0x819, "USB 2.0 Host" },
	{ 0x81A, "USB 2.0 Dev" },
	{ 0x81B, "SDIO Host" },
	{ 0x81C, "RoboSwitch" },
	{ 0x81D, "PATA" },
	{ 0x81E, "SATA XOR" },
	{ 0x81F, "Gb Ethernet" },
	{ 0x820, "PCIe" },
	{ 0x821, "MIMO PHY" },
	{ 0x822, "SRAM Ctrl" },
	{ 0x823, "Mini MACPHY" },
	{ 0x824, "ARM 1176" },
	{ 0x825, "ARM 7TDMI" },
	{ 0x82A, "ARM Cortex-M3" },
	{ 0x000, NULL },
};

const char *
ssb_core_name(uint16_t id)
{
	int i;

	for (i = 0; ssb_core_names[i].name != NULL; i++) {
		if (ssb_core_names[i].id == id)
			return ssb_core_names[i].name;
	}
	return "Unknown";
}

/* ------------------------------------------------------------------ */
/* SSB Bus Attachment                                                   */
/* ------------------------------------------------------------------ */

int
ssb_attach(struct ssb_bus *bus, bus_space_tag_t bst, bus_space_handle_t bsh,
    bus_size_t mapsize)
{
	int error;

	bus->bst = bst;
	bus->bsh = bsh;
	bus->mapsize = mapsize;

	/* Read chip ID from ChipCommon core. */
	bus->chip_id = ssb_read32(bus, 0x0000);
	bus->chip_rev = (bus->chip_id >> 16) & 0xf;
	bus->chip_pkg = (bus->chip_id >> 20) & 0xf;
	bus->chip_id &= 0xffff;

	/* Read ChipCommon capabilities. */
	bus->chipco_caps = ssb_read32(bus, SSB_CHIPCO_CAPABILITIES);

	/* Check for PMU. */
	if (bus->chipco_caps & SSB_CHIPCO_CAP_PMU) {
		uint32_t pmucap;

		pmucap = ssb_read32(bus, SSB_CHIPCO_PMU_CAP);
		bus->chipco_pmu_rev = pmucap & SSB_CHIPCO_PMU_CAP_REV;
	}

	/* Enumerate cores. */
	error = ssb_enum_cores(bus);
	if (error != 0) {
		printf("%s: core enumeration failed\n",
		    bus->dev.dv_xname);
		return error;
	}

	/*
	 * If a PCIe core is present, enable it and disable CLKREQ.
	 * BCM4331 needs the PCIe core active for backplane access,
	 * and CLKREQ must be disabled to prevent link stalls.
	 */
	if (bus->pcie_idx >= 0) {
		uint32_t ctl;
		uint16_t val16;

		ssb_core_enable(bus, bus->pcie_idx);

		/* Disable CLKREQ to prevent PCIe L1 entry stalls. */
		ctl = ssb_core_read32(bus, bus->pcie_idx,
		    SSB_PCIE_CTL);
		ctl &= ~SSB_PCIE_CTL_CLKREQ;
		ssb_core_write32(bus, bus->pcie_idx,
		    SSB_PCIE_CTL, ctl);

		/* Allow exit from L2/L3-Ready state without PERST#.
		 * Required on BCM4331 to recover from D3cold/suspend. */
		val16 = ssb_core_read16(bus, bus->pcie_idx,
		    SSB_PCIE_SPROM(SSB_PCIE_SPROM_MISC_CONFIG));
		if (!(val16 & SSB_PCIE_SPROM_L23READY_EXIT_NOPERST)) {
			val16 |= SSB_PCIE_SPROM_L23READY_EXIT_NOPERST;
			ssb_core_write16(bus, bus->pcie_idx,
			    SSB_PCIE_SPROM(SSB_PCIE_SPROM_MISC_CONFIG),
			    val16);
		}
	}

	printf("%s: Sonics Silicon Backplane, chip BCM%x rev %d",
	    bus->dev.dv_xname, bus->chip_id, bus->chip_rev);
	if (bus->chipco_pmu_rev > 0)
		printf(", PMU rev %d", bus->chipco_pmu_rev);
	printf("\n");

	return 0;
}

/* ------------------------------------------------------------------ */
/* Core Enumeration                                                     */
/* ------------------------------------------------------------------ */

int
ssb_enum_cores(struct ssb_bus *bus)
{
	int i;
	uint32_t enum_base;

	bus->ncores = 0;
	bus->chipcommon_idx = -1;
	bus->pcie_idx = -1;
	bus->ieee80211_idx = -1;
	bus->arm_cm3_idx = -1;
	bus->mimo_phy_idx = -1;

	/*
	 * Enumerate all possible core slots.
	 * BAR0_WIN (PCI config 0x80) maps ChipCommon at BAR0 offset 0.
	 * Core enumeration slots are at SSB_CORE_SIZE (0x1000) offsets
	 * from BAR0 base.  A 16 KB BAR0 covers cores 0–3; 64 KB covers
	 * all 16 slots.  If the MIMO PHY core is not found, check
	 * sc_sz vs. the required window.
	 */
	enum_base = 0;

	for (i = 0; i < SSB_MAX_NR_CORES; i++) {
		uint32_t idhigh, idlow;
		uint16_t core_id;
		int core_rev;
		uint32_t admatch;
		struct ssb_core *core;
		uint32_t offset;

		offset = enum_base + (i * SSB_CORE_SIZE);

		/*
		 * BAR0 boundary check: if the next core slot's registers
		 * extend beyond the mapped window, stop enumerating.
		 * On MacBooks with 16 KB BAR0, slots 4+ are inaccessible.
		 */
		if (offset + SSB_CORE_SIZE > bus->mapsize)
			break;

		/* Read core identification. */
		idhigh = ssb_read32(bus, offset + SSB_IDHIGH);
		idlow = ssb_read32(bus, offset + SSB_IDLOW);

		/* Check for end of core list. */
		if (idhigh == 0xffffffff && idlow == 0xffffffff)
			break;

		if (idhigh == 0 && idlow == 0)
			continue;

		core_id = (idhigh & SSB_IDHIGH_CC) >> SSB_IDHIGH_CC_SHIFT;
		if (core_id == 0)
			continue;

		core_rev = (idhigh & SSB_IDHIGH_RCLO) |
		    ((idhigh & SSB_IDHIGH_RCHI) >> 4);

		core = &bus->cores[bus->ncores];
		core->id = core_id;
		core->rev = core_rev;
		core->index = bus->ncores;

		/* Get core base address from Address Match 0. */
		admatch = ssb_read32(bus, offset + SSB_ADMATCH0);
		if (admatch & SSB_ADM_EN) {
			int adm_type = admatch & SSB_ADM_TYPE;
			switch (adm_type) {
			case 0:
				core->base = admatch & SSB_ADM_BASE0;
				break;
			case 1:
				core->base = admatch & SSB_ADM_BASE1;
				break;
			case 2:
				core->base = admatch & SSB_ADM_BASE2;
				break;
			default:
				core->base = 0;
				break;
			}
		} else {
			core->base = 0;
		}
		core->wrap = core->base + SSB_CORE_SIZE;

		/* Cache TMSLOW/TMSHIGH. */
		core->tmslow = ssb_read32(bus, offset + SSB_TMSLOW);
		core->tmshigh = ssb_read32(bus, offset + SSB_TMSHIGH);

		/* Determine backplane revision. */
		if (core_id == SSB_COREID_CHIPCOMMON) {
			bus->bp_rev = (idlow & SSB_IDLOW_SSBREV) >> 28;
		}

		/* Track important cores. */
		switch (core_id) {
		case SSB_COREID_CHIPCOMMON:
			bus->chipcommon_idx = bus->ncores;
			break;
		case SSB_COREID_PCIE:
			bus->pcie_idx = bus->ncores;
			break;
		case SSB_COREID_80211:
			bus->ieee80211_idx = bus->ncores;
			break;
		case SSB_COREID_ARM_CM3:
			bus->arm_cm3_idx = bus->ncores;
			break;
		case SSB_COREID_MIMO_PHY:
			bus->mimo_phy_idx = bus->ncores;
			break;
		}

		bus->ncores++;
	}

	/*
	 * If the BAR0 window is too small to reach all core slots
	 * (e.g. 16 KB on MacBook Pro — slots 0-3 only, slot 4+ unreachable),
	 * the MIMO PHY core at slot 4 won't be found by the loop above.
	 *
	 * BCM4331 always places the MIMO PHY at slot 4 (backplane offset
	 * 0x4000 from ChipCommon).  Hard-code it when we stopped at the
	 * boundary and we have a BCM4331 chip.
	 */
	if (bus->mimo_phy_idx < 0 && bus->ncores > 0 &&
	    bus->ncores < SSB_MAX_NR_CORES &&
	    bus->chipcommon_idx >= 0 &&
	    bus->chip_id == 0x4331) {
		uint32_t phy_enum_off = bus->ncores * SSB_CORE_SIZE;

		/*
		 * Check if enumeration stopped because the next slot
		 * (ncores * 0x1000) would be at or beyond BAR0 end.
		 */
		if (phy_enum_off >= bus->mapsize ||
		    phy_enum_off + SSB_CORE_SIZE > bus->mapsize) {
			struct ssb_core *core;
			uint32_t cc_backplane;

			cc_backplane = bus->cores[
			    bus->chipcommon_idx].base;
			core = &bus->cores[bus->ncores];
			core->id = SSB_COREID_MIMO_PHY;
			core->rev = 16;	/* N-PHY rev 16 (BCM4331) */
			core->index = bus->ncores;
			/*
			 * Backplane base = ChipCommon base + slot offset.
			 * Will be normalized below to BAR0-relative.
			 */
			core->base = cc_backplane + phy_enum_off;
			core->wrap = core->base + SSB_CORE_SIZE;
			core->tmslow = 0;
			core->tmshigh = 0;
			bus->mimo_phy_idx = bus->ncores;
			bus->ncores++;

			printf("%s:   core%u: MIMO PHY rev %u "
			    "(hard-coded, beyond BAR0 window)\n",
			    bus->dev.dv_xname,
			    bus->ncores - 1, core->rev);
		}
	}

	/*
	 * Normalize core base addresses relative to ChipCommon.
	 * Save non-normalized cc_base for BAR0_WIN switching.
	 */
	if (bus->chipcommon_idx >= 0) {
		uint32_t cc_base;
		int j;

		cc_base = bus->cores[bus->chipcommon_idx].base;
		bus->cc_backplane_base = cc_base;
		for (j = 0; j < bus->ncores; j++)
			bus->cores[j].base -= cc_base;
	}

	/*
	 * Detect if MIMO PHY core is outside the BAR0 window.
	 * With 16KB BAR0, core slot 4 (offset 0x4000) is at the edge.
	 * The last readable register (IDHIGH at +0xFFC) is at 0x4FFC
	 * beyond 0x4000.  We need BAR0_WIN switching.
	 */
	bus->phy_needs_window_switch = 0;
	if (bus->mimo_phy_idx >= 0) {
		uint32_t phy_off = bus->cores[bus->mimo_phy_idx].base;

		if (phy_off + SSB_CORE_SIZE > bus->mapsize)
			bus->phy_needs_window_switch = 1;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Core Control Operations                                              */
/* ------------------------------------------------------------------ */

/*
 * Reset a core by asserting and de-asserting the reset bit.
 *
 * WARNING: The PCIe core must NOT be blindly reset because
 * asserting RESET on the PCIe core drops the PCIe link.  Link
 * retraining takes milliseconds; the microsecond delays here
 * are insufficient.  If the link is down when the next MMIO
 * access arrives, the CPU gets a PCIe completion timeout
 * which hangs the machine.
 *
 * For the PCIe core we only reset if the core is currently
 * in reset (RESET bit already set) and needs to be brought out.
 * Otherwise we leave it untouched.
 */
void
ssb_core_reset(struct ssb_bus *bus, int core_idx)
{
	uint32_t tmslow;

	if (core_idx < 0 || core_idx >= bus->ncores)
		return;

	tmslow = ssb_core_read32(bus, core_idx, SSB_TMSLOW);

	/*
	 * If the core is already out of reset with clock running,
	 * don't touch it.  Resetting a live core needlessly
	 * disrupts hardware state, and resetting a running
	 * PCIe core kills the PCIe link.
	 */
	if ((tmslow & (SSB_TMSLOW_RESET | SSB_TMSLOW_CLOCK)) ==
	    SSB_TMSLOW_CLOCK) {
		return;
	}

	/* Assert reset. */
	ssb_core_write32(bus, core_idx, SSB_TMSLOW,
	    tmslow | SSB_TMSLOW_RESET);

	/* Wait for reset to take effect. */
	if (bus->pcie_idx == core_idx)
		delay(1000);	/* PCIe: 1ms for link to go down */
	else
		delay(10);

	/* Clear any error state on TMSHIGH. */
	if (ssb_core_read32(bus, core_idx, SSB_TMSHIGH) &
	    (SSB_TMSHIGH_SERR | SSB_TMSHIGH_BUSY)) {
		ssb_core_write32(bus, core_idx, SSB_TMSHIGH, 0);
	}

	/* De-assert reset, keep clock and force gated clocks on. */
	ssb_core_write32(bus, core_idx, SSB_TMSLOW,
	    SSB_TMSLOW_CLOCK | SSB_TMSLOW_FGC);

	/* Wait for core to come out of reset. */
	if (bus->pcie_idx == core_idx)
		delay(50000);	/* PCIe: 50ms for link retraining */
	else if (bus->ieee80211_idx == core_idx)
		delay(1000);	/* 802.11 MAC: 1ms for digital logic */
	else
		delay(10);	/* Other cores: 10µs */

	/* Clear force gated clocks. */
	ssb_core_write32(bus, core_idx, SSB_TMSLOW,
	    SSB_TMSLOW_CLOCK);

	/* PCIe: verify link is back up by reading a known register. */
	if (bus->pcie_idx == core_idx) {
		int retry;

		for (retry = 0; retry < 100; retry++) {
			uint32_t v;

			v = ssb_core_read32(bus, core_idx, SSB_TMSLOW);
			if (v != 0xffffffff && v != 0)
				break;
			delay(1000);
		}
	}
}

/*
 * Enable a core: ensure clock is running and core is out of reset.
 */
void
ssb_core_enable(struct ssb_bus *bus, int core_idx)
{
	if (core_idx < 0 || core_idx >= bus->ncores)
		return;
	ssb_core_reset(bus, core_idx);
}

/*
 * Disable a core: assert reset.
 */
void
ssb_core_disable(struct ssb_bus *bus, int core_idx)
{
	uint32_t tmslow;

	if (core_idx < 0 || core_idx >= bus->ncores)
		return;

	tmslow = ssb_core_read32(bus, core_idx, SSB_TMSLOW);
	ssb_core_write32(bus, core_idx, SSB_TMSLOW,
	    tmslow | SSB_TMSLOW_RESET);
}

/*
 * Check if a core is currently enabled (clock running, not in reset).
 */
int
ssb_core_is_enabled(struct ssb_bus *bus, int core_idx)
{
	uint32_t tmslow;

	if (core_idx < 0 || core_idx >= bus->ncores)
		return 0;

	tmslow = ssb_core_read32(bus, core_idx, SSB_TMSLOW);
	return ((tmslow & (SSB_TMSLOW_RESET | SSB_TMSLOW_CLOCK)) ==
	    SSB_TMSLOW_CLOCK);
}

/*
 * Find a core by its device ID.
 */
int
ssb_core_find(struct ssb_bus *bus, uint16_t device_id)
{
	int i;

	for (i = 0; i < bus->ncores; i++) {
		if (bus->cores[i].id == device_id)
			return i;
	}
	return -1;
}

/* ------------------------------------------------------------------ */
/* PMU Initialization                                                   */
/* ------------------------------------------------------------------ */

/*
 * BCM4331 PMU resource up/down sequence.
 * Controls power-up/power-down ordering for on-chip regulators.
 */
static const struct ssb_pmu_res_updown ssb_pmu_res_updown_4331[] = {
	{ SSB_PMURES_4331_XTAL_PU,	0x1501 },
	{ SSB_PMURES_4331_CBUCK_PU,	0x0501 },
	{ SSB_PMURES_4331_CLDO_PU,	0x0501 },
	{ SSB_PMURES_4331_LNLDO1_PU,	0x0501 },
	{ SSB_PMURES_4331_LNLDO2_PU,	0x0501 },
	{ SSB_PMURES_4331_BBPLL_PU,	0x0501 },
	{ SSB_PMURES_4331_RFPLL_PU,	0x0501 },
	{ SSB_PMURES_4331_RX_PWRSW_PU,	0x0501 },
	{ SSB_PMURES_4331_TX_PWRSW_PU,	0x0501 },
	{ SSB_PMURES_4331_LOGEN_PWRSW_PU, 0x0501 },
	{ SSB_PMURES_4331_AFE_PWRSW_PU,	0x0501 },
	{ SSB_PMURES_4331_BBPLL_PWRSW_PU, 0x0501 },
};

/*
 * PMU crystal frequency table.
 * Maps XTALFREQ register values to kHz.
 */
static const struct ssb_pmu_xtalfreq ssb_pmu_xtalfreq_table[] = {
	{ 0, 20000 },
	{ 1, 25000 },
	{ 2, 26000 },
	{ 3, 38400 },
	{ 4, 40000 },
	{ 5, 37400 },
	{ 6, 52000 },
	{ 7, 54000 },
};

/*
 * Initialize the PMU resource up/down timers.
 * These control power sequencing when resources are requested/released.
 */
static void
ssb_pmu_res_updown_setup(struct ssb_bus *bus,
    const struct ssb_pmu_res_updown *tab, unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		uint32_t val;

		val = ssb_read32(bus,
		    SSB_CHIPCO_PMU_RES_UP_TIMER + (tab[i].resource * 8));
		val &= 0xffff0000;
		val |= tab[i].updown;
		ssb_write32(bus,
		    SSB_CHIPCO_PMU_RES_UP_TIMER + (tab[i].resource * 8), val);
	}
}

/*
 * Configure PMU resource dependencies.
 * Sets min/max resource masks to keep required resources powered.
 */
static void
ssb_pmu_resources_init(struct ssb_bus *bus)
{
	uint32_t min_msk = 0, max_msk = 0;

	switch (bus->chip_id) {
	case 0x4331:
		/*
		 * BCM4331: enable crystal, buck regulator, LDOs,
		 * and both PLLs as minimum resources.
		 */
		min_msk = (1 << SSB_PMURES_4331_XTAL_PU) |
			  (1 << SSB_PMURES_4331_CBUCK_PU) |
			  (1 << SSB_PMURES_4331_CLDO_PU) |
			  (1 << SSB_PMURES_4331_LNLDO1_PU) |
			  (1 << SSB_PMURES_4331_LNLDO2_PU) |
			  (1 << SSB_PMURES_4331_BBPLL_PU) |
			  (1 << SSB_PMURES_4331_RFPLL_PU);
		/* Allow everything to stay on if needed. */
		max_msk = 0xffffffff;
		break;
	default:
		/* Other chips: conservative defaults. */
		min_msk = (1 << SSB_PMURES_4331_XTAL_PU) |
			  (1 << SSB_PMURES_4331_BBPLL_PU);
		max_msk = 0xffffffff;
		break;
	}

	if (min_msk)
		ssb_write32(bus, SSB_CHIPCO_PMU_MINRES, min_msk);
	if (max_msk)
		ssb_write32(bus, SSB_CHIPCO_PMU_MAXRES, max_msk);
}

/*
 * Initialize the PMU PLL for BCM4331.
 * Programs crystal frequency and PLL divider settings.
 */
static void
ssb_pmu_pll_init(struct ssb_bus *bus)
{
	uint32_t pmuctl;
	unsigned int i;
	uint32_t xtalfreq = 0; /* 0 = auto-detect */

	/*
	 * BCM4331 uses a 20 MHz crystal on MacBook Pro.
	 * If not specified, read the current XTALFREQ from PMU_CTL.
	 */
	if (xtalfreq == 0) {
		uint32_t xf_idx;

		pmuctl = ssb_read32(bus, SSB_CHIPCO_PMU_CTL);
		xf_idx = (pmuctl & SSB_CHIPCO_PMU_CTL_XTALFREQ) >>
		    SSB_CHIPCO_PMU_CTL_XTALFREQ_SHIFT;
		for (i = 0; i <
		    sizeof(ssb_pmu_xtalfreq_table) / sizeof(ssb_pmu_xtalfreq_table[0]);
		    i++) {
			if (ssb_pmu_xtalfreq_table[i].xf == xf_idx)
				break;
		}
		if (i < sizeof(ssb_pmu_xtalfreq_table) /
		    sizeof(ssb_pmu_xtalfreq_table[0]))
			bus->crystal_freq =
			    ssb_pmu_xtalfreq_table[i].freq;
		else
			bus->crystal_freq = 20000; /* default 20 MHz */
	} else {
		bus->crystal_freq = xtalfreq;
		/* Program the XTALFREQ field. */
		pmuctl = ssb_read32(bus, SSB_CHIPCO_PMU_CTL);
		pmuctl &= ~SSB_CHIPCO_PMU_CTL_XTALFREQ;
		for (i = 0; i <
		    sizeof(ssb_pmu_xtalfreq_table) / sizeof(ssb_pmu_xtalfreq_table[0]);
		    i++) {
			if (ssb_pmu_xtalfreq_table[i].freq == xtalfreq) {
				pmuctl |= (ssb_pmu_xtalfreq_table[i].xf <<
				    SSB_CHIPCO_PMU_CTL_XTALFREQ_SHIFT);
				break;
			}
		}
		ssb_write32(bus, SSB_CHIPCO_PMU_CTL, pmuctl);
	}

	/*
	 * Set ILP divider: (crystal_freq + 127) / 128 - 1.
	 * This ensures the ILP clock runs at ~1 kHz.
	 */
	pmuctl = ssb_read32(bus, SSB_CHIPCO_PMU_CTL);
	pmuctl &= ~SSB_CHIPCO_PMU_CTL_ILP_DIV;
	pmuctl |= (((bus->crystal_freq + 127) / 128 - 1) <<
	    SSB_CHIPCO_PMU_CTL_ILP_DIV_SHIFT) &
	    SSB_CHIPCO_PMU_CTL_ILP_DIV;
	ssb_write32(bus, SSB_CHIPCO_PMU_CTL, pmuctl);

	printf("%s: PMU crystal %u kHz, PLL init\n",
	    bus->dev.dv_xname, bus->crystal_freq);

	/*
	 * BCM4331 spur avoidance PLL settings.
	 * Default mode 0 (160 MHz TSF clock) uses specific PLL divider
	 * values to avoid clock spurs that interfere with WiFi bands.
	 * Reference: Linux drivers/bcma/driver_chipcommon_pmu.c
	 */
	if (bus->chip_id == 0x4331) {
		/* Mode 0 (160 MHz): PLL_CTL0=0x11100014, PLL_CTL2=0x03000a08 */
		ssb_write32(bus, SSB_CHIPCO_PLLCTL_ADDR, 0);
		ssb_write32(bus, SSB_CHIPCO_PLLCTL_DATA, 0x11100014);
		ssb_write32(bus, SSB_CHIPCO_PLLCTL_ADDR, 2);
		ssb_write32(bus, SSB_CHIPCO_PLLCTL_DATA, 0x03000a08);
	}
}

/*
 * Full PMU initialization sequence for BCM4331.
 *
 * Reference: Linux drivers/ssb/driver_chipcommon_pmu.c
 * Reference: https://bcm-v4.sipsolutions.net/802.11/SSB/PmuInit
 */
int
ssb_pmu_init(struct ssb_bus *bus)
{
	uint32_t pmucap;

	if ((bus->chipco_caps & SSB_CHIPCO_CAP_PMU) == 0)
		return 0;

	pmucap = ssb_read32(bus, SSB_CHIPCO_PMU_CAP);
	bus->chipco_pmu_rev = pmucap & SSB_CHIPCO_PMU_CAP_REV;

	printf("%s: PMU rev %u (capabilities 0x%08x)\n",
	    bus->dev.dv_xname, bus->chipco_pmu_rev, pmucap);

	/*
	 * Step 1: Disable NOILPONW for rev > 1 PMU.
	 * On rev 1, keep ILP on during wait states (clear NOILPONW).
	 */
	if (bus->chipco_pmu_rev == 1) {
		uint32_t ctl;

		ctl = ssb_read32(bus, SSB_CHIPCO_PMU_CTL);
		ctl &= ~SSB_CHIPCO_PMU_CTL_NOILPONW;
		ssb_write32(bus, SSB_CHIPCO_PMU_CTL, ctl);
	} else {
		uint32_t ctl;

		ctl = ssb_read32(bus, SSB_CHIPCO_PMU_CTL);
		ctl |= SSB_CHIPCO_PMU_CTL_NOILPONW;
		ssb_write32(bus, SSB_CHIPCO_PMU_CTL, ctl);
	}

	/*
	 * Step 2: Initialize PLL and crystal frequency.
	 */
	ssb_pmu_pll_init(bus);

	/*
	 * Step 3: Set up power resource up/down sequencing.
	 */
	ssb_pmu_res_updown_setup(bus, ssb_pmu_res_updown_4331,
	    sizeof(ssb_pmu_res_updown_4331) /
	    sizeof(ssb_pmu_res_updown_4331[0]));

	/*
	 * Step 4: Configure resource dependency masks.
	 */
	ssb_pmu_resources_init(bus);

	/*
	 * Step 5: Enable external PA lines (required for TX on BCM4331).
	 * The external FEM/PA must be powered up before any transmission.
	 */
	ssb_bcm4331_ext_pa_lines_ctl(bus, 1);

	return 0;
}

/*
 * Reset the PHY PLL via the ChipCommon ChipCtrl register.
 * Required after certain PHY operations to re-lock the PLL.
 */
void
ssb_pmu_pll_reset(struct ssb_bus *bus)
{
	uint32_t tmp;

	if (bus->chipcommon_idx < 0)
		return;

	/*
	 * BCM4331 PHY PLL reset sequence:
	 * 1. Write address 0 to CHIPCTL_ADDR
	 * 2. Clear bit 2 (PLL reset)
	 * 3. Set bit 2 (take PLL out of reset)
	 * 4. Clear bit 2 (normal operation)
	 */
	tmp = 0;
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_ADDR, tmp);

	tmp = ssb_core_read32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA);
	tmp &= ~0x4;
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA, tmp);

	tmp |= 0x4;
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA, tmp);

	delay(10);

	tmp &= ~0x4;
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA, tmp);

	delay(10);
}

/*
 * Set the crystal frequency for PMU calculations.
 * Called early in attach to override auto-detection.
 */
void
ssb_pmu_set_crystal(struct ssb_bus *bus, uint32_t freq_khz)
{
	bus->crystal_freq = freq_khz;
}

/*
 * BCM4331 external PA (Power Amplifier) lines control.
 * The BCM4331 has external PA/FEM that must be enabled for TX.
 * Disable during SPROM access to prevent GPIO conflicts,
 * then re-enable after to allow TX operation.
 * Reference: Linux drivers/bcma/driver_chipcommon_pmu.c
 */
void
ssb_bcm4331_ext_pa_lines_ctl(struct ssb_bus *bus, int enable)
{
	uint32_t val;

	if (bus->chip_id != 0x4331 || bus->chipcommon_idx < 0)
		return;

	/* Select ChipCtrl register 0 (GPIO/PA control). */
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_ADDR, 0);

	val = ssb_core_read32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA);

	if (enable) {
		val |= SSB_CHIPCTL_4331_EXTPA_EN |
		       SSB_CHIPCTL_4331_EXTPA_EN2 |
		       SSB_CHIPCTL_4331_EXTPA_ON_GPIO2_5;
	} else {
		val &= ~(SSB_CHIPCTL_4331_EXTPA_EN |
		         SSB_CHIPCTL_4331_EXTPA_EN2 |
		         SSB_CHIPCTL_4331_EXTPA_ON_GPIO2_5);
	}

	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_CHIPCTL_DATA, val);
}

/* ------------------------------------------------------------------ */
/* PMU Extended Resource Management                                      */
/* ------------------------------------------------------------------ */

/*
 * Request a PMU resource with timeout.
 * Some resources (PLLs, LDOs) take time to stabilize after
 * power-on. This function powers up the resource and waits
 * for the ready indication.
 */
int
ssb_pmu_resource_up_extended(struct ssb_bus *bus, uint32_t resource,
    int timeout_us)
{
	uint32_t pmu_stat;
	int i;

	/* Write resource up request. */
	ssb_write32(bus, SSB_CHIPCO_PMU_CTL,
	    ssb_read32(bus, SSB_CHIPCO_PMU_CTL) | resource);

	/* Wait for resource to be ready. */
	for (i = 0; i < timeout_us / 10; i++) {
		pmu_stat = ssb_read32(bus, SSB_CHIPCO_PMU_STAT);
		if ((pmu_stat & resource) == resource)
			return 0;
		delay(10);
	}

	return ETIMEDOUT;
}

/*
 * Power down a PMU resource.
 */
void
ssb_pmu_resource_down(struct ssb_bus *bus, uint32_t resource)
{
	ssb_write32(bus, SSB_CHIPCO_PMU_CTL,
	    ssb_read32(bus, SSB_CHIPCO_PMU_CTL) & ~resource);
}

/*
 * Initialize the PMU crystal oscillator.
 * The XTAL is the root clock source for all PLLs.
 * Must be stable before any other PMU operation.
 */
int
ssb_pmu_xtal_init(struct ssb_bus *bus)
{
	uint32_t clkst;
	int i;

	/*
	 * BCM4331 XTAL is 20 MHz.
	 * PMU resource up sequence: XTAL → CBUCK → CLDO → LNLDO1 → ...
	 */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_XTAL_PU, 2000);

	/* Wait for XTAL to stabilize. */
	for (i = 0; i < 100; i++) {
		clkst = ssb_read32(bus, SSB_CHIPCO_CLKCTLST);
		if (clkst & 0x00000001)
			return 0;
		delay(100);
	}

	return ETIMEDOUT;
}

/*
 * Initialize all PMU resources for full operation.
 * Powers up the entire radio chain in the correct sequence:
 * XTAL → BBPLL → RFPLL → RX → TX → AFE → LOGEN
 */
int
ssb_pmu_full_init(struct ssb_bus *bus)
{
	int error;

	/* 1. Crystal oscillator — must be first. */
	error = ssb_pmu_xtal_init(bus);
	if (error)
		return error;

	/* 2. Core buck converter (main power rail). */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_CBUCK_PU, 1000);

	/* 3. Core LDOs (low-dropout regulators). */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_CLDO_PU, 500);
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_LNLDO1_PU, 500);
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_LNLDO2_PU, 500);

	/* 4. Baseband and RF PLLs. */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_BBPLL_PU, 1000);
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_RFPLL_PU, 1000);

	/* 5. RX/TX power switches. */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_RX_PWRSW_PU, 500);
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_TX_PWRSW_PU, 500);

	/* 6. Analog front-end. */
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_AFE_PWRSW_PU, 500);
	ssb_pmu_resource_up_extended(bus, SSB_PMURES_4331_LOGEN_PWRSW_PU, 500);

	/* 7. BBPLL power switch. */
	ssb_pmu_resource_up_extended(bus,
	    SSB_PMURES_4331_BBPLL_PWRSW_PU, 500);

	return 0;
}

/*
 * Power down all PMU resources (reverse order).
 */
void
ssb_pmu_full_shutdown(struct ssb_bus *bus)
{
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_BBPLL_PWRSW_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_LOGEN_PWRSW_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_AFE_PWRSW_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_TX_PWRSW_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_RX_PWRSW_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_RFPLL_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_BBPLL_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_LNLDO2_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_LNLDO1_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_CLDO_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_CBUCK_PU);
	ssb_pmu_resource_down(bus, SSB_PMURES_4331_XTAL_PU);
}

/* ------------------------------------------------------------------ */
/* ChipCommon Clock Status                                               */
/* ------------------------------------------------------------------ */

/*
 * Get the current clock status from ChipCommon.
 * Returns a bitmask:
 *   Bit 0: XTAL stable
 *   Bit 1: BBPLL locked
 *   Bit 2: RFPLL locked
 *   Bit 3: CPU PLL locked
 *   Bit 4: Memory PLL locked
 */
uint32_t
ssb_chipco_clk_status(struct ssb_bus *bus)
{
	return ssb_read32(bus, SSB_CHIPCO_CLKCTLST);
}

/*
 * Print ChipCommon capabilities.
 */
void
ssb_chipco_print_caps(struct ssb_bus *bus)
{
	uint32_t caps;

	caps = ssb_read32(bus, SSB_CHIPCO_CAPABILITIES);

	printf("%s: ChipCommon capabilities: 0x%08x\n",
	    bus->dev.dv_xname, caps);
	if (caps & SSB_CHIPCO_CAP_PMU)
		printf("  PMU present (rev %d)\n",
		    bus->chipco_pmu_rev);
	if (caps & 0x00000010)
		printf("  SPROM present\n");
	if (caps & 0x00000040)
		printf("  OTP present\n");
}

/* ------------------------------------------------------------------ */
/* BAR0_WIN Switching for MIMO PHY Core                                 */
/* ------------------------------------------------------------------ */

/*
 * Switch BAR0_WIN to expose the MIMO PHY core.
 * Returns the previous BAR0_WIN value for restore.
 *
 * On MacBooks with 16KB BAR0, the MIMO PHY core at slot 4
 * (BAR0 offset 0x4000) may be outside the mapped window.
 * BAR0_WIN (PCI config 0x80) selects which backplane region
 * appears at BAR0 offset 0.  We temporarily shift it so the
 * PHY core appears at BAR0+0x0000.
 *
 * Caller must hold the driver lock (interrupts masked or
 * in attach context) since ALL BAR0 accesses are redirected
 * during the switch.
 */
uint32_t
ssb_phy_window_switch(struct ssb_bus *bus, pci_chipset_tag_t pc,
    pcitag_t tag)
{
	uint32_t old_win, new_win;

	if (!bus->phy_needs_window_switch || bus->mimo_phy_idx < 0)
		return 0;

	/* Read current BAR0_WIN. */
	old_win = pci_conf_read(pc, tag, SSB_BAR0_WIN);

	/*
	 * Compute new BAR0_WIN: PHY core backplane base,
	 * aligned so that PHY registers start at BAR0+0x0000.
	 * The PHY core backplane base is its normalized base
	 * plus cc_backplane_base.
	 */
	new_win = bus->cores[bus->mimo_phy_idx].base +
	    bus->cc_backplane_base;

	pci_conf_write(pc, tag, SSB_BAR0_WIN, new_win);

	/*
	 * After BAR0_WIN change, flush pending bus transactions
	 * by reading back the register.
	 */
	pci_conf_read(pc, tag, SSB_BAR0_WIN);

	return old_win;
}

/*
 * Restore the original BAR0_WIN value.
 */
void
ssb_phy_window_restore(struct ssb_bus *bus, pci_chipset_tag_t pc,
    pcitag_t tag, uint32_t old_win)
{
	if (!bus->phy_needs_window_switch || bus->mimo_phy_idx < 0)
		return;

	pci_conf_write(pc, tag, SSB_BAR0_WIN, old_win);
	pci_conf_read(pc, tag, SSB_BAR0_WIN);
}
