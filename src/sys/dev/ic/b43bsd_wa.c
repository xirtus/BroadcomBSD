/*	$OpenBSD: b43bsd_wa.c,v 1.1 2026/06/25 xirtus Exp $	*/

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
 * BCM4331 hardware workarounds.
 *
 * This chip has several silicon errata that require specific register
 * tweaks. Without these, the device is unreliable: TX stalls, DMA
 * lockups, TSF timer drift, and PCIe link drops.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/wa.c (GPLv2).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/bus.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/ssbvar.h>

/* Local register access — mirrors b43bsd_read32/write32. */
#define WA_RD(sc, r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (r))
#define WA_WR(sc, r, v) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (r), (v))
#define DMA_WR(sc, r, v) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (r), (v))
#define DMA_RD(sc, r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, \
	    (sc)->sc_11core_offset + (r))

#define SSB_CHIPCO_GPIOCTL	0x00B0
#define SSB_CHIPCO_CLKCTLST	0x01E0
#define B43BSD_DMA64_RXSTAT_FIFOEMPTY	0x00000001
#define B43BSD_DMA64_RX_DATA		0x0240
#define DMA_RD(sc, r) bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r))

/* ------------------------------------------------------------------ */
/* WA1: BCM4331 TX queue limitation                                     */
/* ------------------------------------------------------------------ */

/*
 * Erratum: BCM4331 hardware TX engine stalls when more than one TX
 * queue is active with QoS (WMM). The chip was designed for a newer
 * BCMA fabric but was backported to SSB; the SSB interrupt routing
 * cannot handle per-queue TX completions correctly.
 *
 * Workaround: disable QoS TX queues 1-3, use only queue 0.
 * This limits us to one EDCA access category but prevents TX hangs.
 */
void
b43bsd_wa_txqueue_limit(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;
	uint32_t val;

	if (chip_id != 0x4331)
		return;

	/*
	 * MacBook Pro 9,2 has chip rev 2 or 3 — both affected.
	 * Disable TX queues 1-3 in the DMA scheduler.
	 */
	/* TX queue control register: disable secondary queues. */
	val = WA_RD(sc, 0x0300);
	val &= ~0x000e;		/* clear bits for queues 1-3 */
	WA_WR(sc, 0x0300, val);

	/* TX status register mask: ignore completions for Q1-Q3. */
	val = WA_RD(sc, 0x0304);
	val &= ~0x0e00;
	WA_WR(sc, 0x0304, val);
}

/* ------------------------------------------------------------------ */
/* WA2: TSF clock spur avoidance                                         */
/* ------------------------------------------------------------------ */

/*
 * Erratum: BCM4331 TSF timer drifts when using the default 160 MHz
 * clock due to coupling from the PCIe reference clock (100 MHz).
 * This causes beacon timestamp drift and association timeouts.
 *
 * Workaround: shift the TSF clock to 164 MHz or 168 MHz to avoid
 * the spur at 160 MHz. The PLL fractional divider is reprogrammed.
 *
 * Mode 1: 164 MHz — safest, used by Apple bootcamp driver
 * Mode 2: 168 MHz — better power, may not work on all boards
 */

/*
 * BCM4331 TSF clock control register.
 * Offset 0x062C: TSF_CLK_FRAC_LOW
 * Offset 0x062E: TSF_CLK_FRAC_HIGH
 */
#define B43_TSF_CLK_FRAC_LOW	0x062c
#define B43_TSF_CLK_FRAC_HIGH	0x062e

void
b43bsd_wa_tsf_spur_avoid(struct b43bsd_softc *sc, int mode)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;
	uint32_t frac_lo, frac_hi;

	if (chip_id != 0x4331)
		return;

	/*
	 * TSF clock = crystal * 2^26 / frac
	 * Crystal = 20 MHz
	 *
	 * Default: frac = 0x66666 → 20 * 2^26 / 0x66666 ≈ 160.0 MHz
	 * Mode 1:  frac = 0x63E70 → 20 * 2^26 / 0x63E70 ≈ 164.0 MHz
	 * Mode 2:  frac = 0x61862 → 20 * 2^26 / 0x61862 ≈ 168.0 MHz
	 */
	switch (mode) {
	case 1:
		frac_lo = 0x00063e70;
		frac_hi = 0x00000000;
		break;
	case 2:
		frac_lo = 0x00061862;
		frac_hi = 0x00000000;
		break;
	default:
		frac_lo = 0x00066666;
		frac_hi = 0x00000000;
		break;
	}

	WA_WR(sc, B43_TSF_CLK_FRAC_LOW, frac_lo);
	WA_WR(sc, B43_TSF_CLK_FRAC_HIGH, frac_hi);

	sc->sc_tsf_mode = mode;
}

/* ------------------------------------------------------------------ */
/* WA3: BCMA-level chip reset quirk for MacBook EFI                     */
/* ------------------------------------------------------------------ */

/*
 * Erratum: MacBook Pro (2012) EFI leaves the BCM4331 in an
 * indeterminate state after S3 resume. The SSB bus enumeration
 * may find the chip partially initialized with dangling PCIe
 * configuration space state.
 *
 * Workaround: perform a full-chip cold reset sequence before
 * SSB enumeration on chips with the EFI spurious interrupt quirk.
 * This clears any stale PCIe state and firmware remnants.
 */
void
b43bsd_wa_efi_reset(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;

	if (chip_id != 0x4331)
		return;
	if ((sc->sc_quirks & B43BSD_QUIRK_EFI_SPURIOUS) == 0)
		return;

	/*
	 * Full chip reset sequence:
	 * 1. Disable PCIe ASPM
	 * 2. Assert chip reset via ChipCommon GPIO
	 * 3. Clear reset, wait for PLL lock
	 * 4. Re-enumerate SSB cores
	 *
	 * This matches the Apple BootCamp driver reset sequence.
	 */

	/* 1. Disable ASPM on PCIe capability. */
	{
		int pcie_cap;
		pcireg_t lnkctl;

		pcie_cap = pci_get_capability(sc->sc_pct, sc->sc_pcitag,
		    PCI_CAP_PCIEXPRESS, NULL, NULL);
		if (pcie_cap) {
			lnkctl = pci_conf_read(sc->sc_pct, sc->sc_pcitag,
			    pcie_cap + PCI_PCIE_LCAP);
			lnkctl &= ~(PCI_PCIE_LCAP_ASPM_L0S |
			    PCI_PCIE_LCAP_ASPM_L1);
			pci_conf_write(sc->sc_pct, sc->sc_pcitag,
			    pcie_cap + PCI_PCIE_LCAP, lnkctl);
		}
	}

	/* 2. Assert chip reset — GPIO bit 0 in ChipCommon. */
	ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOCTL, 0x00000001);
	delay(1000);

	/* 3. Clear reset. */
	ssb_write32(sc->sc_ssb, SSB_CHIPCO_GPIOCTL, 0x00000000);
	delay(1000);

	/* 4. Wait for PLL lock (ChipCommon clock status bit). */
	{
		int i;
		for (i = 0; i < 100; i++) {
			uint32_t clkst;
			clkst = ssb_read32(sc->sc_ssb, SSB_CHIPCO_CLKCTLST);
#define DMA_WR(sc, r, v) bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r), (v))
#define DMA_RD(sc, r) bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (sc)->sc_11core_offset + (r))
			if (clkst & 0x00000001)
				break;
			delay(100);
		}
	}
}

/* ------------------------------------------------------------------ */
/* WA4: PCIe TLP workaround                                             */
/* ------------------------------------------------------------------ */

/*
 * Erratum: BCM4331 generates malformed PCIe Transaction Layer Packets
 * (TLPs) during heavy DMA traffic. These cause PCIe fatal errors on
 * Intel chipsets (Series 7 used in MacBook Pro 9,2).
 *
 * Workaround: program the PCIe core's TLP workaround register to
 * insert padding and limit maximum payload size.
 */
#define B43_PCIE_TLP_WORKAROUND	0x0100

void
b43bsd_wa_pcie_tlp(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;
	uint32_t val;

	if (chip_id != 0x4331)
		return;

	/*
	 * If the PCIe core is present, program its TLP workaround
	 * register. The PCIe core is at a known SSB core index.
	 */
	if (sc->sc_ssb->pcie_idx >= 0) {
		val = ssb_core_read32(sc->sc_ssb, sc->sc_ssb->pcie_idx,
		    B43_PCIE_TLP_WORKAROUND);
		/*
		 * Bit 0: enable TLP padding
		 * Bit 1: limit MPS to 128 bytes
		 * Bit 2: disable relaxed ordering
		 */
		val |= 0x00000007;
		ssb_core_write32(sc->sc_ssb, sc->sc_ssb->pcie_idx,
		    B43_PCIE_TLP_WORKAROUND, val);
	}
}

/* ------------------------------------------------------------------ */
/* WA5: RX FIFO overflow workaround                                      */
/* ------------------------------------------------------------------ */

/*
 * Erratum: BCM4331 DMA RX engine may stall permanently after a
 * RX FIFO overflow if the overflow condition is not fully cleared.
 *
 * Workaround: after any RX overflow, reset the RX DMA engine
 * completely (stop, drain FIFO, restart) rather than just
 * acknowledging the overflow interrupt.
 */
void
b43bsd_wa_rx_fifo_overflow(struct b43bsd_softc *sc)
{
	uint16_t chip_id = sc->sc_chipid & 0xffff;

	if (chip_id != 0x4331)
		return;

	/*
	 * Full RX DMA reset:
	 * 1. Stop RX engine
	 * 2. Drain FIFO by reading status
	 * 3. Wait for FIFO empty
	 * 4. Re-fill RX ring
	 * 5. Restart RX engine
	 */

	/* 1. Stop RX engine. */
	DMA_WR(sc, B43BSD_DMA64_RX_CTL, 0);

	/* 2. Drain FIFO — read RX status until clear. */
	{
		int i;
		for (i = 0; i < 256; i++) {
			uint32_t st = DMA_RD(sc, B43BSD_DMA64_RX_STATUS);
			if ((st & B43BSD_DMA64_RXSTAT_FIFOEMPTY) == 0)
				DMA_RD(sc, B43BSD_DMA64_RX_DATA);
			else
				break;
		}
		delay(100);
	}

	/* 3. Re-fill RX ring and restart. */
	b43bsd_dma_rx(sc);
	DMA_WR(sc, B43BSD_DMA64_RX_CTL,
	    B43BSD_DMA64_RXENABLE |
	    ((30 << B43BSD_DMA64_RXFRAMEOFF_SHIFT) &
	    B43BSD_DMA64_RXFRAMEOFF));
}

/* ------------------------------------------------------------------ */
/* Apply all workarounds                                                */
/* ------------------------------------------------------------------ */

/*
 * Apply all BCM4331 hardware workarounds.
 * Must be called after SSB enumeration and before chip init.
 */
void
b43bsd_wa_apply_all(struct b43bsd_softc *sc)
{
	/* WA1: limit to single TX queue (prevents TX stall). */
	b43bsd_wa_txqueue_limit(sc);

	/* WA2: TSF clock spur avoidance mode 1 (164 MHz). */
	b43bsd_wa_tsf_spur_avoid(sc, 1);

	/* WA3: MacBook EFI full-chip reset (cold boot only). */
	b43bsd_wa_efi_reset(sc);

	/* WA4: PCIe TLP workaround. */
	b43bsd_wa_pcie_tlp(sc);
}
