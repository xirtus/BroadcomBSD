/*	$OpenBSD: b43bsd_btcoex.h,v 1.1 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_BTCOEX_H_
#define _DEV_IC_B43BSD_BTCOEX_H_

struct b43bsd_softc;

void	b43bsd_btcoex_init(struct b43bsd_softc *);
void	b43bsd_btcoex_deinit(struct b43bsd_softc *);
void	b43bsd_btcoex_poll(struct b43bsd_softc *);
void	b43bsd_btcoex_tx_active(struct b43bsd_softc *);
void	b43bsd_btcoex_tx_done(struct b43bsd_softc *);

#endif /* _DEV_IC_B43BSD_BTCOEX_H_ */
