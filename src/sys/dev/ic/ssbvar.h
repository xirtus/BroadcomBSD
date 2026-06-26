/*	$OpenBSD: ssbvar.h,v 1.1 2026/06/24 xirtus Exp $	*/

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

#ifndef _DEV_IC_SSBVAR_H_
#define _DEV_IC_SSBVAR_H_

#include <sys/param.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/ic/ssbreg.h>

/*
 * Core descriptor — one per enumerated core on the SSB backplane.
 */
struct ssb_core {
	struct device		dev;
	uint16_t		id;		/* core ID (SSB_DEV_*) */
	uint16_t		rev;		/* core revision */
	uint32_t		base;		/* base address in backplane */
	uint32_t		wrap;		/* wrap address */
	uint32_t		tmslow;		/* cached TMSLOW value */
	uint32_t		tmshigh;	/* cached TMSHIGH value */
	int			index;		/* enumeration index */
};

#define SSB_MAX_CORES		16

/*
 * Core name lookup table entry.
 */
struct ssb_core_name {
	uint16_t	id;
	const char	*name;
};

/*
 * SSB bus softc — represents the entire SSB backplane.
 */
struct ssb_bus {
	struct device		dev;

	/* Bus space for MMIO access. */
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	bus_size_t		mapsize;

	/* Chip identification. */
	uint16_t		chip_id;
	uint16_t		chip_rev;
	uint16_t		chip_pkg;

	/* Backplane revision. */
	uint8_t			bp_rev;

	/* Enumerated cores. */
	struct ssb_core		cores[SSB_MAX_CORES];
	int			ncores;

	/* Index of key cores (-1 if not present). */
	int			chipcommon_idx;
	int			pcie_idx;
	int			ieee80211_idx;
	int			arm_cm3_idx;
	int			mimo_phy_idx;

	/* ChipCommon capabilities. */
	uint32_t		chipco_caps;
	int			chipco_pmu_rev;

	/* PMU crystal frequency (kHz). */
	uint32_t		crystal_freq;

	/* Whether the bus is in MIPS big-endian mode. */
	int			big_endian;

	/* Non-normalized ChipCommon backplane base (for BAR0_WIN). */
	uint32_t		cc_backplane_base;

	/* Whether MIMO PHY core needs BAR0_WIN switching. */
	int			phy_needs_window_switch;
};

/*
 * Register read/write helpers.
 */
static inline uint32_t
ssb_read32(struct ssb_bus *bus, uint32_t offset)
{
	return bus_space_read_4(bus->bst, bus->bsh, offset);
}

static inline void
ssb_write32(struct ssb_bus *bus, uint32_t offset, uint32_t val)
{
	bus_space_write_4(bus->bst, bus->bsh, offset, val);
}

static inline uint16_t
ssb_read16(struct ssb_bus *bus, uint32_t offset)
{
	uint32_t v;

	v = bus_space_read_4(bus->bst, bus->bsh, offset & ~3);
	v >>= (offset & 3) * 8;
	return (uint16_t)v;
}

static inline void
ssb_write16(struct ssb_bus *bus, uint32_t offset, uint16_t val)
{
	uint32_t v;
	uint32_t off = offset & ~3;

	v = bus_space_read_4(bus->bst, bus->bsh, off);
	switch (offset & 3) {
	case 0:
		v = (v & 0xffff0000) | val;
		break;
	case 2:
		v = (v & 0x0000ffff) | ((uint32_t)val << 16);
		break;
	}
	bus_space_write_4(bus->bst, bus->bsh, off, v);
}

/*
 * Core register read/write (offset within the core's register window).
 * Validates that the core has a valid base address before access.
 */
static inline uint32_t
ssb_core_read32(struct ssb_bus *bus, int core_idx, uint16_t offset)
{
	struct ssb_core *core = &bus->cores[core_idx];

	/* Validate: offset must be within 0x1000. */
	if (offset >= SSB_CORE_SIZE)
		return 0;
	return bus_space_read_4(bus->bst, bus->bsh, core->base + offset);
}

static inline void
ssb_core_write32(struct ssb_bus *bus, int core_idx, uint16_t offset,
    uint32_t val)
{
	struct ssb_core *core = &bus->cores[core_idx];

	if (offset >= SSB_CORE_SIZE)
		return;
	bus_space_write_4(bus->bst, bus->bsh, core->base + offset, val);
}

static inline uint16_t
ssb_core_read16(struct ssb_bus *bus, int core_idx, uint16_t offset)
{
	struct ssb_core *core = &bus->cores[core_idx];
	uint32_t v;
	uint32_t abs;

	if (offset >= SSB_CORE_SIZE)
		return 0;
	abs = core->base + offset;
	v = bus_space_read_4(bus->bst, bus->bsh, abs & ~3);
	v >>= (abs & 3) * 8;
	return (uint16_t)v;
}

static inline void
ssb_core_write16(struct ssb_bus *bus, int core_idx, uint16_t offset,
    uint16_t val)
{
	struct ssb_core *core = &bus->cores[core_idx];
	uint32_t v;
	uint32_t abs;

	if (offset >= SSB_CORE_SIZE)
		return;
	abs = core->base + offset;
	v = bus_space_read_4(bus->bst, bus->bsh, abs & ~3);
	switch (abs & 3) {
	case 0:
		v = (v & 0xffff0000) | val;
		break;
	case 2:
		v = (v & 0x0000ffff) | ((uint32_t)val << 16);
		break;
	}
	bus_space_write_4(bus->bst, bus->bsh, abs & ~3, v);
}

/*
 * SSB bus API.
 */
int	ssb_attach(struct ssb_bus *, bus_space_tag_t, bus_space_handle_t,
    bus_size_t);
int	ssb_pmu_init(struct ssb_bus *);
void	ssb_pmu_pll_reset(struct ssb_bus *);
void	ssb_pmu_set_crystal(struct ssb_bus *, uint32_t);
void	ssb_bcm4331_ext_pa_lines_ctl(struct ssb_bus *, int);
void	ssb_core_reset(struct ssb_bus *, int);
void	ssb_core_enable(struct ssb_bus *, int);
void	ssb_core_disable(struct ssb_bus *, int);
int	ssb_core_is_enabled(struct ssb_bus *, int);
int	ssb_core_find(struct ssb_bus *, uint16_t);
const char *ssb_core_name(uint16_t);
uint32_t ssb_phy_window_switch(struct ssb_bus *, pci_chipset_tag_t, pcitag_t);
void	ssb_phy_window_restore(struct ssb_bus *, pci_chipset_tag_t, pcitag_t, uint32_t);

#endif /* _DEV_IC_SSBVAR_H_ */
