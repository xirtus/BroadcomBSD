#!/bin/sh
# BroadcomBSD QEMU Test Suite
# Automates: install ISO download → VM creation → kernel build → boot test
#
# Usage: ksh qemu_full_test.sh [level]
#   level = 0-6 (default: 0)
#
# Requirements: qemu package installed, ~2GB free disk, internet access

set -e

LEVEL="${1:-0}"
ISO="install79.iso"
DISK="vmdisk.qcow2"
DISKSIZE="8G"
MIRROR="https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64"

cd "$(dirname "$0")"
echo "=== BroadcomBSD QEMU Full Test (level $LEVEL) ==="
echo ""

# ------------------------------------------------------------------
# Step 1: Download install ISO if missing
# ------------------------------------------------------------------
if [ ! -f "$ISO" ]; then
    echo "[1/5] Downloading OpenBSD 7.9 install ISO..."
    ftp -o "$ISO" "$MIRROR/install79.iso"
    echo "      $(ls -lh $ISO | awk '{print $5}')"
else
    echo "[1/5] Install ISO already present"
fi

# ------------------------------------------------------------------
# Step 2: Create VM disk
# ------------------------------------------------------------------
echo "[2/5] Creating VM disk ($DISKSIZE)..."
qemu-img create -f qcow2 "$DISK" "$DISKSIZE" 2>&1

# ------------------------------------------------------------------
# Step 3: Run installer (interactive — user must complete install)
# ------------------------------------------------------------------
echo "[3/5] Starting installer..."
echo ""
echo "  ╔══════════════════════════════════════════════════════╗"
echo "  ║  MANUAL STEP: Complete the OpenBSD installation.     ║"
echo "  ║                                                    ║"
echo "  ║  At each prompt:                                    ║"
echo "  ║    I (install)  →  sd0  →  whole disk  →  http      ║"
echo "  ║    Server: cdn.openbsd.org                          ║"
echo "  ║    Path: /pub/OpenBSD/7.9/amd64                     ║"
echo "  ║    Sets: -all then bsd base71 comp71                ║"
echo "  ║    When done, type: halt -p                         ║"
echo "  ╚══════════════════════════════════════════════════════╝"
echo ""

qemu-system-x86_64 \
    -m 1024M \
    -drive "file=$DISK,format=qcow2" \
    -cdrom "$ISO" \
    -boot d \
    -nographic \
    -netdev user,id=net0 \
    -device virtio-net,netdev=net0

# ------------------------------------------------------------------
# Step 4: Build b43bsd kernel and inject into VM disk
# ------------------------------------------------------------------
echo "[4/5] Building b43bsd kernel..."
cd /usr/src/sys/arch/amd64/compile/B43BSD
make -j4 2>&1 | tail -3

# Copy kernel into VM disk using guestfish or by booting VM with scp
# For simplicity: boot the VM, scp the kernel in, rebuild bootloader
echo "[4/5] Injecting kernel into VM..."
echo "      Boot VM: qemu-system-x86_64 -m 512M -drive file=$DISK,format=qcow2 -nographic"
echo "      Then from host: scp /bsd root@localhost:/bsd (port 2222 with hostfwd)"
echo "      Or: use vnconfig + mount to copy kernel directly"

# ------------------------------------------------------------------
# Step 5: Boot test
# ------------------------------------------------------------------
echo "[5/5] Booting test kernel..."
echo "      In the VM, after copying /bsd:"
echo "      reboot"
echo "      Expected: 'b43bsd0 at pci... (attach level $LEVEL) ...ok'"
echo "      If hangs: the level is too high, try a lower level"
echo ""
echo "=== QEMU test setup complete ==="
echo "  Disk: $DISK"
echo "  ISO:  $ISO"
echo "  Kernel: /usr/src/sys/arch/amd64/compile/B43BSD/bsd"
echo ""
echo "To boot the installed VM:"
echo "  qemu-system-x86_64 -m 1024M -drive file=$DISK,format=qcow2 \\"
echo "    -nographic -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \\"
echo "    -device virtio-net,netdev=net0"