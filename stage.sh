#!/bin/sh
# BroadcomBSD Stage Manager
# Usage: ksh stage.sh <level>
#   0 = print PCI IDs only, return (no hardware touch)
#   1 = + BAR0 mapping + size print
#   2 = + chip ID read from MMIO
#   3 = + SSB bus enumeration (core discovery)
#   4 = + PMU init + core enable + 802.11 offset calculation
#   5 = + PHY rev + SPROM parse + flags + net80211 (no interrupts)
#   6 = + interrupt + chip reset + LED (full attach)
#
# After changing stage: reboot to test.
# If it hangs: power-cycle, boot /bsd.safe, then try a lower level.

set -e

LEVEL="${1:?Usage: ksh stage.sh <0-6>}"
SRC=/usr/src/sys/dev/pci/b43bsd.c
BLD=/usr/src/sys/arch/amd64/compile/B43BSD

case "$LEVEL" in
	0|1|2|3|4|5|6) ;;
	*) echo "Invalid level: $LEVEL (must be 0-6)"; exit 1 ;;
esac

echo "Setting b43bsd_attach_level to $LEVEL..."
sed -i "s/b43bsd_attach_level = [0-6]/b43bsd_attach_level = $LEVEL/" "$SRC"
grep 'b43bsd_attach_level =' "$SRC"

echo "Building kernel..."
make -C "$BLD" -j4

echo "Installing to /bsd..."
cp "$BLD/bsd" /bsd

echo ""
echo "=== Done. reboot to test level $LEVEL ==="
case "$LEVEL" in
	0) echo "Expected: 'b43bsd0 at pci...: BCM4331 rev 2 (attach level 0) stub-ok'" ;;
	1) echo "Expected: '... BAR0 0x4000 level1-ok'" ;;
	2) echo "Expected: '... BAR0 0xXXXX win=0xXXXXXXXX: BCM4331 rev 2 level2-ok'" ;;
	3) echo "Expected: '... SSB cores enumerated, level3-ok'" ;;
	4) echo "Expected: '... PMU init + cores enabled, level4-ok'" ;;
	5) echo "Expected: '... SPROM + MAC + net80211, level5-ok'" ;;
	6) echo "Expected: '... full attach, level6-ok'" ;;
esac
echo "If it hangs: boot /bsd.safe, then run: ksh stage.sh <lower-level>"
