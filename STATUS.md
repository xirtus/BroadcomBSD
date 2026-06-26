# BroadcomBSD — Status After Session 2 (2026-06-25)

## What Was Done

7 new modules added: power save (PS-poll, DTIM wake, listen interval),
hardware workarounds (TX queue, TSF spur, PCIe TLP, EFI reset),
BT coexistence (GPIO state machine), per-channel PLL tuning (fractional-N),
per-chain TX power (3×3 MIMO), per-chain IQ calibration (C1+C2),
and regulatory domain enforcement (FCC/ETSI/Japan).

Total: 8,610 lines (+1,556 this session) across 22 files.

## Verification

- 34/34 checks passed
- Kernel compiles with -Werror
- 0 undefined symbols (94 b43bsd + 15 ssb)
- 27/27 simulator tests passed
- bwi conflict resolved (BCM4331 removed from bwi match table)

## New Modules (This Session)
| File | Lines | Purpose |
|------|-------|---------|
| `b43bsd_ps.c/.h` | 428/46 | 802.11 STA power save (PS-poll, DTIM wake) |
| `b43bsd_wa.c/.h` | 325/15 | BCM4331 hardware workarounds (5 errata) |
| `b43bsd_btcoex.c/.h` | 282/14 | BT coexistence GPIO state machine |

## Expanded Files
| File | Old | New | Changes |
|------|-----|-----|---------|
| `b43bsd_phy_n.c` | 1065 | 1370 | PLL tuning tables, per-chain TX, C1+C2 IQ cal |
| `b43bsd_phy_n.h` | 301 | 351 | PLL/per-chain/PAPD register definitions |
| `b43bsd_sprom.c` | 243 | 280 | Country code, regulatory enforcement |
| `b43bsd.c` | 1820 | 1879 | PS/BT/WA wiring, TX wake hooks |
| `b43bsdvar.h` | 287 | 307 | PS state, BT state, TSF mode, new includes |

## Crash Bugs Fixed (Previous Session)
| # | Bug | Status |
|---|-----|--------|
| B1 | SSB enumeration reads beyond BAR0 | Fixed |
| B2 | ChipCommon base==0 guard blocked access | Fixed |
| B3 | PCI interrupts permanently disabled | Fixed |
| B4 | CHIPID mask/shift wrong | Fixed |
| B5 | PCI interrupt re-enabled before IRQ mask | Fixed |
| B6 | No guard for missing 802.11 core | Fixed |

## Remaining
- N-PHY calibration tables (5k LOC): loaded from firmware, not compiled in
- A-MPDU reorder buffer: sessions managed, reorder logic TBD
- Hardware boot test: pending MacBook availability
- New files not yet in kernel build tree (files.pci needs update)

## Safe Boot Instructions
```
boot /bsd -c        # Boot new kernel with UKC
UKC> disable b43bsd  # Disable driver, test kernel boots
UKC> quit            # Continue boot

# If panic: boot /obsd or /safebsd
# If clean boot: config -e -o /bsd.new -f /bsd
#   Then: enable b43bsd, quit, mv /bsd.new /bsd, reboot
```
