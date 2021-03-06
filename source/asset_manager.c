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

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <nds.h>

#include "lz77.h"
#include "message_window.h"
#include "pokemon_strings.h"
#include "util.h"

#include "unknownFront.h"
#include "unknownIcon.h"

#define ASSET_SOURCE_NONE 0
#define ASSET_SOURCE_CART 1
#define ASSET_SOURCE_ROMFILE 2

#define ROM_OFFSET_MASK 0xFFFFFF

#define SPECIES_CASTFORM 385
#define SPECIES_DEOXYS 410

uint8_t wallpaperTiles[0x1000];
uint16_t wallpaperTilemap[0x2d0];
uint16_t wallpaperPal[16 * 4];
const char *activeGameName;
const char *activeGameNameShort;
int activeGameId;
int activeGameLanguage;
uint8_t activeGameGen;
uint8_t activeGameSubGen;

// Each sprite is 2048 bytes
// Need to allocate enough space for 4 sprites because of Castform
static uint8_t tileGfxUncompressed[8192];
static uint32_t tileGfxCompressed[2048];

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
	uint16_t **itemIconTable;
	uint8_t *baseStatTable;
	int game;
	int language;
	FILE *fp;
	FILE *iconFile;
	FILE *baseStatFile;
	FILE *frontSpriteFiles[2];
	FILE *itemIconFile;
	uint8_t buffer[1024];
	uint8_t palettesData[6 * 32];
	uint8_t iconPaletteIndicesSD[440];
} assets_handler_t;
static assets_handler_t handler;

/* Format of dump files:
 *   Extended Header:
 *   24B header
 *   if item_size == 0:
 *     u32 offsets[item_num]
 *   if flags.SHARED_PALETTES:
 *     u32 num_pals
 *     u8 palettes[num_pals][32]
 *     u8 pal_indices[item_num]
 *   if flags.IS_STRING:
 *     u32 lang_offsets[7]
 *
 *   Each element following the header (repeat x item_num):
 *     if item_size == 0:
 *       u32 entry_meta (size, num_pals, num_sprites, is_compressed)
 *     if flags.IS_SPRITE && !flags.SHARED_PALETTES:
 *       u8 palettes[num_pals][32]
 *     u8 data[size or item_size]
 */

struct dump_file_header {
	char magic[8]; // PKMBDUMP
	uint16_t version;
	uint8_t asset_group;
	uint8_t generation;
	uint16_t subgen_mask;
	uint8_t flags;
	uint8_t unused_1;
	uint16_t item_num;
	uint16_t item_size;
	uint32_t unused_2; // maybe this'll hold CRC32 for romhacks
};

union dump_entry_meta {
	uint32_t value;
	struct {
		uint16_t size;
		uint8_t num_pals;
		uint8_t num_sprites : 7;
		uint8_t is_compressed : 1;
	};
};

/* The only differences in the base stat table from RSE to FRLG are:
 *
 * Some held items are completely different. FRLG added held items to some
 * that previously had none, removed some existing held items, and changed
 * some existing held items to different items. We unify this by putting the
 * RSE items in entry.heldItem and FRLG items in heldItemFRLG.
 *
 * Some safariFleeRate data is changed. FRLG added flee rate data to some
 * that previously had none and removed flee rate data from others, but did not
 * change any nonzero values to other nonzero values. Everything with a zero
 * safariFleeRate is not obtainable in that game's Safari Zone. We probably
 * won't ever have a use for this data, but we unify it by putting FRLG flee
 * data in entry.padding[0]
 */
struct BaseStatEntryUnifiedGen3 {
	struct BaseStatEntryGen3 entry;
	union {
		uint16_t heldItemFRLG[2];
		uint32_t heldItemsFRLG;
	};
};

/* To preserve compatibility, these numbers must not change */
enum AssetGroup {
	ASSETS_BOXICONS = 0,
	ASSETS_FRONTSPRITE = 1,
	ASSETS_WALLPAPERS = 3,
	ASSETS_BASESTATS = 4,
	ASSETS_ITEMICONS = 5
	/* Possible future assets:
	 *   Back sprites
	 *   Trainer sprites
	 *   Music
	 *   Pokemon cries
	 *   Encounter tables
	 *   Move learnsets
	 *   Move attributes
	 *   Pokedex entries
	 *   Pokemon names
	 *   Move names
	 *   Item names
	 *   Item descriptions
	 *   Move descriptions
	 *   Location names
	 */
};

/* Some of these flags have relationships with each other:
 *
 * - Mutually exclusive: FLAG_IS_SPRITE, FLAG_IS_STRING, FLAG_IS_AUDIO
 * - Only valid if FLAG_IS_SPRITE: FLAG_SHARED_PALETTES or FLAG_HAS_TILEMAP
 */
enum AssetFlags {
	/* This dump file contains sprites or background graphics */
	FLAG_IS_SPRITE       = 0x0001,
	/* This dump file contains text strings */
	FLAG_IS_STRING       = 0x0002,
	/* This dump file contains audio data */
	FLAG_IS_AUDIO        = 0x0004,
	/* All sprites share the same set of palettes instead of having their own copies */
	FLAG_SHARED_PALETTES = 0x0008,
	/* WIP For stuff like box wallpapers */
	FLAG_HAS_TILEMAP     = 0x0010
};

struct rom_offsets_t {
	char *gamecode;
	int rev;
	uintptr_t iconTable;
	uintptr_t frontSpriteTable;
	uintptr_t wallpaperTable;
	uintptr_t baseStatTable;
	uintptr_t itemIconTable;
};

/* I found most of these offsets by figuring out what data I'm interested in
 * for one ROM, following a pointer from the table to reach data that doesn't
 * include a pointer (not affected by offsets), then searching for a few bytes
 * of that data in every other ROM and then searching for references to that
 * address to find that ROM's table.
 * It's a bit harder to find data that varies between versions; for example,
 * searching for the RSE front sprite data in FRLG won't yield any results.
 * The highest-order "08" byte in all of these points to the GBA cart's memory
 * mapped area. In a dump, an offset like "0x8391a98" is actually at "0x391a98"
 */
static const struct rom_offsets_t rom_offsets[] = {
	// Ruby
	{"AXVJ", 0, 0x8391a98, 0x81bcb60, 0x8390e00, 0x81d09cc, 0},
	{"AXVE", 0, 0x83bbd20, 0x81e8354, 0x83bb0e8, 0x81fec18, 0},
	{"AXVE", 1, 0x83bbd3c, 0x81e836c, 0x83bb104, 0x81fec30, 0},
	{"AXVE", 2, 0x83bbd3c, 0x81e836c, 0x83bb104, 0x81fec30, 0},
	{"AXVF", 0, 0x83c3704, 0x81f075c, 0x83c2acc, 0x8207064, 0},
	{"AXVF", 1, 0x83c3704, 0x81f075c, 0x83c2acc, 0x8207064, 0},
	{"AXVD", 0, 0x83c7c30, 0x81f52d0, 0x83c6ff8, 0x820bbe8, 0},
	{"AXVD", 1, 0x83c7c30, 0x81f52d0, 0x83c6ff8, 0x820bbe8, 0},
	{"AXVS", 0, 0x83bfd84, 0x81ed074, 0x83bf14c, 0x8203994, 0},
	{"AXVS", 1, 0x83bfd84, 0x81ed074, 0x83bf14c, 0x8203994, 0},
	{"AXVI", 0, 0x83bc974, 0x81e9ff0, 0x83bbd3c, 0x82008f0, 0},
	{"AXVI", 1, 0x83bc974, 0x81e9ff0, 0x83bbd3c, 0x82008f0, 0},

	// Sapphire
	{"AXPJ", 0, 0x8391a7c, 0x81bcaf0, 0x8390de4, 0x81d095c, 0},
	{"AXPE", 0, 0x83bbd78, 0x81e82e4, 0x83bb140, 0x81feba8, 0},
	{"AXPE", 1, 0x83bbd98, 0x81e82fc, 0x83bb160, 0x81febc0, 0},
	{"AXPE", 2, 0x83bbd98, 0x81e82fc, 0x83bb160, 0x81febc0, 0},
	{"AXPF", 0, 0x83c3234, 0x81f06ec, 0x83c25fc, 0x8206ff4, 0},
	{"AXPF", 1, 0x83c3234, 0x81f06ec, 0x83c25fc, 0x8206ff4, 0},
	{"AXPD", 0, 0x83c7b9c, 0x81f5264, 0x83c6f64, 0x820bb7c, 0},
	{"AXPD", 1, 0x83c7b9c, 0x81f5264, 0x83c6f64, 0x820bb7c, 0},
	{"AXPS", 0, 0x83bfac0, 0x81ed004, 0x83bee88, 0x8203924, 0},
	{"AXPS", 1, 0x83bfac0, 0x81ed004, 0x83bee88, 0x8203924, 0},
	{"AXPI", 0, 0x83bc618, 0x81e9f80, 0x83bb9e0, 0x8200880, 0},
	{"AXPI", 1, 0x83bc618, 0x81e9f80, 0x83bb9e0, 0x8200880, 0},

	// FireRed
	{"BPRJ", 0, 0x839bca8, 0x81f4690, 0x839af18, 0x821118c, 0x839c79c},
	{"BPRE", 0, 0x83d37a0, 0x82350ac, 0x83d2a10, 0x8254784, 0x83d4294},
	{"BPRE", 1, 0x83d3810, 0x823511c, 0x83d2a80, 0x82547f4, 0x83d4304},
	{"BPRF", 0, 0x83cd5e0, 0x822f4b8, 0x83cc850, 0x824ebd4, 0x83ce114},
	{"BPRD", 0, 0x83d30b4, 0x8234f7c, 0x83d2324, 0x82546a8, 0x83d3be8},
	{"BPRS", 0, 0x83ce958, 0x8230818, 0x83cdbc8, 0x824ff4c, 0x83cf48c},
	{"BPRI", 0, 0x83cc270, 0x822e150, 0x83cb4e0, 0x824d864, 0x83ccda4},

	// LeafGreen
	{"BPGJ", 0, 0x839bb18, 0x81f466c, 0x839ad88, 0x8211168, 0x839c60c},
	{"BPGE", 0, 0x83d35dc, 0x8235088, 0x83d284c, 0x8254760, 0x83d40d0},
	{"BPGE", 1, 0x83d364c, 0x82350f8, 0x83d28bc, 0x82547d0, 0x83d4140},
	{"BPGF", 0, 0x83cd41c, 0x822f494, 0x83cc68c, 0x824ebb0, 0x83cdf50},
	{"BPGD", 0, 0x83d2ef0, 0x8234f58, 0x83d2160, 0x8254684, 0x83d3a24},
	{"BPGS", 0, 0x83ce794, 0x82307f4, 0x83cda04, 0x824ff28, 0x83cf2c8},
	{"BPGI", 0, 0x83cc0ac, 0x822e12c, 0x83cb31c, 0x824d840, 0x83ccbe0},

	// Emerald
	{"BPEJ", 0, 0x8556804, 0x82d4ca8, 0x8551868, 0x82f0d54, 0x85dfcc8},
	{"BPEE", 0, 0x857bca8, 0x8301418, 0x85775b8, 0x83203cc, 0x8614410},
	{"BPEF", 0, 0x8580020, 0x8308f48, 0x857b930, 0x8327f3c, 0x8618798},
	{"BPED", 0, 0x858caa8, 0x8315d88, 0x85883b8, 0x8334d8c, 0x86258d8},
	{"BPES", 0, 0x857e784, 0x830767c, 0x857a094, 0x8326688, 0x8617250},
	{"BPEI", 0, 0x857838c, 0x8300ddc, 0x8573c9c, 0x831fdcc, 0x8610fac},
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
				handler.iconImageTable = (void*) table->iconTable;
				handler.iconPaletteIndices = (void*) (table->iconTable + 0x6e0);
				handler.iconPaletteTable = (void*) (table->iconTable + 0x898);
				handler.frontSpriteTable = (void*) table->frontSpriteTable;
				handler.frontPaletteTable = (void*) (table->frontSpriteTable + 0x2260);
				handler.shinyPaletteTable = (void*) (table->frontSpriteTable + 0x3020);
				handler.wallpaperTable = (void*) table->wallpaperTable;
				handler.baseStatTable = (void*) table->baseStatTable;
				handler.itemIconTable = (void*) table->itemIconTable;
				has_offsets = true;
				break;
			}
		}
	}
	return has_name && has_language && has_offsets;
}

void assets_init() {
	char fname[44];
	struct dump_file_header header;
	FILE *fp;
	int error;

	handler.iconFile = fp = fopen("/pokebox/assets/boxicons03.bin", "rb");
	if (fp) {
		fread(&header, sizeof(header), 1, fp);
		error =
			memcmp(header.magic, "PKMBDUMP", 8) ||
			header.version != 0 ||
			header.asset_group != ASSETS_BOXICONS ||
			header.generation != 3 ||
			header.item_num != 440 ||
			header.item_size != 1024;
		if (error) {
			fclose(fp);
			handler.iconFile = NULL;
		}
	}

	handler.itemIconFile = fp = fopen("/pokebox/assets/items03.bin", "rb");
	if (fp) {
		fread(&header, sizeof(header), 1, fp);
		error =
			memcmp(header.magic, "PKMBDUMP", 8) ||
			header.version != 0 ||
			header.asset_group != ASSETS_ITEMICONS ||
			header.generation != 3;
		if (error) {
			fclose(fp);
			handler.itemIconFile = NULL;
		}
	}

	handler.baseStatFile = fp = fopen("/pokebox/assets/basestats03.bin", "rb");
	if (fp) {
		fread(&header, sizeof(header), 1, fp);
		error =
			memcmp(header.magic, "PKMBDUMP", 8) ||
			header.version != 0 ||
			header.asset_group != ASSETS_BASESTATS ||
			header.generation != 3 ||
			header.item_num != 440 ||
			header.item_size != sizeof(struct BaseStatEntryUnifiedGen3);
		if (error) {
			fclose(fp);
			handler.baseStatFile = NULL;
		}
	}

	for (int i = 0; i < 2; i++) {
		snprintf(fname, sizeof(fname), "/pokebox/assets/frontsprites03%02d.bin", i);
		handler.frontSpriteFiles[i] = fopen(fname, "rb");
	}

	if (handler.iconFile) {
		fseek(handler.iconFile, 24 + 4, SEEK_SET);
		fread(handler.palettesData, 1, 32 * 3, handler.iconFile);
		fread(handler.iconPaletteIndicesSD, 1, 440, handler.iconFile);
	}
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
	memcpy(handler.palettesData + 3 * 32, handler.iconPaletteTable[0], 3 * 32);
	dump_assets_to_sd(false);
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

	indicesCopy = malloc(440);
	fseek(handler.fp, (long) handler.iconPaletteIndices & ROM_OFFSET_MASK, SEEK_SET);
	fread(indicesCopy, 1, 440, handler.fp);
	handler.iconPaletteIndices = indicesCopy;

	uint16_t *palAddress;
	fseek(handler.fp, (long) (handler.iconPaletteTable) & ROM_OFFSET_MASK, SEEK_SET);
	fread(&palAddress, 1, 4, handler.fp);
	fseek(handler.fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
	fread(handler.palettesData + 32 * 3, 1, 32 * 3, handler.fp);

	dump_assets_to_sd(false);
	return true;
}

void assets_free() {
	if (handler.fp)
		fclose(handler.fp);
	if ((uint16_t*) handler.iconPaletteIndices < GBAROM)
		free(handler.iconPaletteIndices);
	//memset(&handler, 0, sizeof(handler));
	activeGameName = "Unknown";
	activeGameNameShort = "Unknown";
	activeGameId = -1;
	activeGameLanguage = -1;
}

int read_romfile_gameid(const char *file) {
	FILE *fp;
	int gameid = -1;
	uint32_t gamecode;
	tGBAHeader header;

	fp = fopen(file, "rb");
	if (fp == NULL)
		return -1;

	fread(&header, sizeof(header), 1, fp);
	fclose(fp);
	gamecode = GET32(header.gamecode, 0);

	for (int i = 0; i < ARRAY_LENGTH(game_names); i++) {
		if (gamecode == GET32(game_names[i].gamecode, 0)) {
			gameid = game_names[i].gameId;
			break;
		}
	}
	return gameid;
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
	uint8_t gen;

	gen = species >> 12;
	species &= 0xFFF;

	if (gen != 0) {
		if (!handler.iconFile)
			return (const uint16_t*) unknownIconTiles;
		fseek(handler.iconFile, 24 + 4 + 32 * 3 + 440 + species * 1024, SEEK_SET);
		fread(handler.buffer, 2, 512, handler.iconFile);
		return (const uint16_t*) handler.buffer;
	}

	if (handler.assetSource == ASSET_SOURCE_NONE)
		return (const uint16_t*) unknownIconTiles;
	if (handler.assetSource == ASSET_SOURCE_CART)
		return handler.iconImageTable[species];

	// Buffering this table would require 440 species * 4B = 1760 bytes
	uint16_t* imageAddress = NULL;
	fseek(handler.fp, (long) (handler.iconImageTable + species) & ROM_OFFSET_MASK, SEEK_SET);
	fread(&imageAddress, 4, 1, handler.fp);
	// Buffering every image would require about 1MB
	fseek(handler.fp, (long) imageAddress & ROM_OFFSET_MASK, SEEK_SET);
	fread(handler.buffer, 2, 512, handler.fp);
	return (const uint16_t*) handler.buffer;
}

uint8_t getIconPaletteIdx(uint16_t species) {
	if (species >> 12 != 0)
		return handler.iconPaletteIndicesSD[species & 0xFFF];
	if (handler.iconPaletteIndices)
		return handler.iconPaletteIndices[species] + 3;
	return 0;
}

const uint16_t* getIconPaletteColors(int index) {
	if (index < 6) {
		return (const uint16_t*) (handler.palettesData + index * 32);
	}
	return unknownIconPal;
}

bool loadItemIcon(uint8_t *tiles_out, uint8_t *palette_out, uint16_t item_idx) {
	uint32_t offset;
	union dump_entry_meta meta;

	if (!item_idx || item_idx > 376)
		return false;
	if (!handler.itemIconFile)
		return false;

	fseek(handler.itemIconFile, sizeof(struct dump_file_header) + 4 * item_idx, SEEK_SET);
	fread(&offset, sizeof(offset), 1, handler.itemIconFile);
	fseek(handler.itemIconFile, offset, SEEK_SET);
	fread(&meta, sizeof(meta), 1, handler.itemIconFile);
	if (meta.num_pals > 1) {
		fseek(handler.itemIconFile, gen3_tmhm_type(item_idx) * 32, SEEK_CUR);
		fread(palette_out, 2, 16, handler.itemIconFile);
		fseek(handler.itemIconFile, offset + sizeof(meta) + 32 * meta.num_pals, SEEK_SET);
	} else {
		fread(palette_out, 2, 16, handler.itemIconFile);
	}
	// Expand the sprite from 24x24 to 32x32
	fread(tiles_out        , 1, 0x60, handler.itemIconFile);
	memset(tiles_out + 0x60, 0, 0x20);
	fread(tiles_out +  0x80, 1, 0x60, handler.itemIconFile);
	memset(tiles_out + 0xE0, 0, 0x20);
	fread(tiles_out + 0x100, 1, 0x60, handler.itemIconFile);
	memset(tiles_out + 0x160, 0, 0xA0);
	return true;
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
		lz77_extract(wallpaperTiles, tileGfxCompressed, sizeof(wallpaperTiles));

		// Read the compressed tile map data
		fseek(handler.fp, tilemap & ROM_OFFSET_MASK, SEEK_SET);
		fread(tileGfxCompressed, 1, sizeof(tileGfxCompressed), handler.fp);
		lz77_extract(wallpaperTilemap, tileGfxCompressed, sizeof(wallpaperTilemap));

		// Read the palette data
		fseek(handler.fp, pal & ROM_OFFSET_MASK, SEEK_SET);
		fread(wallpaperPal, 1, sizeof(wallpaperPal), handler.fp);
	} else {
		lz77_extract(wallpaperTiles, (void*) tiles, sizeof(wallpaperTiles));
		lz77_extract(wallpaperTilemap, (void*) tilemap, sizeof(wallpaperTilemap));
		memcpy(wallpaperPal, (void*) pal, sizeof(wallpaperPal));
	}
	return 1;
}

#define ITEM_TM01 0x121
#define ITEM_HM01 0x153

static void dumpItemIconsRange(FILE *fout, uint16_t idx_start, uint16_t idx_end,
	uint32_t *offsetTable) {
	void *unknownItemAddress = NULL;
	uint16_t **itemIconTable = NULL;
	uint32_t cur_offset;
	FILE *fp = handler.fp;
	uint32_t tmhm_types = 0;
	uint16_t *tmhmPalettes;

	cur_offset = ftell(fout);

	if (handler.assetSource == ASSET_SOURCE_CART) {
		itemIconTable = handler.itemIconTable;
	} else {
		itemIconTable = malloc((idx_end - idx_start) * 8);
		fseek(fp, (long) handler.itemIconTable & ROM_OFFSET_MASK, SEEK_SET);
		fread(itemIconTable, 8, idx_end - idx_start, fp);
	}

	tmhmPalettes = calloc(sizeof(uint16_t), 16 * 18);
	unknownItemAddress = itemIconTable[0];
	for (uint16_t idx = idx_start; idx < idx_end; idx++) {
		void *tileAddress;
		void *palAddress;
		uint16_t palette[16];
		uint8_t palCompressed[40];
		uint32_t size;
		union dump_entry_meta meta;
		bool is_tm;
		bool is_hm;

		tileAddress = itemIconTable[idx * 2];
		palAddress = itemIconTable[idx * 2 + 1];
		if (idx != 0 && tileAddress == unknownItemAddress) {
			offsetTable[idx] = offsetTable[0];
			continue;
		}
		size = 24 * 24 / 2;

		if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
			fseek(fp, (long) tileAddress & ROM_OFFSET_MASK, SEEK_SET);
			fread(tileGfxCompressed, 1,
				MIN(size * 9 / 8 + 4, (u32) (palAddress - tileAddress)), fp);
			fseek(fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
			fread(palCompressed, 1, sizeof(palCompressed), fp);
			tileAddress = tileGfxCompressed;
			palAddress = palCompressed;
		}

		is_tm = idx >= ITEM_TM01 && itemIconTable[idx * 2] == itemIconTable[ITEM_TM01 * 2];
		is_hm = idx >= ITEM_HM01 && itemIconTable[idx * 2] == itemIconTable[ITEM_HM01 * 2];
		if (is_tm || is_hm) {
			uint8_t cur_type;
			bool can_skip = idx != ITEM_TM01 && idx != ITEM_HM01;

			offsetTable[idx] = offsetTable[is_hm ? ITEM_HM01 : ITEM_TM01];
			cur_type = gen3_tmhm_type(idx);
			if ((tmhm_types >> cur_type & 1) != 0 && can_skip)
				continue;

			lz77_extract(tmhmPalettes + 16 * cur_type, palAddress, 32);
			tmhm_types |= 1 << cur_type;
			if (can_skip) {
				continue;
			}
		}

		meta.size = size;
		meta.num_pals = 1;
		meta.num_sprites = 1;
		meta.is_compressed = 0;

		offsetTable[idx] = cur_offset;

		if (!lz77_extract(palette, palAddress, sizeof(palette))) {
			memset(palette, 0, sizeof(palette));
		}
		if (!lz77_extract(tileGfxUncompressed, tileAddress, sizeof(tileGfxUncompressed))) {
			memset(tileGfxUncompressed, 0, sizeof(tileGfxUncompressed));
		}

		if (is_tm || is_hm) {
			meta.num_pals = 18; // Number of types
			fwrite(&meta, sizeof(meta), 1, fout);
			fwrite(tmhmPalettes, 32, meta.num_pals, fout);
		} else {
			fwrite(&meta, sizeof(meta), 1, fout);
			fwrite(&palette, 1, sizeof(palette), fout);
		}
		fwrite(&tileGfxUncompressed, 1, size, fout);
		cur_offset += 4 + meta.num_pals * 32 + size;
	}
	if (tmhm_types != 0) {
		uint32_t prev_offset;
		uint32_t next;
		prev_offset = ftell(fout);
		if ((next = offsetTable[ITEM_TM01]) != 0) {
			fseek(fout, next + 4, SEEK_SET);
			fwrite(tmhmPalettes, 32, 18, fout);
		}
		if ((next = offsetTable[ITEM_HM01]) != 0) {
			fseek(fout, next + 4, SEEK_SET);
			fwrite(tmhmPalettes, 32, 18, fout);
		}
		fseek(fout, prev_offset, SEEK_SET);
	}
	if (handler.assetSource != ASSET_SOURCE_CART) {
		free(itemIconTable);
	}
	free(tmhmPalettes);
}

void* readCompressedFrontImage(uint16_t species, void **prev) {
	void *tileAddress = NULL;

	if (handler.assetSource == ASSET_SOURCE_CART) {
		// Each 8-byte item in this table is a data pointer followed by u16 size, u16 tag
		// 64x64 image needs 2048 bytes
		tileAddress = handler.frontSpriteTable[species * 2];
		if (prev) {
			if (tileAddress == *prev)
				return NULL;
			*prev = tileAddress;
		}
	} else if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
		FILE *fp = handler.fp;
		fseek(fp, (long) (handler.frontSpriteTable + species * 2) & ROM_OFFSET_MASK, SEEK_SET);
		fread(&tileAddress, 4, 1, fp);
		if (prev) {
			if (tileAddress == *prev)
				return NULL;
			*prev = tileAddress;
		}
		fseek(fp, (long) tileAddress & ROM_OFFSET_MASK, SEEK_SET);
		fread(tileGfxCompressed, 1, sizeof(tileGfxCompressed), fp);
		tileAddress = tileGfxCompressed;
	}
	return tileAddress;
}

int readFrontPalette(uint8_t *palette_out, uint16_t species, bool shiny) {
	uint16_t **paletteTable = (shiny) ? handler.shinyPaletteTable : handler.frontPaletteTable;
	void *palAddress = NULL;
	uint32_t outlen;
	u8 palCompressed[128];

	if (handler.assetSource == ASSET_SOURCE_CART) {
		// Each 8-byte item in this table is a data pointer followed by u16 tag, u16 padding
		// 16 color palette is 32 bytes
		palAddress = paletteTable[species * 2];
	} else if (handler.assetSource == ASSET_SOURCE_ROMFILE) {
		FILE *fp = handler.fp;
		fseek(fp, (long) (paletteTable + species * 2) & ROM_OFFSET_MASK, SEEK_SET);
		fread(&palAddress, 4, 1, fp);
		fseek(fp, (long) palAddress & ROM_OFFSET_MASK, SEEK_SET);
		fread(palCompressed, 1, sizeof(palCompressed), fp);
		palAddress = palCompressed;
	}

	outlen = lz77_extract(palette_out, palAddress, 128);
	if (!outlen)
		return 0;

	// Return the number of distinct 32-byte palettes
	return outlen / 32;
}

const uint8_t* readFrontImage(uint8_t *palette_out, uint16_t species,
	bool shiny, uint16_t gameid) {
	const void *tileAddress;
	int pal_res;

	if (gameid) {
		FILE *fp;
		uint32_t offset = 0;
		union dump_entry_meta meta;
		int subgen;

		gameid >>= 8;
		subgen = (gameid == GAMEID_FIRERED || gameid == GAMEID_LEAFGREEN);

		fp = handler.frontSpriteFiles[subgen];
		if (!fp) {
			memcpy(palette_out, unknownFrontPal, 32);
			return (const uint8_t*) unknownFrontTiles;
		}

		fseek(fp, 24 + 4 * species, SEEK_SET);
		fread(&offset, 4, 1, fp);
		fseek(fp, offset, SEEK_SET);
		fread(&meta, 4, 1, fp);
		if (shiny)
			fseek(fp, 32, SEEK_CUR);
		fread(palette_out, 1, 32, fp);
		if (shiny + 1 < meta.num_pals)
			fseek(fp, 32 * (meta.num_pals - 1 - shiny), SEEK_CUR);

		if (meta.is_compressed) {
			fread(tileGfxCompressed, 1, MIN(meta.size, sizeof(tileGfxCompressed)), fp);
			if (!lz77_extract(tileGfxUncompressed, tileGfxCompressed,
				sizeof(tileGfxUncompressed))) {
				memcpy(palette_out, unknownFrontPal, 32);
				return (const uint8_t*) unknownFrontTiles;
			}
		} else {
			fread(tileGfxUncompressed, 1, MIN(meta.size, sizeof(tileGfxUncompressed)), fp);
		}
		return tileGfxUncompressed;
	}

	pal_res = readFrontPalette(palette_out, species, shiny);
	tileAddress = readCompressedFrontImage(species, NULL);

	if (pal_res && lz77_extract(tileGfxUncompressed, tileAddress,
		sizeof(tileGfxUncompressed))) {
		tileAddress = tileGfxUncompressed;
	}
	else {
		memcpy(palette_out, unknownFrontPal, 32);
		tileAddress = unknownFrontTiles;
	}
	return tileAddress;
}

const struct BaseStatEntryGen3* getBaseStatEntry(uint16_t species, uint16_t gameid) {
	static struct BaseStatEntryUnifiedGen3 statEntry;
	uint8_t* tableOffset;

	if (gameid != 0) {
		FILE *fp = handler.baseStatFile;
		if (fp) {
			fseek(fp, 24 + sizeof(statEntry) * species, SEEK_SET);
			fread(&statEntry, sizeof(statEntry), 1, fp);
		} else {
			memset(&statEntry, 0, sizeof(statEntry));
		}
		return &statEntry.entry;
	}

	tableOffset = handler.baseStatTable + species * sizeof(struct BaseStatEntryGen3);

	if (handler.assetSource == ASSET_SOURCE_CART) {
		return (const struct BaseStatEntryGen3*) tableOffset;
	} else {
		fseek(handler.fp, (long) tableOffset & ROM_OFFSET_MASK, SEEK_SET);
		fread(&statEntry.entry, sizeof(statEntry.entry), 1, handler.fp);
		return &statEntry.entry;
	}
}

/* Generally, there are two sets of large sprites: one for Ruby/Sapphire/Emerald
 * and one for Firered/Leafgreen. With a few exceptions:
 * - Deoxys: In Firered, its sprite data contains the Normal and Attack forms.
 *           In Leafgreen, its sprite data contains the Normal and Defense forms.
 * - Emerald changed a few sprite palettes compared to Ruby/Sapphire:
 *           Wartortle (non-shiny only), Caterpie (both), and Deoxys (both)
 * - Emerald has invisible palette changes (changing unused colors) for:
 *           Ursaring, Manectric
 * - Emerald erroneously has very large (256px tall) sprite data with empty space for:
 *           Blaziken, Marshtomp, Poochyena, Walrein, Swablu, and Rayquaza
 *
 * We try to resolve these differences by combining together sprite forms,
 * prioritizing Emerald over Ruby/Sapphire, and truncating the oversized
 * sprites.
 *
 * Note that Jynx (#124) has different palette data between the original
 * Japanese version and all the western releases in RSE and FRLG.
 * It's not a huge difference, so we ignore it.
 *
 * Emerald oddly only has the Normal Deoxys in these dumps. I guess the alternate forms
 * are stored somewhere else in ROM, unlike in FRLG.
 */
static bool write_frlg_deoxys_sprite(FILE *fp) {
	uint32_t offset;
	union dump_entry_meta meta;
	void *tileAddress;

	fseek(fp, sizeof(struct dump_file_header) + 4 * SPECIES_DEOXYS, SEEK_SET);
	fread(&offset, 4, 1, fp);

	fseek(fp, offset, SEEK_SET);
	fread(&meta, sizeof(meta), 1, fp);

	if (meta.is_compressed) {
		// Can't merge this
		return false;
	}

	fseek(fp, meta.num_pals * 32 +
		((activeGameId == GAMEID_LEAFGREEN) ? 4096 : 2048), SEEK_CUR);

	tileAddress = readCompressedFrontImage(SPECIES_DEOXYS, NULL);
	memset(tileGfxUncompressed, 0, sizeof(tileGfxUncompressed));
	lz77_extract(tileGfxUncompressed, tileAddress, sizeof(tileGfxUncompressed));
	fwrite(tileGfxUncompressed + 2048, 1, 2048, fp);

	return true;
}

static uint32_t write_one_frontsprite(FILE *fp, int species, void **prevTiles) {
	void *tileAddress = NULL;
	uint32_t size = 0;
	union dump_entry_meta meta;
	uint8_t palette[256];
	uint8_t num_pals;

	tileAddress = readCompressedFrontImage(species, prevTiles);

	// Avoid writing duplicate data for glitchmons 252-276
	if (!tileAddress) {
		return 0;
	}

	num_pals = (uint8_t) readFrontPalette(palette, species, 0);
	num_pals += (uint8_t) readFrontPalette(palette + num_pals * 32, species, 1);
	size = lz77_compressed_size(tileAddress, sizeof(tileGfxCompressed));

	meta.size = size;
	meta.num_pals = num_pals;
	meta.num_sprites = 1;
	meta.is_compressed = 1;

	if (species == SPECIES_DEOXYS && IS_FIRERED_LEAFGREEN) {
		meta.is_compressed = 0;
		meta.size = size = 2048 * 3;
		memset(tileGfxUncompressed, 0, sizeof(tileGfxUncompressed));
		lz77_extract(tileGfxUncompressed, tileAddress, sizeof(tileGfxUncompressed));
		if (activeGameId == GAMEID_LEAFGREEN) {
			// Move Deoxys-Defense form from the second to third sprite
			memcpy(tileGfxUncompressed + 4096, tileGfxUncompressed + 2048, 2048);
			memset(tileGfxUncompressed + 2048, 0, 2048);
		}
		tileAddress = tileGfxUncompressed;
	}
	if (IS_EMERALD) {
		/* Emerald erroneously has very large (256px tall) sprite data for
		 * some Pokémon that's just filled with empty space below the
		 * single 64x64 sprite, so we truncate it. Affected species include:
		 * Blaziken, Marshtomp, Poochyena, Walrein, Swablu, and Rayquaza
		 */
		if (lz77_extracted_size(tileAddress) > 2048 && species != SPECIES_CASTFORM) {
			if (tileAddress != tileGfxCompressed) {
				memcpy(tileGfxCompressed, tileAddress, size);
				tileAddress = tileGfxCompressed;
			}
			meta.size = size = lz77_truncate(tileGfxCompressed, sizeof(tileGfxCompressed), 2048);
		}
	}

	fwrite(&meta, sizeof(meta), 1, fp);
	fwrite(&palette, 1, 32 * num_pals, fp);
	fwrite(tileAddress, 1, size, fp);
	return 4 + 32 * num_pals + size;
}

int write_frontsprites(bool force) {
	char fname[44];
	FILE *fp;
	uint32_t *offsets = NULL;
	void *prevTiles = NULL;
	uint32_t cur_offset = 0;
	int subgen = IS_FIRERED_LEAFGREEN;
	struct dump_file_header header = {
		.magic = {'P', 'K', 'M', 'B', 'D', 'U', 'M', 'P'},
		.version = 0,
		.asset_group = ASSETS_FRONTSPRITE,
		.generation = activeGameGen,
		.subgen_mask = 0,
		.flags = FLAG_IS_SPRITE,
		.item_num = 440,
		.item_size = 0
	};

	if (IS_FIRERED_LEAFGREEN) {
		header.subgen_mask = 1 << (activeGameSubGen == GAMEID_LEAFGREEN);
	} else {
		/* Emerald dump completely replaces the Ruby/Sapphire one.
		 * rather than merging with it. */
		header.subgen_mask = 1 << (activeGameSubGen == GAMEID_EMERALD) | 1;
	}

	snprintf(fname, sizeof(fname), "/pokebox/assets/frontsprites%02d%02d.bin",
		activeGameGen, subgen);

	fp = handler.frontSpriteFiles[subgen];
	if (fp != NULL) {
		struct dump_file_header header_in;
		if (!force) {
			fseek(fp, 0, SEEK_SET);
			fread(&header_in, sizeof(header_in), 1, fp);
			if ((header.subgen_mask & ~header_in.subgen_mask) == 0) {
				return 1;
			}
		}

		fclose(handler.frontSpriteFiles[subgen]);
		fp = NULL;
		if (IS_FIRERED_LEAFGREEN && !force) {
			// Merge FRLG dumps
			bool merge_success = false;
			header.subgen_mask |= header_in.subgen_mask;
			fp = fopen(fname, "r+b");
			fwrite(&header, sizeof(header), 1, fp);
			merge_success = write_frlg_deoxys_sprite(fp);
			fclose(fp);
			if (merge_success) {
				handler.frontSpriteFiles[subgen] = fopen(fname, "rb");
				return 1;
			}
			header.subgen_mask = 1 << (activeGameSubGen == GAMEID_LEAFGREEN);
			fp = NULL;
		}
	}

	if (!fp) {
		fp = fopen(fname, "wb");
		if (!fp) {
			open_message_window("Error saving asset dump: File create failed (%d)", errno);
			return 0;
		}
	}

	offsets = calloc(440, sizeof(*offsets));

	fwrite(&header, sizeof(header), 1, fp);
	fwrite(offsets, sizeof(*offsets), 440, fp);
	cur_offset = sizeof(header) + 440 * sizeof(*offsets);

	for (int i = 0; i < 440; i++) {
		uint32_t offset_delta;
		offset_delta = write_one_frontsprite(fp, i, &prevTiles);

		if (!offset_delta) {
			offsets[i] = offsets[i - 1];
		} else {
			offsets[i] = cur_offset;
			cur_offset += offset_delta;
		}
	}

	fseek(fp, sizeof(header), SEEK_SET);
	fwrite(offsets, sizeof(*offsets), 440, fp);

	free(offsets);
	fclose(fp);
	handler.frontSpriteFiles[subgen] = fopen(fname, "rb");
	return 1;
}

int write_boxicons(bool force) {
	FILE *fp;
	uint32_t num_pals = 3;
	struct dump_file_header header = {
		.magic = {'P', 'K', 'M', 'B', 'D', 'U', 'M', 'P'},
		.version = 0,
		.asset_group = ASSETS_BOXICONS,
		.generation = activeGameGen,
		.subgen_mask = 0,
		.flags = FLAG_IS_SPRITE | FLAG_SHARED_PALETTES,
		.item_num = 440,
		.item_size = 1024
	};

	if (handler.iconFile) {
		if (force) {
			fclose(handler.iconFile);
			handler.iconFile = NULL;
		} else {
			return 1;
		}
	}

	fp = fopen("/pokebox/assets/boxicons03.bin", "wb");
	if (fp < 0) {
		open_message_window("Error saving asset dump: File create failed (%d)", errno);
		return 0;
	}

	memcpy(handler.palettesData, handler.palettesData + 32 * num_pals, 32 * num_pals);
	memcpy(handler.iconPaletteIndicesSD, handler.iconPaletteIndices, 440);

	fwrite(&header, sizeof(header), 1, fp);
	fwrite(&num_pals, sizeof(num_pals), 1, fp);
	fwrite(handler.palettesData, 1, 32 * num_pals, fp);
	fwrite(handler.iconPaletteIndices, 1, 440, fp);

	/* Note that Jynx (#124) has different sprite data between the original
	 * Japanese version and all the western releases in RSE.
	 * It's not a huge difference, so we ignore it. All releases of FRLG use the JP Jynx.
	 *
	 * The null placeholder (#000) has the same "?" box sprite in FRLG as the placeholders
	 * at 252-276, but has a recolored Bulbasaur sprite in RSE.
	 * Poliwhirl (#061) has different sprite data in Emerald than RS/FRLG.
	 * We ignore these differences too and just treat all of Gen3 as the same.
	 */
	for (int i = 0; i < 440; i++) {
		const uint16_t *iconImage = NULL;
		iconImage = getIconImage(i);
		fwrite(iconImage, 1, 1024, fp);
	}

	fclose(fp);
	handler.iconFile = fopen("/pokebox/assets/boxicons03.bin", "rb");
	return 1;
}

int write_itemicons(bool force) {
	FILE *fp;
	uint32_t *offsets = NULL;
	struct dump_file_header header = {
		.magic = {'P', 'K', 'M', 'B', 'D', 'U', 'M', 'P'},
		.version = 0,
		.asset_group = ASSETS_ITEMICONS,
		.generation = activeGameGen,
		.subgen_mask = 0,
		.flags = FLAG_IS_SPRITE,
		.item_num = 377,
		.item_size = 0
	};

	if (!handler.itemIconTable) {
		// No icons for Ruby/Sapphire
		return 1;
	}

	header.subgen_mask = 1 << IS_EMERALD | 1;

	if (handler.itemIconFile) {
		if (!force) {
			struct dump_file_header header_in;
			fseek(handler.itemIconFile, 0, SEEK_SET);
			fread(&header_in, sizeof(header_in), 1, handler.itemIconFile);
			/* Replace any existing FRLG dump with an Emerald one.
			 * The differences are:
			 * 1. Emerald adds two new items: Magma Emblem and Old Sea Map
			 * 2. Emerald gives HMs a different sprite rather than sharing the TM one.
			 */
			if ((header.subgen_mask & ~header_in.subgen_mask) == 0) {
				return 1;
			}
		}
		fclose(handler.itemIconFile);
		handler.itemIconFile = NULL;
	}

	fp = fopen("/pokebox/assets/items03.bin", "wb");
	if (fp < 0) {
		open_message_window("Error saving asset dump: File create failed (%d)", errno);
		return 0;
	}

	offsets = calloc(header.item_num, sizeof(*offsets));

	fwrite(&header, sizeof(header), 1, fp);
	fwrite(offsets, 4, 377, fp);
	dumpItemIconsRange(fp, 0, IS_EMERALD ? 377 : 375, offsets);
	if (!IS_EMERALD) {
		offsets[375] = sizeof(header);
		offsets[376] = sizeof(header);
	}

	fseek(fp, sizeof(header), SEEK_SET);
	fwrite(offsets, 4, 377, fp);

	fclose(fp);
	free(offsets);
	handler.itemIconFile = fopen("/pokebox/assets/items03.bin", "rb");
	return 1;
}

static void merge_basestats(struct BaseStatEntryUnifiedGen3 *out,
	const struct BaseStatEntryGen3 *in, uint8_t subgen, bool force) {
	uint32_t itemsRSE;
	uint32_t itemsFRLG;
	uint8_t fleeRSE;
	uint8_t fleeFRLG;

	itemsRSE = out->entry.heldItems;
	itemsFRLG = out->heldItemsFRLG;
	fleeRSE = out->entry.safariFleeRate;
	fleeFRLG = out->entry.padding[0];
	if (subgen) {
		itemsFRLG = in->heldItems;
		fleeFRLG = in->safariFleeRate;
	} else {
		itemsRSE = in->heldItems;
		fleeRSE = in->safariFleeRate;
	}

	if (force) {
		memcpy(&out->entry, in, sizeof(out->entry));
	}
	out->entry.heldItems = itemsRSE;
	out->heldItemsFRLG = itemsFRLG;
	out->entry.safariFleeRate = fleeRSE;
	out->entry.padding[0] = fleeFRLG;
}

int write_basestats(bool force) {
	FILE *fp;
	uint8_t subgen = IS_FIRERED_LEAFGREEN;
	struct dump_file_header header = {
		.magic = {'P', 'K', 'M', 'B', 'D', 'U', 'M', 'P'},
		.version = 0,
		.asset_group = ASSETS_BASESTATS,
		.generation = 3,
		.subgen_mask = 0,
		.flags = 0,
		.item_num = 440,
		.item_size = sizeof(struct BaseStatEntryUnifiedGen3)
	};
	struct dump_file_header header_in;
	struct BaseStatEntryUnifiedGen3 stats;

	header.subgen_mask = 1 << subgen;

	if (handler.baseStatFile) {
		fseek(handler.baseStatFile, 0, SEEK_SET);
		fread(&header_in, sizeof(header_in), 1, handler.baseStatFile);
		if ((header_in.subgen_mask >> subgen & 1) != 0 && !force) {
			return 1;
		}

		header.subgen_mask |= header_in.subgen_mask;

		fp = freopen("/pokebox/assets/basestats03.bin", "r+b", handler.baseStatFile);
		fseek(fp, 0, SEEK_SET);
	} else {
		fp = fopen("/pokebox/assets/basestats03.bin", "wb");
		if (fp < 0) {
			open_message_window("Error saving asset dump: File create failed (%d)", errno);
			return 0;
		}
		force = 1;
	}

	fwrite(&header, sizeof(header), 1, fp);

	for (int i = 0; i < 440; i++) {
		const struct BaseStatEntryGen3 *stats_in;
		stats_in = getBaseStatEntry(i, 0);
		if (handler.baseStatFile) {
			fread(&stats, 1, sizeof(stats), fp);
			fseek(fp, -sizeof(stats), SEEK_CUR);
		} else {
			memset(&stats, 0, sizeof(stats));
		}
		merge_basestats(&stats, stats_in, subgen, force);

		fwrite(&stats, sizeof(stats), 1, fp);
	}

	fclose(fp);
	handler.baseStatFile = fopen("/pokebox/assets/basestats03.bin", "rb");
	return 1;
}

int dump_assets_to_sd(bool force) {
	struct stat s;

	// Create the needed directories if they don't already exist
	if (mkdir("/pokebox", 0777) < 0 && errno != EEXIST) {
		open_message_window("Error saving assets: Unable to create directories");
		return 0;
	}
	if (mkdir("/pokebox/assets", 0777) < 0) {
		int createFail =
			errno != EEXIST ||
			stat("/pokebox/assets", &s) < 0 ||
			(s.st_mode & S_IFDIR) == 0;
		if (createFail) {
			open_message_window("Error saving assets: Unable to create directories");
			return 0;
		}
	}
	return write_basestats(force) && write_boxicons(force) &&
		write_frontsprites(force) && write_itemicons(force);
}
