/*	$OpenBSD: b43bsd_xmit.c,v 1.1 2026/06/25 xirtus Exp $	*/

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
 * TX advanced features: multi-rate retry chains, TSF operations,
 * beacon timer setup, radiotap signal metrics.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_var.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/* Local register access mirrors b43bsd_read32/write32. */
#define XM_RD(sc, r) bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r))
#define XM_WR(sc, r, v) bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r), (v))

#include <dev/ic/b43bsd_dma.h>
#include <dev/ic/b43bsd_phy_n.h>
#include <dev/ic/b43bsd_rate.h>

/* ------------------------------------------------------------------ */
/* Multi-Rate TX Retry Chains                                            */
/* ------------------------------------------------------------------ */

/*
 * Build a multi-rate retry chain for a TX descriptor.
 *
 * BCM4331 64-bit DMA engine supports up to 4 alternate rates per frame.
 * If the primary rate fails after N retries, the hardware automatically
 * falls back to the next rate in the chain. This reduces latency vs.
 * software retry and improves throughput in marginal conditions.
 *
 * Chain layout:
 *   Rate 0: Max throughput rate (highest MCS, try 1-2 times)
 *   Rate 1: Mid rate (next lower MCS group, try 2-3 times)
 *   Rate 2: Robust rate (1-2 streams lower, try 3-4 times)
 *   Rate 3: Fallback rate (MCS 0, try remaining times)
 *
 * The hardware retry count is in the control0 word.
 */
void
b43bsd_xmit_build_retry_chain(struct b43bsd_softc *sc,
    uint32_t *ctrl0, int primary_mcs, int nstreams)
{
	uint32_t ctrl = *ctrl0;
	int rate[4];
	int retries[4];
	int max_retries = B43BSD_RC_MAX_RETRY;
	int i;

	/* Build rate chain. */
	rate[0] = primary_mcs;
	retries[0] = 1;

	if (nstreams >= 3 && primary_mcs >= 16) {
		rate[1] = primary_mcs - 8;	/* drop to 2-stream */
		retries[1] = 2;
		rate[2] = 7;			/* single stream MCS 7 */
		retries[2] = 2;
		rate[3] = 0;			/* MCS 0 */
		retries[3] = 1;
	} else if (nstreams >= 2 && primary_mcs >= 8) {
		rate[1] = primary_mcs - 8;
		retries[1] = 2;
		rate[2] = 3;			/* MCS 3 */
		retries[2] = 2;
		rate[3] = 0;
		retries[3] = 1;
	} else {
		rate[1] = primary_mcs > 3 ? primary_mcs - 1 : 0;
		retries[1] = 2;
		rate[2] = primary_mcs > 1 ? primary_mcs - 2 : 0;
		retries[2] = 2;
		rate[3] = 0;
		retries[3] = 1;
	}

	/*
	 * Encode rate fallback chain into control word.
	 * BCM4331 format:
	 *   bits [27:24]: rate 0 MCS
	 *   bits [23:20]: rate 1 MCS
	 *   bits [19:16]: rate 2 MCS
	 *   bits [15:12]: rate 3 MCS
	 *   bits [11:8]:  number of retries per rate (aggregate)
	 */
	ctrl &= ~0x0FFFF000;	/* clear rate/retry fields */

	/* Set MCS for each rate. */
	ctrl |= (rate[0] & 0xF) << 24;
	ctrl |= (rate[1] & 0xF) << 20;
	ctrl |= (rate[2] & 0xF) << 16;
	ctrl |= (rate[3] & 0xF) << 12;

	/* Set total retry count. */
	for (i = 0; i < 4; i++) {
		if (retries[i] > max_retries)
			retries[i] = max_retries;
		ctrl |= ((retries[i] - 1) << (8 + i * 1));
	}

	*ctrl0 = ctrl;
}

/* ------------------------------------------------------------------ */
/* TSF (Timing Synchronization Function) Operations                      */
/* ------------------------------------------------------------------ */

/*
 * Write the 64-bit TSF timer.
 * Used during association to sync with the AP's timestamp.
 */
void
b43bsd_tsf_write(struct b43bsd_softc *sc, uint64_t tsf)
{
	XM_WR(sc, B43_MMIO_TSF_LOW, (uint32_t)(tsf & 0xffffffff));
	XM_WR(sc, B43_MMIO_TSF_HIGH, (uint32_t)(tsf >> 32));
}

/*
 * Setup hardware beacon timers.
 * Programs the TBTT (Target Beacon Transmission Time) and
 * beacon interval timers for power-save synchronization.
 */
void
b43bsd_beacon_timer_setup(struct b43bsd_softc *sc, int interval_tu)
{
	uint64_t tsf;
	uint32_t macctl;

	/*
	 * Read current TSF, compute next TBTT as next multiple
	 * of the beacon interval.
	 */
	tsf = b43bsd_tsf_read(sc);

	{
		uint64_t next_tbtt;

		/* TBTT = next beacon time in µs. */
		next_tbtt = ((tsf / (interval_tu * 1024)) + 1) *
		    (interval_tu * 1024);

		/* Write next TBTT. */
		XM_WR(sc, B43_MMIO_TSF_LOW,
		    (uint32_t)(next_tbtt & 0xffffffff));
		XM_WR(sc, B43_MMIO_TSF_HIGH,
		    (uint32_t)(next_tbtt >> 32));
	}

	/*
	 * Enable beacon timer interrupt.
	 * Set the beacon interval register (in TU).
	 */
	XM_WR(sc, B43_MMIO_TSF_CFP_START, interval_tu);

	/* Ensure TBTT and beacon interrupts are enabled. */
	macctl = XM_RD(sc, B43_MMIO_MACCTL);
	macctl |= B43_MACCTL_HW_BEACON_TIMER;
	XM_WR(sc, B43_MMIO_MACCTL, macctl);
}

/* ------------------------------------------------------------------ */
/* RX Radiotap Header Construction                                       */
/* ------------------------------------------------------------------ */

/*
 * Fill an RX radiotap header with signal quality metrics.
 */
void
b43bsd_rx_radiotap(struct b43bsd_softc *sc,
    struct b43bsd_rx_radiotap_header *tap,
    int rssi_dbm, int rate_mbps, int channel, int is_5ghz)
{
	memset(tap, 0, sizeof(*tap));

	tap->wr_ihdr.it_version = 0;
	tap->wr_ihdr.it_len = htole16(sizeof(*tap));
	tap->wr_ihdr.it_present = htole32(B43BSD_RX_RADIOTAP_PRESENT);

	/* Flags: short preamble if rate > 12 Mbps. */
	tap->wr_flags = (rate_mbps > 72) ? IEEE80211_RADIOTAP_F_SHORTPRE : 0;

	/* Rate in 500 kbps units. */
	tap->wr_rate = (uint8_t)(rate_mbps * 2);

	/* Channel frequency and flags. */
	tap->wr_chan_freq = htole16(
	    ieee80211_ieee2mhz(channel, is_5ghz ?
	    IEEE80211_CHAN_5GHZ : IEEE80211_CHAN_2GHZ));
	tap->wr_chan_flags = htole16(
	    is_5ghz ? IEEE80211_CHAN_5GHZ : IEEE80211_CHAN_2GHZ);

	/* RSSI in dBm. */
	tap->wr_antsignal = (int8_t)rssi_dbm;

	/* Noise floor: estimate from idle RSSI. */
	tap->wr_antnoise = (int8_t)(b43bsd_phy_n_get_rssi(sc, 0));

	/* Antenna: 0 = diversity. */
	tap->wr_antenna = 0;
}

/* ------------------------------------------------------------------ */
/* Shared IRQ Spurious Interrupt Hardening                               */
/* ------------------------------------------------------------------ */

/*
 * Check if this interrupt is a spurious (shared IRQ from another device).
 * Returns 1 if spurious (should ignore), 0 if valid for this device.
 *
 * On MacBook Pro 9,2, the BCM4331 shares INTx line with:
 *   - FireWire controller
 *   - SD card reader
 *   - Possibly other PCIe devices
 *
 * We must reject interrupts from other devices quickly to avoid
 * interrupt storms on shared lines.
 */
int
b43bsd_intr_is_spurious(struct b43bsd_softc *sc)
{
	uint32_t reason;

	/* Device not running? Definitely not ours. */
	if ((sc->sc_flags & B43BSD_FLAG_RUNNING) == 0)
		return 1;

	/* Read interrupt reason. If zero, it's a shared interrupt. */
	reason = XM_RD(sc, B43_MMIO_GEN_IRQ_REASON);
	if (reason == 0) {
		sc->sc_stats.rx_phy_errors++;	/* count as spurious */
		return 1;
	}

	/* Check for impossible interrupt combinations. */
	if ((reason & B43_IRQ_READY) && (reason & B43_IRQ_DMA)) {
		/*
		 * READY and DMA error simultaneously is impossible —
		 * READY only fires during firmware init, DMA only
		 * after DMA is running. This is a spurious interrupt.
		 */
		B43BSD_DPRINTF(sc, B43BSD_DBG_INTR,
		    "spurious: impossible reason 0x%08x\n", reason);
		sc->sc_stats.rx_phy_errors++;
		return 1;
	}

	return 0;
}

/*
 * Check and report interrupt statistics for diagnostics.
 */
void
b43bsd_intr_stats(struct b43bsd_softc *sc)
{
	uint32_t mask, reason;

	mask = XM_RD(sc, B43_MMIO_GEN_IRQ_MASK);
	reason = XM_RD(sc, B43_MMIO_GEN_IRQ_REASON);

	printf("%s: IRQ stats: mask=0x%08x reason=0x%08x "
	    "count=%d storm=%d\n",
	    sc->sc_dev.dv_xname, mask, reason,
	    sc->sc_intr_count, sc->sc_irq_storm);
}
