#!/bin/sh
# BroadcomBSD — QEMU VM Setup (OpenBSD 7.9 + b43bsd driver)
#
# This script sets up everything needed to run OpenBSD in QEMU,
# build the b43bsd kernel, and boot-test it.
#
# Usage:  ksh setup_qemu.sh [install|boot|build|test]

set -e
cd "$(dirname "$0")"

ISO="vm/install79.iso"
DISK="vm/vmdisk.qcow2"
CFGDISK="vm/install.img"
VM_MEM="1024M"
ACTION="${1:-help}"

# ------------------------------------------------------------------
# Helper: Check prerequisites
# ------------------------------------------------------------------
check_prereqs() {
    local missing=""
    for cmd in qemu-system-x86_64 qemu-img curl; do
        which $cmd >/dev/null 2>&1 || missing="$missing $cmd"
    done
    if [ -n "$missing" ]; then
        echo "Missing tools:$missing"
        echo "Install with: pacman -S qemu-desktop curl"
        exit 1
    fi
}

# ------------------------------------------------------------------
# Download ISO
# ------------------------------------------------------------------
download_iso() {
    if [ -f "$ISO" ] && [ "$(stat -c%s "$ISO" 2>/dev/null)" -gt 700000000 ]; then
        echo "ISO already downloaded: $(ls -lh $ISO | awk '{print $5}')"
        return
    fi
    echo "Downloading OpenBSD 7.9 install ISO..."
    curl -L -o "$ISO" "https://cdn.openbsd.org/pub/OpenBSD/7.9/amd64/install79.iso"
    echo "Downloaded: $(ls -lh $ISO | awk '{print $5}')"
}

# ------------------------------------------------------------------
# Create disk images
# ------------------------------------------------------------------
create_disks() {
    if [ ! -f "$DISK" ]; then
        echo "Creating VM disk (8GB)..."
        qemu-img create -f qcow2 "$DISK" 8G
    else
        echo "VM disk already exists: $DISK"
    fi

    if [ ! -f "$CFGDISK" ]; then
        echo "Creating install config disk..."
        dd if=/dev/zero of="$CFGDISK" bs=1M count=10 2>/dev/null
        /sbin/mkfs.vfat "$CFGDISK" 2>/dev/null || /sbin/mkfs.msdos "$CFGDISK" 2>/dev/null
        mcopy -i "$CFGDISK" install.conf ::/install.conf 2>/dev/null
    else
        # Refresh the install.conf on the config disk
        mdel -i "$CFGDISK" ::/install.conf 2>/dev/null || true
        mdel -i "$CFGDISK" ::/INSTAL~1.CON 2>/dev/null || true
        mcopy -i "$CFGDISK" install.conf ::/install.conf 2>/dev/null
    fi
    echo "Disks ready."
}

# ------------------------------------------------------------------
# Step 1: Install OpenBSD (interactive — just type A for autoinstall)
# ------------------------------------------------------------------
run_install() {
    download_iso
    create_disks

    echo ""
    echo "  ╔══════════════════════════════════════════════════════════╗"
    echo "  ║  AUTO-INSTALL: Type 'A' at the first prompt.             ║"
    echo "  ║                                                        ║"
    echo "  ║  The install.conf will be used for all answers.         ║"
    echo "  ║  This takes ~2 minutes to download sets.                ║"
    echo "  ║  After install completes, type: halt -p                ║"
    echo "  ╚══════════════════════════════════════════════════════════╝"
    echo ""

    qemu-system-x86_64 \
        -m "$VM_MEM" \
        -drive "file=$DISK,format=qcow2" \
        -drive "file=$CFGDISK,format=raw" \
        -cdrom "$ISO" \
        -boot d \
        -nographic \
        -netdev user,id=net0 \
        -device e1000,netdev=net0
}

# ------------------------------------------------------------------
# Step 2: Boot installed system
# ------------------------------------------------------------------
boot_vm() {
    if [ ! -f "$DISK" ]; then
        echo "No VM disk found. Run: $0 install"
        exit 1
    fi

    echo "Booting installed OpenBSD VM..."
    echo "Login as root (password: test)"
    echo ""

    qemu-system-x86_64 \
        -m "$VM_MEM" \
        -drive "file=$DISK,format=qcow2" \
        -nographic \
        -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \
        -device e1000,netdev=net0
}

# ------------------------------------------------------------------
# Step 3: Build kernel inside VM (run from INSIDE the VM)
# ------------------------------------------------------------------
build_kernel() {
    echo "=== b43bsd Kernel Build (run INSIDE the OpenBSD VM) ==="
    echo ""
    echo "Step 1: Set up source tree"
    echo "  cd /usr/src"
    echo "  tar xzf /usr/src/sys.tar.gz   # if not already extracted"
    echo ""
    echo "Step 2: Copy driver files into source tree"
    echo "  (run copy_driver_files.sh from this directory)"
    echo ""
    echo "Step 3: Update kernel config"
    echo "  cd /usr/src/sys/arch/amd64/compile/GENERIC.MP"
    echo "  cp GENERIC.MP GENERIC.MP.bak"
    echo "  echo 'b43bsd* at pci?' >> GENERIC.MP"
    echo ""
    echo "Step 4: Add files.pci entry"
    echo "  cat sys/dev/pci/files.pci.b43bsd >> /usr/src/sys/dev/pci/files.pci"
    echo ""
    echo "Step 5: Build"
    echo "  cd /usr/src/sys/arch/amd64/compile/GENERIC.MP"
    echo "  make config"
    echo "  make -j4"
    echo ""
    echo "Step 6: Install and reboot"
    echo "  make install"
    echo "  reboot"
}

# ------------------------------------------------------------------
# Copy driver files into VM (run from HOST)
# ------------------------------------------------------------------
copy_files() {
    echo "Copying b43bsd driver files into place..."
    echo ""
    echo "=== To copy files into the VM ==="
    echo ""
    echo "Option 1: Use SCP (if VM is running with hostfwd)"
    echo "  scp -P 2222 src/sys/dev/pci/b43bsd.c root@localhost:/usr/src/sys/dev/pci/"
    echo "  scp -P 2222 src/sys/dev/pci/b43bsdvar.h root@localhost:/usr/src/sys/dev/pci/"
    echo "  scp -P 2222 src/sys/dev/pci/b43bsdreg.h root@localhost:/usr/src/sys/dev/pci/"
    echo "  scp -P 2222 src/sys/dev/ic/*.c root@localhost:/usr/src/sys/dev/ic/"
    echo "  scp -P 2222 src/sys/dev/ic/*.h root@localhost:/usr/src/sys/dev/ic/"
    echo "  scp -P 2222 src/sys/dev/pci/files.pci.b43bsd root@localhost:/usr/src/sys/dev/pci/"
    echo ""
    echo "Option 2: Create a tar file and mount it in QEMU"
    echo "  tar czf b43bsd_src.tar.gz src/"
    echo "  (then inside VM: ftp or mount to get the file)"
    echo ""
    echo "Option 3: Use QEMU guest agent (if installed)"
    echo "  qemu-ga is not typically available on OpenBSD"
}

# ------------------------------------------------------------------
# Prepare driver tarball for transfer
# ------------------------------------------------------------------
prepare_tarball() {
    echo "Creating b43bsd driver tarball..."
    tar czf vm/b43bsd_src.tar.gz \
        src/sys/dev/pci/b43bsd.c \
        src/sys/dev/pci/b43bsdvar.h \
        src/sys/dev/pci/b43bsdreg.h \
        src/sys/dev/pci/files.pci.b43bsd \
        src/sys/dev/ic/ssb.c \
        src/sys/dev/ic/ssbvar.h \
        src/sys/dev/ic/ssbreg.h \
        src/sys/dev/ic/b43bsd_*.c \
        src/sys/dev/ic/b43bsd_*.h \
        src/sys/arch/amd64/conf/GENERIC.b43bsd \
        src/share/man/man4/b43bsd.4
    echo "Created: $(ls -lh vm/b43bsd_src.tar.gz | awk '{print $5}')"
    echo ""
    echo "To transfer to VM, run the 'boot_vm' with hostfwd, then:"
    echo "  ssh -p 2222 root@localhost 'cat > /tmp/b43bsd_src.tar.gz' < vm/b43bsd_src.tar.gz"
    echo "  ssh -p 2222 root@localhost 'cd /usr/src && tar xzf /tmp/b43bsd_src.tar.gz'"
}

# ------------------------------------------------------------------
# Help
# ------------------------------------------------------------------
show_help() {
    echo "BroadcomBSD QEMU Setup"
    echo ""
    echo "Usage: ksh setup_qemu.sh <action>"
    echo ""
    echo "Actions:"
    echo "  install    — Download ISO, create disks, boot installer"
    echo "               (Type 'A' for autoinstall at the first prompt)"
    echo "  boot       — Boot the installed OpenBSD VM"
    echo "  build      — Show kernel build instructions (run inside VM)"
    echo "  tarball    — Prepare driver tarball for transfer to VM"
    echo "  copy       — Show instructions for copying files into VM"
    echo ""
    echo "Quick start:"
    echo "  1. ksh setup_qemu.sh install    # install OpenBSD"
    echo "  2. ksh setup_qemu.sh boot       # boot the VM"
    echo "  3. ksh setup_qemu.sh tarball    # prepare tarball"
    echo "  4. # Inside VM, follow 'build' instructions"
}

# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------
check_prereqs
case "$ACTION" in
    install)   run_install ;;
    boot)      boot_vm ;;
    build)     build_kernel ;;
    tarball)   prepare_tarball ;;
    copy)      copy_files ;;
    help|*)    show_help ;;
esac
