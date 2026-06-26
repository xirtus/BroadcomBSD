/*	$OpenBSD: b43bsd_tables.h,v 1.1 2026/06/25 xirtus Exp $	*/

#ifndef _DEV_IC_B43BSD_TABLES_H_
#define _DEV_IC_B43BSD_TABLES_H_

struct b43bsd_softc;

void	b43bsd_tables_upload_all(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_core(struct b43bsd_softc *);
void	b43bsd_tables_upload_filters(struct b43bsd_softc *, int channel,
	    int is_5ghz);
void	b43bsd_tables_upload_txgain(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_rxgain(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_noise(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_iq_defaults(struct b43bsd_softc *);
void	b43bsd_tables_upload_mimo(struct b43bsd_softc *, int nstreams);
void	b43bsd_tables_upload_radio_extended(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_mcs_power(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_rfseq(struct b43bsd_softc *);
void	b43bsd_tables_upload_afe(struct b43bsd_softc *);
void	b43bsd_tables_upload_gain_cal(struct b43bsd_softc *);
void	b43bsd_tables_upload_pa_cal(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_vco_cal(struct b43bsd_softc *);
void	b43bsd_tables_upload_band_bw_init(struct b43bsd_softc *,
	    int is_5ghz, int is_40mhz);
void	b43bsd_tables_upload_radio_rev_overrides(struct b43bsd_softc *,
	    int radio_rev, int is_5ghz);
void	b43bsd_tables_upload_spur_avoid(struct b43bsd_softc *,
	    int channel, int is_5ghz);
void	b43bsd_tables_upload_rxfilt_full(struct b43bsd_softc *,
	    int channel, int is_5ghz);
void	b43bsd_tables_upload_phyrev_init(struct b43bsd_softc *, int phy_rev);
void	b43bsd_tables_upload_mcs_chain_power(struct b43bsd_softc *);
void	b43bsd_tables_upload_chan_est(struct b43bsd_softc *, int is_40mhz);
void	b43bsd_tables_upload_stbc(struct b43bsd_softc *, int nstreams);
void	b43bsd_tables_upload_ldpc(struct b43bsd_softc *);
int	b43bsd_tables_temp_compensate(struct b43bsd_softc *, int temp_c,
	    int base_dbm, int is_5ghz);
void	b43bsd_tables_set_cal_mask(struct b43bsd_softc *, uint32_t);
uint32_t b43bsd_tables_get_cal_mask(struct b43bsd_softc *);
void	b43bsd_tables_upload_extended(struct b43bsd_softc *);
void	b43bsd_tables_upload_ant_isolation(struct b43bsd_softc *,
	    int is_5ghz);
void	b43bsd_tables_upload_filter_cal(struct b43bsd_softc *, int is_5ghz);
void	b43bsd_tables_upload_gpio_init(struct b43bsd_softc *);
void	b43bsd_tables_apply_chan_overrides(struct b43bsd_softc *,
	    int channel, int is_5ghz);
void	b43bsd_tables_upload_syn_by_rev(struct b43bsd_softc *, int radio_rev);
void	b43bsd_tables_upload_sgi_init(struct b43bsd_softc *,
	    int is_5ghz, int is_40mhz);
void	b43bsd_tables_upload_chain_power_pct(struct b43bsd_softc *,
	    int nchains, int beamforming);
void	b43bsd_tables_upload_phyrev_extended(struct b43bsd_softc *,
	    int phy_rev);
void	b43bsd_tables_upload_coldboot(struct b43bsd_softc *);
void	b43bsd_tables_upload_tx_chain_full(struct b43bsd_softc *, int);
void	b43bsd_tables_upload_rx_chain_full(struct b43bsd_softc *, int);
void	b43bsd_tables_radio_diag_dump(struct b43bsd_softc *);
void	b43bsd_tables_print_cal_status(struct b43bsd_softc *);
void	b43bsd_tables_radio_full_dump(struct b43bsd_softc *);
int	b43bsd_tables_temp_txpower_adjust(struct b43bsd_softc *,
	    int temp_c, int is_5ghz);
void	b43bsd_tables_upload_rev16_40mhz(struct b43bsd_softc *);

/* Extended tables (R19). */
void	b43bsd_tables_upload_bf_steering(struct b43bsd_softc *);
void	b43bsd_tables_upload_syn_rev_overrides(struct b43bsd_softc *,
	    int radio_rev, int is_5ghz);
int	b43bsd_tables_txpower_fine_tune(struct b43bsd_softc *,
	    int channel, int is_5ghz);

#endif /* _DEV_IC_B43BSD_TABLES_H_ */
