#!/bin/sh
# BroadcomBSD — Comprehensive 9-pass verification scan
# Run: ksh /home/xirtus_bsd/Projects/BroadcomBSD/scan.sh
set -e

SRCDIR=/usr/src/sys
KERNDIR=/usr/src/sys/arch/amd64/compile/B43BSD
FWDIR=/etc/firmware/b43bsd
PROJDIR=/home/xirtus_bsd/Projects/BroadcomBSD

fail() { echo "  [FAIL] pass $1: $2"; exit 1; }

for pass in 1 2 3 4 5 6 7 8 9; do
	echo "=== Pass $pass/9 ==="

	# 1. Source file existence
	for f in \
	    dev/pci/b43bsd.c dev/pci/b43bsdvar.h dev/pci/b43bsdreg.h \
	    dev/ic/ssb.c dev/ic/ssbvar.h dev/ic/ssbreg.h \
	    dev/ic/b43bsd_dma.c dev/ic/b43bsd_dma.h \
	    dev/ic/b43bsd_fw.c dev/ic/b43bsd_fw.h \
	    dev/ic/b43bsd_phy_n.c dev/ic/b43bsd_phy_n.h \
	    dev/ic/b43bsd_rate.c dev/ic/b43bsd_rate.h \
	    dev/ic/b43bsd_sprom.c dev/ic/b43bsd_debug.c; do
		[ -f "$SRCDIR/$f" ] || fail "$pass" "$f missing"
	done

	# 2. Firmware
	for fw in ucode30_mimo.fw n16initvals30.fw n16bsinitvals30.fw pcm30.fw; do
		[ -f "$FWDIR/$fw" ] || fail "$pass" "firmware $fw missing"
		[ "$(wc -c < "$FWDIR/$fw")" -gt 8 ] || fail "$pass" "firmware $fw too small"
	done

	# 3. Kernel binary exists and has b43bsd symbols
	[ -f "$KERNDIR/bsd" ] || fail "$pass" "kernel binary missing"
	n=$(nm "$KERNDIR/bsd" 2>/dev/null | grep -c ' T b43bsd' || true)
	[ "$n" -gt 10 ] || fail "$pass" "too few b43bsd symbols: $n"
	n=$(nm "$KERNDIR/bsd" 2>/dev/null | grep -c ' T ssb_' || true)
	[ "$n" -gt 3 ] || fail "$pass" "too few ssb symbols: $n"

	# 4. Core bugs FIXED in source
	grep -q "mapsize" "$SRCDIR/dev/ic/ssb.c" \
	    || fail "$pass" "ssb.c missing mapsize boundary check"
	grep -q "mapsize" "$SRCDIR/dev/ic/ssbvar.h" \
	    || fail "$pass" "ssbvar.h missing mapsize field"
	! grep -q "core->base == 0" "$SRCDIR/dev/ic/ssbvar.h" \
	    || fail "$pass" "ssbvar.h still has base==0 guard"
	D=$(grep -c "~PCI_COMMAND_INTERRUPT_DISABLE" "$SRCDIR/dev/pci/b43bsd.c" || true)
	[ "$D" -gt 0 ] || fail "$pass" "PCI interrupt disable never cleared"
	grep -q "0x000f0000" "$SRCDIR/dev/pci/b43bsdvar.h" \
	    || fail "$pass" "CHIPID mask not fixed"

	# 5. PCI interrupt ordering (clear AFTER device irq mask programmed)
	# Extract the init function and verify PCI clear comes after IRQ_MASK write
	awk '/^b43bsd_init/,/^}/' "$SRCDIR/dev/pci/b43bsd.c" > /tmp/b43bsd_init.$$.c
	pci_line=$(grep -n "PCI_COMMAND_INTERRUPT_DISABLE" /tmp/b43bsd_init.$$.c | grep "~" | head -1 | cut -d: -f1)
	mask_line=$(grep -n "B43_IRQ_MASK_NORMAL" /tmp/b43bsd_init.$$.c | head -1 | cut -d: -f1)
	[ -n "$pci_line" ] || fail "$pass" "PCI int clear not in init function"
	[ -n "$mask_line" ] || fail "$pass" "IRQ_MASK not in init function"
	[ "$pci_line" -gt "$mask_line" ] \
	    || fail "$pass" "PCI int clear ($pci_line) before IRQ_MASK ($mask_line)"
	rm -f /tmp/b43bsd_init.$$.c

	# 6. 802.11 core guard in init
	grep -q "ieee80211_idx < 0" "$SRCDIR/dev/pci/b43bsd.c" \
	    || fail "$pass" "no 802.11 core guard in init"

	# 7. Kernel binary matches source (recompilable without changes)
	md5_before=$(md5 "$KERNDIR/bsd" 2>/dev/null | awk '{print $NF}')
	cd "$KERNDIR" && make -j4 >/dev/null 2>&1
	md5_after=$(md5 "$KERNDIR/bsd" 2>/dev/null | awk '{print $NF}')
	[ "$md5_before" = "$md5_after" ] || fail "$pass" "kernel not idempotent (rebuild changed binary)"

	# 8. Installed kernel matches build dir
	[ "$(md5 /bsd 2>/dev/null | awk '{print $NF}')" = "$md5_before" ] \
	    || fail "$pass" "/bsd doesn't match build bsd"

	# 9. Fallback kernels exist
	[ -f /obsd ] || fail "$pass" "/obsd missing"
	[ -f /safebsd ] || fail "$pass" "/safebsd missing"

	# 10. Undefined symbols check
	u=$(nm "$KERNDIR/bsd" 2>/dev/null | grep -c ' U ' || true)
	[ "$u" -eq 0 ] || fail "$pass" "$u undefined symbols"

	echo "  PASS $pass"
done

echo ""
echo "=== All 9 passes passed ==="
echo "/bsd: $(md5 /bsd)"
echo "/obsd: $(md5 /obsd)"
echo "/safebsd: $(md5 /safebsd)"
