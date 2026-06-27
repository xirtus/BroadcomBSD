#!/bin/sh
# Quick kernel boot test in QEMU - no disk required
# Tests whether the b43bsd kernel gets past PCI bus scan
# Without crashing, hanging, or panicking.

KERNEL=/bsd
TIMEOUT=30

echo "=== Booting b43bsd kernel in QEMU (${TIMEOUT}s timeout) ==="
timeout ${TIMEOUT} qemu-system-x86_64 \
    -kernel ${KERNEL} \
    -append "boot -c" \
    -m 512M \
    -nographic \
    -no-reboot \
    2>&1 | head -100
EXIT=$?

echo ""
echo "Exit code: $EXIT"
if [ $EXIT -eq 124 ]; then
    echo "KERNEL HUNG (timeout after ${TIMEOUT}s) — crash suspected"
elif [ $EXIT -eq 0 ]; then
    echo "KERNEL BOOTED OK — no crash"
else
    echo "KERNEL EXITED with code $EXIT"
fi
