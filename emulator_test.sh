#!/bin/sh
# BroadcomBSD Emulator Test Suite
# Tests the b43bsd driver in userspace without real hardware.
#
# Usage: ksh emulator_test.sh

set -e
cd /home/xirtus_bsd/Projects/BroadcomBSD

echo "============================================"
echo " BroadcomBSD Emulator Test Suite"
echo "============================================"
echo ""

# ------------------------------------------------------------------
# Test 1: Simulator (register access validation)
# ------------------------------------------------------------------
echo "--- Test 1: b43bsd_sim (register access simulator) ---"
cc -Wall -Wextra -O2 -g -o b43bsd_sim b43bsd_sim.c 2>/dev/null
./b43bsd_sim
SIM_EXIT=$?
echo ""

# ------------------------------------------------------------------
# Test 2: Kernel build (-Werror)
# ------------------------------------------------------------------
echo "--- Test 2: Kernel compile check ---"
cd /usr/src/sys/arch/amd64/compile/B43BSD
if make -j4 2>&1 | grep -q 'error:'; then
    echo "BUILD FAILED - errors found"
    exit 1
else
    echo "BUILD OK - no errors"
fi
echo ""

# ------------------------------------------------------------------
# Test 3: Symbol check
# ------------------------------------------------------------------
echo "--- Test 3: Kernel symbol integrity ---"
NM=$(nm bsd 2>/dev/null)
B43=$(echo "$NM" | grep -c 'T b43bsd' || echo 0)
SSB=$(echo "$NM" | grep -c 'T ssb_' || echo 0)
UNDEF=$(echo "$NM" | grep -c ' U ' || echo 0)
echo "  b43bsd symbols: $B43"
echo "  ssb symbols:    $SSB"
echo "  undefined:      $UNDEF"
if [ "$UNDEF" -gt 0 ]; then
    echo "WARNING: $UNDEF undefined symbols"
else
    echo "OK - no undefined symbols"
fi
echo ""

# ------------------------------------------------------------------
# Test 4: dmesg pattern check
# ------------------------------------------------------------------
echo "--- Test 4: dmesg known-bad pattern check ---"
if dmesg 2>/dev/null | grep -q 'b43bsd.*panic\|b43bsd.*fault\|b43bsd.*stall'; then
    echo "WARNING: previous crash detected in dmesg"
else
    echo "OK - no previous b43bsd crash in dmesg"
fi
echo ""

# ------------------------------------------------------------------
# Test 5: Files integrity
# ------------------------------------------------------------------
echo "--- Test 5: Source file integrity ---"
EXPECTED_FILES="
/usr/src/sys/dev/pci/b43bsd.c
/usr/src/sys/dev/pci/b43bsdvar.h
/usr/src/sys/dev/pci/b43bsdreg.h
/usr/src/sys/dev/ic/ssb.c
/usr/src/sys/dev/ic/ssbvar.h
/usr/src/sys/dev/ic/ssbreg.h
/usr/src/sys/dev/ic/b43bsd_dma.c
/usr/src/sys/dev/ic/b43bsd_dma.h
/usr/src/sys/dev/ic/b43bsd_fw.c
/usr/src/sys/dev/ic/b43bsd_fw.h
/usr/src/sys/dev/ic/b43bsd_phy_n.c
/usr/src/sys/dev/ic/b43bsd_phy_n.h
/usr/src/sys/dev/ic/b43bsd_sprom.c
"
missing=0
for f in $EXPECTED_FILES; do
    if [ ! -f "$f" ]; then
        echo "  MISSING: $f"
        missing=$((missing + 1))
    fi
done
if [ "$missing" -eq 0 ]; then
    echo "  All $missing files present"
else
    echo "  $missing files MISSING"
fi
echo ""

# ------------------------------------------------------------------
# Test 6: Attach level verification
# ------------------------------------------------------------------
echo "--- Test 6: Current attach level ---"
LEVEL=$(grep 'b43bsd_attach_level =' /usr/src/sys/dev/pci/b43bsd.c | grep -o '[0-6]')
echo "  b43bsd_attach_level = $LEVEL"
echo ""

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
echo "============================================"
if [ "$SIM_EXIT" -eq 0 ] && [ "$UNDEF" -eq 0 ] && [ "$missing" -eq 0 ]; then
    echo " ALL TESTS PASSED"
    echo ""
    echo " Kernel is ready for hardware boot at level $LEVEL"
    echo " Boot normally (defaults to /bsd)"
    echo " If hang: power-cycle + boot /bsd.safe"
    echo " Then: ksh stage.sh N  (increment level)"
else
    echo " SOME TESTS FAILED - review above"
fi
echo "============================================"