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
#include "savedata_gen3.h"

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
 * C000-FFFF UI overlays tile data
 *
 * BG palettes for each screen:
 * 00    Console text
 * 04-07 Current box wallpaper
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

struct boxgui_state {
	char **boxNames;
	uint8_t *boxWallpapers;
	int8_t cursor_x;
	int8_t cursor_y;
	uint8_t flags;
	uint8_t cursorMode; //TODO
	int8_t activeBox;
	int8_t activeBox2; //TODO
	uint8_t numBoxes;
	uint8_t numBoxes2; //TODO
	int8_t holdingSourceBox;
	uint8_t holdingSourceGroup; //TODO
	int8_t holdingSource_x;
	int8_t holdingSource_y;
	int8_t holdingMin_x;
	int8_t holdingMax_x;
	int8_t holdingMin_y;
	int8_t holdingMax_y;
	uint16_t boxIcons[30 * 64];
	uint16_t holdIcons[30];
	pkm3_t boxData[30 * 14];
};

static void status_display_update(const union pkm_t *pkm_in) {
	pkm3_t pkm;
	uint16_t checksum;
	u16 species;
	char nickname[12];
	uint8_t palette[32];

	selectBottomConsole();
	consoleClear();

	checksum = decode_pkm_encrypted_data(&pkm, pkm_in->bytes);

	if (pkm.species == 0) {
		selectTopConsole();
		iprintf("\x1b[0;0H%-10s\n", "");
		oamMain.oamMemory[OAM_INDEX_BIGSPRITE].attribute[0] = 0;
		oamMain.oamMemory[OAM_INDEX_BIGSPRITE].attribute[1] = 0;
		oamMain.oamMemory[OAM_INDEX_BIGSPRITE].attribute[2] = 0;
		return;
	}

	print_pokemon_details(&pkm);

	species = pkm_displayed_species(&pkm);
	if (checksum != pkm.checksum) {
		strcpy(nickname, "Bad EGG");
		species = 412;
	} else if (species == 412) {
		strcpy(nickname, "EGG");
	} else {
		decode_gen3_string(nickname, pkm.nickname, 10, pkm.language);
		nickname[10] = 0;
	}

	selectTopConsole();
	iprintf("\x1b[0;0H%-10s\n", nickname);

	readFrontImage(frontSpriteData, palette, species, pkm_is_shiny(&pkm));

	memcpy((uint8_t*) SPRITE_PALETTE + 32 * (4 + activeSprite), palette, 32);
	memcpy((uint8_t*) SPRITE_GFX + OBJ_GFXIDX_BIGSPRITE * 128 + activeSprite * 2048,
		frontSpriteData, 2048);
	oamMain.oamMemory[OAM_INDEX_BIGSPRITE].attribute[0] = OBJ_Y(16) | ATTR0_COLOR_16;
	oamMain.oamMemory[OAM_INDEX_BIGSPRITE].attribute[1] = OBJ_X(8) | ATTR1_SIZE_64;
	oamMain.oamMemory[OAM_INDEX_BIGSPRITE].palette = 4 + activeSprite;
	oamMain.oamMemory[OAM_INDEX_BIGSPRITE].gfxIndex = OBJ_GFXIDX_BIGSPRITE + activeSprite * 16;
	activeSprite ^= 1;
}

static void display_cursor() {
	oamMain.oamMemory[0].attribute[0] = OBJ_Y(60) | ATTR0_COLOR_16;
	oamMain.oamMemory[0].attribute[1] = OBJ_X(100) | ATTR1_SIZE_32;
	oamMain.oamMemory[0].palette = 0;
	oamMain.oamMemory[0].gfxIndex = 0;
	dmaCopy(cursorTiles, SPRITE_GFX, sizeof(cursorTiles));
}

static int display_icon_sprites(
	const uint16_t *speciesList, int oamIndex, int gfxIndex, int x, int y) {

	int obj_idx = 0;

	for (int i = 0; i < 30; i++) {
		uint16_t species = speciesList[i];
		SpriteEntry *oam = &oamMain.oamMemory[oamIndex + i];

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
			(uint8_t*) SPRITE_GFX + gfxIndex * 128 + i * 1024,
			1024);

		obj_idx++;
	}
	return obj_idx;
}

static void move_icon_sprites(int oamIndex, int x, int y) {
	for (int i = 0; i < 30; i++) {
		SpriteEntry *oam = &oamMain.oamMemory[oamIndex + i];

		if (oam->attribute[0] == 0)
			continue;

		oam->attribute[0] = OBJ_Y((i / 6) * 24 + y) | ATTR0_COLOR_16;
		oam->attribute[1] = OBJ_X((i % 6) * 24 + x) | ATTR1_SIZE_32;
	}
}

static void clear_icon_sprites(int oamIndex) {
	for (int i = 0; i < 30; i++) {
		SpriteEntry *oam = &oamMain.oamMemory[oamIndex + i];
		oam->attribute[0] = 0;
		oam->attribute[1] = 0;
		oam->attribute[2] = 0;
	}
}

static void decode_boxes(struct boxgui_state *guistate) {
	uint16_t checksum;
	uint16_t species;
	pkm3_t pkm;
	for (int pkmIdx = 0; pkmIdx < 30 * 14; pkmIdx++) {
		const uint8_t *bytes = guistate->boxData[pkmIdx].bytes;
		checksum = decode_pkm_encrypted_data(&pkm, bytes);
		if (checksum != pkm.checksum)
			species = 412; // Egg icon for Bad EGG
		else
			species = pkm_displayed_species(&pkm);
		guistate->boxIcons[pkmIdx] = species;
	}
}

static void update_cursor(struct boxgui_state *guistate) {
	int cur_poke = guistate->cursor_y * 6 + guistate->cursor_x;
	oamMain.oamMemory[0].x = guistate->cursor_x * 24 + 100;
	oamMain.oamMemory[0].y = guistate->cursor_y * 24 + 60;
	if (guistate->flags & GUI_FLAG_HOLDING) {
		move_icon_sprites(OAM_INDEX_HOLDING,
			100 + guistate->holdingMin_x * 24,
			48 + guistate->holdingMin_y * 24);
	} else {
		status_display_update(guistate->boxData + guistate->activeBox * 30 + cur_poke);
	}
	if (guistate->flags & (GUI_FLAG_SELECTING | GUI_FLAG_HOLDING)) {
		// TODO draw colored background behind the selection
	}
}

static int display_box(const struct boxgui_state *guistate) {
	char *name = guistate->boxNames[guistate->activeBox];
	int wallpaper = guistate->boxWallpapers[guistate->activeBox];

	selectTopConsole();
	iprintf("\x1b[7;16H\x1b[30;0m%-8s\x1b[39;0m", name);

	if (loadWallpaper(wallpaper)) {
		int wallpaperPalOffset = 4;
		bgInit(BG_LAYER_WALLPAPER, BgType_Text4bpp, BgSize_T_256x256,
			BG_MAPBASE_WALLPAPER, BG_TILEBASE_WALLPAPER);
		// This memset is placed after loadWallpaper to prevent tearing when loading
		// from ROM files, which is much slower than reading from Slot-2 carts.
		memset(BG_MAP_RAM(BG_MAPBASE_WALLPAPER), 0, 2048);
		memcpy(BG_TILE_RAM(BG_TILEBASE_WALLPAPER), wallpaperTiles, sizeof(wallpaperTiles));
		memcpy((uint8_t*) BG_PALETTE + 32 * wallpaperPalOffset,
			wallpaperPal, sizeof(wallpaperPal));
		for (int rowIdx = 0; rowIdx < 18; rowIdx++) {
			for (int colIdx = 0; colIdx < 20; colIdx++) {
				uint16_t tspec = wallpaperTilemap[rowIdx * 20 + colIdx];
				uint8_t pal = tspec >> 12;
				if (pal)
					pal += wallpaperPalOffset - 1;
				tspec = (pal << 12) | (tspec & 0xFFF);
				BG_MAP_RAM(BG_MAPBASE_WALLPAPER)[(rowIdx + 6) * 32 + colIdx + 12] = tspec;
			}
		}
	} else {
		memset(BG_MAP_RAM(BG_MAPBASE_WALLPAPER), 0, 2048);
	}

	return display_icon_sprites(
		guistate->boxIcons + guistate->activeBox * 30,
		OAM_INDEX_CURBOX, OBJ_GFXIDX_CURBOX,
		100, 60);
}

static int switch_box(struct boxgui_state *guistate, int rel) {
	int activeBox = guistate->activeBox + rel;
	if (activeBox < 0)
		activeBox = 13;
	else if (activeBox > 13)
		activeBox = 0;
	guistate->activeBox = activeBox;
	display_box(guistate);
	update_cursor(guistate);
	return 1;
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
}

static void pickup_selection(struct boxgui_state *guistate) {
	uint8_t flags = GUI_FLAG_HOLDING;
	int width  = guistate->holdingMax_x - guistate->holdingMin_x + 1;
	int height = guistate->holdingMax_y - guistate->holdingMin_y + 1;
	int dx = guistate->holdingMin_x;
	int dy = guistate->holdingMin_y;
	uint16_t *curBoxIcons = guistate->boxIcons + 30 * guistate->activeBox;

	if (width * height > 1)
		flags |= GUI_FLAG_HOLDING_MULTIPLE;
	guistate->flags = flags;
	guistate->holdingSourceBox = guistate->activeBox;
	guistate->holdingSource_x = guistate->holdingMin_x;
	guistate->holdingSource_y = guistate->holdingMin_y;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			guistate->holdIcons[y * 6 + x] = curBoxIcons[(y + dy) * 6 + (x + dx)];
			curBoxIcons[(y + dy) * 6 + (x + dx)] = 0;
		}
	}
	display_icon_sprites(
		guistate->holdIcons, OAM_INDEX_HOLDING, OBJ_GFXIDX_HOLDING,
		100 + 24 * dx, 48 + 24 * dy);
	display_box(guistate);
}

static void drop_holding(struct boxgui_state *guistate) {
	int width  = guistate->holdingMax_x - guistate->holdingMin_x + 1;
	int height = guistate->holdingMax_y - guistate->holdingMin_y + 1;
	int sx = guistate->holdingSource_x;
	int sy = guistate->holdingSource_y;
	uint16_t *srcBoxIcons = guistate->boxIcons + 30 * guistate->holdingSourceBox;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			srcBoxIcons[(y + sy) * 6 + (x + sx)] = guistate->holdIcons[y * 6 + x];
			guistate->holdIcons[y * 6 + x] = 0;
		}
	}

	guistate->cursor_x += guistate->holdingSource_x - guistate->holdingMin_x;
	guistate->cursor_y += guistate->holdingSource_y - guistate->holdingMin_y;
	guistate->activeBox = guistate->holdingSourceBox;
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
	uint16_t *dstBoxIcons = guistate->boxIcons + guistate->activeBox * 30;
	uint16_t *srcBoxIcons = guistate->boxIcons + guistate->holdingSourceBox * 30;
	pkm3_t *dstBoxData = guistate->boxData + guistate->activeBox * 30;
	pkm3_t *srcBoxData = guistate->boxData + guistate->holdingSourceBox * 30;

	if (guistate->flags & GUI_FLAG_HOLDING_MULTIPLE) {
		// Do nothing if any spot in the destination is occupied.
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				if (dstBoxIcons[(y + dy) * 6 + (x + dx)])
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
			pkm3_t tmpPkm;
			int srcIdx, dstIdx;

			srcIdx = (y + sy) * 6 + (x + sx);
			dstIdx = (y + dy) * 6 + (x + dx);

			srcBoxIcons[srcIdx] = dstBoxIcons[dstIdx];
			dstBoxIcons[dstIdx] = guistate->holdIcons[y * 6 + x];

			tmpPkm = dstBoxData[dstIdx];
			dstBoxData[dstIdx] = srcBoxData[srcIdx];
			srcBoxData[srcIdx] = tmpPkm;
		}
	}

	guistate->flags = 0;
	memset(guistate->holdIcons, 0, sizeof(guistate->holdIcons));
	clear_icon_sprites(OAM_INDEX_HOLDING);

	display_box(guistate);
	update_cursor(guistate);
}

void open_boxes_gui() {
	struct boxgui_state *guistate;
	const int NUM_BOXES = 14;
	char box_name_buffer[9 * NUM_BOXES];
	char *box_names[NUM_BOXES];
	char *box_name;

	sysSetBusOwners(true, true);
	swiDelay(10);

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);

	oamInit(&oamMain, SpriteMapping_1D_128, false);

	initConsoles();
	clearConsoles();

	// Load box names
	memcpy(box_name_buffer, GET_SAVEDATA_SECTION(13) + 0x744, sizeof(box_name_buffer));
	box_name = box_name_buffer;

	// Box names can be up to 8 characters and always include a 0xFF terminator for 9 bytes
	for (int i = 0; i < NUM_BOXES; i++, box_name += 9) {
		decode_gen3_string(box_name, (uint8_t*) box_name, 9, 0);
		box_names[i] = box_name;
	}

	// Initial GUI state
	guistate = calloc(1, sizeof(struct boxgui_state));
	guistate->boxNames = box_names;
	guistate->boxWallpapers = GET_SAVEDATA_SECTION(13) + 0x7C2;
	guistate->activeBox = load_boxes_savedata((uint8_t*) guistate->boxData);

	// Load all Pokemon box icon palettes into VRAM
	dmaCopy(getIconPaletteColors(0), (uint8_t*) SPRITE_PALETTE, 32 * 3);

	// Initial display
	display_cursor();
	decode_boxes(guistate);
	display_box(guistate);
	status_display_update(guistate->boxData + 30 * guistate->activeBox);
	oamUpdate(&oamMain);
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
	}

	free(guistate);
	clearConsoles();
}

/*
 * Valid color values for GBA/DS 5-bits-per-channel:
	00 08 10 18 20 29 31 39 41 4a 52 5a 62 6a 73 7b
	83 8b 94 9c a4 ac b4 bd c5 cd d5 de e6 ee f6 ff
*/

