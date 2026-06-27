#!/bin/sh
# BroadcomBSD — Kernel Build Script (run INSIDE the OpenBSD VM)
#
# This script sets up the kernel source tree, copies the b43bsd driver
# files, updates kernel config, and builds the custom kernel.
#
# Usage (inside VM): ksh build_kernel_vm.sh

set -e

SRC="/usr/src/sys"
BLD="$SRC/arch/amd64/compile/B43BSD"
DRV="/home/xirtus_bsd/Projects/BroadcomBSD"

echo "=== BroadcomBSD Kernel Builder ==="
echo ""

# ------------------------------------------------------------------
# Step 1: Verify we're on OpenBSD
# ------------------------------------------------------------------
if [ "$(uname -s)" != "OpenBSD" ]; then
    echo "ERROR: This script must run inside the OpenBSD VM."
    echo "Run setup_qemu.sh first to create and boot the VM."
    exit 1
fi

echo "[1/7] Checking source tree..."
if [ ! -d "$SRC/arch/amd64" ]; then
    echo "Source tree not found. Extracting..."
    cd /usr/src
    if [ -f /usr/src/sys.tar.gz ]; then
        tar xzf sys.tar.gz
    elif [ -f /home/xirtus_bsd/Projects/BroadcomBSD/sys.tar.gz ]; then
        tar xzf /home/xirtus_bsd/Projects/BroadcomBSD/sys.tar.gz
    else
        echo "ERROR: sys.tar.gz not found. Download with:"
        echo "  ftp https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64/sys.tar.gz"
        exit 1
    fi
fi
echo "  Source tree OK"

# ------------------------------------------------------------------
# Step 2: Copy driver files
# ------------------------------------------------------------------
echo "[2/7] Copying b43bsd driver files..."
mkdir -p "$SRC/dev/pci" "$SRC/dev/ic" "$SRC/share/man/man4"

# PCI driver files
cp "$DRV/src/sys/dev/pci/b43bsd.c"     "$SRC/dev/pci/"     2>/dev/null || true
cp "$DRV/src/sys/dev/pci/b43bsdvar.h"   "$SRC/dev/pci/"     2>/dev/null || true
cp "$DRV/src/sys/dev/pci/b43bsdreg.h"   "$SRC/dev/pci/"     2>/dev/null || true
cp "$DRV/src/sys/dev/pci/files.pci.b43bsd" "$SRC/dev/pci/"  2>/dev/null || true

# IC (bus-independent) files
cp "$DRV/src/sys/dev/ic/ssb.c"          "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/ssbvar.h"       "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/ssbreg.h"       "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_dma.c"   "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_dma.h"   "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_fw.c"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_fw.h"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_phy_n.c" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_phy_n.h" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_rate.c"  "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_rate.h"  "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_sprom.c" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_debug.c" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_ps.c"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_ps.h"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_wa.c"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_wa.h"    "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_btcoex.c" "$SRC/dev/ic/"     2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_btcoex.h" "$SRC/dev/ic/"     2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_tables.c" "$SRC/dev/ic/"     2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_tables.h" "$SRC/dev/ic/"     2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_radio.c" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_radio.h" "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_xmit.c"  "$SRC/dev/ic/"      2>/dev/null || true
cp "$DRV/src/sys/dev/ic/b43bsd_xmit.h"  "$SRC/dev/ic/"      2>/dev/null || true

# Man page
cp "$DRV/src/share/man/man4/b43bsd.4"   "$SRC/share/man/man4/" 2>/dev/null || true

echo "  Driver files copied"

# ------------------------------------------------------------------
# Step 3: Update files.pci
# ------------------------------------------------------------------
echo "[3/7] Updating kernel config..."
if ! grep -q "files.pci.b43bsd" "$SRC/dev/pci/files.pci" 2>/dev/null; then
    echo 'include "dev/pci/files.pci.b43bsd"' >> "$SRC/dev/pci/files.pci"
    echo "  Added include to files.pci"
else
    echo "  files.pci already includes b43bsd"
fi

# ------------------------------------------------------------------
# Step 4: Create B43BSD kernel config
# ------------------------------------------------------------------
echo "[4/7] Creating B43BSD kernel config..."
mkdir -p "$BLD"

if [ -f "$SRC/arch/amd64/conf/GENERIC" ]; then
    BASE="$SRC/arch/amd64/conf/GENERIC"
else
    BASE="$SRC/arch/amd64/conf/GENERIC.MP"
fi

cat "$BASE" > "$SRC/arch/amd64/conf/B43BSD"
echo "" >> "$SRC/arch/amd64/conf/B43BSD"
echo "# Broadcom BCM4331 WiFi driver" >> "$SRC/arch/amd64/conf/B43BSD"
echo "b43bsd*	at pci?" >> "$SRC/arch/amd64/conf/B43BSD"
echo "  Created B43BSD config from $BASE"

# ------------------------------------------------------------------
# Step 5: Run config
# ------------------------------------------------------------------
echo "[5/7] Running config..."
cd "$BLD"
make config 2>&1 || {
    echo "  Config failed! Trying alternate method..."
    config -b "$BLD" -s "$SRC" "$SRC/arch/amd64/conf/B43BSD"
}
echo "  Config OK"

# ------------------------------------------------------------------
# Step 6: Build kernel
# ------------------------------------------------------------------
echo "[6/7] Building kernel (this may take a few minutes)..."
make -j4 2>&1 | tail -20
echo "  Build complete"

# ------------------------------------------------------------------
# Step 7: Verify and install
# ------------------------------------------------------------------
echo "[7/7] Verifying kernel..."
if [ -f "$BLD/bsd" ]; then
    SIZE=$(ls -lh "$BLD/bsd" | awk '{print $5}')
    echo "  Kernel: $BLD/bsd ($SIZE)"
    echo ""
    echo "  Checking b43bsd symbols..."
    nm "$BLD/bsd" | grep -c 'b43bsd' || echo 0
    echo "  symbols found"
else
    echo "  ERROR: bsd kernel not built!"
    exit 1
fi

echo ""
echo "=== Build complete! ==="
echo ""
echo "To install the new kernel:"
echo "  make -C $BLD install"
echo "  reboot"
echo ""
echo "If the new kernel won't boot, at the boot> prompt:"
echo "  boot /obsd"
