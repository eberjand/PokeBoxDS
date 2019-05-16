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
#include "cursor.h"
#include "sav_loader.h"

static PrintConsole topConsole;
static PrintConsole bottomConsole;

int activeSprite = 0;

// Each sprite is 2048 bytes
// Need to allocate enough space for two sprites because of Deoxys
uint8_t frontSpriteData[4096];

void info_display_update(const union pkm_t *pkm, uint16_t checksum) {
	consoleSelect(&bottomConsole);
	consoleClear();

	if (pkm->species == 0) {
		consoleSelect(&topConsole);
		iprintf("\x1b[0;0H%-10s\n", "");
		oamMain.oamMemory[32].attribute[0] = 0;
		oamMain.oamMemory[32].attribute[1] = 0;
		oamMain.oamMemory[32].attribute[2] = 0;
		return;
	}

	char nickname[12];
	u16 species = pkm->species;

	print_pokemon_details(pkm);

	species = pkm_displayed_species(pkm);
	if (checksum != pkm->checksum) {
		strcpy(nickname, "BAD EGG");
		species = 412;
	} else if (species == 412) {
		strcpy(nickname, "EGG");
	} else {
		string_to_ascii(nickname, pkm->nickname, 10);
		nickname[10] = 0;
	}

	consoleSelect(&topConsole);
	iprintf("\x1b[0;0H%-10s\n", nickname);

	uint8_t palette[32];
	readFrontImage(frontSpriteData, palette, species, pkm_is_shiny(pkm));

	memcpy((uint8_t*) SPRITE_PALETTE + 32 * (4 + activeSprite), palette, 32);
	memcpy((uint8_t*) SPRITE_GFX + 32 * 1024 + activeSprite * 2048, frontSpriteData, 2048);
	oamMain.oamMemory[32].attribute[0] = OBJ_Y(16) | ATTR0_COLOR_16;
	oamMain.oamMemory[32].attribute[1] = OBJ_X(8) | ATTR1_SIZE_64;
	oamMain.oamMemory[32].palette = 4 + activeSprite;
	oamMain.oamMemory[32].gfxIndex = 8 * 32 + activeSprite * 16;
	activeSprite ^= 1;

	consoleSelect(&topConsole);
}

void display_box_name(const char *name) {
	// Box sprites start at X=104 (after 13 8px tiles)
	// Box sprites start at Y=64 (after 8 8px tiles) so display on the line above
	iprintf("\x1b[7;13H%-8s", name);
}

void display_cursor() {
	oamInit(&oamMain, SpriteMapping_1D_128, false);
	oamMain.oamMemory[0].attribute[0] = OBJ_Y(64) | ATTR0_COLOR_16;
	oamMain.oamMemory[0].attribute[1] = OBJ_X(104) | ATTR1_SIZE_32;
	oamMain.oamMemory[0].palette = 0;
	oamMain.oamMemory[0].gfxIndex = 0;
	dmaCopy(cursorTiles, SPRITE_GFX, sizeof(cursorTiles));
}

void decode_box(uint8_t *box_data, uint16_t *checksums) {
	for (int i = 0; i < 30; i++) {
		union pkm_t *pkm = (union pkm_t*) (box_data + i * 80);
		uint16_t checksum = decode_pkm_encrypted_data(pkm->bytes);
		if (checksums)
			checksums[i] = checksum;
	}
}

int display_box(uint8_t *box_data, uint16_t *checksums) {
	int obj_idx = 0;

	for (int i = 0; i < 30; i++) {
		union pkm_t *pkm = (union pkm_t*) (box_data + i * 80);
		uint16_t species = pkm->species;

		// Skip anything with 0 in species field (empty space).
		// These entries may have leftover/garbage data in the other fields.
		if (species == 0)
			continue;

		species = pkm_displayed_species(pkm);
		if (checksums && checksums[i] != pkm->checksum)
			species = 412; // Egg icon for BAD EGG

		oamMain.oamMemory[obj_idx + 2].attribute[0] = OBJ_Y((i / 6) * 24 + 64) | ATTR0_COLOR_16;
		oamMain.oamMemory[obj_idx + 2].attribute[1] = OBJ_X((i % 6) * 24 + 104) | ATTR1_SIZE_32;
		oamMain.oamMemory[obj_idx + 2].palette = getIconPaletteIdx(species);
		oamMain.oamMemory[obj_idx + 2].gfxIndex = (i + 2) * 8;
		// Each 32x32@4bpp sprite is 512 bytes.
		// 2 animation frames at 512 bytes each = 1024 bytes per Pokemon.
		dmaCopy(getIconImage(species), (uint8_t*) SPRITE_GFX + (i + 2) * 1024, 1024);

		obj_idx++;
	}
	for (int i = obj_idx; i < 30; i++) {
		// Clear all unused OAM entries.
		oamMain.oamMemory[i + 2].attribute[0] = 0;
		oamMain.oamMemory[i + 2].attribute[1] = 0;
		oamMain.oamMemory[i + 2].attribute[2] = 0;
	}
	return obj_idx;
}

void open_boxes_gui(uint8_t *savedata, size_t *sections) {
	sysSetBusOwners(true, true);
	swiDelay(10);

	videoSetModeSub(MODE_0_2D);

	vramSetBankB(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);
	consoleSelect(&bottomConsole);
	consoleClear();
	consoleSelect(&topConsole);
	consoleClear();

	uint8_t box_data[BOX_SIZE_BYTES];
	uint16_t checksums[30];

	// Load box names
	const int NUM_BOXES = 14;
	char box_name_buffer[9 * NUM_BOXES];
	char *box_names[NUM_BOXES];
	char *box_name;
	memcpy(box_name_buffer, savedata + sections[13] + 0x744, sizeof(box_name_buffer));
	box_name = box_name_buffer;
	// Box names can be up to 8 characters and always include a 0xFF terminator for 9 bytes
	for (int i = 0; i < NUM_BOXES; i++, box_name += 9) {
		string_to_ascii(box_name, (uint8_t*) box_name, 9);
		box_names[i] = box_name;
	}

	// Initial GUI state
	int cursor_x = 0;
	int cursor_y = 0;
	int active_box = 0;
	int cur_poke = 0;
	active_box = load_box_savedata(box_data, savedata, sections, -1);

	// Load all Pokemon box icon palettes into VRAM
	dmaCopy(getIconPaletteColors(0), (uint8_t*) SPRITE_PALETTE, 32 * 3);

	// Initial display
	display_cursor();
	decode_box(box_data, checksums);
	display_box(box_data, checksums);
	display_box_name(box_names[active_box]);
	info_display_update((union pkm_t*) box_data + cur_poke, checksums[cur_poke]);
	oamUpdate(&oamMain);
	keysSetRepeat(20, 10);

	int frameTimer = 0;
	for (;;) {
		KEYPAD_BITS keys;
		swiWaitForVBlank();
		int cursor_moved = 0;

		// Toggle animation frames once per second
		frameTimer++;
		if (frameTimer == 60) {
			for (int i = 0; i < 30; i++)
				oamMain.oamMemory[i + 2].gfxIndex ^= 4;
			frameTimer = 0;
		}

		scanKeys();
		if (keysDown() & KEY_B)
			break;
		keys = (KEYPAD_BITS) keysDownRepeat();
		if (keys & (KEY_LEFT | KEY_RIGHT)) {
			cursor_x += (keys & KEY_LEFT) ? -1 : 1;
			if (cursor_x < 0)
				cursor_x = 5;
			else if (cursor_x > 5)
				cursor_x = 0;
			oamMain.oamMemory[0].x = cursor_x * 24 + 104;
			cursor_moved = 1;
		} else if (keys & (KEY_UP | KEY_DOWN)) {
			cursor_y += (keys & KEY_UP) ? -1 : 1;
			if (cursor_y < 0)
				cursor_y = 4;
			else if (cursor_y > 4)
				cursor_y = 0;
			oamMain.oamMemory[0].y = cursor_y * 24 + 64;
			cursor_moved = 1;
		} else if (keys & (KEY_L | KEY_R)) {
			active_box += (keys & KEY_L) ? -1 : 1;
			if (active_box < 0)
				active_box = 13;
			else if (active_box > 13)
				active_box = 0;
			load_box_savedata(box_data, savedata, sections, active_box);
			decode_box(box_data, checksums);
			display_box(box_data, checksums);
			display_box_name(box_names[active_box]);
			cursor_moved = 1;
		}
		if (cursor_moved) {
			cur_poke = cursor_y * 6 + cursor_x;
			info_display_update(&((union pkm_t*) box_data)[cur_poke], checksums[cur_poke]);
		}
		oamUpdate(&oamMain);
	}
}

/*
 * Valid color values for GBA/DS 5-bits-per-channel:
	00 08 10 18 20 29 31 39 41 4a 52 5a 62 6a 73 7b
	83 8b 94 9c a4 ac b4 bd c5 cd d5 de e6 ee f6 ff
*/

