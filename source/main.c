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
#include <nds.h>
#include <fat.h>

#include <stdio.h>
#include <unistd.h>

//#include "ConsoleMenu.h"
#include "asset_manager.h"
#include "box_gui.h"
#include "console_helper.h"
#include "file_picker.h"
#include "list_menu.h"
#include "savedata_gen3.h"
#include "savefile_picker.h"
#include "util.h"

static char savPath[512] = {};
static char romPath[512] = {};

void openGameFromSD() {
	for (;;) {
		int rc;

		if (romPath[0] == 0)
			strcpy(romPath, "/");

		if (!file_picker(romPath, sizeof(romPath), FILE_FILTER_ROM, "Select a ROM file"))
			break;

		selectBottomConsole();
		if (!savefilePicker(savPath, romPath, sizeof(savPath)))
			break;

		if (!assets_init_romfile(romPath))
			assets_init_placeholder();

		rc = load_savedata(savPath);
		if (!rc) {
			wait_for_button();
			continue;
		}

		open_boxes_gui();
		break;
	}
}

void openGameFromCart() {
	if (!assets_init_cart()) {
		if ((int8_t) GBA_HEADER.gamecode[0] <= 0) {
			iprintf("Error: No GBA cartridge found.\n");
		} else {
			iprintf(
				"Unsupported GBA game cart.\n"
				"Title: %.12s\n"
				"Code:  %.4s Rev %d\n",
				GBA_HEADER.title, GBA_HEADER.gamecode, GBA_HEADER.version);
		}
		wait_for_button();
		return;
	}

	if (!load_savedata(NULL)) {
		wait_for_button();
		return;
	}

	open_boxes_gui();
}

int main(int argc, char **argv) {
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankD(VRAM_B_MAIN_SPRITE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankD(VRAM_D_SUB_SPRITE);

	initConsoles();
	selectBottomConsole();

	if (!fatInitDefault()) {
		iprintf("fatInitDefault failure\n");
		wait_for_button();
	}

	struct ListMenuItem top_menu_items[] = {
		{"Slot-2 GBA Cartridge"},
		{"ROM/SAV file on SD card"}
	};

	struct ListMenuConfig top_menu_cfg = {
		.header1 = "Load Pokémon save data from...",
		.items = top_menu_items,
		.size = ARRAY_LENGTH(top_menu_items)
	};

	for (;;) {
		int selecting;

		selecting = list_menu_open(&top_menu_cfg);
		if (selecting < 0)
			continue;

		selectBottomConsole();
		consoleClear();

		if (selecting == 0)
			openGameFromCart();
		else if (selecting == 1)
			openGameFromSD();
	}

	// Unreachable
	return 0;
}
