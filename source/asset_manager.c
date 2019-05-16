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
#include "asset_manager.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <nds.h>

#include "unknownFront.h"
#include "unknownIcon.h"
#include "util.h"

#define ASSET_SOURCE_NONE 0
#define ASSET_SOURCE_CART 1
#define ASSET_SOURCE_ROMFILE 2

#define ROM_OFFSET_MASK 0xFFFFFF

uint16_t frontSpriteCompressed[2048];

typedef struct assets_handler {
	int assetSource;
	char *file;

	// Don't access these pointers directly. Use getters instead.
	uint16_t **iconImageTable;
	uint8_t *iconPaletteIndices;
	uint16_t **iconPaletteTable;
	uint16_t **frontSpriteTable;
	uint16_t **frontPaletteTable;
	uint16_t **shinyPaletteTable;
	FILE *fp;
	uint8_t buffer[1024];
	uint8_t palettesData[3 * 32];
} assets_handler_t;
static assets_handler_t handler;

// Define the offsets PC icon assets in all known versions of GBA Pokémon
// Languages: J=Japanese, E=English, F=French, D=German S=Spanish, I=Italian
struct rom_offsets_t {
	char *gamecode;
	int rev;
	void *iconTable;
	void *frontSpriteTable;
};
static const struct rom_offsets_t rom_offsets[] = {
	// Ruby
	{"AXVJ", 0, (void*) 0x8391a98, (void*) 0x81bcb60},
	{"AXVE", 0, (void*) 0x83bbd20, (void*) 0x81e8354},
	{"AXVE", 1, (void*) 0x83bbd3c, (void*) 0x81e836c},
	{"AXVE", 2, (void*) 0x83bbd3c, (void*) 0x81e836c},
	{"AXVF", 0, (void*) 0x83c3704, (void*) 0x81f075c},
	{"AXVF", 1, (void*) 0x83c3704, (void*) 0x81f075c},
	{"AXVD", 0, (void*) 0x83c7c30, (void*) 0x81f52d0},
	{"AXVD", 1, (void*) 0x83c7c30, (void*) 0x81f52d0},
	{"AXVS", 0, (void*) 0x83bfd84, (void*) 0x81ed074},
	{"AXVS", 1, (void*) 0x83bfd84, (void*) 0x81ed074},
	{"AXVI", 0, (void*) 0x83bc974, (void*) 0x81e9ff0},
	{"AXVI", 1, (void*) 0x83bc974, (void*) 0x81e9ff0},

	// Sapphire
	{"AXPJ", 0, (void*) 0x8391a7c, (void*) 0x81bcaf0},
	{"AXPE", 0, (void*) 0x83bbd78, (void*) 0x81e82e4},
	{"AXPE", 1, (void*) 0x83bbd98, (void*) 0x81e82fc},
	{"AXPE", 2, (void*) 0x83bbd98, (void*) 0x81e82fc},
	{"AXPF", 0, (void*) 0x83c3234, (void*) 0x81f06ec},
	{"AXPF", 1, (void*) 0x83c3234, (void*) 0x81f06ec},
	{"AXPD", 0, (void*) 0x83c7b9c, (void*) 0x81f5264},
	{"AXPD", 1, (void*) 0x83c7b9c, (void*) 0x81f5264},
	{"AXPS", 0, (void*) 0x83bfac0, (void*) 0x81ed004},
	{"AXPS", 1, (void*) 0x83bfac0, (void*) 0x81ed004},
	{"AXPI", 0, (void*) 0x83bc618, (void*) 0x81e9f80},
	{"AXPI", 1, (void*) 0x83bc618, (void*) 0x81e9f80},

	// FireRed
	{"BPRJ", 0, (void*) 0x839bca8, (void*) 0x81f4690},
	{"BPRE", 0, (void*) 0x83d37a0, (void*) 0x82350ac},
	{"BPRE", 1, (void*) 0x83d3810, (void*) 0x823511c},
	{"BPRF", 0, (void*) 0x83cd5e0, (void*) 0x822f4b8},
	{"BPRD", 0, (void*) 0x83d30b4, (void*) 0x8234f7c},
	{"BPRS", 0, (void*) 0x83ce958, (void*) 0x8230818},
	{"BPRI", 0, (void*) 0x83cc270, (void*) 0x822e150},

	// LeafGreen
	{"BPGJ", 0, (void*) 0x839bb18, (void*) 0x81f466c},
	{"BPGE", 0, (void*) 0x83d35dc, (void*) 0x8235088},
	{"BPGE", 1, (void*) 0x83d364c, (void*) 0x82350f8},
	{"BPGF", 0, (void*) 0x83cd41c, (void*) 0x822f494},
	{"BPGD", 0, (void*) 0x83d2ef0, (void*) 0x8234f58},
	{"BPGS", 0, (void*) 0x83ce794, (void*) 0x82307f4},
	{"BPGI", 0, (void*) 0x83cc0ac, (void*) 0x822e12c},

	// Emerald
	{"BPEJ", 0, (void*) 0x8556804, (void*) 0x82d4ca8},
	{"BPEE", 0, (void*) 0x857bca8, (void*) 0x8301418},
	{"BPEF", 0, (void*) 0x8580020, (void*) 0x8303f48},
	{"BPED", 0, (void*) 0x858caa8, (void*) 0x8315d88},
	{"BPES", 0, (void*) 0x857e784, (void*) 0x830767c},
	{"BPEI", 0, (void*) 0x857838c, (void*) 0x8300ddc},
};

static bool getIconOffsets(tGBAHeader *header) {
	uint32_t gamecode = GET32(header->gamecode, 0);

	for (int i = 0; i < ARRAY_LENGTH(rom_offsets); i++) {
		const struct rom_offsets_t *table = &rom_offsets[i];
		if (gamecode == GET32(table->gamecode, 0)) {
			if (header->version == table->rev) {
				handler.iconImageTable = table->iconTable;
				handler.iconPaletteIndices = table->iconTable + 0x6e0;
				handler.iconPaletteTable = table->iconTable + 0x898;
				handler.frontSpriteTable = table->frontSpriteTable;
				handler.frontPaletteTable = table->frontSpriteTable + 0x2260;
				handler.shinyPaletteTable = table->frontSpriteTable + 0x3020;
				return true;
			}
		}
	}
	return false;
}

void assets_init_placeholder() {
	assets_free();
	handler.assetSource = ASSET_SOURCE_NONE;
	handler.file = NULL;
	handler.fp = NULL;
}

bool assets_init_cart() {
	assets_free();
	sysSetBusOwners(true, true);
	swiDelay(10);

	handler.assetSource = ASSET_SOURCE_CART;
	handler.file = NULL;
	handler.fp = NULL;
	if (!getIconOffsets(&GBA_HEADER))
		return false;
	return true;
}

bool assets_init_romfile(const char *file) {
	tGBAHeader header;
	uint8_t *indicesCopy;

	assets_free();
	handler.assetSource = ASSET_SOURCE_ROMFILE;
	handler.fp = fopen(file, "rb");
	if (handler.fp == NULL)
		return false;
	fread(&header, sizeof(header), 1, handler.fp);

	if (!getIconOffsets(&header)) {
		fclose(handler.fp);
		handler.fp = NULL;
		handler.assetSource = 0;
		return false;
	}

	indicesCopy = malloc(439);
	fseek(handler.fp, (long) handler.iconPaletteIndices & ROM_OFFSET_MASK, SEEK_SET);
	fread(indicesCopy, 1, 439, handler.fp);
	handler.iconPaletteIndices = indicesCopy;

	uint16_t *palAddress;
	fseek(handler.fp, (long) (handler.iconPaletteTable) & ROM_OFFSET_MASK, SEEK_SET);
	fread(&palAddress, 1, 4, handler.fp);
	fseek(handler.fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
	fread(handler.palettesData, 1, 32 * 3, handler.fp);

	return true;
}

void assets_free() {
	if (handler.fp)
		fclose(handler.fp);
	if ((uint16_t*) handler.iconPaletteIndices < GBAROM)
		free(handler.iconPaletteIndices);
	memset(&handler, 0, sizeof(handler));
}

const uint16_t* getIconImage(uint16_t species) {
	if (handler.assetSource == ASSET_SOURCE_NONE)
		return (const uint16_t*) unknownIconTiles;
	if (handler.assetSource == ASSET_SOURCE_CART)
		return handler.iconImageTable[species];

	// Buffering this table would require 439 species * 4B = 1756 bytes
	uint16_t* imageAddress = NULL;
	fseek(handler.fp, (long) (handler.iconImageTable + species) & ROM_OFFSET_MASK, SEEK_SET);
	fread(&imageAddress, 4, 1, handler.fp);
	// Buffering every image would require about 1MB
	fseek(handler.fp, (long) imageAddress & ROM_OFFSET_MASK, SEEK_SET);
	fread(handler.buffer, 2, 512, handler.fp);
	return (const uint16_t*) handler.buffer;
}

uint8_t getIconPaletteIdx(uint16_t species) {
	if (handler.iconPaletteIndices)
		return handler.iconPaletteIndices[species];
	return 0;
}

const uint16_t* getIconPaletteColors(int index) {
	if (handler.assetSource == ASSET_SOURCE_NONE)
		return unknownIconPal;
	if (handler.assetSource == ASSET_SOURCE_CART)
		return handler.iconPaletteTable[index * 2];

	return (const uint16_t*) (handler.palettesData + index * 32);
}

void readFrontImage(uint8_t *tiles_out, uint8_t *palette_out, uint16_t species, int shiny) {
	uint16_t **paletteTable = (shiny) ? handler.shinyPaletteTable : handler.frontPaletteTable;
	void *tileAddress = NULL;
	void *palAddress = NULL;
	u8 palCompressed[64];

	if (handler.assetSource == ASSET_SOURCE_CART) {
		// Each 8-byte item in this table is a data pointer followed by u16 size, u16 tag
		// 64x64 image needs 2048 bytes
		tileAddress = handler.frontSpriteTable[species * 2];
		// Each 8-byte item in this table is a data pointer followed by u16 tag, u16 padding
		// 16 color palette is 32 bytes
		palAddress = paletteTable[species * 2];
	} else if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
		FILE *fp = handler.fp;
		fseek(fp, (long) (handler.frontSpriteTable + species * 2) & ROM_OFFSET_MASK, SEEK_SET);
		fread(&tileAddress, 4, 1, fp);
		fseek(fp, (long) (paletteTable + species * 2) & ROM_OFFSET_MASK, SEEK_SET);
		fread(&palAddress, 4, 1, fp);
		fseek(fp, (long) tileAddress & ROM_OFFSET_MASK, SEEK_SET);
		fread(frontSpriteCompressed, 1, sizeof(frontSpriteCompressed), fp);
		fseek(fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
		fread(palCompressed, 1, sizeof(palCompressed), fp);
		tileAddress = frontSpriteCompressed;
		palAddress = palCompressed;
	}

	// Avoid buffer overflows by checking the extracted size
	if (tileAddress == NULL || (GET32(tileAddress, 0) >> 8) > 4096)
		memcpy(tiles_out, unknownFrontTiles, 2048);
	else
		swiDecompressLZSSWram(tileAddress, tiles_out);

	if (palAddress == NULL || (GET32(palAddress, 0) >> 8) > 64)
		memcpy(palette_out, unknownFrontPal, 32);
	else
		swiDecompressLZSSWram(palAddress, palette_out);
}
