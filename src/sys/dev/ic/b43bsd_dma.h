/*	$OpenBSD: b43bsd_dma.h,v 1.1 2026/06/24 xirtus Exp $	*/

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

#ifndef _DEV_IC_B43BSD_DMA_H_
#define _DEV_IC_B43BSD_DMA_H_

/*
 * BCM4331 64-bit DMA descriptor and ring management.
 * Ported from Linux drivers/net/wireless/broadcom/b43/dma.h (GPLv2).
 */

/* ------------------------------------------------------------------ */
/* 64-bit DMA Descriptor (16 bytes)                                    */
/* ------------------------------------------------------------------ */

struct b43bsd_dma_desc64 {
	uint32_t	ctrl0;		/* Flags: DTABLEEND, IRQ, FRAMEEND, FRAMESTART */
	uint32_t	ctrl1;		/* Byte count + address extension */
	uint32_t	addr_lo;	/* Low 32 bits of DMA address */
	uint32_t	addr_hi;	/* High 32 bits of DMA address */
} __packed;

/* Control0 bits. */
#define B43BSD_DMA64_DCTL0_DTABLEEND	0x10000000
#define B43BSD_DMA64_DCTL0_IRQ		0x20000000
#define B43BSD_DMA64_DCTL0_FRAMEEND	0x40000000
#define B43BSD_DMA64_DCTL0_FRAMESTART	0x80000000

/* Control0 bits 13:0 contain the buffer/frame length (RX/TX). */
#define B43BSD_DMA64_DCTL0_LEN		0x00001FFF

/* Control1 bits. */
#define B43BSD_DMA64_DCTL1_BYTECNT	0x00001FFF
#define B43BSD_DMA64_DCTL1_ADDREXT_MASK	0x00030000
#define B43BSD_DMA64_DCTL1_ADDREXT_SHIFT	16

/* ------------------------------------------------------------------ */
/* 64-bit DMA Engine Registers                                         */
/* ------------------------------------------------------------------ */

/* TX registers (engine 0 at base 0x200). */
#define B43BSD_DMA64_TX_CTL		0x0200
#define B43BSD_DMA64_TX_DESC_RINGLO	0x0204
#define B43BSD_DMA64_TX_DESC_RINGHI	0x0208
#define B43BSD_DMA64_TX_INDEX		0x020c
#define B43BSD_DMA64_TX_STATUS		0x0210

/* RX registers (engine 0 at base 0x220). */
#define B43BSD_DMA64_RX_CTL		0x0220
#define B43BSD_DMA64_RX_DESC_RINGLO	0x0224
#define B43BSD_DMA64_RX_DESC_RINGHI	0x0228
#define B43BSD_DMA64_RX_INDEX		0x022c
#define B43BSD_DMA64_RX_STATUS		0x0230

/* DMA control bits. */
#define B43BSD_DMA64_TXENABLE		0x00000001
#define B43BSD_DMA64_TXSUSPEND		0x00000002
#define B43BSD_DMA64_TXFLUSH		0x00000008
#define B43BSD_DMA64_TXPARITY		0x00000010

#define B43BSD_DMA64_RXENABLE		0x00000001
#define B43BSD_DMA64_RXFRAMEOFF		0x000000FE
#define B43BSD_DMA64_RXFRAMEOFF_SHIFT	1

/* DMA status bits. */
#define B43BSD_DMA64_STAT_DISABLED	0x00000001
#define B43BSD_DMA64_STAT_SUSPENDED	0x00000002
#define B43BSD_DMA64_STAT_STOPPED	0x00000004
#define B43BSD_DMA64_STAT_IDLEWAIT	0x00000008
#define B43BSD_DMA64_STAT_ERROR		0x00000010

#define B43BSD_DMA64_RXSTAT_MASK	0x00000FFF
#define B43BSD_DMA64_RXSTATDPTR_SHIFT	4

/* DMA interrupt reason bits. */
#define B43BSD_DMAIRQ_RX_DONE		(1 << 16)
#define B43BSD_DMAIRQ_RDESC_UFLOW	(1 << 13)
#define B43BSD_DMAIRQ_FATALMASK		\
	((1 << 10) | (1 << 11) | (1 << 12) | (1 << 14) | (1 << 15))

/* ------------------------------------------------------------------ */
/* Ring Sizes & Constants                                              */
/* ------------------------------------------------------------------ */

#define B43BSD_DMA64_RINGMEMSIZE	8192
#define B43BSD_TX_SLOTS			256
#define B43BSD_RX_SLOTS			256
#define B43BSD_RX_BUFSZ			4096	/* RX mbuf cluster size */

/* A-MPDU reorder buffer. */
#define B43BSD_AMPDU_REORDER_MAX	64	/* max subframes buffered */
#define B43BSD_AMPDU_DELIMITER_LEN	4	/* delimiter between subframes */

struct b43bsd_ampdu_reorder {
	int		tid;		/* traffic identifier */
	int		active;		/* reorder buffer in use */
	uint16_t	win_start;	/* start of reorder window (SSN) */
	uint16_t	win_size;	/* reorder window size */
	uint16_t	head_seq;	/* next expected sequence number */
	struct mbuf	*frames[64];	/* buffered subframes indexed by seq */
	uint8_t		frame_valid[64]; /* frame at this offset is ready */
	int		n_frames;	/* frames currently buffered */
	uint16_t	last_seq;	/* last received sequence number */
};

/* ------------------------------------------------------------------ */
/* DMA Ring Structure                                                  */
/* ------------------------------------------------------------------ */

struct b43bsd_softc;

struct b43bsd_dma_ring {
	bus_dma_tag_t		ring_tag;
	bus_dmamap_t		ring_map;
	bus_dma_segment_t	ring_seg;
	struct b43bsd_dma_desc64 *ring_desc;
	bus_addr_t		ring_paddr;
	int			nslots;
	int			ring_nsegs;	/* actual nsegs from bus_dmamem_alloc */
	int			cur_tx;		/* next TX slot */
	int			cur_rx;		/* next RX slot */
	int			used;		/* active TX slots */

	/* Per-slot metadata for TX. */
	struct {
		bus_dmamap_t		map;
		struct mbuf		*m;
		struct ieee80211_node	*ni;
		int			len;
		uint8_t			rate_idx;	/* MCS rate used */
	} tx_slot[B43BSD_TX_SLOTS];

	/* Per-slot metadata for RX. */
	struct {
		bus_dmamap_t	map;
		struct mbuf	*m;
	} rx_slot[B43BSD_RX_SLOTS];

	/* DMA tag for buffers. */
	bus_dma_tag_t		buf_tag;

	/* A-MPDU reorder buffers (one per TID). */
	struct b43bsd_ampdu_reorder reorder[8];
};

/* ------------------------------------------------------------------ */
/* DMA API                                                             */
/* ------------------------------------------------------------------ */

int	b43bsd_dma_init(struct b43bsd_softc *);
void	b43bsd_dma_free(struct b43bsd_softc *);
int	b43bsd_dma_tx_start(struct b43bsd_softc *, struct mbuf *,
	    struct ieee80211_node *, int rate_idx);
void	b43bsd_dma_tx_done(struct b43bsd_softc *);
void	b43bsd_dma_rx(struct b43bsd_softc *);
void	b43bsd_dma_rx_process(struct b43bsd_softc *);
void	b43bsd_dma_rx_overflow(struct b43bsd_softc *);
void	b43bsd_ampdu_reorder_init(struct b43bsd_dma_ring *, int tid);
int	b43bsd_ampdu_reorder_add(struct b43bsd_softc *,
	    struct b43bsd_dma_ring *, int tid, uint16_t seq,
	    struct mbuf *m);
struct mbuf *b43bsd_ampdu_reorder_flush(struct b43bsd_softc *,
	    struct b43bsd_dma_ring *, int tid);
int	b43bsd_dma_error_recover(struct b43bsd_softc *);
void	b43bsd_dma_tx_status_process(struct b43bsd_softc *);
int	b43bsd_pio_tx(struct b43bsd_softc *, struct mbuf *);
struct mbuf *b43bsd_pio_rx(struct b43bsd_softc *);
void	b43bsd_dma_suspend(struct b43bsd_softc *);
void	b43bsd_dma_resume(struct b43bsd_softc *);
void	b43bsd_dma_rx_flush(struct b43bsd_softc *);
void	b43bsd_dma_tx_flush(struct b43bsd_softc *);
int	b43bsd_dma_tx_fragmented(struct b43bsd_softc *, struct mbuf *,
	    struct ieee80211_node *, int, int);
int	b43bsd_dma_tx_multiseg(struct b43bsd_softc *, struct mbuf *,
	    struct ieee80211_node *, int);
void	b43bsd_dma_64bit_quirk_check(struct b43bsd_softc *);

/* DMA diagnostics (R19). */
void	b43bsd_dma_diag(struct b43bsd_softc *);

#endif /* _DEV_IC_B43BSD_DMA_H_ */
