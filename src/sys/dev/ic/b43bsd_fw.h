/*	$OpenBSD: b43bsd_fw.h,v 1.1 2026/06/24 xirtus Exp $	*/

/*
 * Copyright (c) 2026 BroadcomBSD Project
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _DEV_IC_B43BSD_FW_H_
#define _DEV_IC_B43BSD_FW_H_

/*
 * b43 firmware file format definitions.
 * Ported from Linux drivers/net/wireless/broadcom/b43/b43.h (GPLv2).
 */

/* Firmware file type tags. */
#define B43BSD_FW_TYPE_UCODE	'u'
#define B43BSD_FW_TYPE_PCM	'p'
#define B43BSD_FW_TYPE_IV	'i'

/* Firmware header (8 bytes, big-endian). */
struct b43bsd_fw_header {
	uint8_t		type;
	uint8_t		ver;
	uint8_t		_padding[2];
	uint32_t	size;		/* big-endian: byte count or IV entry count */
} __packed;

/* Initvals entry. */
#define B43BSD_IV_OFFSET_MASK	0x7FFF
#define B43BSD_IV_32BIT		0x8000

struct b43bsd_iv {
	uint16_t	offset_size;	/* big-endian */
	union {
		uint16_t	d16;	/* big-endian */
		uint32_t	d32;	/* big-endian */
	} data;
} __packed;

/* SHM routing constants. */
#define B43BSD_MMIO_SHM_CONTROL		0x0160
#define B43BSD_MMIO_SHM_DATA		0x0164
#define B43BSD_MMIO_SHM_DATA_UNALIGNED	0x0166

#define B43BSD_SHM_UCODE		0x0000
#define B43BSD_SHM_SHARED		0x0001
#define B43BSD_SHM_SCRATCH		0x0002
#define B43BSD_SHM_HW			0x0003

#define B43BSD_SHM_AUTOINC_W		0x0100
#define B43BSD_SHM_AUTOINC_R		0x0200

/* SHM_SHARED offsets for firmware info. */
#define B43BSD_SHM_UCODEREV		0x0000
#define B43BSD_SHM_UCODEPATCH		0x0002
#define B43BSD_SHM_UCODEDATE		0x0004
#define B43BSD_SHM_UCODETIME		0x0006
#define B43BSD_SHM_UCODESTAT		0x0040
#define B43BSD_SHM_FWCAPA		0x0042
#define B43BSD_SHM_PHYVER		0x0050
#define B43BSD_SHM_PHYTYPE		0x0052

/* SHM firmware command interface. */
#define B43BSD_SHM_CMD			0x0030
#define B43BSD_SHM_CMD_STAT		0x0032
#define B43BSD_SHM_CMD_DATA0		0x0034
#define B43BSD_SHM_CMD_DATA1		0x0036
#define B43BSD_SHM_CMD_DATA2		0x0038
#define B43BSD_SHM_CMD_DATA3		0x003A

/* Firmware command IDs. */
#define B43BSD_FWCMD_SET_CHANNEL	0x0001
#define B43BSD_FWCMD_SET_ANTENNA	0x0002
#define B43BSD_FWCMD_SET_BSSID		0x0005
#define B43BSD_FWCMD_SET_BEACON_INT	0x0006
#define B43BSD_FWCMD_SET_PS		0x0008
#define B43BSD_FWCMD_SET_TSF		0x000A
#define B43BSD_FWCMD_BEACON_ENABLE	0x000D
#define B43BSD_FWCMD_WAKE		0x0010

/* Ucode status values. */
#define B43BSD_UCODESTAT_INVALID	0
#define B43BSD_UCODESTAT_INIT		1
#define B43BSD_UCODESTAT_ACTIVE		2
#define B43BSD_UCODESTAT_SUSPEND	3
#define B43BSD_UCODESTAT_SLEEP		4

/* MACCTL bits for firmware startup. */
#define B43BSD_MACCTL_PSM_RUN		0x00000002
#define B43BSD_MACCTL_PSM_JMP0		0x00000004

/* Firmware header format versions (based on ucode revision). */
#define B43BSD_FW_HDR_351	351
#define B43BSD_FW_HDR_410	410
#define B43BSD_FW_HDR_598	598

/* Firmware filenames for BCM4331 (core rev 30, PHY-N). */
#define B43BSD_FW_UCODE		"ucode30_mimo"
#define B43BSD_FW_INITVALS	"n0initvals30"
#define B43BSD_FW_INITVALS_BAND	"n0bsinitvals30"
#define B43BSD_FW_PCM		"pcm30"

/* Firmware directory path. */
#define B43BSD_FWPATH		"/etc/firmware/b43bsd"

/* ------------------------------------------------------------------ */
/* Firmware API                                                        */
/* ------------------------------------------------------------------ */

struct b43bsd_softc;

int	b43bsd_upload_microcode(struct b43bsd_softc *);
int	b43bsd_write_initvals(struct b43bsd_softc *, const u_char *, size_t);
int	b43bsd_upload_initvals(struct b43bsd_softc *);
int	b43bsd_fw_init(struct b43bsd_softc *);
void	b43bsd_fw_free(struct b43bsd_softc *);

void	b43bsd_shm_write16(struct b43bsd_softc *, int, uint16_t, uint16_t);
void	b43bsd_shm_write32(struct b43bsd_softc *, int, uint16_t, uint32_t);
uint16_t b43bsd_shm_read16(struct b43bsd_softc *, int, uint16_t);

void	b43bsd_fw_set_channel(struct b43bsd_softc *, int);
void	b43bsd_fw_set_bssid(struct b43bsd_softc *);
void	b43bsd_fw_set_beacon_interval(struct b43bsd_softc *, int);
void	b43bsd_fw_set_ps(struct b43bsd_softc *, int);
void	b43bsd_fw_set_tsf(struct b43bsd_softc *, uint64_t);
void	b43bsd_fw_beacon_enable(struct b43bsd_softc *, int);
void	b43bsd_fw_wake(struct b43bsd_softc *);

#endif /* _DEV_IC_B43BSD_FW_H_ */
