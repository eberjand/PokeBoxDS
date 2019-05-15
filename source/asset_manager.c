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
	//{"AXVJ", 0, 0, 0, 0},
	{"AXVE", 0, (void*) 0x83bbd20, (void*) 0x83bc400, (void*) 0x83bc5b8},
	{"AXVE", 1, (void*) 0x83bbd3c, (void*) 0x83bc41c, (void*) 0x83bc5d4},
	{"AXVE", 2, (void*) 0x83bbd3c, (void*) 0x83bc41c, (void*) 0x83bc5d4},
	//{"AXVF", 0, 0, 0, 0},
	//{"AXVF", 1, 0, 0, 0},
	//{"AXVD", 0, 0, 0, 0},
	//{"AXVD", 1, 0, 0, 0},
	//{"AXVS", 0, 0, 0, 0},
	//{"AXVS", 1, 0, 0, 0},
	//{"AXVI", 0, 0, 0, 0},
	//{"AXVI", 1, 0, 0, 0},

	// Sapphire
	//{"AXPJ", 0, 0, 0, 0},
	{"AXPE", 0, (void*) 0x83bbd78, (void*) 0x83bc458, (void*) 0x83bc610},
	{"AXPE", 1, (void*) 0x83bbd98, (void*) 0x83bc478, (void*) 0x83bc630},
	{"AXPE", 2, (void*) 0x83bbd98, (void*) 0x83bc478, (void*) 0x83bc630},
	//{"AXPF", 0, 0, 0, 0},
	//{"AXPF", 1, 0, 0, 0},
	//{"AXPD", 0, 0, 0, 0},
	//{"AXPD", 1, 0, 0, 0},
	//{"AXPS", 0, 0, 0, 0},
	//{"AXPS", 1, 0, 0, 0},
	//{"AXPI", 0, 0, 0, 0},
	//{"AXPI", 1, 0, 0, 0},

	// FireRed
	//{"BPRJ", 0, 0, 0, 0},
	//{"BPRE", 0, 0, 0, 0},
	//{"BPRE", 1, 0, 0, 0},
	//{"BPRF", 0, 0, 0, 0},
	//{"BPRD", 0, 0, 0, 0},
	//{"BPRS", 0, 0, 0, 0},
	//{"BPRI", 0, 0, 0, 0},

	// LeafGreen
	//{"BPGJ", 0, 0, 0, 0},
	//{"BPGE", 0, 0, 0, 0},
	//{"BPGE", 1, 0, 0, 0},
	//{"BPGF", 0, 0, 0, 0},
	//{"BPGD", 0, 0, 0, 0},
	//{"BPGS", 0, 0, 0, 0},
	//{"BPGI", 0, 0, 0, 0},

	// Emerald
	//{"BPEJ", 0, 0, 0, 0},
	//{"BPEE", 0, 0, 0, 0},
	//{"BPEF", 0, 0, 0, 0},
	//{"BPED", 0, 0, 0, 0},
	//{"BPES", 0, 0, 0, 0},
	//{"BPEI", 0, 0, 0, 0},
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
