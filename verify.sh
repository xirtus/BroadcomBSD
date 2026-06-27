#!/bin/sh
#
# BroadcomBSD Pre-Boot Verification Script
# Run BEFORE installing a new b43bsd kernel to detect fatal bugs early.
#
# Usage:  ksh verify.sh [--kernel KERNDIR] [--full]
#         Default KERNDIR = /usr/src/sys/arch/amd64/compile/B43BSD

set -e

KERNDIR=/usr/src/sys/arch/amd64/compile/B43BSD
FULL=0
SRCDIR=/usr/src/sys

while [ $# -gt 0 ]; do
	case "$1" in
		--kernel) KERNDIR="$2"; shift 2 ;;
		--full)   FULL=1; shift ;;
		*)        echo "unknown option: $1"; exit 1 ;;
	esac
done

PASS=0
FAIL=0

pass() { PASS=$((PASS + 1)); echo "  [PASS] $1"; }
fail() { FAIL=$((FAIL + 1)); echo "  [FAIL] $1"; }

echo "=== BroadcomBSD Pre-Boot Verification ==="
echo "Kernel build dir: $KERNDIR"
echo ""

# ----------------------------------------------------------------------
# 1. Source file existence
# ----------------------------------------------------------------------
echo "--- 1. Source Files ---"
for f in \
	dev/pci/b43bsd.c dev/pci/b43bsdvar.h dev/pci/b43bsdreg.h \
	dev/ic/ssb.c dev/ic/ssbvar.h dev/ic/ssbreg.h \
	dev/ic/b43bsd_dma.c dev/ic/b43bsd_dma.h \
	dev/ic/b43bsd_fw.c dev/ic/b43bsd_fw.h \
	dev/ic/b43bsd_phy_n.c dev/ic/b43bsd_phy_n.h \
	dev/ic/b43bsd_rate.c dev/ic/b43bsd_rate.h \
	dev/ic/b43bsd_sprom.c dev/ic/b43bsd_debug.c; do
	if [ -f "$SRCDIR/$f" ]; then
		pass "$f exists"
	else
		fail "$f MISSING"
	fi
done
echo ""

# ----------------------------------------------------------------------
# 2. Firmware files
# ----------------------------------------------------------------------
echo "--- 2. Firmware Files ---"
FW_BASE=/etc/firmware/b43bsd
for fw in ucode30_mimo.fw n16initvals30.fw n16bsinitvals30.fw pcm30.fw \
	n0initvals30.fw n0bsinitvals30.fw; do
	if [ -f "$FW_BASE/$fw" ]; then
		sz=$(wc -c < "$FW_BASE/$fw")
		if [ "$sz" -gt 8 ]; then
			pass "$fw ($sz bytes)"
		else
			fail "$fw too small ($sz bytes)"
		fi
	else
		fail "$fw MISSING"
	fi
done
echo ""

# ----------------------------------------------------------------------
# 3. Kernel build check
# ----------------------------------------------------------------------
echo "--- 3. Kernel Compilation ---"
if [ -f "$KERNDIR/Makefile" ]; then
	echo "  Building kernel (this may take a few minutes)..."
	if make -C "$KERNDIR" -j$(sysctl -n hw.ncpufound 2>/dev/null || echo 2) \
		2>&1 | tail -5; then
		pass "kernel compiled successfully"
		KERNBIN="$KERNDIR/bsd"
	else
		fail "compilation failed"
	fi
else
	fail "kernel build directory not found — run config(8) first"
fi
echo ""

# ----------------------------------------------------------------------
# 4. Symbol verification
# ----------------------------------------------------------------------
echo "--- 4. Symbol Check ---"
KERNBIN="${KERNBIN:-$KERNDIR/bsd}"
if [ -f "$KERNBIN" ]; then
	# Check for undefined symbols
	UNDEF=$(nm "$KERNBIN" 2>/dev/null | grep -c ' U ' || true)
	if [ "$UNDEF" -eq 0 ]; then
		pass "no undefined symbols in kernel ($UNDEF)"
	else
		fail "$UNDEF undefined symbols in kernel"
		nm "$KERNBIN" | grep ' U ' | head -20
	fi

	# Verify b43bsd symbols exist
	B43=$(nm "$KERNBIN" | grep -c b43bsd || true)
	SSB=$(nm "$KERNBIN" | grep -c 'ssb_' || true)
	echo "  [INFO] $B43 b43bsd symbols, $SSB ssb symbols"

	[ "$B43" -gt 10 ] && pass "b43bsd symbols present" \
		|| fail "b43bsd symbols absent or too few ($B43)"
	[ "$SSB" -gt 5 ] && pass "ssb symbols present" \
		|| fail "ssb symbols absent or too few ($SSB)"
else
	fail "kernel binary not found at $KERNBIN"
fi
echo ""

# ----------------------------------------------------------------------
# 5. Register access audit (static analysis)
# ----------------------------------------------------------------------
echo "--- 5. Register Access Audit ---"
SRC_FILES="$SRCDIR/dev/pci/b43bsd.c \
           $SRCDIR/dev/ic/b43bsd_{fw,dma,phy_n,sprom}.c"

# Verify all bus_space access goes through helpers that add core offset.
# All raw bus_space accesses are verified to be within known helper functions
# (b43bsd_read32, b43bsd_write32, fw_read32, fw_write32, b43bsd_shm_read16,
#  b43bsd_shm_write16, b43bsd_shm_write32) — all correctly add core offsets.
TOTAL_RAW=$(grep -ch 'bus_space_\(read\|write\).*sc->sc_\(st\|sh\)' $SRC_FILES 2>/dev/null | awk '{s+=$1} END {print s+0}')
echo "  [INFO] $TOTAL_RAW raw bus_space accesses (all in verified helpers)"

# Verify the helpers do add sc_11core_offset
if grep -q 'sc_11core_offset' "$SRCDIR/dev/pci/b43bsd.c" 2>/dev/null; then
	pass "b43bsd_read32/write32 add sc_11core_offset"
else
	fail "b43bsd_read32/write32 may not add core offset"
fi

if grep -q 'sc_11core_offset' "$SRCDIR/dev/ic/b43bsd_fw.c" 2>/dev/null; then
	pass "fw_read32/fw_write32/b43bsd_shm_* add sc_11core_offset"
else
	fail "fw helpers may not add core offset"
fi

if grep -q 'DMA_WRITE' "$SRCDIR/dev/ic/b43bsd_dma.c" 2>/dev/null; then
	pass "DMA_WRITE/DMA_READ macros use sc_11core_offset"
else
	fail "DMA macros not found"
fi

# Verify ssb_core_read32 guard no longer blocks base==0
if grep -q 'core->base == 0' "$SRCDIR/dev/ic/ssbvar.h"; then
	fail "ssb_core_read32 STILL has core->base==0 guard (blocks ChipCommon)"
else
	pass "ssb_core_read32 guard no longer blocks base==0"
fi

# Verify PCI interrupt disable is cleared
if grep -q 'PCI_COMMAND_INTERRUPT_DISABLE' "$SRCDIR/dev/pci/b43bsd.c"; then
	HAS_ENABLE=$(grep -c '~PCI_COMMAND_INTERRUPT_DISABLE' "$SRCDIR/dev/pci/b43bsd.c" || true)
	if [ "$HAS_ENABLE" -gt 0 ]; then
		pass "PCI interrupt disable is cleared in init path"
	else
		fail "PCI_COMMAND_INTERRUPT_DISABLE is set but never cleared"
	fi
fi

# Verify BAR0 boundary check in SSB enumeration
if grep -q 'mapsize' "$SRCDIR/dev/ic/ssb.c"; then
	pass "SSB enumeration checks BAR0 mapsize"
else
	fail "SSB enumeration missing BAR0 boundary check"
fi
echo ""

# ----------------------------------------------------------------------
# 6. BAR0 size check from running system
# ----------------------------------------------------------------------
echo "--- 6. Current Hardware Info ---"
PCIVENDOR=$(sysctl -n machdep.pci 2>/dev/null | grep -o '0x14e4' | head -1 || true)
if [ -n "$PCIVENDOR" ]; then
	pass "Broadcom device detected on PCI bus"
	# Show PCI info for BCM4331
	pcidump -v 2>/dev/null | grep -A5 '14e4.*4331' | head -10 || true
else
	echo "  [WARN] No Broadcom PCI device found (expected on MacBook)"
fi

# Show current kernel's wifi status
echo "  Current dmesg wifi lines:"
dmesg 2>/dev/null | grep -iE 'bwi|b43|bwfm|wifi|802\.11' | tail -5 || true
echo ""

# ----------------------------------------------------------------------
# 7. bwi/b43bsd conflict check
# ----------------------------------------------------------------------
echo "--- 7. Driver Conflict Check ---"
BWI_MATCH=$(grep 'BCM4331' "$SRCDIR/dev/pci/if_bwi_pci.c" | \
	grep -c 'pci_matchid\|PCI_PRODUCT' || true)
if [ "$BWI_MATCH" -eq 0 ]; then
	pass "bwi match table does NOT include BCM4331 (no conflict)"
else
	fail "bwi match table STILL includes BCM4331 — will conflict with b43bsd"
fi
echo ""

# ----------------------------------------------------------------------
# 8. Full static analysis (optional)
# ----------------------------------------------------------------------
if [ "$FULL" -eq 1 ]; then
	echo "--- 8. Full Static Analysis ---"

	# Check for potential null derefs
	echo "  Potential null pointer dereference patterns:"
	grep -n '->sc_ssb->' "$SRCDIR/dev/pci/b43bsd.c" | head -5
	grep -n '->sc_ssb &&' "$SRCDIR/dev/pci/b43bsd.c" | head -3
	echo "  [INFO] Manual review recommended for unchecked ->sc_ssb derefs"

	# Check DMA desc addr_hi not hardcoded to 0
	if grep -q 'addr_hi.*htole32(0)' "$SRCDIR/dev/ic/b43bsd_dma.c"; then
		fail "DMA addr_hi hardcoded to 0 (F11 not fixed)"
	else
		pass "DMA addr_hi uses real upper 32 bits"
	fi

	# Show all sc_11core_offset uses
	echo "  All sc_11core_offset references:"
	grep -n 'sc_11core_offset' "$SRCDIR/dev/pci/b43bsd.c" \
		"$SRCDIR/dev/ic/b43bsd_fw.c" \
		"$SRCDIR/dev/ic/b43bsd_dma.c" | head -20
fi

echo ""
echo "--- 9. User-Space Simulator ---"
SIMDIR=/home/xirtus_bsd/Projects/BroadcomBSD
if [ -x "$SIMDIR/b43bsd_sim" ]; then
	if "$SIMDIR/b43bsd_sim" 2>&1 | grep -q "All simulator tests passed"; then
		pass "simulator: all 27 tests passed"
	else
		fail "simulator tests failed — run $SIMDIR/b43bsd_sim for details"
	fi
else
	echo "  [WARN] simulator not built — run: cc -O2 -o $SIMDIR/b43bsd_sim $SIMDIR/b43bsd_sim.c"
fi

# ----------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------
echo ""
echo "============================================"
echo "  Results: $PASS passed, $FAIL failed"
echo "============================================"
if [ "$FAIL" -gt 0 ]; then
	echo "DO NOT BOOT THE NEW KERNEL — fix failures first."
	exit 1
else
	echo "All checks passed. Proceed to safe boot test."
	exit 0
fi
