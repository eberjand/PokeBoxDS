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

uint16_t tileGfxCompressed[4096];

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
	uint16_t **wallpaperTable;
	int game;
	int language;
	FILE *fp;
	uint8_t buffer[1024];
	uint8_t palettesData[3 * 32];
} assets_handler_t;
static assets_handler_t handler;

#define IS_RUBY_SAPPHIRE ((handler.game >> 1) == 0)
#define IS_FIRERED_LEAFGREEN ((handler.game >> 1) == 1)

// Define the offsets PC icon assets in all known versions of GBA Pokémon
// Languages: J=Japanese, E=English, F=French, D=German S=Spanish, I=Italian
struct rom_offsets_t {
	char *gamecode;
	int rev;
	void *iconTable;
	void *frontSpriteTable;
	void *wallpaperTable;
};
// I found most of these offsets by figuring out what data I'm interested in
// for one ROM, following a pointer from the table to reach data that doesn't
// include a pointer (not affected by offsets), then searching for a few bytes
// of that data in every other ROM and then searching for references to that
// address to find that ROM's table.
// Note that RSE and FRLG have different full-size/front sprite data.
// The highest-order "08" byte in all of these points to the GBA cart's memory
// mapped area. In a dump, an offset like "0x8391a98" is actually at "0x391a98"
static const struct rom_offsets_t rom_offsets[] = {
	// Ruby
	{"AXVJ", 0, (void*) 0x8391a98, (void*) 0x81bcb60, (void*) 0x8390e00},
	{"AXVE", 0, (void*) 0x83bbd20, (void*) 0x81e8354, (void*) 0x83bb0e8},
	{"AXVE", 1, (void*) 0x83bbd3c, (void*) 0x81e836c, (void*) 0x83bb104},
	{"AXVE", 2, (void*) 0x83bbd3c, (void*) 0x81e836c, (void*) 0x83bb104},
	{"AXVF", 0, (void*) 0x83c3704, (void*) 0x81f075c, (void*) 0x83c2acc},
	{"AXVF", 1, (void*) 0x83c3704, (void*) 0x81f075c, (void*) 0x83c2acc},
	{"AXVD", 0, (void*) 0x83c7c30, (void*) 0x81f52d0, (void*) 0x83c6ff8},
	{"AXVD", 1, (void*) 0x83c7c30, (void*) 0x81f52d0, (void*) 0x83c6ff8},
	{"AXVS", 0, (void*) 0x83bfd84, (void*) 0x81ed074, (void*) 0x83bf14c},
	{"AXVS", 1, (void*) 0x83bfd84, (void*) 0x81ed074, (void*) 0x83bf14c},
	{"AXVI", 0, (void*) 0x83bc974, (void*) 0x81e9ff0, (void*) 0x83bbd3c},
	{"AXVI", 1, (void*) 0x83bc974, (void*) 0x81e9ff0, (void*) 0x83bbd3c},

	// Sapphire
	{"AXPJ", 0, (void*) 0x8391a7c, (void*) 0x81bcaf0, (void*) 0x8390de4},
	{"AXPE", 0, (void*) 0x83bbd78, (void*) 0x81e82e4, (void*) 0x83bb140},
	{"AXPE", 1, (void*) 0x83bbd98, (void*) 0x81e82fc, (void*) 0x83bb160},
	{"AXPE", 2, (void*) 0x83bbd98, (void*) 0x81e82fc, (void*) 0x83bb160},
	{"AXPF", 0, (void*) 0x83c3234, (void*) 0x81f06ec, (void*) 0x83c25fc},
	{"AXPF", 1, (void*) 0x83c3234, (void*) 0x81f06ec, (void*) 0x83c25fc},
	{"AXPD", 0, (void*) 0x83c7b9c, (void*) 0x81f5264, (void*) 0x83c6f64},
	{"AXPD", 1, (void*) 0x83c7b9c, (void*) 0x81f5264, (void*) 0x83c6f64},
	{"AXPS", 0, (void*) 0x83bfac0, (void*) 0x81ed004, (void*) 0x83bee88},
	{"AXPS", 1, (void*) 0x83bfac0, (void*) 0x81ed004, (void*) 0x83bee88},
	{"AXPI", 0, (void*) 0x83bc618, (void*) 0x81e9f80, (void*) 0x83bb9e0},
	{"AXPI", 1, (void*) 0x83bc618, (void*) 0x81e9f80, (void*) 0x83bb9e0},

	// FireRed
	{"BPRJ", 0, (void*) 0x839bca8, (void*) 0x81f4690, (void*) 0x839af18},
	{"BPRE", 0, (void*) 0x83d37a0, (void*) 0x82350ac, (void*) 0x83d2a10},
	{"BPRE", 1, (void*) 0x83d3810, (void*) 0x823511c, (void*) 0x83d2a80},
	{"BPRF", 0, (void*) 0x83cd5e0, (void*) 0x822f4b8, (void*) 0x83cc850},
	{"BPRD", 0, (void*) 0x83d30b4, (void*) 0x8234f7c, (void*) 0x83d2324},
	{"BPRS", 0, (void*) 0x83ce958, (void*) 0x8230818, (void*) 0x83cdbc8},
	{"BPRI", 0, (void*) 0x83cc270, (void*) 0x822e150, (void*) 0x83cb4e0},

	// LeafGreen
	{"BPGJ", 0, (void*) 0x839bb18, (void*) 0x81f466c, (void*) 0x839ad88},
	{"BPGE", 0, (void*) 0x83d35dc, (void*) 0x8235088, (void*) 0x83d284c},
	{"BPGE", 1, (void*) 0x83d364c, (void*) 0x82350f8, (void*) 0x83d28bc},
	{"BPGF", 0, (void*) 0x83cd41c, (void*) 0x822f494, (void*) 0x83cc68c},
	{"BPGD", 0, (void*) 0x83d2ef0, (void*) 0x8234f58, (void*) 0x83d2160},
	{"BPGS", 0, (void*) 0x83ce794, (void*) 0x82307f4, (void*) 0x83cda04},
	{"BPGI", 0, (void*) 0x83cc0ac, (void*) 0x822e12c, (void*) 0x83cb31c},

	// Emerald
	{"BPEJ", 0, (void*) 0x8556804, (void*) 0x82d4ca8, (void*) 0x8551868},
	{"BPEE", 0, (void*) 0x857bca8, (void*) 0x8301418, (void*) 0x85775b8},
	{"BPEF", 0, (void*) 0x8580020, (void*) 0x8303f48, (void*) 0x857b930},
	{"BPED", 0, (void*) 0x858caa8, (void*) 0x8315d88, (void*) 0x85883b8},
	{"BPES", 0, (void*) 0x857e784, (void*) 0x830767c, (void*) 0x857a094},
	{"BPEI", 0, (void*) 0x857838c, (void*) 0x8300ddc, (void*) 0x8573c9c},
};
static const char *const gamecode_list[] = {
	"AXV", "AXP", "BPR", "BPG", "BPE"
};
static const char language_list[] = "JEFDSI";

static bool getIconOffsets(tGBAHeader *header) {
	uint32_t gamecode = GET32(header->gamecode, 0);

	handler.game = -1;
	handler.language = -1;
	// Check only the first 3 letters of gamecode
	for (int i = 0; i < ARRAY_LENGTH(gamecode_list); i++) {
		if ((gamecode & 0xFFFFFF) == ((uint32_t**) gamecode_list)[i][0]) {
			handler.game = i;
			break;
		}
	}
	for (int i = 0; i < ARRAY_LENGTH(language_list); i++) {
		if ((gamecode >> 24) == language_list[i]) {
			handler.language = i;
			break;
		}
	}
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
				handler.wallpaperTable = table->wallpaperTable;
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

uint32_t readRomWord(void *address) {
	uint32_t out = 0;
	if (handler.assetSource == ASSET_SOURCE_CART) {
		out = GET32(address, 0);
	} else if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
		fseek(handler.fp, (uint32_t) address & ROM_OFFSET_MASK, SEEK_SET);
		fread(&out, 4, 1, handler.fp);
	}
	return out;
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

int loadWallpaper(int index) {
	uint32_t tiles, tilemap, pal;
	if (handler.assetSource == ASSET_SOURCE_NONE)
		return 0;
	if (IS_RUBY_SAPPHIRE) {
		// Ruby and Sapphire have 4 entries, with the second one being unused
		tiles = readRomWord(handler.wallpaperTable + index * 4);
		tilemap = readRomWord(handler.wallpaperTable + index * 4 + 2);
		// Unlike FRLG/E, RS have an all blank palette here first that we skip.
		pal = readRomWord(handler.wallpaperTable + index * 4 + 3) + 32;
	} else {
		tiles = readRomWord(handler.wallpaperTable + index * 3);
		tilemap = readRomWord(handler.wallpaperTable + index * 3 + 1);
		pal = readRomWord(handler.wallpaperTable + index * 3 + 2);
	}
	if ((readRomWord((void*) tiles) >> 8) > sizeof(wallpaperTiles))
		return 0;
	if ((readRomWord((void*) tilemap) >> 8) > sizeof(wallpaperTilemap))
		return 0;

	// Tiles and tilemap are LZ77 compressed, but palette isn't
	if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
		// Read the compressed tile data
		fseek(handler.fp, tiles & ROM_OFFSET_MASK, SEEK_SET);
		fread(tileGfxCompressed, 1, sizeof(tileGfxCompressed), handler.fp);
		swiDecompressLZSSWram(tileGfxCompressed, wallpaperTiles);

		// Read the compressed tile map data
		fseek(handler.fp, tilemap & ROM_OFFSET_MASK, SEEK_SET);
		fread(tileGfxCompressed, 1, sizeof(tileGfxCompressed), handler.fp);
		swiDecompressLZSSWram(tileGfxCompressed, wallpaperTilemap);

		// Read the palette data
		fseek(handler.fp, pal & ROM_OFFSET_MASK, SEEK_SET);
		fread(wallpaperPal, 1, sizeof(wallpaperPal), handler.fp);
	} else {
		swiDecompressLZSSWram((void*) tiles, wallpaperTiles);
		swiDecompressLZSSWram((void*) tilemap, wallpaperTilemap);
		memcpy(wallpaperPal, (void*) pal, sizeof(wallpaperPal));
	}
	return 1;
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
		fread(tileGfxCompressed, 1, sizeof(tileGfxCompressed), fp);
		fseek(fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
		fread(palCompressed, 1, sizeof(palCompressed), fp);
		tileAddress = tileGfxCompressed;
		palAddress = palCompressed;
	}

	// Avoid buffer overflows by checking the extracted size
	if (tileAddress == NULL || (GET32(tileAddress, 0) >> 8) > 8192)
		memcpy(tiles_out, unknownFrontTiles, 2048);
	else
		swiDecompressLZSSWram(tileAddress, tiles_out);

	if (palAddress == NULL || (GET32(palAddress, 0) >> 8) > 64)
		memcpy(palette_out, unknownFrontPal, 32);
	else
		swiDecompressLZSSWram(palAddress, palette_out);
}
