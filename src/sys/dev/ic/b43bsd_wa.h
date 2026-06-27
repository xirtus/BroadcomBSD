/*	$OpenBSD: b43bsd_wa.h,v 1.1 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_WA_H_
#define _DEV_IC_B43BSD_WA_H_

struct b43bsd_softc;

void	b43bsd_wa_apply_all(struct b43bsd_softc *);
void	b43bsd_wa_txqueue_limit(struct b43bsd_softc *);
void	b43bsd_wa_tsf_spur_avoid(struct b43bsd_softc *, int);
void	b43bsd_wa_efi_reset(struct b43bsd_softc *);
void	b43bsd_wa_pcie_tlp(struct b43bsd_softc *);
void	b43bsd_wa_rx_fifo_overflow(struct b43bsd_softc *);

#endif /* _DEV_IC_B43BSD_WA_H_ */
