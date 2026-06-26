/*	$OpenBSD: b43bsd_rate.h,v 1.2 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_RATE_H_
#define _DEV_IC_B43BSD_RATE_H_

/* Max hardware retries per frame. */
#define B43BSD_RC_MAX_RETRY	4

/* MCS groups (1, 2, or 3 spatial streams). */
#define B43BSD_RC_NUM_GROUPS	3

#define B43BSD_RC_MAX_MCS	24

/* Sampling table size. */
#define B43BSD_RC_NUM_SAMPLES	20

struct b43bsd_softc;
struct ieee80211_node;

void	b43bsd_rc_init(struct b43bsd_softc *, struct ieee80211_node *);
void	b43bsd_rc_deinit(struct b43bsd_softc *, struct ieee80211_node *);
uint8_t	b43bsd_rc_rateidx(struct b43bsd_softc *, struct ieee80211_node *,
	    int is_data);
void	b43bsd_rc_update(struct b43bsd_softc *, struct ieee80211_node *,
	    int success, int retry_count);

#endif /* _DEV_IC_B43BSD_RATE_H_ */
