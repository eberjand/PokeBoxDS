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

#include "unknownIcon.h"
#include "util.h"

#define ASSET_SOURCE_NONE 0
#define ASSET_SOURCE_CART 1
#define ASSET_SOURCE_ROMFILE 2

#define ROM_OFFSET_MASK 0xFFFFFF

typedef struct assets_handler {
	int assetSource;
	char *file;

	// Don't access these pointers directly. Use getters instead.
	uint16_t **iconImageTable;
	uint8_t *iconPaletteIndices;
	uint16_t **iconPaletteTable;
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
	void *iconPaletteIndices;
	void *iconPaletteTable;
};
static const struct rom_offsets_t rom_offsets[] = {
	// Ruby
	{"AXVJ", 0, (void*) 0x8391a98, (void*) 0x8392178, (void*) 0x8392330},
	{"AXVE", 0, (void*) 0x83bbd20, (void*) 0x83bc400, (void*) 0x83bc5b8},
	{"AXVE", 1, (void*) 0x83bbd3c, (void*) 0x83bc41c, (void*) 0x83bc5d4},
	{"AXVE", 2, (void*) 0x83bbd3c, (void*) 0x83bc41c, (void*) 0x83bc5d4},
	{"AXVF", 0, (void*) 0x83c3704, (void*) 0x83c3de4, (void*) 0x83c3f9c},
	{"AXVF", 1, (void*) 0x83c3704, (void*) 0x83c3de4, (void*) 0x83c3f9c},
	{"AXVD", 0, (void*) 0x83c7c30, (void*) 0x83c8310, (void*) 0x83c84c8},
	{"AXVD", 1, (void*) 0x83c7c30, (void*) 0x83c8310, (void*) 0x83c84c8},
	{"AXVS", 0, (void*) 0x83bfd84, (void*) 0x83c0464, (void*) 0x83c061c},
	{"AXVS", 1, (void*) 0x83bfd84, (void*) 0x83c0464, (void*) 0x83c061c},
	{"AXVI", 0, (void*) 0x83bc974, (void*) 0x83bd054, (void*) 0x83bd20c},
	{"AXVI", 1, (void*) 0x83bc974, (void*) 0x83bd054, (void*) 0x83bd20c},

	// Sapphire
	{"AXPJ", 0, (void*) 0x8391a7c, (void*) 0x839215c, (void*) 0x8392314},
	{"AXPE", 0, (void*) 0x83bbd78, (void*) 0x83bc458, (void*) 0x83bc610},
	{"AXPE", 1, (void*) 0x83bbd98, (void*) 0x83bc478, (void*) 0x83bc630},
	{"AXPE", 2, (void*) 0x83bbd98, (void*) 0x83bc478, (void*) 0x83bc630},
	{"AXPF", 0, (void*) 0x83c3234, (void*) 0x83c3914, (void*) 0x83c3acc},
	{"AXPF", 1, (void*) 0x83c3234, (void*) 0x83c3914, (void*) 0x83c3acc},
	{"AXPD", 0, (void*) 0x83c7b9c, (void*) 0x83c827c, (void*) 0x83c8434},
	{"AXPD", 1, (void*) 0x83c7b9c, (void*) 0x83c827c, (void*) 0x83c8434},
	{"AXPS", 0, (void*) 0x83bfac0, (void*) 0x83c01a0, (void*) 0x83c0358},
	{"AXPS", 1, (void*) 0x83bfac0, (void*) 0x83c01a0, (void*) 0x83c0358},
	{"AXPI", 0, (void*) 0x83bc618, (void*) 0x83bccf8, (void*) 0x83bceb0},
	{"AXPI", 1, (void*) 0x83bc618, (void*) 0x83bccf8, (void*) 0x83bceb0},

	// FireRed
	{"BPRJ", 0, (void*) 0x839bca8, (void*) 0x839c388, (void*) 0x839c540},
	{"BPRE", 0, (void*) 0x83d37a0, (void*) 0x83d3e80, (void*) 0x83d4038},
	{"BPRE", 1, (void*) 0x83d3810, (void*) 0x83d3ef0, (void*) 0x83d40a8},
	{"BPRF", 0, (void*) 0x83cd5e0, (void*) 0x83cdcc0, (void*) 0x83cde78},
	{"BPRD", 0, (void*) 0x83d30b4, (void*) 0x83d3794, (void*) 0x83d394c},
	{"BPRS", 0, (void*) 0x83ce958, (void*) 0x83cf038, (void*) 0x83cf1f0},
	{"BPRI", 0, (void*) 0x83cc270, (void*) 0x83cc950, (void*) 0x83ccb08},

	// LeafGreen
	{"BPGJ", 0, (void*) 0x839bb18, (void*) 0x839c1f8, (void*) 0x839c3b0},
	{"BPGE", 0, (void*) 0x83d35dc, (void*) 0x83d3cbc, (void*) 0x83d3e74},
	{"BPGE", 1, (void*) 0x83d364c, (void*) 0x83d3d2c, (void*) 0x83d3ee4},
	{"BPGF", 0, (void*) 0x83cd41c, (void*) 0x83cdafc, (void*) 0x83cdcb4},
	{"BPGD", 0, (void*) 0x83d2ef0, (void*) 0x83d35d0, (void*) 0x83d3788},
	{"BPGS", 0, (void*) 0x83ce794, (void*) 0x83cee74, (void*) 0x83cf02c},
	{"BPGI", 0, (void*) 0x83cc0ac, (void*) 0x83cc78c, (void*) 0x83cc944},

	// Emerald
	{"BPEJ", 0, (void*) 0x8556804, (void*) 0x8556ee4, (void*) 0x855709c},
	{"BPEE", 0, (void*) 0x857bca8, (void*) 0x857c388, (void*) 0x857c540},
	{"BPEF", 0, (void*) 0x8580020, (void*) 0x8580700, (void*) 0x85808b8},
	{"BPED", 0, (void*) 0x858caa8, (void*) 0x858d188, (void*) 0x858d340},
	{"BPES", 0, (void*) 0x857e784, (void*) 0x857ee64, (void*) 0x857f01c},
	{"BPEI", 0, (void*) 0x857838c, (void*) 0x8578a6c, (void*) 0x8578c24},
};

static bool getIconOffsets(tGBAHeader *header) {
	uint32_t gamecode = GET32(header->gamecode, 0);

	for (int i = 0; i < ARRAY_LENGTH(rom_offsets); i++) {
		const struct rom_offsets_t *table = &rom_offsets[i];
		if (gamecode == GET32(table->gamecode, 0)) {
			if (header->version == table->rev) {
				handler.iconImageTable = table->iconTable;
				handler.iconPaletteIndices = table->iconPaletteIndices;
				handler.iconPaletteTable = table->iconPaletteTable;
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
