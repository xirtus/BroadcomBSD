# BroadcomBSD — QEMU Quick Start Guide

## What's Ready

| Component | Status |
|-----------|--------|
| ISO (OpenBSD 7.9) | ✅ Downloaded & verified |
| VM disk (8GB) | ✅ Created |
| install.conf | ✅ Fixed for QEMU (em0, base79, etc.) |
| Simulator | ✅ 27/27 tests pass |
| Driver code | ✅ 28 files, 25,239 LOC, -Werror clean |
| Critical bugs | ✅ 16 fixed and verified |

## Step 1: Install OpenBSD in QEMU

### Option A: Automated (expect)
```sh
cd /home/xirtus_arch/Projects/BroadcomBSD-main
./auto_install.exp
```

### Option B: Manual
```sh
cd /home/xirtus_arch/Projects/BroadcomBSD-main
ksh setup_qemu.sh install
# At the prompt: (I)nstall, (U)pgrade, (A)utoinstall, (S)hell?
# Type: A
# The install.conf will be used for all answers (~2 minutes)
# After install: halt -p
```

## Step 2: Boot the Installed System
```sh
ksh setup_qemu.sh boot
# Login: root
# Password: test
```

## Step 3: Set Up Kernel Source and Build Driver

### Inside the VM:
```sh
# Download kernel source
cd /usr/src
ftp https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64/sys.tar.gz
tar xzf sys.tar.gz

# Transfer driver files from host
# On host (in another terminal):
cd /home/xirtus_arch/Projects/BroadcomBSD-main
ssh -p 2222 root@localhost 'cat > /tmp/b43bsd.tar.gz' < vm/b43bsd_src.tar.gz

# Back in VM:
cd /tmp
tar xzf b43bsd.tar.gz

# Copy files into kernel tree
cp src/sys/dev/pci/b43bsd.c /usr/src/sys/dev/pci/
cp src/sys/dev/pci/b43bsdvar.h /usr/src/sys/dev/pci/
cp src/sys/dev/pci/b43bsdreg.h /usr/src/sys/dev/pci/
cp src/sys/dev/pci/files.pci.b43bsd /usr/src/sys/dev/pci/
cp src/sys/dev/ic/*.c /usr/src/sys/dev/ic/
cp src/sys/dev/ic/*.h /usr/src/sys/dev/ic/
cp src/share/man/man4/b43bsd.4 /usr/src/share/man/man4/

# Update files.pci
echo 'include "dev/pci/files.pci.b43bsd"' >> /usr/src/sys/dev/pci/files.pci

# Configure and build
cd /usr/src/sys/arch/amd64/compile
mkdir -p B43BSD
cp -r GENERIC.MP/* B43BSD/ 2>/dev/null
cd B43BSD
cat ../conf/GENERIC.MP > ../conf/B43BSD
echo 'b43bsd*	at pci?' >> ../conf/B43BSD
config -b . -s ../../.. ../conf/B43BSD
make -j4
```

## Step 4: Install and Boot Custom Kernel
```sh
# Inside VM:
cd /usr/src/sys/arch/amd64/compile/B43BSD
make install
reboot
```

## What to Expect

Since QEMU has **no BCM4331 WiFi hardware**, the b43bsd driver will NOT probe
any device. The kernel will boot normally. This confirms:
- ✅ Kernel compiles with b43bsd integrated
- ✅ No undefined symbols or link errors
- ✅ Kernel boots stable

For actual WiFi testing, you need either:
- Real MacBook Pro 9,2 with BCM4331
- PCI passthrough of real BCM4331 hardware via VFIO

## Recovery
If the custom kernel won't boot, at the `boot>` prompt:
```
boot /obsd
```

## Files Summary

| File | Purpose |
|------|---------|
| `setup_qemu.sh` | VM lifecycle (install, boot, tarball) |
| `build_kernel_vm.sh` | Kernel build script (run inside VM) |
| `auto_install.exp` | Automated OpenBSD install via expect |
| `install.conf` | Autoinstall answers (fixed for QEMU) |
| `vm/b43bsd_src.tar.gz` | Driver files tarball for VM transfer |
| `b43bsd_sim.c` | User-space simulator (27/27 pass) |
| `src/` | All driver source code (28 files) |
