# BroadcomBSD

**Linux b43 → OpenBSD kernel driver for BCM4331 WiFi (MacBook Pro 9,2)**

🚧 **Under construction — not yet functional** 🚧

## Status

- 25,239 lines across 28 files ported from Linux b43
- 330 kernel symbols, compiles `-Werror` clean
- 27/27 simulator tests pass
- **Fails to attach on real hardware** — system hangs during `b43bsd_attach()`
- Active debugging with staged attach (6 progressive levels)

## Bugs Found & Fixed

| Bug | Severity | Description |
|-----|----------|-------------|
| F23 | Critical | PCIe core reset killed link — 1µs delays insufficient for retraining |
| F20 | Critical | DMA descriptor: buffer length in wrong word, wrong ownership bit |
| F19 | Critical | `b43bsd_chip_init()` never called on initial boot |
| F16 | Critical | SHM firmware command offsets wrong (+4 bytes) |
| F21 | High | MIMO PHY core inaccessible on 16KB BAR0 |
| F22 | Medium | No SPROM CRC16 validation |

## Quick Start

```sh
# Build kernel
cd /usr/src/sys/arch/amd64/compile/B43BSD && make -j4

# Install (with safe fallback)
cp /bsd /bsd.safe        # keep stock kernel as fallback
cp bsd /bsd

# Set attach level (0=print-only, 6=full)
ksh stage.sh 0

# Reboot
reboot
```

**If it hangs:** power-cycle, `boot /bsd.safe` at the bootloader prompt.

## Staged Attach

The attach function is controlled by `b43bsd_attach_level`:

| Level | What it does | Expected dmesg |
|-------|-------------|----------------|
| 0 | Print PCI IDs, no hardware touch | `stub-ok` |
| 1 | + BAR0 mapping + size print | `level1-ok` |
| 2 | + chip ID read from MMIO | `level2-ok` |
| 3 | + SSB bus enumeration (cores) | `level3-ok` |
| 4 | + PMU init + core enable + offset | `level4-ok` |
| 5 | + SPROM + MAC + net80211 attach | `level5-ok` |
| 6 | + interrupt + chip reset + LED | `level6-ok` |

Use `ksh stage.sh <level>` to switch levels.

## QEMU Testing

```sh
# 1. Download install ISO (one time)
ftp -o install79.iso https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64/install79.iso

# 2. Create VM disk
qemu-img create -f qcow2 vmdisk.qcow2 8G

# 3. Boot installer
qemu-system-x86_64 -m 1024M \
  -drive file=vmdisk.qcow2,format=qcow2 \
  -cdrom install79.iso -boot d \
  -nographic -netdev user,id=net0 -device virtio-net,netdev=net0

# 4. After install: boot with b43bsd kernel
#    Copy /bsd from host into VM, reboot, test attach levels
```

## Simulator

```sh
cc -Wall -O2 -o b43bsd_sim b43bsd_sim.c && ./b43bsd_sim
```

Validates: BAR0 boundary checking, SSB enumeration, core normalization, core-relative register access, DMA descriptor format. 27 tests covering all register access patterns.

## Files

```
src/sys/
├── arch/amd64/conf/GENERIC.b43bsd    # kernel config snippet
├── dev/pci/
│   ├── b43bsd.c                      # main driver (4222 lines)
│   ├── b43bsdvar.h                   # softc struct
│   ├── b43bsdreg.h                   # MMIO register defs
│   └── files.pci.b43bsd              # kernel build wiring
└── dev/ic/
    ├── ssb.c                         # SSB bus (enum, PMU, reset)
    ├── ssbvar.h                      # SSB structs + accessors
    ├── ssbreg.h                      # SSB register defs
    ├── b43bsd_dma.c/.h               # 64-bit DMA engine
    ├── b43bsd_fw.c/.h                # firmware upload
    ├── b43bsd_phy_n.c/.h             # N-PHY init + calibration
    ├── b43bsd_tables.c/.h            # register init tables
    ├── b43bsd_radio.c/.h             # BCM2056 radio
    ├── b43bsd_rate.c/.h              # Minstrel-HT rate control
    ├── b43bsd_sprom.c                # SPROM parser
    ├── b43bsd_debug.c                # debug dumps
    ├── b43bsd_ps.c/.h                # power save
    ├── b43bsd_wa.c/.h                # workarounds
    ├── b43bsd_btcoex.c/.h            # BT coexistence
    └── b43bsd_xmit.c/.h              # advanced TX
```

## License

ISC — see individual file headers.
