/*	$OpenBSD: b43bsd_radio.h,v 1.1 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_RADIO_H_
#define _DEV_IC_B43BSD_RADIO_H_

struct b43bsd_softc;

void	b43bsd_radio_reset(struct b43bsd_softc *);
void	b43bsd_radio_sleep(struct b43bsd_softc *);
void	b43bsd_radio_wake(struct b43bsd_softc *);
int	b43bsd_radio_vco_cal(struct b43bsd_softc *);
int	b43bsd_radio_read_temp(struct b43bsd_softc *);
void	b43bsd_radio_temp_compensation(struct b43bsd_softc *, int);
int	b43bsd_radio_loopback_test(struct b43bsd_softc *);
void	b43bsd_radio_rssi_cal(struct b43bsd_softc *);
int	b43bsd_radio_selftest(struct b43bsd_softc *);
void	b43bsd_radio_set_channel(struct b43bsd_softc *, int, int);
int	b43bsd_radio_temp(struct b43bsd_softc *);

#ifdef B43BSD_DEBUG
void	b43bsd_radio_dump_regs(struct b43bsd_softc *);
#endif
uint16_t b43bsd_radio_reg_read(struct b43bsd_softc *, uint16_t);
void	b43bsd_radio_reg_write(struct b43bsd_softc *, uint16_t, uint16_t);

#endif /* _DEV_IC_B43BSD_RADIO_H_ */
