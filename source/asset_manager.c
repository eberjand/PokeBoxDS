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

uint8_t wallpaperTiles[0x1000];
uint16_t wallpaperTilemap[0x2d0];
uint16_t wallpaperPal[16 * 4];
const char *activeGameName;
const char *activeGameNameShort;
int activeGameId;
int activeGameLanguage;
uint8_t activeGameGen;
uint8_t activeGameSubGen;

static uint16_t tileGfxCompressed[4096];

typedef struct assets_handler {
	int assetSource;
	char *file;

	uint16_t **iconImageTable;
	uint8_t *iconPaletteIndices;
	uint16_t **iconPaletteTable;
	uint16_t **frontSpriteTable;
	uint16_t **frontPaletteTable;
	uint16_t **shinyPaletteTable;
	uint16_t **wallpaperTable;
	uint8_t *baseStatTable;
	int game;
	int language;
	FILE *fp;
	uint8_t buffer[1024];
	uint8_t palettesData[3 * 32];
} assets_handler_t;
static assets_handler_t handler;


struct rom_offsets_t {
	char *gamecode;
	int rev;
	void *iconTable;
	void *frontSpriteTable;
	void *wallpaperTable;
	void *baseStatTable;
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
	{"AXVJ", 0, (void*) 0x8391a98, (void*) 0x81bcb60, (void*) 0x8390e00, (void*) 0x81d09cc},
	{"AXVE", 0, (void*) 0x83bbd20, (void*) 0x81e8354, (void*) 0x83bb0e8, (void*) 0x81fec18},
	{"AXVE", 1, (void*) 0x83bbd3c, (void*) 0x81e836c, (void*) 0x83bb104, (void*) 0x81fec30},
	{"AXVE", 2, (void*) 0x83bbd3c, (void*) 0x81e836c, (void*) 0x83bb104, (void*) 0x81fec30},
	{"AXVF", 0, (void*) 0x83c3704, (void*) 0x81f075c, (void*) 0x83c2acc, (void*) 0x8207064},
	{"AXVF", 1, (void*) 0x83c3704, (void*) 0x81f075c, (void*) 0x83c2acc, (void*) 0x8207064},
	{"AXVD", 0, (void*) 0x83c7c30, (void*) 0x81f52d0, (void*) 0x83c6ff8, (void*) 0x820bbe8},
	{"AXVD", 1, (void*) 0x83c7c30, (void*) 0x81f52d0, (void*) 0x83c6ff8, (void*) 0x820bbe8},
	{"AXVS", 0, (void*) 0x83bfd84, (void*) 0x81ed074, (void*) 0x83bf14c, (void*) 0x8203994},
	{"AXVS", 1, (void*) 0x83bfd84, (void*) 0x81ed074, (void*) 0x83bf14c, (void*) 0x8203994},
	{"AXVI", 0, (void*) 0x83bc974, (void*) 0x81e9ff0, (void*) 0x83bbd3c, (void*) 0x82008f0},
	{"AXVI", 1, (void*) 0x83bc974, (void*) 0x81e9ff0, (void*) 0x83bbd3c, (void*) 0x82008f0},

	// Sapphire
	{"AXPJ", 0, (void*) 0x8391a7c, (void*) 0x81bcaf0, (void*) 0x8390de4, (void*) 0x81d095c},
	{"AXPE", 0, (void*) 0x83bbd78, (void*) 0x81e82e4, (void*) 0x83bb140, (void*) 0x81feba8},
	{"AXPE", 1, (void*) 0x83bbd98, (void*) 0x81e82fc, (void*) 0x83bb160, (void*) 0x81febc0},
	{"AXPE", 2, (void*) 0x83bbd98, (void*) 0x81e82fc, (void*) 0x83bb160, (void*) 0x81febc0},
	{"AXPF", 0, (void*) 0x83c3234, (void*) 0x81f06ec, (void*) 0x83c25fc, (void*) 0x8206ff4},
	{"AXPF", 1, (void*) 0x83c3234, (void*) 0x81f06ec, (void*) 0x83c25fc, (void*) 0x8206ff4},
	{"AXPD", 0, (void*) 0x83c7b9c, (void*) 0x81f5264, (void*) 0x83c6f64, (void*) 0x820bb7c},
	{"AXPD", 1, (void*) 0x83c7b9c, (void*) 0x81f5264, (void*) 0x83c6f64, (void*) 0x820bb7c},
	{"AXPS", 0, (void*) 0x83bfac0, (void*) 0x81ed004, (void*) 0x83bee88, (void*) 0x8203924},
	{"AXPS", 1, (void*) 0x83bfac0, (void*) 0x81ed004, (void*) 0x83bee88, (void*) 0x8203924},
	{"AXPI", 0, (void*) 0x83bc618, (void*) 0x81e9f80, (void*) 0x83bb9e0, (void*) 0x8200880},
	{"AXPI", 1, (void*) 0x83bc618, (void*) 0x81e9f80, (void*) 0x83bb9e0, (void*) 0x8200880},

	// FireRed
	{"BPRJ", 0, (void*) 0x839bca8, (void*) 0x81f4690, (void*) 0x839af18, (void*) 0x821118c},
	{"BPRE", 0, (void*) 0x83d37a0, (void*) 0x82350ac, (void*) 0x83d2a10, (void*) 0x8254784},
	{"BPRE", 1, (void*) 0x83d3810, (void*) 0x823511c, (void*) 0x83d2a80, (void*) 0x82547f4},
	{"BPRF", 0, (void*) 0x83cd5e0, (void*) 0x822f4b8, (void*) 0x83cc850, (void*) 0x824ebd4},
	{"BPRD", 0, (void*) 0x83d30b4, (void*) 0x8234f7c, (void*) 0x83d2324, (void*) 0x82546a8},
	{"BPRS", 0, (void*) 0x83ce958, (void*) 0x8230818, (void*) 0x83cdbc8, (void*) 0x824ff4c},
	{"BPRI", 0, (void*) 0x83cc270, (void*) 0x822e150, (void*) 0x83cb4e0, (void*) 0x824d864},

	// LeafGreen
	{"BPGJ", 0, (void*) 0x839bb18, (void*) 0x81f466c, (void*) 0x839ad88, (void*) 0x8211168},
	{"BPGE", 0, (void*) 0x83d35dc, (void*) 0x8235088, (void*) 0x83d284c, (void*) 0x8254760},
	{"BPGE", 1, (void*) 0x83d364c, (void*) 0x82350f8, (void*) 0x83d28bc, (void*) 0x82547d0},
	{"BPGF", 0, (void*) 0x83cd41c, (void*) 0x822f494, (void*) 0x83cc68c, (void*) 0x824ebb0},
	{"BPGD", 0, (void*) 0x83d2ef0, (void*) 0x8234f58, (void*) 0x83d2160, (void*) 0x8254684},
	{"BPGS", 0, (void*) 0x83ce794, (void*) 0x82307f4, (void*) 0x83cda04, (void*) 0x824ff28},
	{"BPGI", 0, (void*) 0x83cc0ac, (void*) 0x822e12c, (void*) 0x83cb31c, (void*) 0x824d840},

	// Emerald
	{"BPEJ", 0, (void*) 0x8556804, (void*) 0x82d4ca8, (void*) 0x8551868, (void*) 0x82f0d54},
	{"BPEE", 0, (void*) 0x857bca8, (void*) 0x8301418, (void*) 0x85775b8, (void*) 0x83203cc},
	{"BPEF", 0, (void*) 0x8580020, (void*) 0x8303f48, (void*) 0x857b930, (void*) 0x8327f3c},
	{"BPED", 0, (void*) 0x858caa8, (void*) 0x8315d88, (void*) 0x85883b8, (void*) 0x8334d8c},
	{"BPES", 0, (void*) 0x857e784, (void*) 0x830767c, (void*) 0x857a094, (void*) 0x8326688},
	{"BPEI", 0, (void*) 0x857838c, (void*) 0x8300ddc, (void*) 0x8573c9c, (void*) 0x831fdcc},
};
static const struct {
	char *gamecode;
	int gameId;
	const char* nameShort;
	const char* nameLong;
} game_names[] = {
	{"AXVJ", 0, "Ruby (JP)", "Pocket Monsters Ruby (Japanese)"},
	{"AXVE", 0, "Ruby (EN)", "Pokemon Ruby Version (English)"},
	{"AXVF", 0, "Ruby (FR)", "Pokemon Version Rubis (French)"},
	{"AXVD", 0, "Ruby (DE)", "Pokemon Rubin-Edition (German)"},
	{"AXVS", 0, "Ruby (ES)", "Pokemon Edicion Rubi (Spanish)"},
	{"AXVI", 0, "Ruby (IT)", "Pokemon Versione Rubino (Italian)"},
	{"AXPJ", 1, "Sapphire (JP)", "Pocket Monsters Sapphire (Japanese)"},
	{"AXPE", 1, "Sapphire (EN)", "Pokemon Sapphire Version (English)"},
	{"AXPF", 1, "Sapphire (FR)", "Pokemon Version Saphir (French)"},
	{"AXPD", 1, "Sapphire (DE)", "Pokemon Saphir-Edition (German)"},
	{"AXPS", 1, "Sapphire (ES)", "Pokemon Edicion Zafiro (Spanish)"},
	{"AXPI", 1, "Sapphire (TI)", "Pokemon Versione Zaffiro (Italian)"},
	{"BPRJ", 2, "FireRed (JP)", "Pocket Monsters FireRed (Japanese)"},
	{"BPRE", 2, "FireRed (EN)", "Pokemon FireRed Version (English)"},
	{"BPRF", 2, "FireRed (FR)", "Pokemon Version Rouge Feu (French)"},
	{"BPRD", 2, "FireRed (DE)", "Pokemon Feuerrote Edition (German)"},
	{"BPRS", 2, "FireRed (ES)", "Pokemon Edicion Rojo Fuego (Spanish)"},
	{"BPRI", 2, "FireRed (IT)", "Pokemon Versione Rosso Fuoco (Italian)"},
	{"BPGJ", 3, "LeafGreen (JP)", "Pocket Monsters LeafGreen (Japanese)"},
	{"BPGE", 3, "LeafGreen (EN)", "Pokemon LeafGreen Version (English)"},
	{"BPGF", 3, "LeafGreen (FR)", "Pokemon Version Vert Feuille (French)"},
	{"BPGD", 3, "LeafGreen (DE)", "Pokemon Blattgrune Edition (German)"},
	{"BPGS", 3, "LeafGreen (ES)", "Pokemon Edicion Verde Hoja (Spanish)"},
	{"BPGI", 3, "LeafGreen (IT)", "Pokemon Versione Verde Foglia (Italian)"},
	{"BPEJ", 4, "Emerald (JP)", "Pocket Monsters Emerald (Japanese)"},
	{"BPEE", 4, "Emerald (EN)", "Pokemon Emerald Version (English)"},
	{"BPEF", 4, "Emerald (FR)", "Pokemon Version Emeraude (French)"},
	{"BPED", 4, "Emerald (DE)", "Pokemon Smaragd-Edition (German)"},
	{"BPES", 4, "Emerald (ES)", "Pokemon Edicion Esmeralda (Spanish)"},
	{"BPEI", 4, "Emerald (IT)", "Pokemon Versione Smeraldo (Italian)"}
};
static const struct {
	char c;
	int lang;
} language_codes[] = {
	{'J', LANG_JAPANESE},
	{'E', LANG_ENGLISH},
	{'F', LANG_FRENCH},
	{'D', LANG_GERMAN},
	{'S', LANG_SPANISH},
	{'I', LANG_ITALIAN}
};

static bool initFromHeader(tGBAHeader *header) {
	uint32_t gamecode = GET32(header->gamecode, 0);
	int has_offsets = 0;
	int has_name = 0;
	int has_language = 0;

	activeGameGen = 3;

	// Determine the game name
	for (int i = 0; i < ARRAY_LENGTH(game_names); i++) {
		if (gamecode == GET32(game_names[i].gamecode, 0)) {
			activeGameId = game_names[i].gameId;
			activeGameSubGen = (uint8_t) activeGameId;
			activeGameName = game_names[i].nameLong;
			activeGameNameShort = game_names[i].nameShort;
			has_name = 1;
			break;
		}
	}

	// Determine the game's language
	for (int i = 0; i < ARRAY_LENGTH(language_codes); i++) {
		if ((gamecode >> 24) == language_codes[i].c) {
			activeGameLanguage = language_codes[i].lang;
			has_language = 1;
			break;
		}
	}

	// Get all rom offsets for this game
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
				handler.baseStatTable = table->baseStatTable;
				has_offsets = true;
				break;
			}
		}
	}
	return has_name && has_language && has_offsets;
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
	if (!initFromHeader(&GBA_HEADER))
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

	if (!initFromHeader(&header)) {
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
	activeGameName = "Unknown";
	activeGameNameShort = "Unknown";
	activeGameId = -1;
	activeGameLanguage = -1;
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

const struct BaseStatEntryGen3* getBaseStatEntry(uint16_t species) {
	static struct BaseStatEntryGen3 statEntry;
	uint8_t* tableOffset;
	tableOffset = handler.baseStatTable + species * sizeof(struct BaseStatEntryGen3);
	if (handler.assetSource == ASSET_SOURCE_CART) {
		return (const struct BaseStatEntryGen3*) tableOffset;
	} else {
		fseek(handler.fp, (long) tableOffset & ROM_OFFSET_MASK, SEEK_SET);
		fread(&statEntry, sizeof(statEntry), 1, handler.fp);
		return &statEntry;
	}
}
