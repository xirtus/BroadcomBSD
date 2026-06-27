# $OpenBSD: Makefile,v 1.2 2026/06/25 xirtus Exp $

COMMENT =		firmware for Broadcom BCM4331 WiFi (b43bsd driver)

V =			784.2
DISTNAME =		b43bsd-firmware-${V}
CATEGORIES =		sysutils

HOMEPAGE =		https://wireless.docs.kernel.org/en/latest/en/users/drivers/b43/firmware.html

# Broadcom firmware license allows redistribution (same as linux-firmware.git).
# Firmware must be extracted from Broadcom binary driver using b43-fwcutter.
PERMIT_PACKAGE =	Yes

# The b43 firmware is NOT distributed in linux-firmware.git.
# It must be extracted from the Broadcom proprietary driver package
# using b43-fwcutter (sysutils/b43-fwcutter).
#
# For BCM4331 (core rev 30, PHY-N), the driver needs:
#   ucode30_mimo.fw
#   n0initvals30.fw
#   n0bsinitvals30.fw
#   pcm30.fw
#
# These are extracted via:
#   # pkg_add b43-fwcutter
#   # b43-fwcutter -w /etc/firmware/b43bsd broadcom-wl-5.100.138.tar.bz2
#
# Source: https://wireless.docs.kernel.org/en/latest/en/users/drivers/b43.html

NO_BUILD =		Yes
NO_TEST =		Yes

PKG_ARCH =		*

FIRMWARE_DIR =		/etc/firmware/b43bsd

# Require b43-fwcutter as a build dependency for extraction.
# Users must supply the Broadcom binary driver archive.
FETCH_MANUALLY =	"The b43 firmware must be extracted from the Broadcom"
FETCH_MANUALLY +=	"binary driver using b43-fwcutter. See pkg/DESCR for"
FETCH_MANUALLY +=	"instructions."

do-install:
	${INSTALL_DATA_DIR} ${PREFIX}/${FIRMWARE_DIR}
	@echo "Firmware files must be extracted using b43-fwcutter."
	@echo "See pkg/DESCR for detailed instructions."
	@echo "Expected files in ${PREFIX}/${FIRMWARE_DIR}:"
	@echo "  ucode30_mimo.fw, n0initvals30.fw, n0bsinitvals30.fw, pcm30.fw"

.include <bsd.port.mk>
