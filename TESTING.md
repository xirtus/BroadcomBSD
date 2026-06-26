# BroadcomBSD — Safe Testing Methodology

## Why It Panicked (Root Cause Analysis)

After auditing all 2800+ lines of driver code, four bugs were found. The
most likely panic cause was:

### Bug A: SSB enumeration reads beyond BAR0 (CRITICAL — likely the panic)
`ssb_enum_cores()` iterated all 16 core slots regardless of BAR0 size.
BCM4331 MacBooks often have BAR0 = 0x4000 (16 KB). Slot 4 (MIMO PHY core)
has registers at BAR0+0x4FFC, which is **beyond** a 16 KB mapping.
`bus_space_read_4()` on OpenBSD/amd64 accesses virtual memory directly —
reading beyond the mapped region causes a page fault → kernel panic.

**Fix:** Added `bus->mapsize` field; `ssb_enum_cores` now checks
`offset + SSB_CORE_SIZE > mapsize` before reading each slot.

### Bug B: `ssb_core_read32` guard blocks ChipCommon (CRITICAL)
After SSB normalization, ChipCommon's `base == 0`. The guard
`if (core->base == 0) return 0` silently blocked ALL ChipCommon access.
This broke SPROM reads (wrong MAC address), PLL reset (silent no-op),
and would break any future ChipCommon operations.

**Fix:** Removed `core->base == 0` check from all four `ssb_core_read*/write*`
functions. After normalization, a base of 0 is valid for ChipCommon.

### Bug C: PCI interrupts permanently disabled (HIGH)
`b43bsd_attach` sets `PCI_COMMAND_INTERRUPT_DISABLE` to prevent EFI-spurious
interrupts but **never clears it**. The device's `B43_IRQ_MASK_NORMAL` was
programmed but PCI would never deliver the interrupt.

**Fix:** Clear `PCI_COMMAND_INTERRUPT_DISABLE` in `b43bsd_init` after
device interrupts are configured.

### Bug D: CHIPID mask/shift wrong (LOW)
`B43_CHIPID_MASK = 0x0000ffff, B43_CHIPID_SHIFT = 0` extracted bits 0–15
(chip ID) instead of bits 16–19 (revision). The `printf` used a different
(safe) inline extraction, so this was cosmetic but could confuse debugging.

**Fix:** Changed to `B43_CHIPID_MASK = 0x000f0000, SHIFT = 16`.

---

## Pre-Boot Certainty Checklist

Before ANY boot, run:

```sh
ksh /home/xirtus_bsd/Projects/BroadcomBSD/verify.sh
```

This checks:
1. All source files present
2. All firmware files present and non-empty
3. Kernel compiles without errors
4. Zero undefined symbols in kernel binary
5. All bus_space accesses use core-offset helpers (no raw BAR0 writes to wrong core)
6. SSB enumeration has BAR0 boundary check
7. `ssb_core_read32` guard fixed
8. `PCI_COMMAND_INTERRUPT_DISABLE` cleared in init path
9. bwi match table no longer includes BCM4331 (no driver conflict)

**Only boot if all checks pass.**

---

## Safe Boot Strategy

### Option 1: Boot config -c (disable at boot)
At the `boot>` prompt:
```
boot /bsd -c
```
Then at `UKC>`:
```
disable b43bsd
quit
```
This boots the new kernel with the driver **disabled**. Verify the kernel
works at all (no unrelated breakage). Then re-enable with `config -e` or
at next boot.

### Option 2: Dual kernel with fallback
Keep `/obsd` as the known-working kernel.
```
cp /bsd /bsd.b43bsd-test
```
If the test kernel panics:
```
boot /obsd
```

### Option 3: Boot serial console
If available, boot with serial console to capture panic messages:
```
boot /bsd -c
UKC> disable b43bsd
UKC> quit
```
Then `ifconfig b43bsd0 up` after boot to trigger the init path manually.

---

## Expected dmesg (Working Case)

```
b43bsd0 at pci9 dev 0 function 0 "Broadcom BCM4331" rev 0x02: BAR0 0x4000 win=0xXXXXXXXX: BCM4331 rev 2
ssb0 at b43bsd0: Sonics Silicon Backplane, chip BCM4331 rev 2
  core0: ChipCommon rev 0x28
  core1: PCIe rev 0x11
  core2: IEEE 802.11 rev 0x1d
  core3: ARM Cortex-M3 (ucode) rev 0x01
b43bsd0: SPROM rev 8
b43bsd0: MAC address xx:xx:xx:xx:xx:xx
```

If core enumeration shows **only 4 cores** (no MIMO PHY), BAR0 is 16 KB
and the MIMO PHY core is at slot 4 beyond the window. The driver will
attach but fail `ifconfig up` with "PHY-N attach failed." This is a
**non-panic graceful failure** — fixable with BAR0_WIN switching.

---

## Recovery Commands

```
boot /obsd              # Boot previous working kernel
boot -c                 # UKC> disable b43bsd; quit
boot bsd.rd             # Ramdisk kernel for upgrade/recovery
```

---

## Post-Boot Verification

After successful boot with the new kernel:

```sh
# 1. Check driver attached
dmesg | grep b43bsd

# 2. Check SSB cores enumerated
dmesg | grep ssb

# 3. Try bringing interface up
ifconfig b43bsd0 up

# 4. Check for firmware load errors
dmesg | grep -E 'firmware|initval|PHY'

# 5. If up, scan for networks
ifconfig b43bsd0 scan
```
