/*	$OpenBSD: b43bsd_debug.c,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * Debug infrastructure — sysctls, statistics, register dumps.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/*
 * Debug level variable — defined in b43bsd.c.
 */
#ifdef B43BSD_DEBUG
extern int b43bsd_debug;
#endif

/*
 * Dump MMIO registers for debugging.
 */
void
b43bsd_debug_dump_regs(struct b43bsd_softc *sc)
{
	uint32_t off = sc->sc_11core_offset;

	printf("%s: === Register Dump ===\n", sc->sc_dev.dv_xname);
	printf("%s: CHIPID          = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, B43_CHIPID));
	printf("%s: GEN_IRQ_REASON  = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43_MMIO_GEN_IRQ_REASON));
	printf("%s: GEN_IRQ_MASK    = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43_MMIO_GEN_IRQ_MASK));
	printf("%s: MACCTL          = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43_MMIO_MACCTL));
	printf("%s: DMA64_TX_CTL    = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43BSD_DMA64_TX_CTL));
	printf("%s: DMA64_TX_STATUS = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43BSD_DMA64_TX_STATUS));
	printf("%s: DMA64_RX_CTL    = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43BSD_DMA64_RX_CTL));
	printf("%s: DMA64_RX_STATUS = 0x%08x\n",
	    sc->sc_dev.dv_xname,
	    bus_space_read_4(sc->sc_st, sc->sc_sh, off + B43BSD_DMA64_RX_STATUS));
}

/*
 * Dump SSB core table.
 */
void
b43bsd_debug_dump_cores(struct b43bsd_softc *sc)
{
	struct ssb_bus *bus = sc->sc_ssb;
	int i;

	if (bus == NULL)
		return;

	printf("%s: === SSB Core Table (n=%d) ===\n",
	    sc->sc_dev.dv_xname, bus->ncores);

	for (i = 0; i < bus->ncores; i++) {
		struct ssb_core *c = &bus->cores[i];

		printf("%s:   core %d: %s (id 0x%03x) rev %d "
		    "base 0x%08x\n",
		    sc->sc_dev.dv_xname, i,
		    ssb_core_name(c->id), c->id, c->rev, c->base);
	}
}

/*
 * Dump interrupt statistics.
 */
void
b43bsd_debug_dump_irq(struct b43bsd_softc *sc)
{
	printf("%s: interrupt count = %d, storm = %d\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_intr_count, sc->sc_irq_storm);
}
