/*	$OpenBSD: b43bsd_fw.c,v 1.1 2026/06/24 xirtus Exp $	*/

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

/*
 * b43 firmware loader for BCM4331.
 *
 * Ported from Linux drivers/net/wireless/broadcom/b43/main.c (GPLv2).
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <machine/bus.h>

#include <dev/pci/b43bsdvar.h>
#include <dev/pci/b43bsdreg.h>
#include <dev/ic/b43bsd_fw.h>

/* ------------------------------------------------------------------ */
/* Internal Helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * 802.11 core register access — must add the core's BAR0 offset.
 */
static inline uint32_t
fw_read32(struct b43bsd_softc *sc, uint16_t offset)
{
	return bus_space_read_4(sc->sc_st, sc->sc_sh,
	    sc->sc_11core_offset + offset);
}

static inline void
fw_write32(struct b43bsd_softc *sc, uint16_t offset, uint32_t val)
{
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    sc->sc_11core_offset + offset, val);
}

static inline uint32_t
be32_to_cpu(uint32_t x)
{
	return betoh32(x);
}

static inline uint16_t
be16_to_cpu(uint16_t x)
{
	return betoh16(x);
}

/*
 * Write a value to shared memory.
 */
void
b43bsd_shm_write16(struct b43bsd_softc *sc, int routing, uint16_t offset,
    uint16_t value)
{
	uint32_t ctl;
	uint32_t base = sc->sc_11core_offset;

	ctl = (routing << 16) | (offset & 0xffff);
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_CONTROL, ctl);
	bus_space_write_2(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_DATA, value);
}

/*
 * Write a 32-bit value to shared memory via auto-increment.
 */
void
b43bsd_shm_write32(struct b43bsd_softc *sc, int routing, uint16_t offset,
    uint32_t value)
{
	uint32_t ctl;
	uint32_t base = sc->sc_11core_offset;

	ctl = (routing << 16) | (offset & 0xffff);
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_CONTROL, ctl);
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_DATA, value);
}

/*
 * Read a 16-bit value from shared memory.
 */
uint16_t
b43bsd_shm_read16(struct b43bsd_softc *sc, int routing, uint16_t offset)
{
	uint32_t ctl;
	uint32_t base = sc->sc_11core_offset;

	ctl = (routing << 16) | (offset & 0xffff);
	bus_space_write_4(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_CONTROL, ctl);
	return bus_space_read_2(sc->sc_st, sc->sc_sh,
	    base + B43BSD_MMIO_SHM_DATA);
}

/* ------------------------------------------------------------------ */
/* Firmware File Loading                                                */
/* ------------------------------------------------------------------ */

/*
 * Load a firmware file from disk into a kernel buffer.
 * If cached is non-NULL and already loaded, use the cached copy.
 * Otherwise, load from disk and optionally cache.
 * Returns 0 on success, or an errno on failure.
 */
static int
b43bsd_load_fw_file(struct b43bsd_softc *sc, const char *name,
    u_char **cached, size_t *cached_sz, u_char **bufp, size_t *sizep)
{
	u_char *buf;
	size_t size;
	int error;
	char path[128];

	/* Use cached copy if available. */
	if (cached != NULL && *cached != NULL && *cached_sz > 0) {
		*bufp = *cached;
		*sizep = *cached_sz;
		return 0;
	}

	snprintf(path, sizeof(path), "%s/%s.fw",
	    B43BSD_FWPATH, name);

	B43BSD_DPRINTF(sc, B43BSD_DBG_FW,
	    "loading firmware %s\n", path);

	error = loadfirmware(path, &buf, &size);
	if (error != 0) {
		printf("%s: could not read firmware %s (error %d)\n",
		    sc->sc_dev.dv_xname, path, error);
		return error;
	}

	if (size < sizeof(struct b43bsd_fw_header)) {
		printf("%s: firmware %s too small (%zu bytes)\n",
		    sc->sc_dev.dv_xname, path, size);
		free(buf, M_DEVBUF, size);
		return EINVAL;
	}

	/* Cache the firmware in softc if a cache pointer was provided. */
	if (cached != NULL) {
		*cached = buf;
		*cached_sz = size;
	}

	*bufp = buf;
	*sizep = size;
	return 0;
}

/*
 * Validate a firmware file header.
 */
static int
b43bsd_validate_fw(struct b43bsd_softc *sc, const u_char *buf,
    size_t size, uint8_t expected_type)
{
	const struct b43bsd_fw_header *hdr =
	    (const struct b43bsd_fw_header *)buf;

	if (hdr->type != expected_type) {
		printf("%s: firmware type mismatch (got '%c', "
		    "expected '%c')\n",
		    sc->sc_dev.dv_xname, hdr->type, expected_type);
		return EINVAL;
	}

	if (hdr->ver != 1) {
		printf("%s: firmware version %d != 1\n",
		    sc->sc_dev.dv_xname, hdr->ver);
		return EINVAL;
	}

	return 0;
}

/* ------------------------------------------------------------------ */
/* Microcode Upload                                                     */
/* ------------------------------------------------------------------ */

/*
 * Upload microcode to the chip and start it.
 */
int
b43bsd_upload_microcode(struct b43bsd_softc *sc)
{
	u_char *buf;
	size_t size;
	const struct b43bsd_fw_header *hdr;
	const uint32_t *ucode;
	uint32_t ucode_size;
	uint16_t fwrev, fwpatch;
	uint32_t macctl;
	int i, error;

	/* Load microcode file. */
	error = b43bsd_load_fw_file(sc, B43BSD_FW_UCODE,
	    &sc->sc_fw.ucode, &sc->sc_fw.ucode_size, &buf, &size);
	if (error != 0)
		return error;

	error = b43bsd_validate_fw(sc, buf, size, B43BSD_FW_TYPE_UCODE);
	if (error != 0) {
		free(buf, M_DEVBUF, size);
		return error;
	}

	hdr = (const struct b43bsd_fw_header *)buf;
	ucode_size = be32_to_cpu(hdr->size);
	ucode = (const uint32_t *)(buf + sizeof(*hdr));

	if (ucode_size > (size - sizeof(*hdr))) {
		printf("%s: firmware size mismatch\n",
		    sc->sc_dev.dv_xname);
		free(buf, M_DEVBUF, size);
		return EINVAL;
	}

	ucode_size /= 4;

	B43BSD_DPRINTF(sc, B43BSD_DBG_FW,
	    "uploading %u words of microcode\n", ucode_size);

	/*
	 * 1. Jump microcode PSM to offset 0.
	 */
	macctl = fw_read32(sc, B43_MMIO_MACCTL);
	macctl |= B43BSD_MACCTL_PSM_JMP0;
	macctl &= ~B43BSD_MACCTL_PSM_RUN;
	fw_write32(sc, B43_MMIO_MACCTL, macctl);

	/*
	 * 2. Zero scratch memory (64 words).
	 */
	for (i = 0; i < 64; i++)
		b43bsd_shm_write16(sc, B43BSD_SHM_SCRATCH, (uint16_t)i, 0);

	/*
	 * 3. Zero shared memory (first 4096 bytes).
	 */
	for (i = 0; i < 4096; i += 2)
		b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, (uint16_t)i, 0);

	/*
	 * 4. Upload microcode via auto-increment writes.
	 */
	fw_write32(sc, B43BSD_MMIO_SHM_CONTROL,
	    (B43BSD_SHM_UCODE | B43BSD_SHM_AUTOINC_W) << 16);

	for (i = 0; i < ucode_size; i++) {
		bus_space_write_4(sc->sc_st, sc->sc_sh,
		    sc->sc_11core_offset + B43BSD_MMIO_SHM_DATA,
		    be32_to_cpu(ucode[i]));
		delay(10);
	}

	/*
	 * 5. Start microcode.
	 */
	macctl = fw_read32(sc, B43_MMIO_MACCTL);
	macctl &= ~B43BSD_MACCTL_PSM_JMP0;
	macctl |= B43BSD_MACCTL_PSM_RUN;
	fw_write32(sc, B43_MMIO_MACCTL, macctl);

	/*
	 * 6. Wait for firmware ready (IRQ_READY signal, timeout ~500ms).
	 */
	for (i = 0; i < 500; i++) {
		uint32_t reason;

		reason = fw_read32(sc, B43_MMIO_GEN_IRQ_REASON);
		if (reason & B43_IRQ_READY) {
			/* Acknowledge the READY interrupt. */
			fw_write32(sc, B43_MMIO_GEN_IRQ_REASON,
			    B43_IRQ_READY);
			break;
		}
		delay(1000); /* 1 ms */
	}

	if (i >= 500) {
		printf("%s: firmware start timeout\n",
		    sc->sc_dev.dv_xname);
		return ETIMEDOUT;
	}

	/*
	 * 7. Read firmware revision.
	 */
	fwrev = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
	    B43BSD_SHM_UCODEREV);
	fwpatch = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
	    B43BSD_SHM_UCODEPATCH);

	printf("%s: firmware rev %d patch %d loaded\n",
	    sc->sc_dev.dv_xname, fwrev, fwpatch);

	sc->sc_fw.rev = fwrev;
	sc->sc_fw.patch = fwpatch;
	sc->sc_fw.running = 1;
	sc->sc_flags |= B43BSD_FLAG_FW_LOADED;

	return 0;
}

/* ------------------------------------------------------------------ */
/* Initvals Upload                                                      */
/* ------------------------------------------------------------------ */

/*
 * Write a set of initvals to the device registers.
 * Each initval is a {offset, value[, mask]} tuple that writes to MMIO.
 */
int
b43bsd_write_initvals(struct b43bsd_softc *sc, const u_char *data,
    size_t data_size)
{
	const u_char *end = data + data_size;
	int count = 0;

	B43BSD_DPRINTF(sc, B43BSD_DBG_FW,
	    "writing %zu bytes of initvals\n", data_size);

	while (data < end) {
		const struct b43bsd_iv *iv;
		uint16_t offset;
		int bit32;

		if (data + 2 > end) {
			printf("%s: truncated initval\n",
			    sc->sc_dev.dv_xname);
			return EINVAL;
		}

		iv = (const struct b43bsd_iv *)data;
		offset = be16_to_cpu(iv->offset_size);
		bit32 = !!(offset & B43BSD_IV_32BIT);
		offset &= B43BSD_IV_OFFSET_MASK;

		/* Validate offset against MMIO window size. */
		if (offset >= 0x1000 || offset >= sc->sc_sz) {
			printf("%s: invalid initval offset 0x%04x "
			    "(max 0x%lx)\n",
			    sc->sc_dev.dv_xname, offset,
			    (unsigned long)sc->sc_sz);
			return EINVAL;
		}

		if (bit32) {
			uint32_t val;

			if (data + 6 > end) {
				printf("%s: truncated 32-bit initval\n",
				    sc->sc_dev.dv_xname);
				return EINVAL;
			}

			val = be32_to_cpu(iv->data.d32);
			fw_write32(sc, offset, val);
			data += 6;
		} else {
			uint16_t val16;
			uint32_t val32, aligned;

			if (data + 4 > end) {
				printf("%s: truncated 16-bit initval\n",
				    sc->sc_dev.dv_xname);
				return EINVAL;
			}

			val16 = be16_to_cpu(iv->data.d16);

			/*
			 * SSB devices don't support 16-bit MMIO writes.
			 * Use read-modify-write on the aligned 32-bit word.
			 */
			aligned = offset & ~3;
			val32 = fw_read32(sc, (uint16_t)aligned);
			switch (offset & 3) {
			case 0:
				val32 = (val32 & 0xffff0000) | val16;
				break;
			case 2:
				val32 = (val32 & 0x0000ffff) |
				    ((uint32_t)val16 << 16);
				break;
			}
			fw_write32(sc, (uint16_t)aligned, val32);
			data += 4;
		}
		count++;
	}

	printf("%s: %d initvals written\n", sc->sc_dev.dv_xname, count);
	return 0;
}

/*
 * Upload initvals from a firmware file.
 */
static int
b43bsd_upload_initvals_file(struct b43bsd_softc *sc, const char *name,
    u_char **cached, size_t *cached_sz)
{
	u_char *buf;
	size_t size;
	const struct b43bsd_fw_header *hdr;
	int error;

	error = b43bsd_load_fw_file(sc, name, cached, cached_sz,
	    &buf, &size);
	if (error != 0)
		return error;

	error = b43bsd_validate_fw(sc, buf, size, B43BSD_FW_TYPE_IV);
	if (error != 0 && (cached == NULL || *cached == NULL)) {
		free(buf, M_DEVBUF, size);
		return error;
	}
	if (error != 0)
		return error;

	hdr = (const struct b43bsd_fw_header *)buf;

	error = b43bsd_write_initvals(sc,
	    buf + sizeof(*hdr), size - sizeof(*hdr));

	return error;
}

/*
 * Upload all initvals (base + band-specific).
 * Constructs filename from PHY revision, with fallbacks.
 */
int
b43bsd_upload_initvals(struct b43bsd_softc *sc)
{
	char fw_name[32];
	int error, phy_rev, try;

	phy_rev = sc->sc_radio.phy_rev;

	/* Try PHY-specific rev, then 16, then 0 as fallbacks. */
	for (try = 0; try < 3; try++) {
		int r;

		switch (try) {
		case 0: r = phy_rev; break;
		case 1: r = 16; break;
		default: r = 0; break;
		}

		/* Build initvals filename: n{rev}initvals30 */
		snprintf(fw_name, sizeof(fw_name), "n%dinitvals30", r);

		error = b43bsd_upload_initvals_file(sc, fw_name,
		    &sc->sc_fw.initvals, &sc->sc_fw.initvals_size);
		if (error == 0) {
			/* Build band-switch filename. */
			snprintf(fw_name, sizeof(fw_name),
			    "n%dbsinitvals30", r);

			error = b43bsd_upload_initvals_file(sc, fw_name,
			    &sc->sc_fw.initvals_band,
			    &sc->sc_fw.initvals_band_size);
			if (error == 0) {
				printf("%s: initvals loaded (rev %d)\n",
				    sc->sc_dev.dv_xname, r);
				return 0;
			}
		}
		if (try == 0 && phy_rev == 16) continue; /* skip duplicate */
		if (try == 1 && phy_rev == 0) continue;  /* skip duplicate */
	}

	printf("%s: failed to upload initvals\n", sc->sc_dev.dv_xname);
	return ENOENT;
}

/* ------------------------------------------------------------------ */
/* Full Firmware Initialization                                         */
/* ------------------------------------------------------------------ */

/*
 * Complete firmware initialization: upload ucode, initvals,
 * and prepare the chip for operation.
 */
int
b43bsd_fw_init(struct b43bsd_softc *sc)
{
	int error;

	/* Upload and start microcode. */
	error = b43bsd_upload_microcode(sc);
	if (error != 0)
		return error;

	/* Upload initvals. */
	error = b43bsd_upload_initvals(sc);
	if (error != 0)
		return error;

	/* Upload PCM firmware (TX pulse-code modulation samples). */
	error = b43bsd_load_fw_file(sc, B43BSD_FW_PCM,
	    &sc->sc_fw.pcm, &sc->sc_fw.pcm_size,
	    &sc->sc_fw.pcm, &sc->sc_fw.pcm_size);
	if (error != 0)
		printf("%s: PCM firmware not loaded (non-fatal)\n",
		    sc->sc_dev.dv_xname);

	sc->sc_fw.fw_cached = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Firmware Cache Cleanup                                               */
/* ------------------------------------------------------------------ */

void
b43bsd_fw_free(struct b43bsd_softc *sc)
{
	if (sc->sc_fw.ucode != NULL) {
		free(sc->sc_fw.ucode, M_DEVBUF, sc->sc_fw.ucode_size);
		sc->sc_fw.ucode = NULL;
		sc->sc_fw.ucode_size = 0;
	}
	if (sc->sc_fw.initvals != NULL) {
		free(sc->sc_fw.initvals, M_DEVBUF, sc->sc_fw.initvals_size);
		sc->sc_fw.initvals = NULL;
		sc->sc_fw.initvals_size = 0;
	}
	if (sc->sc_fw.initvals_band != NULL) {
		free(sc->sc_fw.initvals_band, M_DEVBUF,
		    sc->sc_fw.initvals_band_size);
		sc->sc_fw.initvals_band = NULL;
		sc->sc_fw.initvals_band_size = 0;
	}
	if (sc->sc_fw.pcm != NULL) {
		free(sc->sc_fw.pcm, M_DEVBUF, sc->sc_fw.pcm_size);
		sc->sc_fw.pcm = NULL;
		sc->sc_fw.pcm_size = 0;
	}
	sc->sc_fw.fw_cached = 0;
}

/*
 * Firmware SHM command interface.
 * The driver communicates with the microcode (running on the
 * ARM Cortex-M3) through shared memory.  Commands are written
 * to SHM_SHARED, the firmware processes them asynchronously,
 * and the driver polls a status word for completion.
 *
 * SHM offsets match Linux b43: CMD=0x0030, STAT=0x0032,
 * ARG0=0x0034, ARG1=0x0036, ARG2=0x0038, ARG3=0x003A.
 */

/*
 * Send a command to the firmware and wait for completion.
 * Returns 0 on success, ETIMEDOUT if firmware doesn't respond.
 */
static int
b43bsd_fw_cmd(struct b43bsd_softc *sc, uint16_t cmd, uint16_t arg0,
    uint16_t arg1)
{
	int i;

	if (!sc->sc_fw.running)
		return ENXIO;

	/* Write arguments. */
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD_DATA0, arg0);
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD_DATA1, arg1);

	/* Clear status, then write command (order matters). */
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD_STAT, 0);
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD, cmd);

	/* Poll for completion (50ms timeout). */
	for (i = 0; i < 50; i++) {
		uint16_t stat;

		stat = b43bsd_shm_read16(sc, B43BSD_SHM_SHARED,
		    B43BSD_SHM_CMD_STAT);
		if (stat != 0)
			return 0;
		delay(1000);
	}

	printf("%s: firmware command 0x%04x timeout\n",
	    sc->sc_dev.dv_xname, cmd);
	return ETIMEDOUT;
}

/*
 * Notify firmware of channel change.
 * Called after PHY is configured for the new channel.
 */
void
b43bsd_fw_set_channel(struct b43bsd_softc *sc, int channel)
{
	b43bsd_fw_cmd(sc, 0x0001, (uint16_t)channel, 0);
}

/*
 * Notify firmware of BSSID change.
 * Called after association sets the BSSID filter.
 */
void
b43bsd_fw_set_bssid(struct b43bsd_softc *sc)
{
	b43bsd_fw_cmd(sc, 0x0005, 0, 0);
}

/*
 * Set beacon interval in firmware.
 */
void
b43bsd_fw_set_beacon_interval(struct b43bsd_softc *sc, int interval)
{
	b43bsd_fw_cmd(sc, B43BSD_FWCMD_SET_BEACON_INT,
	    (uint16_t)(interval & 0xffff),
	    (uint16_t)((interval >> 16) & 0xffff));
}

/*
 * Configure power save mode in firmware.
 * enable: 1 = enter PS, 0 = exit PS
 */
void
b43bsd_fw_set_ps(struct b43bsd_softc *sc, int enable)
{
	b43bsd_fw_cmd(sc, B43BSD_FWCMD_SET_PS,
	    (uint16_t)(enable ? 1 : 0), 0);
}

/*
 * Set TSF timer value in firmware.
 * Used to sync the hardware TSF with the AP's beacon timestamp.
 */
void
b43bsd_fw_set_tsf(struct b43bsd_softc *sc, uint64_t tsf)
{
	b43bsd_fw_cmd(sc, B43BSD_FWCMD_SET_TSF,
	    (uint16_t)(tsf & 0xffff),
	    (uint16_t)((tsf >> 16) & 0xffff));
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD_DATA2,
	    (uint16_t)((tsf >> 32) & 0xffff));
	b43bsd_shm_write16(sc, B43BSD_SHM_SHARED, B43BSD_SHM_CMD_DATA3,
	    (uint16_t)((tsf >> 48) & 0xffff));
}

/*
 * Enable/disable beacon processing in firmware.
 * enable: 1 = process beacons, 0 = ignore beacons
 */
void
b43bsd_fw_beacon_enable(struct b43bsd_softc *sc, int enable)
{
	b43bsd_fw_cmd(sc, B43BSD_FWCMD_BEACON_ENABLE,
	    (uint16_t)(enable ? 1 : 0), 0);
}

/*
 * Force firmware to wake from sleep.
 * Used when TX is pending and the chip is in PS mode.
 */
void
b43bsd_fw_wake(struct b43bsd_softc *sc)
{
	b43bsd_fw_cmd(sc, B43BSD_FWCMD_WAKE, 0, 0);
}
