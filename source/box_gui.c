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
#include "box_gui.h"
#include <nds.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"

#include "asset_manager.h"
#include "console_helper.h"
#include "cursor.h"
#include "defWallpapers.h"
#include "guiTileset.h"
#include "gui_tilemaps.h"
#include "pkmx_format.h"
#include "pokemon_strings.h"
#include "savedata_gen3.h"
#include "string_gen3.h"
#include "sd_boxes.h"
#include "text_draw.h"
#include "utf8.h"

/* VRAM layout:
 * 5000000-50001FF (512B) BG Palettes A (Top Screen)
 * 5000200-50003FF (512B) OBJ Palettes A (Top Screen)
 * 5000400-50005FF (512B) BG Palettes B (Bottom Screen)
 * 5000600-50007FF (512B) OBJ Palettes B (Bottom Screen)
 * 6000000-607FFFF (512k) BG VRAM A (Top Screen)
 * 6200000-621FFFF (128k) BG VRAM B (Bottom Screen)
 * 6400000-643FFFF (256k) OBJ VRAM A (Top Screen)
 * 6600000-661FFFF (128k) OBJ VRAM B (Bottom Screen)
 * 7000000-70003FF (  1k) OAM A (Top Screen)
 * 7000400-70007FF (  1k) OAM B (Bottom Screen)
 *
 * BG data for each screen:
 * 0000-07FF console tile map
 * 0800-0FFF console tile map (next box)
 * 1000-17FF wallpaper tile map
 * 1800-1FFF wallpaper tile map (next box)
 * 2000-27FF UI overlays tile map
 * 4000-5FFF console tile data (font, 256 tiles)
 * 6000-7FFF console tile data (unused)
 * 8000-8FFF wallpaper tile data
 * 9000-9FFF wallpaper tile data (next box)
 * A000-BFFF wallpaper tile data (unused)
 * C000-FFFF UI overlays tile data (1024 tiles)
 *
 * BG palettes for each screen:
 * 00    Console text
 * 04-07 Current box wallpaper
 * 08    UI overlays
 *
 * OAM entries for each screen: (limit 0x80)
 * 00    Cursor
 * 10    Large front sprite
 * 20-3D Pokemon in holding
 * 40-5D Pokemon in current box
 * 60-7D Pokemon in next box
 *
 * OBJ data for each screen:
 * 00000-001FF Cursor
 * 04000-047FF Large front sprite (double buffered)
 * 08000-0FFFF Pokemon in holding
 * 10000-17FFF Pokemon in current box
 * 18000-1FFFF Pokemon in next box
 *
 * OBJ palettes for each screen: (each palette is 32 bytes)
 * 00-02 Box icon sprites (only 3 palettes are needed total for every species)
 * 04-05 Large front sprite (double buffered)
 * 08    Cursor
 *
 * All the "next box" sections are currently unused, but reserved for
 * implementing the sliding animation in changing between boxes
 */

#define BG_LAYER_TEXT 0
#define BG_LAYER_BUTTONS 1
#define BG_LAYER_WALLPAPER 2
#define BG_LAYER_BACKGROUND 3

// Map offset = VRAM + MAPBASE * 0x800
#define BG_MAPBASE_WALLPAPER 2
#define BG_MAPBASE_BUTTONS 4

// Tileset offset = BG_GFX + TILEBASE * 0x4000
#define BG_TILEBASE_WALLPAPER 2
#define BG_TILEBASE_BUTTONS 3

#define OAM_INDEX_CURSOR 0
#define OAM_INDEX_BIGSPRITE 0x10
#define OAM_INDEX_HOLDING 0x20
#define OAM_INDEX_CURBOX 0x40

// Sprite gfx = SPRITE_GFX + GFXIDX * 128
// The boundary size is 128 because we pass SpriteMapping_1D_128 to oamInit
#define OBJ_GFXIDX_BIGSPRITE 0x80
#define OBJ_GFXIDX_HOLDING 0x100
#define OBJ_GFXIDX_CURBOX 0x200

static int activeSprite = 0;

// Each sprite is 2048 bytes
// Need to allocate enough space for 4 sprites because of Castform
static uint8_t frontSpriteData[8192];

#define GUI_FLAG_SELECTING 0x01
#define GUI_FLAG_HOLDING 0x02
#define GUI_FLAG_HOLDING_MULTIPLE 0x04

static const textLabel_t botLabelGroup = {1, 1, 1, 16, 0x100};
static const textLabel_t botLabelBox3  = {1, 5, 6, 12, 0x120};
static const textLabel_t botLabelBox4  = {1, 5, 5, 12, 0x120};
static const textLabel_t botLabelsInfo[] = {
	{1, 22,  0, 10, 0x140},
	{1, 22,  2, 10, 0x160},
	{1, 22, 13, 10, 0x180},
	{1, 22, 15,  6, 0x1A0},
	{1, 28, 15,  2, 0x1C0}
};

struct boxgui_groupView {
	uint8_t groupIdx;
	int8_t activeBox;
	uint8_t numBoxes;
	uint8_t generation;
	uint16_t pkmSize;
	uint16_t boxSizeBytes;
	uint16_t **boxNames;
	uint8_t *boxWallpapers;
	uint8_t *boxData;
	uint16_t *boxIcons;
};

struct boxgui_state {
	int8_t cursor_x;
	int8_t cursor_y;
	uint8_t flags;
	uint8_t cursorMode; //TODO
	struct boxgui_groupView topScreen;
	struct boxgui_groupView botScreen;
	int8_t holdingSourceBox;
	uint8_t holdingSourceGroup;
	int8_t holdingSource_x;
	int8_t holdingSource_y;
	int8_t holdingMin_x;
	int8_t holdingMax_x;
	int8_t holdingMin_y;
	int8_t holdingMax_y;
	uint16_t boxIcons1[32 * 30];
	uint16_t boxIcons2[32 * 30];
	uint16_t holdIcons[30];
	uint8_t boxData1[32 * BOX_SIZE_BYTES_X];
	uint8_t boxData2[32 * BOX_SIZE_BYTES_X];
};

static void draw_gui_tilemap(const uint8_t *tilemap, uint8_t screen, uint8_t x, uint8_t y) {
	uint8_t width = tilemap[0];
	uint8_t height = tilemap[1];
	uint16_t *mapRam;
	tilemap += 2;
	if (screen) {
		mapRam = BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS);
	} else {
		mapRam = BG_MAP_RAM(BG_MAPBASE_BUTTONS);
	}
	for (int rowIdx = 0; rowIdx < height; rowIdx++) {
		for (int colIdx = 0; colIdx < width; colIdx++) {
			uint16_t tspec = (8 << 12) | tilemap[rowIdx * width + colIdx];
			mapRam[(rowIdx + y) * 32 + colIdx + x] = tspec;
		}
	}
}

static void draw_builtin_wallpaper(const uint8_t *tilemap, uint8_t screen, uint8_t x, uint8_t y) {
	uint8_t width = tilemap[0];
	uint8_t height = tilemap[1];
	uint16_t *mapRam;
	tilemap += 2;
	if (screen) {
		mapRam = BG_MAP_RAM_SUB(BG_MAPBASE_WALLPAPER);
	} else {
		mapRam = BG_MAP_RAM(BG_MAPBASE_WALLPAPER);
	}
	for (int rowIdx = 0; rowIdx < height; rowIdx++) {
		for (int colIdx = 0; colIdx < width; colIdx++) {
			uint16_t tspec = (4 << 12) | tilemap[rowIdx * width + colIdx];
			mapRam[(rowIdx + y) * 32 + colIdx + x] = tspec;
		}
	}
}

static void status_display_update(const uint8_t *pkm_in, int generation) {
	pkm3_t pkm;
	uint16_t checksum;
	u16 species;
	const char *nickname;
	uint8_t palette[32];
	struct SimplePKM simple;
	const textLabel_t *textLabels = botLabelsInfo;

	selectTopConsole();
	consoleClear();

	if (generation == 0) {
		uint8_t curGen = pkm_in[0];
		/* The other 3 bytes in PKMX are reserved for:
		 *   curSubGen (eg distinguishing between RSE and FRLG)
		 *   originGen (keeping track of generation conversions)
		 *   originSubGen
		 */
		if (curGen == 3) {
			checksum = decode_pkm_encrypted_data(&pkm, pkm_in + 4);
		} else {
			// Ignore Pokemon from any other generation.
			// This allows some level of compatibility with future versions of PokeBoxDS.
			pkm.species = 0;
		}
	} else if (generation == 3) {
		checksum = decode_pkm_encrypted_data(&pkm, pkm_in);
	} else {
		pkm.species = 0;
	}

	if (pkm.species == 0) {
		oamSub.oamMemory[OAM_INDEX_BIGSPRITE].attribute[0] = 0;
		oamSub.oamMemory[OAM_INDEX_BIGSPRITE].attribute[1] = 0;
		oamSub.oamMemory[OAM_INDEX_BIGSPRITE].attribute[2] = 0;
		for (int i = 0; i < 5; i++)
			clearText(&textLabels[i]);
		return;
	}

	pkm3_to_simplepkm(&simple, &pkm);
	print_pokemon_details(&pkm);

	species = pkm_displayed_species(&pkm);
	if (checksum != pkm.checksum) {
		nickname = "Bad EGG";
		species = 412;
	} else if (species == 412) {
		nickname = "EGG";
	} else {
		nickname = NULL;
	}

	uint16_t genderStr[2];
	uint8_t genderColor = FONT_BLACK;
	genderStr[0] = 0;
	genderStr[1] = 0;
	if (simple.gender == 0) {
		genderStr[0] = 0x2642;
		genderColor = FONT_BLUE;
	} else if (simple.gender == 1) {
		genderStr[0] = 0x2640;
		genderColor = FONT_PINK;
	}
	drawText(   &textLabels[0], FONT_BLACK, FONT_WHITE, get_pokemon_name_by_dex(simple.dexNumber));
	drawTextFmt(&textLabels[1], FONT_BLACK, FONT_WHITE, "#%03d", simple.dexNumber);
	if (nickname)
		drawText(&textLabels[2], FONT_BLACK, FONT_WHITE, nickname);
	else
		drawText16(&textLabels[2], FONT_BLACK, FONT_WHITE, simple.nickname);
	drawTextFmt(&textLabels[3], FONT_BLACK, FONT_WHITE, "Lv %3d", simple.level);
	drawText16(&textLabels[4], genderColor, FONT_WHITE, genderStr);

	readFrontImage(frontSpriteData, palette, species, pkm_is_shiny(&pkm));

	memcpy((uint8_t*) SPRITE_PALETTE_SUB + 32 * (4 + activeSprite), palette, 32);
	memcpy((uint8_t*) SPRITE_GFX_SUB + OBJ_GFXIDX_BIGSPRITE * 128 + activeSprite * 2048,
		frontSpriteData, 2048);
	oamSub.oamMemory[OAM_INDEX_BIGSPRITE].attribute[0] = OBJ_Y(36) | ATTR0_COLOR_16;
	oamSub.oamMemory[OAM_INDEX_BIGSPRITE].attribute[1] = OBJ_X(180) | ATTR1_SIZE_64;
	oamSub.oamMemory[OAM_INDEX_BIGSPRITE].palette = 4 + activeSprite;
	oamSub.oamMemory[OAM_INDEX_BIGSPRITE].gfxIndex = OBJ_GFXIDX_BIGSPRITE + activeSprite * 16;
	activeSprite ^= 1;
}

static void load_cursor() {
	oamSub.oamMemory[0].attribute[0] = OBJ_Y(60) | ATTR0_COLOR_16;
	oamSub.oamMemory[0].attribute[1] = OBJ_X(12) | ATTR1_SIZE_32;
	oamSub.oamMemory[0].palette = 8;
	oamSub.oamMemory[0].gfxIndex = 0;
	dmaCopy(cursorTiles, SPRITE_GFX_SUB, sizeof(cursorTiles));
	dmaCopy(cursorPal, SPRITE_PALETTE_SUB + 16 * 8, sizeof(cursorPal));
}

static int display_icon_sprites(
	const uint16_t *speciesList, int oamIndex, int gfxIndex, int x, int y) {

	int obj_idx = 0;

	for (int i = 0; i < 30; i++) {
		uint16_t species = speciesList[i];
		SpriteEntry *oam = &oamSub.oamMemory[oamIndex + i];

		if (species == 0) {
			oam->attribute[0] = 0;
			oam->attribute[1] = 0;
			oam->attribute[2] = 0;
			continue;
		}

		oam->attribute[0] = OBJ_Y((i / 6) * 24 + y) | ATTR0_COLOR_16;
		oam->attribute[1] = OBJ_X((i % 6) * 24 + x) | ATTR1_SIZE_32;
		oam->palette = getIconPaletteIdx(species);
		oam->gfxIndex = gfxIndex + i * 8;
		// Each 32x32@4bpp sprite is 512 bytes.
		// 2 animation frames at 512 bytes each = 1024 bytes per Pokemon.
		dmaCopy(
			getIconImage(species),
			(uint8_t*) SPRITE_GFX_SUB + gfxIndex * 128 + i * 1024,
			1024);

		obj_idx++;
	}
	return obj_idx;
}

static void move_icon_sprites(int oamIndex, int x, int y) {
	for (int i = 0; i < 30; i++) {
		SpriteEntry *oam = &oamSub.oamMemory[oamIndex + i];

		if (oam->attribute[0] == 0)
			continue;

		oam->attribute[0] = OBJ_Y((i / 6) * 24 + y) | ATTR0_COLOR_16;
		oam->attribute[1] = OBJ_X((i % 6) * 24 + x) | ATTR1_SIZE_32;
	}
}

static void clear_icon_sprites(int oamIndex) {
	for (int i = 0; i < 30; i++) {
		SpriteEntry *oam = &oamSub.oamMemory[oamIndex + i];
		oam->attribute[0] = 0;
		oam->attribute[1] = 0;
		oam->attribute[2] = 0;
	}
}

static void clear_selection_shadow() {
	for (int rowIdx = 9; rowIdx < 24; rowIdx++) {
		for (int colIdx = 0; colIdx < 21; colIdx++) {
			BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS)[rowIdx * 32 + colIdx] = 0;
		}
	}
}

static void decode_boxes(struct boxgui_groupView *group) {
	uint16_t checksum;
	uint16_t species;
	pkm3_t pkm;
	for (int pkmIdx = 0; pkmIdx < 30 * group->numBoxes; pkmIdx++) {
		const uint8_t *bytes;
		int generation;
		bytes = group->boxData + pkmIdx * group->pkmSize;
		generation = group->generation;
		if (generation == 0) {
			generation = bytes[0];
			bytes += 4;
			if (generation == 0) {
				// Blank space
				group->boxIcons[pkmIdx] = 0;
				continue;
			}
		}
		if (generation != 3) {
			// Question mark for other generations we can't decode yet
			group->boxIcons[pkmIdx] = 252;
			continue;
		}
		checksum = decode_pkm_encrypted_data(&pkm, bytes);
		if (checksum != pkm.checksum)
			species = 412; // Egg icon for Bad EGG
		else
			species = pkm_displayed_species(&pkm);
		group->boxIcons[pkmIdx] = species;
	}
}

static void update_cursor(struct boxgui_state *guistate) {
	int cur_poke = guistate->cursor_y * 6 + guistate->cursor_x;
	const struct boxgui_groupView *group;
	int icons_x, icons_y;

	group = &guistate->botScreen;

	if (group->generation == 3) {
		oamSub.oamMemory[0].x = guistate->cursor_x * 24 + 12;
		oamSub.oamMemory[0].y = guistate->cursor_y * 24 + 60;
		icons_x = 12 + guistate->holdingMin_x * 24;
		icons_y = 48 + guistate->holdingMin_y * 24;
	} else {
		oamSub.oamMemory[0].x = guistate->cursor_x * 24 + 8;
		oamSub.oamMemory[0].y = guistate->cursor_y * 24 + 60;
		icons_x = 8 + guistate->holdingMin_x * 24;
		icons_y = 48 + guistate->holdingMin_y * 24;
	}

	if (guistate->flags & GUI_FLAG_HOLDING) {
		move_icon_sprites(OAM_INDEX_HOLDING, icons_x, icons_y);
	} else {
		status_display_update(guistate->botScreen.boxData +
			guistate->botScreen.activeBox * guistate->botScreen.boxSizeBytes +
			cur_poke * guistate->botScreen.pkmSize,
			guistate->botScreen.generation);
	}
	clear_selection_shadow();
	if (guistate->flags & (GUI_FLAG_SELECTING | GUI_FLAG_HOLDING)) {
		uint8_t min_x = (guistate->holdingMin_x    ) * 3 + 2;
		uint8_t max_x = (guistate->holdingMax_x + 1) * 3 + 2;
		uint8_t min_y = (guistate->holdingMin_y    ) * 3 + 9;
		uint8_t max_y = (guistate->holdingMax_y + 1) * 3 + 9;
		for (uint8_t rowIdx = min_y; rowIdx < max_y - 1; rowIdx++) {
			for (uint8_t colIdx = min_x; colIdx < max_x; colIdx++) {
				uint16_t tspec = (8 << 12) | 0x20;
				BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS)[rowIdx * 32 + colIdx] = tspec;
			}
		}
		if (group->generation != 3) {
			for (uint8_t rowIdx = min_y; rowIdx < max_y - 1; rowIdx++) {
				uint16_t tspec = (8 << 12) | 0x21;
				BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS)[rowIdx * 32 + min_x - 1] = tspec;
				BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS)[rowIdx * 32 + max_x - 1] = tspec + 1;
			}
		}
	}
}

static int display_box(const struct boxgui_state *guistate) {
	uint16_t *name;
	int rc;
	const struct boxgui_groupView *group;
	char namebuf[20];
	const textLabel_t *nameLabel;

	group = &guistate->botScreen;
	if (group->boxNames) {
		name = group->boxNames[group->activeBox];
		utf8_encode(namebuf, name, sizeof(namebuf));
	} else {
		sprintf(namebuf, "BOX %d", group->activeBox + 1);
		name = NULL;
	}
	rc = 0;
	if (group->boxWallpapers) {
		int wallpaper;
		wallpaper = group->boxWallpapers[group->activeBox];
		rc = loadWallpaper(wallpaper);
	}

	selectBottomConsole();
	if (group->generation == 3) {
		nameLabel = &botLabelBox3;
		clearText(&botLabelBox4);
	} else {
		nameLabel = &botLabelBox4;
		clearText(&botLabelBox3);
	}
	drawText(nameLabel, FONT_BLACK, FONT_WHITE, namebuf);

	bgInit(BG_LAYER_BUTTONS, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_BUTTONS, BG_TILEBASE_BUTTONS);
	bgInit(BG_LAYER_WALLPAPER, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_WALLPAPER, BG_TILEBASE_WALLPAPER);
	bgInitSub(BG_LAYER_BUTTONS, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_BUTTONS, BG_TILEBASE_BUTTONS);
	bgInitSub(BG_LAYER_WALLPAPER, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_WALLPAPER, BG_TILEBASE_WALLPAPER);

	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	memset(BG_MAP_RAM_SUB(BG_MAPBASE_WALLPAPER), 0, 2048);

	memcpy(BG_TILE_RAM_SUB(BG_TILEBASE_BUTTONS), guiTilesetTiles, sizeof(guiTilesetTiles));
	memcpy((uint8_t*) BG_PALETTE_SUB + 32 * 8, guiTilesetPal, sizeof(guiTilesetPal));

	draw_gui_tilemap(tilemap_pokeStatusPane, 1, 21, 0);
	if (group->generation == 3) {
		draw_gui_tilemap(tilemap_boxLeftButton, 1, 1, 6);
		draw_gui_tilemap(tilemap_boxRightButton, 1, 19, 6);
	} else {
		draw_gui_tilemap(tilemap_boxLeftButton, 1, 1, 5);
		draw_gui_tilemap(tilemap_boxRightButton, 1, 18, 5);
	}

	if (rc) {
		int wallpaperPalOffset = 4;
		memcpy(BG_TILE_RAM_SUB(BG_TILEBASE_WALLPAPER), wallpaperTiles, sizeof(wallpaperTiles));
		memcpy((uint8_t*) BG_PALETTE_SUB + 32 * wallpaperPalOffset,
			wallpaperPal, sizeof(wallpaperPal));
		for (int rowIdx = 0; rowIdx < 18; rowIdx++) {
			for (int colIdx = 0; colIdx < 20; colIdx++) {
				uint16_t tspec = wallpaperTilemap[rowIdx * 20 + colIdx];
				uint8_t pal = tspec >> 12;
				if (pal)
					pal += wallpaperPalOffset - 1;
				tspec = (pal << 12) | (tspec & 0xFFF);
				BG_MAP_RAM_SUB(BG_MAPBASE_WALLPAPER)[(rowIdx + 6) * 32 + colIdx + 1] = tspec;
			}
		}
	} else {
		memcpy(BG_TILE_RAM_SUB(BG_TILEBASE_WALLPAPER), defWallpapersTiles,
			sizeof(defWallpapersTiles));
		memcpy((uint8_t*) BG_PALETTE_SUB + 32 * 4, defWallpapersPal, sizeof(defWallpapersPal));
		draw_builtin_wallpaper(tilemap_blankWallpaper, 1, 0, 5);
	}

	int icons_x, icons_y;
	if (group->generation == 3) {
		icons_x = 12;
		icons_y = 60;
	} else {
		icons_x = 8;
		icons_y = 60;
	}

	rc = display_icon_sprites(
		group->boxIcons + group->activeBox * 30,
		OAM_INDEX_CURBOX, OBJ_GFXIDX_CURBOX,
		icons_x, icons_y);
	return rc;
}

static int switch_box(struct boxgui_state *guistate, int rel) {
	struct boxgui_groupView *group;
	int activeBox;

	group = &guistate->botScreen;
	activeBox = group->activeBox + rel;

	if (activeBox < 0)
		activeBox = group->numBoxes - 1;
	else if (activeBox >= group->numBoxes)
		activeBox = 0;
	group->activeBox = activeBox;
	display_box(guistate);
	update_cursor(guistate);
	return 1;
}

static void swap_screens(struct boxgui_state *guistate) {
	struct boxgui_groupView tmpGroup;
	tmpGroup = guistate->topScreen;
	guistate->topScreen = guistate->botScreen;
	guistate->botScreen = tmpGroup;
	display_box(guistate);
	update_cursor(guistate);
}

static void move_cursor_x(struct boxgui_state *guistate, int rel) {
	int8_t cursor_x = guistate->cursor_x + rel;
	int8_t min_x = guistate->holdingMin_x;
	int8_t max_x = guistate->holdingMax_x;

	if (guistate->flags & GUI_FLAG_HOLDING_MULTIPLE) {
		min_x += rel;
		max_x += rel;

		// Switch boxes when passing the left/right edges of a box
		if (min_x < 0) {
			switch_box(guistate, -1);
			cursor_x += 5 - max_x;
			min_x += 5 - max_x;
			max_x = 5;
		}
		else if (max_x > 5) {
			switch_box(guistate, 1);
			cursor_x -= min_x;
			max_x -= min_x;
			min_x = 0;
		}
	} else if (guistate->flags & GUI_FLAG_SELECTING) {
		// Stop when trying to pass the left/right edges of a box
		if (cursor_x < 0 || cursor_x > 5)
			return;
		if (min_x > cursor_x)
			min_x = cursor_x;
		else if (max_x < cursor_x)
			max_x = cursor_x;
		else if (rel > 0)
			min_x = cursor_x;
		else if (rel < 0)
			max_x = cursor_x;
	} else {
		// Wraparound when passing the left/right edges of a box
		if (cursor_x < 0)
			cursor_x = 5;
		else if (cursor_x > 5)
			cursor_x = 0;
		max_x = min_x = cursor_x;
	}

	guistate->cursor_x = cursor_x;
	guistate->holdingMin_x = min_x;
	guistate->holdingMax_x = max_x;
	update_cursor(guistate);
}

static void move_cursor_y(struct boxgui_state *guistate, int rel) {
	int8_t cursor_y = guistate->cursor_y + rel;
	int8_t min_y = guistate->holdingMin_y;
	int8_t max_y = guistate->holdingMax_y;

	if (guistate->flags & GUI_FLAG_HOLDING_MULTIPLE) {
		min_y += rel;
		max_y += rel;
		// Stop when trying to pass the top/bottom edges of a box
		if (min_y < 0 || max_y > 4)
			return;
	} else if (guistate->flags & GUI_FLAG_SELECTING) {
		// Stop when trying to pass the top/bottom edges of a box
		if (cursor_y < 0 || cursor_y > 4)
			return;
		if (min_y > cursor_y)
			min_y = cursor_y;
		else if (max_y < cursor_y)
			max_y = cursor_y;
		else if (rel > 0)
			min_y = cursor_y;
		else if (rel < 0)
			max_y = cursor_y;
	} else {
		// Wraparound when passing the top/bottom edges of a box
		if (cursor_y < 0)
			cursor_y = 4;
		else if (cursor_y > 4)
			cursor_y = 0;
		max_y = min_y = cursor_y;
	}

	guistate->cursor_y = cursor_y;
	guistate->holdingMin_y = min_y;
	guistate->holdingMax_y = max_y;
	update_cursor(guistate);
}

static void start_selection(struct boxgui_state *guistate) {
	guistate->holdingMin_x = guistate->cursor_x;
	guistate->holdingMax_x = guistate->cursor_x;
	guistate->holdingMin_y = guistate->cursor_y;
	guistate->holdingMax_y = guistate->cursor_y;
	guistate->flags = GUI_FLAG_SELECTING;
	update_cursor(guistate);
}

static void pickup_selection(struct boxgui_state *guistate) {
	uint8_t flags = GUI_FLAG_HOLDING;
	int width  = guistate->holdingMax_x - guistate->holdingMin_x + 1;
	int height = guistate->holdingMax_y - guistate->holdingMin_y + 1;
	int dx = guistate->holdingMin_x;
	int dy = guistate->holdingMin_y;
	int icons_x, icons_y;
	struct boxgui_groupView *group = &guistate->botScreen;
	uint16_t *curBoxIcons = group->boxIcons + 30 * group->activeBox;
	int isPopulated = 0;

	if (width * height > 1)
		flags |= GUI_FLAG_HOLDING_MULTIPLE;
	guistate->flags = flags;
	guistate->holdingSourceBox = group->activeBox;
	guistate->holdingSourceGroup = group->groupIdx;
	guistate->holdingSource_x = guistate->holdingMin_x;
	guistate->holdingSource_y = guistate->holdingMin_y;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint16_t tmp;
			tmp = guistate->holdIcons[y * 6 + x] = curBoxIcons[(y + dy) * 6 + (x + dx)];
			curBoxIcons[(y + dy) * 6 + (x + dx)] = 0;
			if (tmp)
				isPopulated = 1;
		}
	}

	// Lose the selection if nothing is actually there
	if (!isPopulated)
		guistate->flags = 0;

	if (group->generation == 3) {
		icons_x = 12 + 24 * dx;
		icons_y = 48 + 24 * dy;
	} else {
		icons_x = 8 + 24 * dx;
		icons_y = 48 + 24 * dy;
	}

	display_icon_sprites(
		guistate->holdIcons, OAM_INDEX_HOLDING, OBJ_GFXIDX_HOLDING,
		icons_x, icons_y);
	display_box(guistate);
	update_cursor(guistate);
}

// Drop the held Pokemon back where they came from
static void drop_holding(struct boxgui_state *guistate) {
	int width  = guistate->holdingMax_x - guistate->holdingMin_x + 1;
	int height = guistate->holdingMax_y - guistate->holdingMin_y + 1;
	int sx = guistate->holdingSource_x;
	int sy = guistate->holdingSource_y;
	struct boxgui_groupView *group = &guistate->botScreen;
	struct boxgui_groupView *srcGroup = group;
	uint16_t *srcBoxIcons;

	if (group->groupIdx != guistate->holdingSourceGroup)
		srcGroup = &guistate->topScreen;
	srcBoxIcons = srcGroup->boxIcons + 30 * guistate->holdingSourceBox;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			srcBoxIcons[(y + sy) * 6 + (x + sx)] = guistate->holdIcons[y * 6 + x];
			guistate->holdIcons[y * 6 + x] = 0;
		}
	}

	guistate->cursor_x += guistate->holdingSource_x - guistate->holdingMin_x;
	guistate->cursor_y += guistate->holdingSource_y - guistate->holdingMin_y;
	srcGroup->activeBox = guistate->holdingSourceBox;
	guistate->flags = 0;
	clear_icon_sprites(OAM_INDEX_HOLDING);

	display_box(guistate);
	update_cursor(guistate);
}

static void store_holding(struct boxgui_state *guistate) {
	int width  = guistate->holdingMax_x - guistate->holdingMin_x + 1;
	int height = guistate->holdingMax_y - guistate->holdingMin_y + 1;
	int dx = guistate->holdingMin_x;
	int dy = guistate->holdingMin_y;
	int sx = guistate->holdingSource_x;
	int sy = guistate->holdingSource_y;
	int x_start, x_iter, x_end, y_start, y_iter, y_end;
	struct boxgui_groupView *srcGroup, *dstGroup;
	uint16_t *dstBoxIcons, *srcBoxIcons;
	uint8_t *dstBoxData, *srcBoxData;

	dstGroup = &guistate->botScreen;
	if (guistate->holdingSourceGroup == guistate->botScreen.groupIdx) {
		srcGroup = &guistate->botScreen;
	} else {
		srcGroup = &guistate->topScreen;
	}
	dstBoxIcons = dstGroup->boxIcons + dstGroup->activeBox * 30;
	srcBoxIcons = srcGroup->boxIcons + guistate->holdingSourceBox * 30;
	dstBoxData = dstGroup->boxData + dstGroup->activeBox * dstGroup->boxSizeBytes;
	srcBoxData = srcGroup->boxData + guistate->holdingSourceBox * srcGroup->boxSizeBytes;

	if (guistate->flags & GUI_FLAG_HOLDING_MULTIPLE) {
		// Do nothing if any spot in the destination is occupied.
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				if (dstBoxIcons[(y + dy) * 6 + (x + dx)] && guistate->holdIcons[y * 6 + x])
					return;
			}
		}
	}

	/* Reverse the iteration order depending on the source/dest relative positions.
	 * When the source/dest regions are in the same box and overlapping, this avoids potential
	 * problems with overwriting source data before using it. There's no need to check whether
	 * the regions actually overlap because the iteration order doesn't matter otherwise.
	 */
	x_start = y_start = 0;
	x_iter = y_iter = 1;
	x_end = width;
	y_end = height;
	if (dx > sx) {
		x_start = x_end - 1;
		x_end = -1;
		x_iter = -1;
	}
	if (dy > sy) {
		y_start = y_end - 1;
		y_end = -1;
		y_iter = -1;
	}

	// Swap the contents of holdingSource and the destination
	for (int y = y_start; y != y_end; y += y_iter) {
		for (int x = x_start; x != x_end; x += x_iter) {
			uint8_t *srcPkm;
			uint8_t *dstPkm;
			uint8_t tmpPkm1[PKMX_SIZE];
			uint8_t tmpPkm2[PKMX_SIZE];
			int srcIdx, dstIdx;

			if (guistate->holdIcons[y * 6 + x] == 0)
				continue;

			srcIdx = (y + sy) * 6 + (x + sx);
			dstIdx = (y + dy) * 6 + (x + dx);

			srcPkm = srcBoxData + srcIdx * srcGroup->pkmSize;
			dstPkm = dstBoxData + dstIdx * dstGroup->pkmSize;

			srcBoxIcons[srcIdx] = dstBoxIcons[dstIdx];
			dstBoxIcons[dstIdx] = guistate->holdIcons[y * 6 + x];

			pkm_to_pkmx(tmpPkm1, srcPkm, srcGroup->generation);
			pkm_to_pkmx(tmpPkm2, dstPkm, dstGroup->generation);

			// If unable to put a Pokemon down, keep it in holding
			if (!pkmx_convert_generation(tmpPkm1, dstGroup->generation))
				continue;
			if (!pkmx_convert_generation(tmpPkm2, srcGroup->generation))
				continue;

			// TODO Save any lost-in-conversion data when depositing to a game
			// ...after implementing any actual generation conversions
			pkmx_to_pkm(dstPkm, tmpPkm1, dstGroup->generation);
			pkmx_to_pkm(srcPkm, tmpPkm2, srcGroup->generation);

			// Clear this Pokemon from the holding list
			guistate->holdIcons[y * 6 + x] = 0;
			SpriteEntry *oam = &oamSub.oamMemory[OAM_INDEX_HOLDING + y * 6 + x];
			oam->attribute[0] = 0;
			oam->attribute[1] = 0;
			oam->attribute[2] = 0;
		}
	}

	int isStillHolding = 0;
	for (int i = 0; i < 30; i++) {
		if (guistate->holdIcons[i]) {
			isStillHolding = 1;
			break;
		}
	}

	if (!isStillHolding)
		guistate->flags = 0;

	display_box(guistate);
	update_cursor(guistate);
}

void open_boxes_gui() {
	struct boxgui_state *guistate;
	const int NUM_BOXES = 14;
	uint16_t box_name_buffer[9 * NUM_BOXES];
	uint16_t *box_names[NUM_BOXES];

	sysSetBusOwners(true, true);
	swiDelay(10);

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_SUB_SPRITE);

	initConsoles();
	clearConsoles();

	// Load box names
	{
		const uint8_t *box_name;
		box_name = GET_SAVEDATA_SECTION(13) + 0x744;
		// Box names can be up to 8 characters and always include a 0xFF terminator for 9 bytes
		for (int i = 0; i < NUM_BOXES; i++, box_name += 9) {
			uint16_t *out;
			out = box_names[i] = box_name_buffer + 9 * i;
			decode_gen3_string16(out, box_name, 9, activeGameLanguage);
		}
	}

	// Initial GUI state
	guistate = calloc(1, sizeof(struct boxgui_state));
	guistate->botScreen.boxNames = box_names;
	guistate->botScreen.boxWallpapers = GET_SAVEDATA_SECTION(13) + 0x7C2;
	guistate->botScreen.groupIdx = 0x40;
	guistate->botScreen.generation = 3;
	guistate->botScreen.numBoxes = 14;
	guistate->botScreen.pkmSize = PKM3_SIZE;
	guistate->botScreen.boxSizeBytes = PKM3_SIZE * 30;
	guistate->botScreen.activeBox = load_boxes_savedata(guistate->boxData1);
	guistate->botScreen.boxData = guistate->boxData1;
	guistate->botScreen.boxIcons = guistate->boxIcons1;
	guistate->topScreen.groupIdx = 0;
	guistate->topScreen.generation = 0;
	guistate->topScreen.numBoxes = 32;
	guistate->topScreen.boxData = guistate->boxData2;
	guistate->topScreen.boxIcons = guistate->boxIcons2;
	guistate->topScreen.pkmSize = PKMX_SIZE;
	guistate->topScreen.boxSizeBytes = PKMX_SIZE * 30;

	if (!sd_boxes_load(guistate->topScreen.boxData, 0, &guistate->topScreen.numBoxes)) {
		free(guistate);
		iprintf("Error loading from SD card\n");
		wait_for_button();
	}

	oamInit(&oamMain, SpriteMapping_1D_128, false);
	oamInit(&oamSub, SpriteMapping_1D_128, false);

	// Load all Pokemon box icon palettes into VRAM
	dmaCopy(getIconPaletteColors(0), (uint8_t*) SPRITE_PALETTE, 32 * 3);
	dmaCopy(getIconPaletteColors(0), (uint8_t*) SPRITE_PALETTE_SUB, 32 * 3);

	// Initial display
	load_cursor();
	resetTextLabels(1);
	decode_boxes(&guistate->botScreen);
	decode_boxes(&guistate->topScreen);
	display_box(guistate);
	update_cursor(guistate);
	oamUpdate(&oamMain);
	oamUpdate(&oamSub);
	keysSetRepeat(20, 10);

	for (;;) {
		KEYPAD_BITS keys;
		swiWaitForVBlank();

		scanKeys();
		keys = keysDown();
		if (keys & KEY_A) {
			if (guistate->flags & GUI_FLAG_HOLDING) {
				store_holding(guistate);
			} else if ((guistate->flags & GUI_FLAG_SELECTING) == 0) {
				start_selection(guistate);
			}
		} else if (keys & KEY_B) {
			if (guistate->flags & GUI_FLAG_HOLDING) {
				drop_holding(guistate);
			} else {
				break;
			}
		} else if (keys & KEY_X) {
			if ((guistate->flags & GUI_FLAG_SELECTING) == 0) {
				swap_screens(guistate);
			}
		}
		if ((keysHeld() & KEY_A) == 0 && (guistate->flags & GUI_FLAG_SELECTING)) {
			pickup_selection(guistate);
		}
		keys = (KEYPAD_BITS) keysDownRepeat();
		if (keys & (KEY_LEFT | KEY_RIGHT)) {
			move_cursor_x(guistate, (keys & KEY_LEFT) ? -1 : 1);
		} else if (keys & (KEY_UP | KEY_DOWN)) {
			move_cursor_y(guistate, (keys & KEY_UP) ? -1 : 1);
		} else if (keys & (KEY_L | KEY_R)) {
			if ((guistate->flags & GUI_FLAG_SELECTING) == 0)
				switch_box(guistate, (keys & KEY_L) ? -1 : 1);
		}
		oamUpdate(&oamMain);
		oamUpdate(&oamSub);
	}

	videoBgDisable(BG_LAYER_BUTTONS);
	videoBgDisable(BG_LAYER_WALLPAPER);
	videoBgDisableSub(BG_LAYER_BUTTONS);
	videoBgDisableSub(BG_LAYER_WALLPAPER);
	oamDisable(&oamMain);
	oamDisable(&oamSub);
	clearConsoles();
	selectTopConsole();
	write_boxes_savedata(guistate->boxData1);
	if (!sd_boxes_save(guistate->boxData2, 0, 32))
		wait_for_button();
	else if (!write_savedata()) {
		wait_for_button();
	}
	free(guistate);
	clearConsoles();
}

/*
 * Valid color values for GBA/DS 5-bits-per-channel:
	00 08 10 18 20 29 31 39 41 4a 52 5a 62 6a 73 7b
	83 8b 94 9c a4 ac b4 bd c5 cd d5 de e6 ee f6 ff
*/

