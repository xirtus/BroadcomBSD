/*	$OpenBSD: b43bsd_dma.c,v 1.3 2026/06/25 xirtus Exp $	*/

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
 * BCM4331 64-bit DMA engine — ring management, TX, RX.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/dma.c (GPLv2).
 * Adapted for OpenBSD 7.9 bus_dma(9) API.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/b43bsd_dma.h>
#include <dev/ic/b43bsd_phy_n.h>
#include <dev/ic/b43bsd_rate.h>

/* ------------------------------------------------------------------ */
/* DMA Register Access                                                  */
/* ------------------------------------------------------------------ */

#define DMA_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (reg), (val))
#define DMA_READ(sc, reg) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (reg))

/* ------------------------------------------------------------------ */
/* DMA Descriptor Helpers                                               */
/* ------------------------------------------------------------------ */

/*
 * Write a 64-bit DMA descriptor.
 * ctrl0: flags (bits 31:14) + buffer length (bits 13:0)
 * ctrl1: address extension (bits 17:16 = addr[33:32])
 * addr_lo: address bits 31:0
 * addr_hi: address bits 63:32
 */
static inline void
dma_desc_write(struct b43bsd_dma_ring *ring, int slot,
    uint32_t ctrl, uint32_t len, bus_addr_t paddr)
{
	struct b43bsd_dma_desc64 *desc = &ring->ring_desc[slot];

	desc->ctrl0 = htole32(ctrl | (len & B43BSD_DMA64_DCTL0_LEN));
	desc->ctrl1 = htole32(((uint32_t)((paddr >> 32) & 0x3)) <<
	    B43BSD_DMA64_DCTL1_ADDREXT_SHIFT);
	desc->addr_lo = htole32((uint32_t)(paddr & 0xffffffffULL));
	desc->addr_hi = htole32((uint32_t)((paddr >> 32) & 0xffffffffULL));
}

/* ------------------------------------------------------------------ */
/* DMA Ring Allocation (OpenBSD 7.9 bus_dma)                            */
/* ------------------------------------------------------------------ */

static int
b43bsd_dma_alloc_ring(struct b43bsd_softc *sc, struct b43bsd_dma_ring *ring,
    int nslots)
{
	int error;

	ring->nslots = nslots;
	ring->cur_tx = 0;
	ring->cur_rx = 0;
	ring->used = 0;

	/* Allocate DMA map for descriptor ring. */
	error = bus_dmamap_create(sc->sc_dmat,
	    B43BSD_DMA64_RINGMEMSIZE, 1, B43BSD_DMA64_RINGMEMSIZE,
	    0, BUS_DMA_NOWAIT | BUS_DMA_64BIT, &ring->ring_map);
	if (error != 0)
		return error;

	/* Allocate DMA memory for descriptors. */
	error = bus_dmamem_alloc(sc->sc_dmat,
	    B43BSD_DMA64_RINGMEMSIZE, B43BSD_DMA64_RINGMEMSIZE, 0,
	    &ring->ring_seg, 1, &ring->ring_nsegs, BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail_map;

	/* Map DMA memory to kernel virtual address. */
	error = bus_dmamem_map(sc->sc_dmat, &ring->ring_seg,
	    ring->ring_nsegs, B43BSD_DMA64_RINGMEMSIZE,
	    (caddr_t *)&ring->ring_desc, BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail_mem;

	/* Load the DMA map. */
	error = bus_dmamap_load(sc->sc_dmat, ring->ring_map,
	    (caddr_t)ring->ring_desc, B43BSD_DMA64_RINGMEMSIZE, NULL,
	    BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail_load;

	ring->ring_paddr = ring->ring_map->dm_segs[0].ds_addr;
	memset(ring->ring_desc, 0, B43BSD_DMA64_RINGMEMSIZE);

	return 0;

fail_load:
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->ring_desc,
	    B43BSD_DMA64_RINGMEMSIZE);
fail_mem:
	bus_dmamem_free(sc->sc_dmat, &ring->ring_seg, ring->ring_nsegs);
fail_map:
	bus_dmamap_destroy(sc->sc_dmat, ring->ring_map);
	return error;
}

static void
b43bsd_dma_free_ring(struct b43bsd_softc *sc, struct b43bsd_dma_ring *ring)
{
	int i;

	if (ring->ring_desc == NULL)
		return;

	/* Free any pending TX mbufs and destroy per-slot maps. */
	for (i = 0; i < ring->nslots; i++) {
		if (ring->tx_slot[i].m != NULL) {
			if (ring->tx_slot[i].map != NULL)
				bus_dmamap_unload(sc->sc_dmat,
				    ring->tx_slot[i].map);
			m_freem(ring->tx_slot[i].m);
			ring->tx_slot[i].m = NULL;
		}
		if (ring->tx_slot[i].map != NULL) {
			bus_dmamap_destroy(sc->sc_dmat,
			    ring->tx_slot[i].map);
			ring->tx_slot[i].map = NULL;
		}
		if (ring->tx_slot[i].ni != NULL) {
			ieee80211_release_node(&sc->sc_ic,
			    ring->tx_slot[i].ni);
			ring->tx_slot[i].ni = NULL;
		}
	}
	/* Free RX mbufs and destroy per-slot maps. */
	for (i = 0; i < ring->nslots; i++) {
		if (ring->rx_slot[i].m != NULL) {
			if (ring->rx_slot[i].map != NULL)
				bus_dmamap_unload(sc->sc_dmat,
				    ring->rx_slot[i].map);
			m_freem(ring->rx_slot[i].m);
			ring->rx_slot[i].m = NULL;
		}
		if (ring->rx_slot[i].map != NULL) {
			bus_dmamap_destroy(sc->sc_dmat,
			    ring->rx_slot[i].map);
			ring->rx_slot[i].map = NULL;
		}
	}

	bus_dmamap_unload(sc->sc_dmat, ring->ring_map);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->ring_desc,
	    B43BSD_DMA64_RINGMEMSIZE);
	bus_dmamem_free(sc->sc_dmat, &ring->ring_seg, ring->ring_nsegs);
	bus_dmamap_destroy(sc->sc_dmat, ring->ring_map);

	ring->ring_desc = NULL;
	ring->ring_map = NULL;
}

/* ------------------------------------------------------------------ */
/* DMA Initialization                                                   */
/* ------------------------------------------------------------------ */

int
b43bsd_dma_init(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *tring = &sc->sc_txring;
	struct b43bsd_dma_ring *rring = &sc->sc_rxring;
	int error, i;

	memset(tring, 0, sizeof(*tring));
	memset(rring, 0, sizeof(*rring));

	/* Allocate TX descriptor ring. */
	error = b43bsd_dma_alloc_ring(sc, tring, B43BSD_TX_SLOTS);
	if (error != 0)
		return error;

	/* Allocate RX descriptor ring. */
	error = b43bsd_dma_alloc_ring(sc, rring, B43BSD_RX_SLOTS);
	if (error != 0) {
		b43bsd_dma_free_ring(sc, tring);
		return error;
	}

	/* Create per-slot TX DMA maps (F2 fix).
	 * nsegments=8: enough for a jumbo 802.11 frame fragmented
	 * across an mbuf chain (typical WiFi frames use 1-2 segments).
	 */
	for (i = 0; i < tring->nslots; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    8, MCLBYTES, 0,
		    BUS_DMA_NOWAIT | BUS_DMA_64BIT,
		    &tring->tx_slot[i].map);
		if (error != 0) {
			b43bsd_dma_free_ring(sc, rring);
			b43bsd_dma_free_ring(sc, tring);
			return error;
		}
	}

	/* Create per-slot RX DMA maps (F3 fix). */
	for (i = 0; i < rring->nslots; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_64BIT,
		    &rring->rx_slot[i].map);
		if (error != 0) {
			b43bsd_dma_free_ring(sc, rring);
			b43bsd_dma_free_ring(sc, tring);
			return error;
		}
	}

	/* Program TX ring base address into hardware. */
	DMA_WRITE(sc, B43BSD_DMA64_TX_DESC_RINGLO,
	    (uint32_t)tring->ring_paddr);
	DMA_WRITE(sc, B43BSD_DMA64_TX_DESC_RINGHI,
	    (uint32_t)(tring->ring_paddr >> 32));
	DMA_WRITE(sc, B43BSD_DMA64_TX_INDEX, 0);

	/* Program RX ring base address into hardware. */
	DMA_WRITE(sc, B43BSD_DMA64_RX_DESC_RINGLO,
	    (uint32_t)rring->ring_paddr);
	DMA_WRITE(sc, B43BSD_DMA64_RX_DESC_RINGHI,
	    (uint32_t)(rring->ring_paddr >> 32));
	DMA_WRITE(sc, B43BSD_DMA64_RX_INDEX,
	    (uint32_t)rring->nslots);

	/* Fill RX ring with empty mbufs and load DMA maps. */
	b43bsd_dma_rx(sc);

	/* Enable DMA engines. */
	DMA_WRITE(sc, B43BSD_DMA64_TX_CTL, B43BSD_DMA64_TXENABLE);
	/*
	 * RX frame offset: 30 bytes (BCM4331 RX status header).
	 * The hardware prepends a 30-byte header containing
	 * RSSI, rate, and PHY status before the 802.11 frame.
	 * Offset is in 4-byte units, shifted left by 1.
	 */
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
	    B43BSD_DMA64_RXENABLE |
	    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
	    B43BSD_DMA64_RXFRAMEOFF));

	return 0;
}

void
b43bsd_dma_free(struct b43bsd_softc *sc)
{
	/* Disable DMA engines. */
	DMA_WRITE(sc, B43BSD_DMA64_TX_CTL, 0);
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL, 0);

	b43bsd_dma_free_ring(sc, &sc->sc_txring);
	b43bsd_dma_free_ring(sc, &sc->sc_rxring);
}

/* ------------------------------------------------------------------ */
/* TX Path                                                              */
/* ------------------------------------------------------------------ */

int
b43bsd_dma_tx_start(struct b43bsd_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, int rate_idx)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	int slot, error;

	if (m == NULL)
		return EINVAL;

	slot = ring->cur_tx;
	if (ring->tx_slot[slot].m != NULL)
		return ENOBUFS;
	if (ring->tx_slot[slot].map == NULL)
		return ENXIO;

	/* Load mbuf into pre-allocated DMA map (F2 fix). */
	error = bus_dmamap_load_mbuf(sc->sc_dmat,
	    ring->tx_slot[slot].map, m, BUS_DMA_NOWAIT);
	if (error != 0)
		return error;

	bus_dmamap_sync(sc->sc_dmat, ring->tx_slot[slot].map,
	    0, ring->tx_slot[slot].map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Write descriptor using physical DMA address.
	 * ctrl0: FRAMESTART|FRAMEEND|IRQ + length[13:0]
	 * ctrl1: address extension [17:16]
	 */
	dma_desc_write(ring, slot,
	    B43BSD_DMA64_DCTL0_DTABLEEND |
	    B43BSD_DMA64_DCTL0_FRAMESTART | B43BSD_DMA64_DCTL0_FRAMEEND |
	    B43BSD_DMA64_DCTL0_IRQ,
	    (uint32_t)m->m_pkthdr.len,
	    ring->tx_slot[slot].map->dm_segs[0].ds_addr);

	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    slot * sizeof(struct b43bsd_dma_desc64),
	    sizeof(struct b43bsd_dma_desc64),
	    BUS_DMASYNC_PREWRITE);

	ring->tx_slot[slot].m = m;
	ring->tx_slot[slot].ni = ni;
	ring->tx_slot[slot].len = m->m_pkthdr.len;
	ring->tx_slot[slot].rate_idx = rate_idx;

	ring->cur_tx = (slot + 1) % ring->nslots;
	ring->used++;

	/* Ring doorbell — advance TX index. */
	DMA_WRITE(sc, B43BSD_DMA64_TX_INDEX, ring->cur_tx);

	return 0;
}

void
b43bsd_dma_tx_done(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int i;

	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    0, B43BSD_DMA64_RINGMEMSIZE, BUS_DMASYNC_POSTREAD);

	for (i = 0; i < ring->nslots; i++) {
		struct b43bsd_dma_desc64 *desc = &ring->ring_desc[i];

		if (ring->tx_slot[i].m == NULL)
			continue;

		/* Check if descriptor has been consumed by hardware.
		 * DTABLEEND indicates ownership; hardware clears it on TX done.
		 */
		if (letoh32(desc->ctrl0) & B43BSD_DMA64_DCTL0_DTABLEEND)
			continue;

		bus_dmamap_sync(sc->sc_dmat, ring->tx_slot[i].map,
		    0, ring->tx_slot[i].map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->tx_slot[i].map);

		/* Feed rate control with TX result. */
		if (ring->tx_slot[i].ni != NULL) {
			b43bsd_rc_update(sc, ring->tx_slot[i].ni, 1, 0);
			ieee80211_release_node(&sc->sc_ic,
			    ring->tx_slot[i].ni);
			ring->tx_slot[i].ni = NULL;
		}
		m_freem(ring->tx_slot[i].m);
		ring->tx_slot[i].m = NULL;
		ring->used--;
	}

	/* If TX was stalled (IFF_OACTIVE) and space is now available, restart. */
	if (ifp->if_flags & IFF_OACTIVE) {
		if (ring->used < ring->nslots) {
			ifp->if_flags &= ~IFF_OACTIVE;
			ifp->if_start(ifp);
		}
	}
}

/* ------------------------------------------------------------------ */
/* RX Path                                                              */
/* ------------------------------------------------------------------ */

void
b43bsd_dma_rx(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_rxring;
	int i;

	/* Fill empty slots with new mbufs and load DMA maps (F3 fix). */
	for (i = 0; i < ring->nslots; i++) {
		struct mbuf *m;
		int slot = ring->cur_rx;

		if (ring->rx_slot[slot].m != NULL)
			continue;

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL)
			break;

		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			break;
		}

		/* Load mbuf into pre-allocated DMA map for physical addr. */
		if (bus_dmamap_load_mbuf(sc->sc_dmat,
		    ring->rx_slot[slot].map, m,
		    BUS_DMA_NOWAIT) != 0) {
			m_freem(m);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, ring->rx_slot[slot].map,
		    0, ring->rx_slot[slot].map->dm_mapsize,
		    BUS_DMASYNC_PREREAD);

		ring->rx_slot[slot].m = m;

		/* Write RX descriptor with physical DMA address.
		 * DTABLEEND gives hardware ownership; hardware clears it
		 * when the RX buffer is filled.  FRAMESTART marks frame start.
		 * Length goes in ctrl0[13:0].
		 */
		dma_desc_write(ring, slot,
		    B43BSD_DMA64_DCTL0_DTABLEEND |
		    B43BSD_DMA64_DCTL0_FRAMESTART |
		    B43BSD_DMA64_DCTL0_IRQ,
		    (uint32_t)MCLBYTES,
		    ring->rx_slot[slot].map->dm_segs[0].ds_addr);

		ring->cur_rx = (slot + 1) % ring->nslots;
	}

	/* Sync descriptors to device. */
	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    0, B43BSD_DMA64_RINGMEMSIZE, BUS_DMASYNC_PREWRITE);
}

/*
 * Process completed RX frames and pass them to net80211 (F6 fix).
 * Called from interrupt handler when B43_IRQ_RX_OK fires.
 */
void
b43bsd_dma_rx_process(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_rxring;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int i;

	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    0, B43BSD_DMA64_RINGMEMSIZE, BUS_DMASYNC_POSTREAD);

	for (i = 0; i < ring->nslots; i++) {
		struct b43bsd_dma_desc64 *desc = &ring->ring_desc[i];
		struct mbuf *m;
		struct ieee80211_node *ni;
		struct ieee80211_rxinfo rxi;

		if (ring->rx_slot[i].m == NULL)
			continue;

		/* Check if descriptor has been filled by hardware.
		 * DTABLEEND is the ownership bit: hardware clears it
		 * when the RX buffer is filled.
		 */
		if (letoh32(desc->ctrl0) & B43BSD_DMA64_DCTL0_DTABLEEND)
			continue;

		m = ring->rx_slot[i].m;

		bus_dmamap_sync(sc->sc_dmat, ring->rx_slot[i].map,
		    0, ring->rx_slot[i].map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, ring->rx_slot[i].map);

		ring->rx_slot[i].m = NULL;

		/*
		 * RXFRAMEOFF=30: hardware writes 30-byte status header
		 * at buffer start, 802.11 frame at offset 30.
		 * Advance m_data past the header.
		 */
		m->m_data += 30;
		m->m_pkthdr.len = m->m_len =
		    letoh32(desc->ctrl0) & B43BSD_DMA64_DCTL0_LEN;

		/* Build RX info with RSSI and channel. */
		memset(&rxi, 0, sizeof(rxi));
		rxi.rxi_rssi = b43bsd_phy_n_get_rssi(sc, 0);

		/* Find sender node, pass frame with RX info. */
		ni = ieee80211_find_rxnode(ic,
		    mtod(m, const struct ieee80211_frame *));
		ieee80211_input(ifp, m, ni, &rxi);
		if (ni != NULL)
			ieee80211_release_node(ic, ni);

		/* Replenish the slot. */
		b43bsd_dma_rx(sc);
	}
}

void
b43bsd_dma_rx_overflow(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_rxring;

	/* Reset RX engine. */
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL, 0);
	DMA_WRITE(sc, B43BSD_DMA64_RX_INDEX,
	    (uint32_t)ring->nslots);
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
	    B43BSD_DMA64_RXENABLE |
	    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
	    B43BSD_DMA64_RXFRAMEOFF));

	b43bsd_dma_rx(sc);
}

/* ------------------------------------------------------------------ */
/* A-MPDU Reorder Buffer                                                */
/* ------------------------------------------------------------------ */

/*
 * Initialize an A-MPDU reorder buffer for a TID.
 */
void
b43bsd_ampdu_reorder_init(struct b43bsd_dma_ring *ring, int tid)
{
	struct b43bsd_ampdu_reorder *reorder;

	if (tid < 0 || tid >= 8)
		return;
	reorder = &ring->reorder[tid];

	memset(reorder, 0, sizeof(*reorder));
	reorder->tid = tid;
	reorder->win_size = 64;
}

/*
 * Add a received A-MPDU subframe to the reorder buffer.
 * Returns:
 *   0: subframe buffered (not yet ready to deliver)
 *   1: a completed frame sequence is ready (call flush to get frames)
 *  -1: duplicate or out-of-window frame dropped
 */
int
b43bsd_ampdu_reorder_add(struct b43bsd_softc *sc,
    struct b43bsd_dma_ring *ring, int tid, uint16_t seq,
    struct mbuf *m)
{
	struct b43bsd_ampdu_reorder *reorder;
	uint16_t idx, rel;

	if (tid < 0 || tid >= 8)
		return -1;
	reorder = &ring->reorder[tid];

	if (!reorder->active)
		return -1;

	/*
	 * Check if sequence number is within the current window.
	 * Window = [win_start, win_start + win_size).
	 */
	rel = seq - reorder->win_start;
	if (rel >= reorder->win_size) {
		/* Outside reorder window — flush old and advance. */
		b43bsd_ampdu_reorder_flush(sc, ring, tid);
		reorder->win_start = seq;
		rel = 0;
	}

	/*
	 * If this seq is before win_start and we haven't seen it,
	 * it's stale. Drop.
	 */
	if (seq < reorder->win_start)
		return -1;

	idx = seq % 64;

	if (reorder->frame_valid[idx]) {
		/* Duplicate — drop. */
		m_freem(m);
		return -1;
	}

	/* Buffer the frame. */
	reorder->frames[idx] = m;
	reorder->frame_valid[idx] = 1;
	reorder->n_frames++;

	/* If the head-of-line frame arrived, we can flush. */
	if (seq == reorder->head_seq) {
		return 1;
	}

	return 0;
}

/*
 * Flush completed frames from the reorder buffer.
 * Frames are delivered in sequence order starting from head_seq.
 * Returns the mbuf chain of completed frames, or NULL if none.
 */
struct mbuf *
b43bsd_ampdu_reorder_flush(struct b43bsd_softc *sc,
    struct b43bsd_dma_ring *ring, int tid)
{
	struct b43bsd_ampdu_reorder *reorder;
	int count = 0;

	if (tid < 0 || tid >= 8)
		return NULL;
	reorder = &ring->reorder[tid];

	if (!reorder->active || reorder->n_frames == 0)
		return NULL;

	/*
	 * Walk forward from head_seq, collecting consecutive frames.
	 * Deliver each frame individually through net80211 input.
	 */
	while (count < 64) {
		uint16_t idx = reorder->head_seq % 64;

		if (!reorder->frame_valid[idx])
			break;

		/* Frame is ready — dequeue. */
		struct mbuf *m = reorder->frames[idx];
		reorder->frames[idx] = NULL;
		reorder->frame_valid[idx] = 0;
		reorder->n_frames--;

		if (m != NULL) {
			struct ieee80211com *ic = &sc->sc_ic;
			struct ifnet *ifp = &ic->ic_if;
			struct ieee80211_node *ni;
			struct ieee80211_rxinfo rxi;

			m->m_pkthdr.len = m->m_len;

			memset(&rxi, 0, sizeof(rxi));
			rxi.rxi_rssi = b43bsd_phy_n_get_rssi(sc, 0);

			ni = ieee80211_find_rxnode(ic,
			    mtod(m, const struct ieee80211_frame *));
			ieee80211_input(ifp, m, ni, &rxi);
			if (ni != NULL)
				ieee80211_release_node(ic, ni);
		}

		reorder->head_seq++;
		count++;
	}

	reorder->last_seq = reorder->head_seq - 1;

	return NULL;	/* frames delivered inline */
}

/* ------------------------------------------------------------------ */
/* DMA Error Recovery                                                    */
/* ------------------------------------------------------------------ */

/*
 * Recover from a fatal DMA error.
 * Stops and restarts the DMA engines, preserving ring state where possible.
 *
 * Called from interrupt handler when DMA fatal error bits are set.
 * Returns 0 if recovery succeeded, non-zero if full reinit is needed.
 */
int
b43bsd_dma_error_recover(struct b43bsd_softc *sc)
{
	uint32_t tx_status, rx_status;
	int need_reinit = 0;

	tx_status = DMA_READ(sc, B43BSD_DMA64_TX_STATUS);
	rx_status = DMA_READ(sc, B43BSD_DMA64_RX_STATUS);

	printf("%s: DMA error: TX status 0x%08x RX status 0x%08x\n",
	    sc->sc_dev.dv_xname, tx_status, rx_status);

	/*
	 * Check for fatal errors that require full reinit.
	 */
	if (tx_status & B43BSD_DMA64_STAT_ERROR) {
		printf("%s: TX DMA fatal error, reinitializing\n",
		    sc->sc_dev.dv_xname);
		need_reinit = 1;
	}
	if (rx_status & B43BSD_DMA64_STAT_ERROR) {
		printf("%s: RX DMA fatal error, reinitializing\n",
		    sc->sc_dev.dv_xname);
		need_reinit = 1;
	}

	if (need_reinit) {
		b43bsd_dma_free(sc);
		return b43bsd_dma_init(sc);
	}

	/*
	 * Non-fatal: flush and restart.
	 */
	if (tx_status & B43BSD_DMA64_STAT_STOPPED) {
		/* TX engine stopped — restart it. */
		DMA_WRITE(sc, B43BSD_DMA64_TX_CTL, 0);
		delay(10);
		DMA_WRITE(sc, B43BSD_DMA64_TX_CTL, B43BSD_DMA64_TXENABLE);
	}

	if (rx_status & B43BSD_DMA64_STAT_STOPPED) {
		/* RX engine stopped — restart it. */
		DMA_WRITE(sc, B43BSD_DMA64_RX_CTL, 0);
		delay(10);
		{
			struct b43bsd_dma_ring *ring = &sc->sc_rxring;
			DMA_WRITE(sc, B43BSD_DMA64_RX_INDEX,
			    (uint32_t)ring->nslots);
			DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
			    B43BSD_DMA64_RXENABLE |
			    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
			    B43BSD_DMA64_RXFRAMEOFF));
		}
		b43bsd_dma_rx(sc);
	}

	return 0;
}

/*
 * Process TX status descriptor ring.
 * BCM4331 64-bit DMA writes TX completion status to a separate
 * status ring. This is more efficient than scanning the full
 * descriptor ring for completions.
 *
 * The status ring is at offset 0x0240 (TX status) and 0x0260 (RX status).
 */
void
b43bsd_dma_tx_status_process(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	uint32_t status;
	int i;

	/*
	 * Read TX status register.
	 * Hardware writes the index of the last completed descriptor.
	 * We walk from our last-known completion point to the hardware index.
	 */
	status = DMA_READ(sc, B43BSD_DMA64_TX_STATUS);
	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    0, B43BSD_DMA64_RINGMEMSIZE, BUS_DMASYNC_POSTREAD);
	{
		uint32_t hw_idx = (status >> 4) & 0x0FFF;
		uint32_t sw_idx = ring->cur_tx;

		/*
		 * Process all slots between sw_idx and hw_idx.
		 * Bounded to ring size to prevent infinite loops
		 * from hardware reporting garbage indices.
		 */
		for (i = 0; i < ring->nslots; i++) {
			if (ring->tx_slot[sw_idx].m == NULL)
				break;

			/* Check if hardware has consumed this descriptor. */
			{
				struct b43bsd_dma_desc64 *desc =
				    &ring->ring_desc[sw_idx];
				if (letoh32(desc->ctrl0) &
				    (B43BSD_DMA64_DCTL0_FRAMESTART |
				     B43BSD_DMA64_DCTL0_FRAMEEND)) {
					/* Hardware still owns it. */
					break;
				}
			}

			/* Unload and free the completed TX. */
			bus_dmamap_sync(sc->sc_dmat,
			    ring->tx_slot[sw_idx].map,
			    0, ring->tx_slot[sw_idx].map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat,
			    ring->tx_slot[sw_idx].map);

			/* Feed rate control. */
			if (ring->tx_slot[sw_idx].ni != NULL) {
				b43bsd_rc_update(sc,
				    ring->tx_slot[sw_idx].ni, 1, 0);
				ieee80211_release_node(&sc->sc_ic,
				    ring->tx_slot[sw_idx].ni);
				ring->tx_slot[sw_idx].ni = NULL;
			}
			m_freem(ring->tx_slot[sw_idx].m);
			ring->tx_slot[sw_idx].m = NULL;
			ring->used--;

			sw_idx = (sw_idx + 1) % ring->nslots;
			if (sw_idx == hw_idx)
				break;
		}
	}

	/* If TX was stalled and space is now available, restart queue. */
	{
		struct ifnet *ifp = &sc->sc_ic.ic_if;

		if (ifp->if_flags & IFF_OACTIVE) {
			if (ring->used < ring->nslots) {
				ifp->if_flags &= ~IFF_OACTIVE;
				ifp->if_start(ifp);
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* PIO (Programmed I/O) Mode Fallback                                    */
/* ------------------------------------------------------------------ */

/*
 * PIO mode is a DMA-less fallback for chips without functional DMA
 * engines. BCM4331 always uses 64-bit DMA, but PIO is useful for
 * diagnostics and recovery from DMA engine failures.
 *
 * In PIO mode, the CPU reads/writes each word of a frame directly
 * through MMIO registers, avoiding DMA entirely.
 */

/*
 * PIO transmit: write a frame word-by-word through TX FIFO.
 * Returns 0 on success, non-zero on error.
 */
int
b43bsd_pio_tx(struct b43bsd_softc *sc, struct mbuf *m)
{
	const uint32_t TX_FIFO = 0x0240;	/* PIO TX FIFO register */
	uint8_t *data;
	int len, words, i;

	data = mtod(m, uint8_t *);
	len = m->m_pkthdr.len;
	words = (len + 3) / 4;

	/* Wait for TX FIFO to have space. */
	for (i = 0; i < 100; i++) {
		uint32_t st = DMA_READ(sc, B43BSD_DMA64_TX_STATUS);
		if ((st & B43BSD_DMA64_STAT_STOPPED) == 0)
			break;
		delay(10);
	}
	if (i >= 100)
		return EIO;

	/* Write frame word-by-word. */
	for (i = 0; i < words; i++) {
		uint32_t w = 0;
		int j;

		for (j = 0; j < 4 && (i * 4 + j) < len; j++)
			w |= (uint32_t)data[i * 4 + j] << (j * 8);

		DMA_WRITE(sc, TX_FIFO, w);
	}

	/* Trigger TX. */
	DMA_WRITE(sc, TX_FIFO + 4, (uint32_t)len);

	return 0;
}

/*
 * PIO receive: read a frame word-by-word from RX FIFO.
 * Returns mbuf on success, NULL if no frame available.
 */
struct mbuf *
b43bsd_pio_rx(struct b43bsd_softc *sc)
{
	const uint32_t RX_FIFO = 0x0260;	/* PIO RX FIFO register */
	uint32_t status, len;
	struct mbuf *m;
	uint8_t *data;
	int words, i;

	/* Check if a frame is available. */
	status = DMA_READ(sc, B43BSD_DMA64_RX_STATUS);
	if ((status & 0x00000001) == 0)
		return NULL;	/* no frame */

	/* Read frame length. */
	len = DMA_READ(sc, RX_FIFO + 4) & 0x1FFF;
	if (len == 0 || len > 4096)
		return NULL;

	/* Allocate mbuf. */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return NULL;
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return NULL;
	}

	m->m_pkthdr.len = m->m_len = len;
	data = mtod(m, uint8_t *);
	words = (len + 3) / 4;

	/* Read frame word-by-word. */
	for (i = 0; i < words; i++) {
		uint32_t w = DMA_READ(sc, RX_FIFO);
		int j;

		for (j = 0; j < 4 && (i * 4 + j) < len; j++)
			data[i * 4 + j] = (uint8_t)((w >> (j * 8)) & 0xff);
	}

	return m;
}

/* ------------------------------------------------------------------ */
/* DMA Suspend / Resume                                                  */
/* ------------------------------------------------------------------ */

/*
 * Suspend DMA engines (for power save or channel change).
 * Stops TX and RX DMA without freeing resources.
 */
void
b43bsd_dma_suspend(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *tx = &sc->sc_txring;
	struct b43bsd_dma_ring *rx = &sc->sc_rxring;

	/* Suspend TX: stop accepting new frames. */
	if (tx->ring_desc != NULL) {
		DMA_WRITE(sc, B43BSD_DMA64_TX_CTL,
		    B43BSD_DMA64_TXSUSPEND);
	}

	/* Suspend RX: stop DMA. */
	if (rx->ring_desc != NULL) {
		DMA_WRITE(sc, B43BSD_DMA64_RX_CTL, 0);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_DMA,
	    "DMA suspended\n");
}

/*
 * Resume DMA engines after suspend.
 */
void
b43bsd_dma_resume(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *tx = &sc->sc_txring;
	struct b43bsd_dma_ring *rx = &sc->sc_rxring;

	/* Resume TX. */
	if (tx->ring_desc != NULL) {
		DMA_WRITE(sc, B43BSD_DMA64_TX_CTL, B43BSD_DMA64_TXENABLE);
	}

	/* Resume RX: re-enable and re-fill. */
	if (rx->ring_desc != NULL) {
		DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
		    B43BSD_DMA64_RXENABLE |
		    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
		    B43BSD_DMA64_RXFRAMEOFF));
		b43bsd_dma_rx(sc);
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_DMA,
	    "DMA resumed\n");
}

/*
 * Flush RX DMA ring.
 * Frees all pending RX buffers and resets the RX ring to empty state.
 * Used when switching channels to discard stale frames.
 */
void
b43bsd_dma_rx_flush(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_rxring;
	int i;

	/* Stop RX engine. */
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL, 0);
	delay(10);

	/* Free all pending RX buffers. */
	for (i = 0; i < ring->nslots; i++) {
		if (ring->rx_slot[i].m != NULL) {
			bus_dmamap_sync(sc->sc_dmat,
			    ring->rx_slot[i].map,
			    0, ring->rx_slot[i].map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat,
			    ring->rx_slot[i].map);
			m_freem(ring->rx_slot[i].m);
			ring->rx_slot[i].m = NULL;
		}
		/* Clear descriptor. */
		memset(&ring->ring_desc[i], 0,
		    sizeof(struct b43bsd_dma_desc64));
	}

	ring->cur_rx = 0;

	/* Re-fill with fresh buffers. */
	b43bsd_dma_rx(sc);

	/* Re-enable RX engine. */
	DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
	    B43BSD_DMA64_RXENABLE |
	    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
	    B43BSD_DMA64_RXFRAMEOFF));

	B43BSD_DPRINTF(sc, B43BSD_DBG_DMA,
	    "RX DMA flushed, %d slots reset\n", ring->nslots);
}

/*
 * Flush TX DMA ring.
 * Waits for pending TX to complete, then frees completed frames.
 */
void
b43bsd_dma_tx_flush(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	int i, pending;

	/* Count pending TX frames. */
	pending = 0;
	for (i = 0; i < ring->nslots; i++) {
		if (ring->tx_slot[i].m != NULL)
			pending++;
	}

	if (pending == 0)
		return;

	/* Wait up to 100ms for TX to complete. */
	for (i = 0; i < 100; i++) {
		b43bsd_dma_tx_done(sc);
		pending = 0;
		{
			int j;
			for (j = 0; j < ring->nslots; j++) {
				if (ring->tx_slot[j].m != NULL)
					pending++;
			}
		}
		if (pending == 0)
			break;
		delay(1000);
	}

	if (pending > 0) {
		/* Force-flush remaining frames. */
		printf("%s: TX flush: %d frames timed out, "
		    "discarding\n",
		    sc->sc_dev.dv_xname, pending);
		for (i = 0; i < ring->nslots; i++) {
			if (ring->tx_slot[i].m != NULL) {
				if (ring->tx_slot[i].map != NULL)
					bus_dmamap_unload(sc->sc_dmat,
					    ring->tx_slot[i].map);
				m_freem(ring->tx_slot[i].m);
				ring->tx_slot[i].m = NULL;
				if (ring->tx_slot[i].ni != NULL) {
					ieee80211_release_node(
					    &sc->sc_ic,
					    ring->tx_slot[i].ni);
					ring->tx_slot[i].ni = NULL;
				}
				ring->used--;
			}
		}
	}

	B43BSD_DPRINTF(sc, B43BSD_DBG_DMA,
	    "TX DMA flushed\n");
}

/* ------------------------------------------------------------------ */
/* TX Fragmentation Support                                              */
/* ------------------------------------------------------------------ */

/*
 * Transmit a frame that may be fragmented.
 * If the frame exceeds the fragmentation threshold, split it into
 * multiple fragments and transmit each one separately.
 *
 * Returns 0 on success, non-zero on error.
 */
int
b43bsd_dma_tx_fragmented(struct b43bsd_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, int rate_idx, int frag_threshold)
{
	int total_len, offset = 0;
	int frag_num = 0;

	if (m == NULL)
		return EINVAL;

	total_len = m->m_pkthdr.len;

	/* If frame is smaller than threshold, send as single frame. */
	if (total_len <= frag_threshold) {
		return b43bsd_dma_tx_start(sc, m, ni, rate_idx);
	}

	/*
	 * Fragment the frame.
	 * Each fragment (except last) = frag_threshold bytes.
	 * Last fragment = remainder.
	 */
	while (offset < total_len) {
		struct mbuf *frag;
		int frag_len;

		frag_len = total_len - offset;
		if (frag_len > frag_threshold)
			frag_len = frag_threshold;

		/* Allocate fragment mbuf. */
		MGETHDR(frag, M_DONTWAIT, MT_DATA);
		if (frag == NULL)
			return ENOBUFS;

		MCLGET(frag, M_DONTWAIT);
		if ((frag->m_flags & M_EXT) == 0) {
			m_freem(frag);
			return ENOBUFS;
		}

		/* Copy fragment data. */
		m_copydata(m, offset, frag_len, mtod(frag, caddr_t));
		frag->m_pkthdr.len = frag->m_len = frag_len;

		/* Set More Fragments bit in Frame Control for all but last. */
		if ((offset + frag_len) < total_len) {
			uint8_t *fc = mtod(frag, uint8_t *);
			fc[1] |= 0x04;	/* More Fragments */
		}

		/* Transmit fragment. */
		if (b43bsd_dma_tx_start(sc, frag, ni, rate_idx) != 0) {
			m_freem(frag);
			m_freem(m);
			return ENOBUFS;
		}

		offset += frag_len;
		frag_num++;
	}

	/* Free original mbuf. */
	m_freem(m);

	B43BSD_DPRINTF(sc, B43BSD_DBG_TX,
	    "TX fragmented: %d fragments, %d bytes total\n",
	    frag_num, total_len);

	return 0;
}

/* ------------------------------------------------------------------ */
/* Multi-Segment DMA Support                                             */
/* ------------------------------------------------------------------ */

/*
 * Transmit a frame with multiple DMA segments.
 * Handles mbuf chains and scatter-gather DMA.
 */
int
b43bsd_dma_tx_multiseg(struct b43bsd_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, int rate_idx)
{
	struct b43bsd_dma_ring *ring = &sc->sc_txring;
	int slot, error, nsegs;

	slot = ring->cur_tx;
	if (ring->tx_slot[slot].m != NULL)
		return ENOBUFS;
	if (ring->tx_slot[slot].map == NULL)
		return ENXIO;

	/* Load mbuf chain into DMA map. */
	error = bus_dmamap_load_mbuf(sc->sc_dmat,
	    ring->tx_slot[slot].map, m, BUS_DMA_NOWAIT);
	if (error != 0)
		return error;

	nsegs = ring->tx_slot[slot].map->dm_nsegs;

	/*
	 * For single-segment frames, use fast path.
	 */
	if (nsegs == 1) {
		bus_dmamap_sync(sc->sc_dmat, ring->tx_slot[slot].map,
		    0, ring->tx_slot[slot].map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		dma_desc_write(ring, slot,
		    B43BSD_DMA64_DCTL0_DTABLEEND |
		    B43BSD_DMA64_DCTL0_FRAMESTART |
		    B43BSD_DMA64_DCTL0_FRAMEEND |
		    B43BSD_DMA64_DCTL0_IRQ,
		    (uint32_t)m->m_pkthdr.len,
		    ring->tx_slot[slot].map->dm_segs[0].ds_addr);
	} else {
		/*
		 * Multi-segment: write each segment as a separate
		 * descriptor entry. First segment has FRAMESTART,
		 * last has FRAMEEND, middle segments have neither.
		 */
		int seg;
		uint32_t total_len = 0;

		for (seg = 0; seg < nsegs; seg++) {
			int cur_slot = (slot + seg) % ring->nslots;
			uint32_t ctrl = B43BSD_DMA64_DCTL0_DTABLEEND | B43BSD_DMA64_DCTL0_IRQ;

			if (seg == 0)
				ctrl |= B43BSD_DMA64_DCTL0_FRAMESTART;
			if (seg == nsegs - 1)
				ctrl |= B43BSD_DMA64_DCTL0_FRAMEEND;

			/* Check slot availability. */
			if (ring->tx_slot[cur_slot].m != NULL) {
				bus_dmamap_unload(sc->sc_dmat,
				    ring->tx_slot[slot].map);
				return ENOBUFS;
			}

			bus_dmamap_sync(sc->sc_dmat,
			    ring->tx_slot[slot].map,
			    0,
			    ring->tx_slot[slot].map->dm_segs[seg].ds_len,
			    BUS_DMASYNC_PREWRITE);

			dma_desc_write(ring, cur_slot, ctrl,
			    (uint32_t)ring->tx_slot[slot].map->
			    dm_segs[seg].ds_len,
			    ring->tx_slot[slot].map->
			    dm_segs[seg].ds_addr);

			total_len += ring->tx_slot[slot].map->
			    dm_segs[seg].ds_len;

			/* Only store mbuf in the first slot. */
			if (seg == 0) {
				ring->tx_slot[cur_slot].m = m;
				ring->tx_slot[cur_slot].ni = ni;
				ring->tx_slot[cur_slot].len =
				    m->m_pkthdr.len;
				ring->tx_slot[cur_slot].rate_idx =
				    rate_idx;
			}
		}

		/* Advance TX index past all segments. */
		ring->cur_tx = (slot + nsegs) % ring->nslots;
		ring->used += nsegs - 1;	/* +1 for the frame itself */
	}

	bus_dmamap_sync(sc->sc_dmat, ring->ring_map,
	    slot * sizeof(struct b43bsd_dma_desc64),
	    nsegs * sizeof(struct b43bsd_dma_desc64),
	    BUS_DMASYNC_PREWRITE);

	if (nsegs == 1) {
		ring->tx_slot[slot].m = m;
		ring->tx_slot[slot].ni = ni;
		ring->tx_slot[slot].len = m->m_pkthdr.len;
		ring->tx_slot[slot].rate_idx = rate_idx;
		ring->cur_tx = (slot + 1) % ring->nslots;
		ring->used++;
	}

	/* Ring doorbell. */
	DMA_WRITE(sc, B43BSD_DMA64_TX_INDEX, ring->cur_tx);

	return 0;
}

/* ------------------------------------------------------------------ */
/* 64-bit DMA Quirk Handling                                             */
/* ------------------------------------------------------------------ */

/*
 * Check and apply 64-bit DMA quirks.
 * Some BCM4331 silicon steppings have issues with addresses
 * above 4 GB. Detect and work around.
 */
void
b43bsd_dma_64bit_quirk_check(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;
	uint32_t cap;

	if (chip_id != 0x4331)
		return;

	/*
	 * Read DMA capabilities register.
	 * Bit 0: 64-bit addressing supported
	 * Bit 1: 64-bit addressing requires workaround
	 */
	cap = DMA_READ(sc, B43BSD_DMA64_TX_CTL);

	if ((cap & 0x00010000) == 0) {
		/*
		 * 64-bit addressing not supported on this stepping.
		 * Fall back to 32-bit DMA. Set address extension mask
		 * in TX and RX control registers.
		 */
		DMA_WRITE(sc, B43BSD_DMA64_TX_CTL,
		    DMA_READ(sc, B43BSD_DMA64_TX_CTL) |
		    0x00020000);
		DMA_WRITE(sc, B43BSD_DMA64_RX_CTL,
		    DMA_READ(sc, B43BSD_DMA64_RX_CTL) |
		    0x00020000);

		printf("%s: 64-bit DMA quirk enabled "
		    "(32-bit addressing fallback)\n",
		    sc->sc_dev.dv_xname);
	}
}

/* ------------------------------------------------------------------ */
/* DMA Diagnostic Dump                                                   */
/* ------------------------------------------------------------------ */

/*
 * Dump DMA engine state for debugging.
 */
void
b43bsd_dma_diag(struct b43bsd_softc *sc)
{
	struct b43bsd_dma_ring *tx = &sc->sc_txring;
	struct b43bsd_dma_ring *rx = &sc->sc_rxring;

	printf("%s: DMA diagnostics:\n", sc->sc_dev.dv_xname);
	printf("  TX: ctl=0x%08x idx=%d/%d used=%d\n",
	    DMA_READ(sc, B43BSD_DMA64_TX_CTL),
	    tx->cur_tx, tx->nslots, tx->used);
	printf("  TX status: 0x%08x\n",
	    DMA_READ(sc, B43BSD_DMA64_TX_STATUS));
	printf("  RX: ctl=0x%08x idx=%d/%d\n",
	    DMA_READ(sc, B43BSD_DMA64_RX_CTL),
	    rx->cur_rx, rx->nslots);
	printf("  RX status: 0x%08x\n",
	    DMA_READ(sc, B43BSD_DMA64_RX_STATUS));

	/* Show first 4 TX descriptors. */
	{
		int i;

		printf("  TX descriptors (first 4):\n");
		for (i = 0; i < 4 && i < tx->nslots; i++) {
			struct b43bsd_dma_desc64 *desc =
			    &tx->ring_desc[i];

			printf("    [%d] ctrl0=0x%08x ctrl1=0x%08x "
			    "addr_lo=0x%08x addr_hi=0x%08x\n",
			    i, letoh32(desc->ctrl0),
			    letoh32(desc->ctrl1),
			    letoh32(desc->addr_lo),
			    letoh32(desc->addr_hi));
		}
	}
}
