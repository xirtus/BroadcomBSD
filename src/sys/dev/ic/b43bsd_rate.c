/*	$OpenBSD: b43bsd_rate.c,v 1.4 2026/06/25 xirtus Exp $	*/

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
 * Minstrel-HT rate control for BCM4331 802.11n.
 *
 * Fully reimplemented from the Linux Minstrel-HT algorithm
 * (net/mac80211/rc80211_minstrel_ht.c).
 *
 * Tracks per-MCS statistics with EWMA probability estimation,
 * periodically samples higher and lower rates, and switches to the
 * rate with the best estimated throughput. Supports:
 *   - 3 spatial streams (MCS 0-23)
 *   - Short Guard Interval (SGI)
 *   - 40 MHz channels (HT40)
 *   - Automatic fallback on high loss
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/ic/b43bsd_rate.h>

/* ------------------------------------------------------------------ */
/* Constants                                                             */
/* ------------------------------------------------------------------ */

#define MINSTREL_HT_SAMPLE_RATE	10	/* sample every N packets */
#define MINSTREL_HT_EWMA_LEVEL	75	/* EWMA weight (percent) */
#define MINSTREL_HT_PROB_SHIFT	10	/* fixed-point probability shift */
#define MINSTREL_HT_UPDATE_INTERVAL	50	/* update stats every N pkts */

/* Max retry chain depth. */
#define MINSTREL_HT_MAX_RATE	4

/* MCS bitrates (Mbps) for 20 MHz, 800ns GI, Nss=1..3. */
static const int mcs_bitrate_20_sgi0[3][8] = {
	{ 65, 130, 195, 260, 390, 520, 585, 650 },	/* 1ss */
	{ 130, 260, 390, 520, 780, 1040, 1170, 1300 },	/* 2ss */
	{ 195, 390, 585, 780, 1170, 1560, 1755, 1950 },	/* 3ss */
};

/* MCS bitrates for 40 MHz, 800ns GI. */
static const int mcs_bitrate_40_sgi0[3][8] = {
	{ 135, 270, 405, 540, 810, 1080, 1215, 1350 },
	{ 270, 540, 810, 1080, 1620, 2160, 2430, 2700 },
	{ 405, 810, 1215, 1620, 2430, 3240, 3645, 4050 },
};

/* SGI adjustment: multiply by 11/10 (~+11%). */
static inline int
sgi_adjust(int rate_kbps)
{
	return (rate_kbps * 11) / 10;
}

/* ------------------------------------------------------------------ */
/* Rate Table Lookup                                                     */
/* ------------------------------------------------------------------ */

static int
mcs_to_bitrate(int mcs, int nss, int is_ht40, int is_sgi)
{
	int rate_kbps;
	int stream = (nss > 0 && nss <= 3) ? (nss - 1) : 0;
	int idx = mcs % 8;

	if (is_ht40)
		rate_kbps = mcs_bitrate_40_sgi0[stream][idx];
	else
		rate_kbps = mcs_bitrate_20_sgi0[stream][idx];

	rate_kbps *= 100;	/* convert to kbps */

	if (is_sgi)
		rate_kbps = sgi_adjust(rate_kbps);

	return rate_kbps;
}

/* ------------------------------------------------------------------ */
/* Minstrel-HT State                                                     */
/* ------------------------------------------------------------------ */

/*
 * Per-rate Minstrel-HT statistics.
 */
struct minstrel_ht_rate {
	uint32_t	attempts;	/* total TX attempts */
	uint32_t	success;	/* successful TX */
	uint32_t	last_attempts;
	uint32_t	last_success;
	uint32_t	cur_prob;	/* current EWMA probability */
	uint32_t	cur_tp;		/* current throughput (kbps) */
	uint32_t	probability;	/* smoothed probability (fixed-pt) */
	int		sample_skipped;	/* skip counter for sampling */
	int		retry_updated;
};

/*
 * Per-MCS-group state.
 */
struct minstrel_ht_group {
	struct minstrel_ht_rate	rates[24];	/* max 24 MCS rates/group */
	int			max_mcs;	/* highest active MCS */
	int			max_tp_rate[4];	/* best TP per # of streams */
	int			max_prob_rate;	/* best probability rate */
	int			n_rates;	/* active rates in group */
	int			supported;	/* group is usable */
};

/*
 * Per-station Minstrel-HT data.
 */
struct minstrel_ht_sta {
	struct minstrel_ht_group	groups[3];	/* 1ss, 2ss, 3ss */

	int		total_packets;
	int		sample_packets;
	int		sample_count;
	int		sample_group;
	int		sample_idx;	/* rate currently being sampled */

	int		is_ht40;
	int		is_sgi;

	/* Best overall rate (across all groups). */
	int		max_tp_rate;
	int		max_tp_rate_group;
	uint32_t	max_tp_rate_tp;
};

/* Per-driver Minstrel-HT instance. */
static struct minstrel_ht_sta minstrel_sta;

/* Interface state tracking. */
static int rc_initialized = 0;
static int rc_nss = 3;		/* max spatial streams */
static int rc_is_ht40 = 0;	/* 40 MHz enabled? */
static int rc_is_sgi = 1;	/* SGI enabled? */

/* ------------------------------------------------------------------ */
/* EWMA Probability Update                                               */
/* ------------------------------------------------------------------ */

/*
 * Update EWMA success probability for a rate.
 * prob = (EWMA_LEVEL * old_prob + (100 - EWMA_LEVEL) * new_pkt_prob) / 100
 */
static void
minstrel_ht_update_prob(struct minstrel_ht_rate *mr)
{
	uint32_t pkt_prob;

	if (mr->last_attempts == 0)
		return;

	pkt_prob = (mr->last_success * (1 << MINSTREL_HT_PROB_SHIFT))
	    / mr->last_attempts;

	/* EWMA update. */
	if (mr->probability == 0)
		mr->probability = pkt_prob;
	else
		mr->probability = (MINSTREL_HT_EWMA_LEVEL *
		    mr->probability +
		    (100 - MINSTREL_HT_EWMA_LEVEL) * pkt_prob) / 100;

	mr->last_attempts = 0;
	mr->last_success = 0;
}

/* ------------------------------------------------------------------ */
/* Throughput Calculation                                                */
/* ------------------------------------------------------------------ */

/*
 * Calculate estimated throughput for a rate in kbps.
 * tp = probability * bitrate / 100
 */
static uint32_t
minstrel_ht_calc_tp(struct minstrel_ht_rate *mr, int bitrate_kbps)
{
	uint32_t prob;

	prob = mr->probability >> MINSTREL_HT_PROB_SHIFT;	/* 0..100 */
	if (prob == 0)
		return 0;

	return (bitrate_kbps * prob) / 100;
}

/* ------------------------------------------------------------------ */
/* Rate Table Update                                                     */
/* ------------------------------------------------------------------ */

/*
 * Recalculate best rates for a group after statistics update.
 */
static void
minstrel_ht_update_group(struct minstrel_ht_group *mg, int nss)
{
	int i, best_tp_idx = 0, best_prob_idx = 0;
	uint32_t best_tp = 0, best_prob = 0;

	for (i = 0; i < mg->n_rates; i++) {
		struct minstrel_ht_rate *mr = &mg->rates[i];
		uint32_t tp;

		/* Update EWMA probability. */
		minstrel_ht_update_prob(mr);

		/* Calculate throughput. */
		tp = minstrel_ht_calc_tp(mr,
		    mcs_to_bitrate(i, nss, rc_is_ht40, rc_is_sgi));

		mr->cur_prob = mr->probability >> MINSTREL_HT_PROB_SHIFT;
		mr->cur_tp = tp;

		if (tp > best_tp) {
			best_tp = tp;
			best_tp_idx = i;
		}
		if (mr->cur_prob > best_prob) {
			best_prob = mr->cur_prob;
			best_prob_idx = i;
		}
	}

	mg->max_tp_rate[nss - 1] = best_tp_idx;
	mg->max_prob_rate = best_prob_idx;
}

/* ------------------------------------------------------------------ */
/* Sample Rate Selection                                                 */
/* ------------------------------------------------------------------ */

/*
 * Pick a rate to sample. Returns MCS index to try next.
 * Alternates between probing higher rates and sampling across groups.
 */
static int
minstrel_ht_get_sample_rate(struct minstrel_ht_sta *ms)
{
	int g = ms->sample_group;
	struct minstrel_ht_group *mg;
	int idx;

	if (g >= 3)
		g = ms->sample_group = 0;

	mg = &ms->groups[g];
	if (mg->n_rates == 0) {
		ms->sample_group++;
		return -1;
	}

	/*
	 * Probe behavior:
	 * - Sample 1 rate above max_tp_rate (faster MCS)
	 * - Sample 1 rate below max_prob_rate (more reliable)
	 * - Cycle through groups
	 */
	if (ms->sample_count++ < 3) {
		idx = mg->max_tp_rate[g] + 1;
		if (idx < mg->n_rates) {
			ms->sample_idx = idx;
			ms->sample_group = (g + 1) % 3;
			return idx;
		}
	}

	/* Try the max probability rate (reliable fallback). */
	idx = mg->max_prob_rate;
	ms->sample_group = (g + 1) % 3;
	return (idx >= 0 && idx < mg->n_rates) ? idx : -1;
}

/* ------------------------------------------------------------------ */
/* Public API                                                            */
/* ------------------------------------------------------------------ */

void
b43bsd_rc_init(struct b43bsd_softc *sc, struct ieee80211_node *ni)
{
	struct minstrel_ht_sta *ms = &minstrel_sta;
	int g, i;

	memset(ms, 0, sizeof(*ms));

	rc_nss = 3;	/* BCM4331 supports 3 streams */
	rc_is_ht40 = (sc->sc_flags & B43BSD_FLAG_40MHZ) ? 1 : 0;
	rc_is_sgi = 1;

	/*
	 * Initialize groups: Nss=1 (MCS 0-7), Nss=2 (MCS 8-15), Nss=3 (MCS 16-23).
	 */
	for (g = 0; g < 3; g++) {
		struct minstrel_ht_group *mg = &ms->groups[g];
		int nrates = 8;	/* 8 MCS rates per spatial stream */

		mg->n_rates = nrates;
		mg->max_mcs = nrates - 1;
		mg->supported = (g < rc_nss) ? 1 : 0;

		for (i = 0; i < nrates; i++) {
			mg->rates[i].probability =
			    (50 << MINSTREL_HT_PROB_SHIFT);
			mg->rates[i].cur_prob = 50;
		}

		/*
		 * Default best rates:
		 * - MCS 3 for single stream (reliable 19.5 Mbps)
		 * - MCS 3 for dual stream (39 Mbps)
		 * - MCS 3 for triple stream (58.5 Mbps)
		 */
		mg->max_tp_rate[g] = 3;
		mg->max_prob_rate = 0;
	}

	ms->is_ht40 = rc_is_ht40;
	ms->is_sgi = rc_is_sgi;
	ms->max_tp_rate = 0;
	ms->max_tp_rate_group = 0;

	rc_initialized = 1;
}

void
b43bsd_rc_deinit(struct b43bsd_softc *sc, struct ieee80211_node *ni)
{
	memset(&minstrel_sta, 0, sizeof(minstrel_sta));
	rc_initialized = 0;
}

uint8_t
b43bsd_rc_rateidx(struct b43bsd_softc *sc, struct ieee80211_node *ni,
    int is_data)
{
	struct minstrel_ht_sta *ms = &minstrel_sta;
	int rate;

	if (!is_data || !rc_initialized)
		return 0;	/* management: MCS 0 */

	/* Every N packets, sample a different rate. */
	if (ms->total_packets > 0 &&
	    (ms->total_packets % MINSTREL_HT_SAMPLE_RATE) == 0) {
		rate = minstrel_ht_get_sample_rate(ms);
		if (rate >= 0) {
			ms->sample_packets++;
			return (uint8_t)rate;
		}
	}

	/* Use best throughput rate from the best group. */
	rate = ms->max_tp_rate;
	return (uint8_t)(rate < 24 ? rate : 0);
}

void
b43bsd_rc_update(struct b43bsd_softc *sc, struct ieee80211_node *ni,
    int success, int retry_count)
{
	struct minstrel_ht_sta *ms = &minstrel_sta;
	int rate_idx;
	int nss, g;

	if (!rc_initialized)
		return;

	/* Determine which rate index was used. */
	rate_idx = sc->sc_cur_rateidx;
	if (rate_idx >= 24)
		return;

	nss = rate_idx / 8 + 1;
	g = nss - 1;

	if (g >= 3)
		return;

	{
		struct minstrel_ht_group *mg = &ms->groups[g];
		int local_idx = rate_idx % 8;

		mg->rates[local_idx].last_attempts++;
		mg->rates[local_idx].attempts++;
		if (success) {
			mg->rates[local_idx].last_success++;
			mg->rates[local_idx].success++;
		}
	}

	ms->total_packets++;

	/* Periodic update of rate tables. */
	if ((ms->total_packets % MINSTREL_HT_UPDATE_INTERVAL) == 0) {
		int best_tp = 0;
		int best_rate = 0, best_group = 0;
		int grp;

		for (grp = 0; grp < 3; grp++) {
			struct minstrel_ht_group *mg = &ms->groups[grp];

			if (!mg->supported)
				continue;

			minstrel_ht_update_group(mg, grp + 1);

			{
				int r = mg->max_tp_rate[grp];
				if (r >= 0 && r < mg->n_rates) {
					uint32_t tp =
					    mg->rates[r].cur_tp;
					if (tp > best_tp) {
						best_tp = tp;
						best_rate = r + grp * 8;
						best_group = grp;
					}
				}
			}
		}

		if (best_tp > 0) {
			ms->max_tp_rate = best_rate;
			ms->max_tp_rate_group = best_group;
			ms->max_tp_rate_tp = best_tp;
		}
	}
}
