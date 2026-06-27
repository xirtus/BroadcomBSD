# BroadcomBSD Post-Mortem — Final Pre-Boot Audit

**Date:** 2026-06-25 20:00 PDT  
**Kernel:** `/bsd` rebuilt 3× (ssb.c + b43bsd_dma.c + b43bsd.c)  
**Fallback:** `/obsd`

---

## The Fundamental Question

> Imagine the kernel panicked on first boot. Why?

Answer: **SSB backplane address model mismatch.** The source used absolute backplane addresses (0x18000000+) as bus_space offsets into a typical 16 KB PCI BAR0. Each access would fault ~384 MB past the mapped region.

## Bugs Fixed (This Session)

| # | Bug | File | Severity | Would Cause |
|---|-----|------|----------|-------------|
| F9 | SSB enumeration used absolute backplane addresses as BAR0 offsets | `ssb.c:159` | **Critical** | Instant page fault during `ssb_enum_cores` |
| F10 | Core base addresses not normalized after enumeration | `ssb.c:249-260` | **Critical** | Fault on `ssb_core_enable()` for any non-ChipCommon core |
| F11 | DMA descriptor `addr_hi` hardcoded to 0 (truncates addresses > 4 GB) | `b43bsd_dma.c:73-74` | **High** | Silent memory corruption on >4 GB machines |
| F12 | BAR0 size + BAR0_WIN only printed with `B43BSD_DEBUG` | `b43bsd.c:220-229` | **Low** | Delayed diagnosis; invisible on stock kernel |

### Fix Details

**F9 (ssb.c:159):** Changed `enum_base = SSB_ENUM_BASE` (0x18000000) to `enum_base = 0`. BAR0_WIN maps ChipCommon at BAR0 offset 0, so core slots are at relative offsets 0x0000, 0x1000, 0x2000, ... not absolute backplane addresses.

**F10 (ssb.c:249-260):** Added core base normalization after enumeration. ADMATCH0 reports absolute backplane addresses (e.g. 0x18002000 for 802.11 core). These are now normalized to ChipCommon-relative (e.g. 0x2000) so `ssb_core_read/write` functions compute correct BAR0 offsets.

```c
if (bus->chipcommon_idx >= 0) {
    uint32_t cc_base = bus->cores[bus->chipcommon_idx].base;
    for (j = 0; j < bus->ncores; j++)
        bus->cores[j].base -= cc_base;
}
```

**F11 (b43bsd_dma.c:73-74):** `addr_hi` now receives the full upper 32 bits of the physical address instead of being hardcoded to 0. `addr_lo` masks to low 32 bits.

**F12 (b43bsd.c:220-229):** Unconditional `printf` of BAR0 size and PCI config 0x80 (BAR0_WIN) during attach. Previously gated behind `B43BSD_DEBUG`.

---

## Remaining Known Risks

### R1: MIMO PHY Core Outside BAR0 Window (20% probability)

If BAR0 is 16 KB (0x4000), the MIMO PHY core at slot 4 (BAR0+0x4000) is **outside the mapped window**. The enumeration terminates at slot 3 (reading 0xFFFFFFFF from unmapped space = end-of-list marker).

**Effect:** `mimo_phy_idx = -1` → `b43bsd_phy_n_attach` returns `ENXIO` → `b43bsd_init` fails → interface never comes up. **Does not panic — fails gracefully.**

**Diagnosis on first boot:** dmesg will show:
```
b43bsd0: BAR0 0x4000 win=0xXXXXXXXX: BCM4331 rev 2
ssb0 at b43bsd0: Sonics Silicon Backplane, chip BCM4331 rev 2
  core0: ChipCommon rev 0x28
  core1: PCIe rev 0x11
  core2: IEEE 802.11 rev 0x1d
  core3: ARM Cortex-M3 rev 0x01
```
No MIMO PHY core → PHY attach fails.

**Workaround if needed:** Program BAR0_WIN to shift the window to cover the MIMO PHY core, or map a second bus_space subregion.

### R2: RX Completion Detection (30% probability)

`b43bsd_dma_rx_process` detects completion by checking if hardware cleared `FRAMESTART`. The BCM4331 64-bit DMA engine may use a different ownership mechanism. If wrong:
- (a) All descriptors appear "owned by hardware" → infinite loop → hang
- (b) All descriptors appear "complete" → use-after-free → corruption

**Mitigation:** The loop iterates `ring->nslots` times (bounded to 256) so (a) would hang for at most 256 iterations per interrupt, not infinite. Case (b) is more dangerous.

### R3: Interrupt Sharing on MacBook (10% probability)

The BCM4331 on MacBook Pro 9,2 shares its PCI interrupt line with other devices (FireWire, SD card reader). The handler correctly returns 0 for non-our interrupts. But if another device generates interrupts during firmware upload (when DMA is not yet set up and `B43BSD_FLAG_RUNNING` is clear), the handler masks interrupts and returns 0. This shouldn't cause issues.

### R4: SPROM Data Integrity (5% probability)

No CRC16 checksum verification. If the SPROM is corrupted or the parsing is wrong, the MAC address, TX power limits, or regulatory domain could be wrong. The MAC address is validated (not all-zero, not multicast). TX power and regulatory are not validated.

### R5: Initvals Board-Specific Tuning (10% probability)

`n16initvals30.fw` was extracted from broadcom-wl-5.100.138. These register values are board-specific. If the extracted firmware is for a different board revision with different FEM/LNA/antenna configuration, TX power and RX sensitivity could be suboptimal (not fatal).

---

## Certainty Breakdown (Post-Fix)

| Component | Before Fix | After Fix | Risk |
|-----------|-----------|-----------|------|
| PCI attach + BAR0 map | 95% | 95% | Standard pattern |
| SSB enumeration | **30%** | 95% | F9+F10 fixed; R1 remains |
| Core offset calculation | 90% | 90% | Works post-normalization |
| Core enable (SSB) | **30%** | 95% | F10 fixed; R1 remains |
| PMU init | 85% | 85% | Uses raw ssb_read32 (correct) |
| SPROM parse | 85% | 85% | No checksum; R4 risk |
| PCIe ASPM disable | 90% | 90% | Correct logic |
| net80211 attach | 95% | 95% | All API calls verified |
| Firmware upload | 90% | 90% | All offsets verified ✓ |
| Initvals upload | 90% | 90% | All offsets verified ✓ |
| PHY init | **75%** | 85% | R1: MIMO PHY visibility |
| DMA init | **85%** | 95% | F11 fixed 64-bit addr |
| Interrupt handler | 85% | 85% | Storm protection works |
| RX processing | **60%** | **70%** | R2: completion detection |
| TX processing | 80% | 85% | Per-slot maps correct |
| **Overall (attach + fw + DMA + IRQ)** | **~70%** | **~92%** | |
| **End-to-end functional WiFi** | **~50%** | **~75%** | R1+R2 dominate |

---

## What dmesg Should Show (If Everything Works)

```
b43bsd0 at pci9 dev 0 function 0 "Broadcom BCM4331" rev 0x02: BAR0 0x4000 win=0x18000000: BCM4331 rev 2
ssb0 at b43bsd0: Sonics Silicon Backplane, chip BCM4331 rev 2, PMU rev X
  core0: ChipCommon rev 0x28
  core1: PCIe rev 0x11
  core2: IEEE 802.11 rev 0x1d
  core3: ARM Cortex-M3 (ucode) rev 0x01
  core4: MIMO PHY rev 0x10
b43bsd0: SPROM rev 8
b43bsd0: MAC address xx:xx:xx:xx:xx:xx
b43bsd0: firmware rev X patch Y loaded
b43bsd0: initvals loaded (rev 16)
b43bsd0: BCM2056 radio rev 9
b43bsd0: 64-bit DMA enabled, 256 TX / 256 RX descriptors
```

If `core4: MIMO PHY` is **missing**: BAR0 is too small (R1). The device will attach but `ifconfig b43bsd0 up` will fail with "PHY-N attach failed."

---

## Recovery

```
boot /obsd              # Previous working kernel
boot -c                 # Then "disable b43bsd" then "quit"
```

---

## Next Steps (Post First Boot)

1. **Check BAR0 size from dmesg.** If 0x4000 and no MIMO PHY core, implement BAR0_WIN switching.
2. **If PHY attaches:** run `ifconfig b43bsd0 up` and check for firmware timeout or DMA init errors.
3. **If interface comes up:** scan with `ifconfig b43bsd0 scan` to verify RX path.
4. **Verify RX completion logic** against Linux `b43_dma_rx()` if no frames are received.
5. **Add SPROM CRC16 check** for production quality.
