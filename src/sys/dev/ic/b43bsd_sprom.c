/*	$OpenBSD: b43bsd_sprom.c,v 1.1 2026/06/24 xirtus Exp $	*/

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
 * BCM4331 SPROM (Serial PROM) / OTP reader.
 * Extracts board-specific calibration data: MAC address, TX power limits,
 * antenna gain, regulatory domain, etc.
 *
 * Ported from Linux drivers/ssb/pci.c SPROM parsing (GPLv2).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <net/if.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/ic/ssbvar.h>
#include <dev/ic/ssbreg.h>

/*
 * Read a 16-bit SPROM word from the ChipCommon core.
 */
static uint16_t
sprom_read(struct b43bsd_softc *sc, uint16_t offset)
{
	struct ssb_bus *bus = sc->sc_ssb;
	uint32_t spromctl;
	int i;

	if (bus == NULL || bus->chipcommon_idx < 0)
		return 0xffff;

	/*
	 * Write offset to the ChipCommon SPROM control register
	 * and poll for completion.
	 */
	spromctl = (offset & SSB_CHIPCO_SPROMCTL_OFFSET);
	ssb_core_write32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_SPROMCTL, spromctl);

	/* Wait for SPROM access to complete (busy bit clears). */
	for (i = 0; i < 1000; i++) {
		uint32_t val;

		val = ssb_core_read32(bus, bus->chipcommon_idx,
		    SSB_CHIPCO_SPROMCTL);
		if ((val & SSB_CHIPCO_SPROMCTL_BUSY) == 0)
			break;
		delay(10);
	}

	/* Read data word from SPROM data register. */
	return (uint16_t)(ssb_core_read32(bus, bus->chipcommon_idx,
	    SSB_CHIPCO_SPROMCTL) & 0xffff);
}

/*
 * CRC16-CCITT (polynomial 0x1021) for SPROM validation.
 * The SPROM stores a CRC16 over the first N-1 words.
 */
static uint16_t
sprom_crc16(const uint16_t *data, int nwords)
{
	uint16_t crc = 0xffff;
	int i, j;

	for (i = 0; i < nwords; i++) {
		crc ^= data[i] << 8;
		for (j = 0; j < 8; j++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
		crc ^= data[i] & 0xff;
		for (j = 0; j < 8; j++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
	}
	return crc;
}

/*
 * Parse SPROM revision 8 (BCM4331).
 * Reference: Linux include/linux/ssb/ssb_regs.h SPROM rev 8 layout.
 */
int
b43bsd_sprom_parse(struct b43bsd_softc *sc)
{
	uint16_t sprom_rev, sprom_crc_word;
	uint8_t mac[IEEE80211_ADDR_LEN];
	int i;

	/*
	 * BCM4331: external PA lines must be disabled during SPROM
	 * access to avoid GPIO conflicts, then re-enabled for TX.
	 */
	if (sc->sc_ssb != NULL)
		ssb_bcm4331_ext_pa_lines_ctl(sc->sc_ssb, 0);

	/* Read SPROM revision and CRC from word 0x007E.
	 * Low byte = SPROM revision, high byte = CRC8 over words 0x0000-0x007D. */
	sprom_crc_word = sprom_read(sc, 0x007E);
	sprom_rev = sprom_crc_word & 0xff;

	printf("%s: SPROM rev %d\n", sc->sc_dev.dv_xname, sprom_rev);

	if (sprom_rev < 8) {
		printf("%s: SPROM rev %d too old, using defaults\n",
		    sc->sc_dev.dv_xname, sprom_rev);
		return 0;
	}

	/*
	 * Read MAC address from SPROM offset 0x0018 (Rev 8+).
	 * MAC is stored as 3 16-bit words in big-endian.
	 */
	for (i = 0; i < 3; i++) {
		uint16_t w;

		w = sprom_read(sc, SSB_SPROM8_IL0MACADDR + (i * 2));
		mac[i * 2] = (w >> 8) & 0xff;
		mac[i * 2 + 1] = w & 0xff;
	}

	if (IEEE80211_IS_MULTICAST(mac) ||
	    (mac[0] == 0 && mac[1] == 0 && mac[2] == 0 &&
	     mac[3] == 0 && mac[4] == 0 && mac[5] == 0)) {
		printf("%s: invalid MAC address in SPROM\n",
		    sc->sc_dev.dv_xname);
		return EINVAL;
	}

	IEEE80211_ADDR_COPY(sc->sc_macaddr, mac);

	printf("%s: MAC address %s\n",
	    sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_macaddr));

	/*
	 * Read board flags (lo/hi/2).
	 */
	{
		uint16_t bfl, bfh, bfl2;

		bfl = sprom_read(sc, SSB_SPROM8_BF_LO);
		bfh = sprom_read(sc, SSB_SPROM8_BF_HI);
		bfl2 = sprom_read(sc, SSB_SPROM8_BOARDFLAGS2);

		printf("%s: board flags lo=0x%04x hi=0x%04x 2=0x%04x\n",
		    sc->sc_dev.dv_xname, bfl, bfh, bfl2);

		/* Check capabilities from board flags. */
		if (bfl & SSB_BFL_FEM)
			printf("%s: external FEM detected\n",
			    sc->sc_dev.dv_xname);
		if (bfl & SSB_BFL_EXTLNA)
			printf("%s: external LNA detected\n",
			    sc->sc_dev.dv_xname);
		if (bfl2 & SSB_BFL2_5G_PWRGAIN)
			sc->sc_flags |= B43BSD_FLAG_5GHZ;
		if (bfl2 & SSB_BFL2_TXPWRCTRL_EN)
			printf("%s: hardware TX power control active\n",
			    sc->sc_dev.dv_xname);
		if (bfl2 & SSB_BFL2_PCIEWAR_OVR) {
			printf("%s: PCIe ASPM/Clkreq override required\n",
			    sc->sc_dev.dv_xname);
			sc->sc_quirks |= B43BSD_QUIRK_PCIE_WAR;
		}
	}

	/*
	 * Read board revision.
	 */
	{
		uint16_t boardrev;

		boardrev = sprom_read(sc, SSB_SPROM8_BOARDREV);
		printf("%s: board rev 0x%04x\n",
		    sc->sc_dev.dv_xname, boardrev);
	}

	/*
	 * Read regulatory domain (country code + revision).
	 * SPROM rev 8 offset 0x0058: lower byte = Broadcom numeric
	 * country code, upper byte = regulatory revision.
	 * Country codes: 0 = world, 1 = USA (FCC), others = ETSI.
	 */
	{
		uint16_t reg;
		int country;

		reg = sprom_read(sc, 0x0058);
		country = reg & 0xff;

		printf("%s: regulatory domain country=%d rev=%d\n",
		    sc->sc_dev.dv_xname, country, (reg >> 8) & 0xff);

		/*
		 * Apply regulatory restrictions:
		 * - USA (FCC, code 1): max 30 dBm 2.4 GHz
		 * - ETSI / world: max 20 dBm 2.4 GHz / 5 GHz
		 */
		if (country == 0) {
			/* World — use SPROM defaults. */
		} else if (country == 1) {
			/* USA (FCC): standard limits. */
			if (sc->sc_maxpwr_2ghz > 30)
				sc->sc_maxpwr_2ghz = 30;
		} else {
			/*
			 * ETSI / rest of world:
			 * 2.4 GHz max 20 dBm, 5 GHz max 20 dBm.
			 */
			if (sc->sc_maxpwr_2ghz > 20)
				sc->sc_maxpwr_2ghz = 20;
			if (sc->sc_maxpwr_5ghz > 20)
				sc->sc_maxpwr_5ghz = 20;
		}
	}

	/*
	 * Read TX power offsets for 802.11n rates.
	 * SPROM stores 8 16-bit words per band, each containing two
	 * 8-bit signed offsets (dBm * 4).  This gives 16 offsets
	 * covering MCS 0-15.  MCS 16-23 duplicate 8-15.
	 * Base power: 20 dBm (2.4 GHz), 18 dBm (5 GHz).
	 */
	{
		int maxpwr_2g = 20, maxpwr_5g = 18;
		int j;

		for (i = 0; i < 8; i++) {
			uint16_t mcs_2g, mcs_5g;

			mcs_2g = sprom_read(sc,
			    SSB_SPROM8_2G_MCSPO + (i * 2));
			mcs_5g = sprom_read(sc,
			    SSB_SPROM8_5G_MCSPO + (i * 2));

			/* Low byte = even MCS, high byte = odd MCS. */
			sc->sc_mcs_pwr_2g[i * 2]     = (int8_t)(mcs_2g & 0xff);
			sc->sc_mcs_pwr_2g[i * 2 + 1] = (int8_t)(mcs_2g >> 8);
			sc->sc_mcs_pwr_5g[i * 2]     = (int8_t)(mcs_5g & 0xff);
			sc->sc_mcs_pwr_5g[i * 2 + 1] = (int8_t)(mcs_5g >> 8);
		}
		/* MCS 16-23 mirror MCS 8-15. */
		for (j = 8; j < 16; j++) {
			sc->sc_mcs_pwr_2g[j + 8] = sc->sc_mcs_pwr_2g[j];
			sc->sc_mcs_pwr_5g[j + 8] = sc->sc_mcs_pwr_5g[j];
		}

		/* Compute max power from the most restrictive offset.
		 * sc_mcs_pwr arrays contain signed offsets in units of 0.25 dBm.
		 * Negative offsets reduce power; positive offsets are clamped to 0. */
		for (i = 0; i < 16; i++) {
			if (sc->sc_mcs_pwr_2g[i] < maxpwr_2g) {
				int8_t off = sc->sc_mcs_pwr_2g[i];

				if (off > 0)
					off = 0;
				maxpwr_2g += (off / 4);
			}
			if (sc->sc_mcs_pwr_5g[i] < maxpwr_5g) {
				int8_t off = sc->sc_mcs_pwr_5g[i];

				if (off > 0)
					off = 0;
				maxpwr_5g += (off / 4);
			}
		}

		sc->sc_maxpwr_2ghz = maxpwr_2g;
		sc->sc_maxpwr_5ghz = maxpwr_5g;

		printf("%s: TX power limits: 2.4 GHz %d dBm, "
		    "5 GHz %d dBm\n",
		    sc->sc_dev.dv_xname, maxpwr_2g, maxpwr_5g);
	}

	/* Re-enable external PA lines now that SPROM access is complete. */
	if (sc->sc_ssb != NULL)
		ssb_bcm4331_ext_pa_lines_ctl(sc->sc_ssb, 1);

	/*
	 * Validate SPROM CRC16-CCITT.
	 * For SPROM rev 8+, words 0x0000-0x007D are covered,
	 * with the CRC stored in the high byte of word 0x007E.
	 */
	if (sprom_rev >= 8) {
		uint16_t sprom_data[0x7E];
		uint16_t computed_crc, stored_crc;
		int j;

		for (j = 0; j < 0x7E; j++)
			sprom_data[j] = sprom_read(sc, (uint16_t)j);

		computed_crc = sprom_crc16(sprom_data, 0x7E);
		/*
		 * The CRC8 is stored in the high byte of word 0x007E;
		 * the low byte is the SPROM revision (already extracted).
		 */
		stored_crc = (sprom_crc_word & 0xff00) >> 8;

		if (computed_crc == stored_crc) {
			printf("%s: SPROM CRC16 valid (0x%02x)\n",
			    sc->sc_dev.dv_xname, computed_crc);
		} else {
			printf("%s: SPROM CRC16 MISMATCH (computed 0x%02x, "
			    "stored 0x%02x) — data may be corrupted\n",
			    sc->sc_dev.dv_xname, computed_crc, stored_crc);
		}
	}

	return 0;
}
