/*	$OpenBSD: b43bsd_xmit.h,v 1.1 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_XMIT_H_
#define _DEV_IC_B43BSD_XMIT_H_

#include <sys/param.h>

struct b43bsd_softc;
struct b43bsd_rx_radiotap_header;

void	b43bsd_xmit_build_retry_chain(struct b43bsd_softc *,
	    uint32_t *ctrl0, int primary_mcs, int nstreams);
uint64_t b43bsd_tsf_read(struct b43bsd_softc *);
void	b43bsd_tsf_write(struct b43bsd_softc *, uint64_t);
void	b43bsd_beacon_timer_setup(struct b43bsd_softc *, int);
void	b43bsd_rx_radiotap(struct b43bsd_softc *,
	    struct b43bsd_rx_radiotap_header *, int, int, int, int);
int	b43bsd_intr_is_spurious(struct b43bsd_softc *);
void	b43bsd_intr_stats(struct b43bsd_softc *);

#endif /* _DEV_IC_B43BSD_XMIT_H_ */
