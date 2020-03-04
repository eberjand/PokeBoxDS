/*
 * This file is part of the PokeBoxDS project.
 * Copyright (C) 2019 Jennifer Berringer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; even with the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "sd_boxes.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "message_window.h"
#include "pkmx_format.h"
#include "util.h"

#define BOXDATA_MAGIC "PKMBBOXG"

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Only little-endian is supported."
#endif

struct boxg_file_header {
	char magic[8]; // PKMBBOXG
	uint16_t version;
	uint8_t activeSlot;
	uint8_t groupNumber;
	uint32_t slot2Offset;
	uint16_t groupName[16]; // UCS-2LE encoding
};

struct boxg_slot_header {
	uint32_t saveCounter;
	uint32_t checksum; // unused
	uint32_t timestamp; // unused
	uint16_t timestamp_msb; // 48-bit timestamp avoids Y2038, but NDS RTC only goes to 2099
	uint16_t numBoxes;
};

struct boxg_boxmeta {
	uint16_t wallpaper;
	uint16_t flags;
	uint16_t boxName[14]; // UCS-2LE encoding
};

int sd_boxes_load(uint8_t *boxData, uint8_t group, uint8_t *numBoxes_out) {
	FILE *fp;
	struct boxg_file_header fileHeader;
	struct boxg_slot_header slotHeader;
	size_t rc;
	uint16_t numBoxes;
	size_t readSize;

	fp = fopen("/pokebox/boxes/group000.bin", "rb");
	if (!fp) {
		if (errno != ENOENT) {
			fclose(fp);
			open_message_window("Error loading SD boxes: File open failed (%d)", errno);
		}
		// Initialize 32 empty boxes by default
		*numBoxes_out = 32;
		return 1;
	}

	// Read and validate the file header
	rc = fread(&fileHeader, 1, sizeof(fileHeader), fp) < sizeof(fileHeader) ||
		memcmp(fileHeader.magic, BOXDATA_MAGIC, sizeof(fileHeader.magic));
	if (rc) {
		fclose(fp);
		open_message_window("Error loading SD boxes: Invalid file type");
		return 0;
	}
	if (fileHeader.version != 0) {
		fclose(fp);
		open_message_window("Error loading SD boxes: Invalid file version");
		return 0;
	}

	// Read the slot header
	if (fileHeader.activeSlot) {
		fseek(fp, fileHeader.slot2Offset, SEEK_SET);
	}
	rc = fread(&slotHeader, 1, sizeof(slotHeader), fp);
	if (rc < sizeof(slotHeader)) {
		fclose(fp);
		open_message_window("Error loading SD boxes: Unexpected EOF");
		return 0;
	}

	// Allow files with more than 32 boxes, but ignore boxes 33+
	numBoxes = slotHeader.numBoxes;
	if (numBoxes > 32)
		numBoxes = 32;
	*numBoxes_out = numBoxes;

	// Ignore the box metadata for now
	fseek(fp, sizeof(struct boxg_boxmeta) * slotHeader.numBoxes, SEEK_CUR);

	// Read the actual PKMX data
	readSize = PKMX_SIZE * 30 * numBoxes;
	rc = fread(boxData, 1, readSize, fp);
	if (rc < readSize) {
		if (feof(fp))
			open_message_window("Error loading SD boxes: Unexpected EOF");
		else
			open_message_window("Error loading SD boxes: Read error (%d)", errno);
		fclose(fp);
		return 0;
	}

	fclose(fp);
	return 1;
}

static int copy_file_blocks(uint32_t dstOff, uint32_t srcOff, uint32_t size, FILE *fp) {
	uint8_t buffer[1024];
	int rc;
	while (size > sizeof(buffer)) {
		rc =
			fseek(fp, srcOff, SEEK_SET) < 0 ||
			fread(buffer, 1, sizeof(buffer), fp) < sizeof(buffer) ||
			fseek(fp, dstOff, SEEK_SET) < 0 ||
			fwrite(buffer, 1, sizeof(buffer), fp) < sizeof(buffer);
		if (rc)
			return 0;
		size -= sizeof(buffer);
		srcOff += sizeof(buffer);
		dstOff += sizeof(buffer);
	}
	if (size) {
		rc =
			fseek(fp, srcOff, SEEK_SET) < 0 ||
			fread(buffer, 1, size, fp) < size ||
			fseek(fp, dstOff, SEEK_SET) < 0 ||
			fwrite(buffer, 1, size, fp) < size;
	}
	return !rc;
}

static int sd_boxes_create(uint8_t *boxData, uint8_t group, uint16_t numBoxes) {
	FILE *fp;
	uint32_t slotSize;
	struct boxg_file_header fileHeader = {0};
	struct boxg_slot_header slotHeader = {0};

	// Create a new file
	fp = fopen("/pokebox/boxes/group000.bin", "wb");
	if (fp < 0) {
		open_message_window("Error saving SD boxes: File create failed (%d)", errno);
		return 0;
	}

	// Write the file header
	slotSize = sizeof(slotHeader) + numBoxes * (sizeof(struct boxg_boxmeta) + 30 * PKMX_SIZE);
	memset(&fileHeader, 0, sizeof(fileHeader));
	memcpy(&fileHeader.magic, BOXDATA_MAGIC, sizeof(fileHeader.magic));
	fileHeader.groupNumber = group;
	fileHeader.slot2Offset = sizeof(fileHeader) + slotSize;
	fwrite(&fileHeader, 1, sizeof(fileHeader), fp);
	if (ferror(fp))
		goto create_write_error;

	// Write the slot header
	memset(&slotHeader, 0, sizeof(slotHeader));
	slotHeader.numBoxes = numBoxes;
	fwrite(&slotHeader, 1, sizeof(slotHeader), fp);
	if (ferror(fp))
		goto create_write_error;

	// Write the box metadata
	for (int i = 0; i < numBoxes; i++) {
		const struct boxg_boxmeta boxmeta = {0};
		fwrite(&boxmeta, 1, sizeof(boxmeta), fp);
	}
	if (ferror(fp))
		goto create_write_error;

	// Write the box PKMX data
	fwrite(boxData, 1, numBoxes * 30 * PKMX_SIZE, fp);
	if (ferror(fp))
		goto create_write_error;

	// Fill Slot 2 with zeroes
	for (int i = 0; i < slotSize / 16; i++) {
		const uint8_t zeroes[16] = {0};
		fwrite(zeroes, 1, 16, fp);
	}
	fflush(fp);
	if (ferror(fp))
		goto create_write_error;

	fclose(fp);
	return 1;

create_write_error:
	open_message_window("Error saving SD boxes: Write error (%d)", errno);
	fclose(fp);
	return 0;
}

static int sd_boxes_update(uint8_t *boxData, uint16_t numBoxes, FILE *fp) {
	size_t rc;
	struct boxg_file_header fileHeader;
	struct boxg_slot_header slotHeader;
	uint16_t prevNumBoxes;
	uint32_t nextSlotOffset, prevSlotOffset;

	// Verify existing file header
	rc = fread(&fileHeader, 1, sizeof(fileHeader), fp) < sizeof(fileHeader) ||
		memcmp(fileHeader.magic, BOXDATA_MAGIC, sizeof(fileHeader.magic));
	if (rc) {
		open_message_window("Error saving SD boxes: Invalid file type");
		return 0;
	}
	if (fileHeader.version != 0) {
		open_message_window("Error saving SD boxes: Invalid file version");
		return 0;
	}

	// Read the previous save slot header
	if (fileHeader.activeSlot) {
		prevSlotOffset = fileHeader.slot2Offset;
		nextSlotOffset = sizeof(fileHeader);
		fseek(fp, prevSlotOffset, SEEK_SET);
	} else {
		prevSlotOffset = sizeof(fileHeader);
		nextSlotOffset = fileHeader.slot2Offset;
	}
	fread(&slotHeader, 1, sizeof(slotHeader), fp);
	fseek(fp, nextSlotOffset, SEEK_SET);

	// Write the updated slot header
	slotHeader.saveCounter++;
	prevNumBoxes = slotHeader.numBoxes;
	slotHeader.numBoxes = numBoxes >= prevNumBoxes ? numBoxes : prevNumBoxes;
	fwrite(&slotHeader, 1, sizeof(slotHeader), fp);
	if (ferror(fp))
		goto update_write_error;

	// Overwrite box metadata
	copy_file_blocks(
		nextSlotOffset + sizeof(slotHeader),
		prevSlotOffset + sizeof(slotHeader),
		numBoxes * sizeof(struct boxg_boxmeta),
		fp);
	if (ferror(fp))
		goto update_write_error;

	// Overwrite box data
	fwrite(boxData, 1, numBoxes * 30 * PKMX_SIZE, fp);
	if (ferror(fp))
		goto update_write_error;

	// Copy trailing box data
	if (prevNumBoxes > numBoxes) {
		uint32_t tailOff = sizeof(slotHeader) +
			numBoxes * 30 * PKMX_SIZE +
			prevNumBoxes * sizeof(struct boxg_boxmeta);
		copy_file_blocks(
			nextSlotOffset + tailOff,
			prevSlotOffset + tailOff,
			(prevNumBoxes - numBoxes) * 30 * PKMX_SIZE,
			fp);
	}

	// Finalize the save
	fileHeader.activeSlot = !fileHeader.activeSlot;
	rewind(fp);
	fwrite(&fileHeader, 1, sizeof(fileHeader), fp);
	fclose(fp);

	return 1;

update_write_error:
	open_message_window("Error saving SD boxes: Write error (%d)", errno);
	return 0;
}

int sd_boxes_save(uint8_t *boxData, uint8_t group, uint16_t numBoxes) {
	FILE *fp;
	size_t rc;
	struct stat s;

	if (numBoxes <= 0 || numBoxes >= 256) {
		open_message_window("Error saving SD boxes: Too many boxes in group");
		return 0;
	}

	// Create the needed directories if they don't already exist
	if (mkdir("/pokebox", 0777) < 0 && errno != EEXIST) {
		open_message_window("Error saving SD boxes: Unable to create directories");
		return 0;
	}
	if (mkdir("/pokebox/boxes", 0777) < 0) {
		int createFail =
			errno != EEXIST ||
			stat("/pokebox/boxes", &s) < 0 ||
			(s.st_mode & S_IFDIR) == 0;
		if (createFail) {
			open_message_window("Error saving SD boxes: Unable to create directories");
			return 0;
		}
	}

	if (stat("/pokebox/boxes/group000.bin", &s) < 0) {
		return sd_boxes_create(boxData, group, numBoxes);
	}

	fp = fopen("/pokebox/boxes/group000.bin", "r+b");
	if (fp < 0) {
		open_message_window("Error saving SD boxes: File open failed (%d)", errno);
		return 0;
	}

	rc = sd_boxes_update(boxData, numBoxes, fp);
	fclose(fp);
	return rc;
}
