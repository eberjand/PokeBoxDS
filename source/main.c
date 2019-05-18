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

#include "ConsoleMenu.h"
#include "console_helper.h"
#include "asset_manager.h"
#include "file_picker.h"
#include "sav_loader.h"
#include "slot2.h"
#include "util.h"

// DS only has 4MB RAM. This takes 128K (1/32) of it.
// If RAM becomes a problem later, we should read on demand instead.
uint8_t saveBuffer[1024 * 128];

void findRomAndSav(char *romPath_out, char *savPath_out, const char *path_in) {
	int len;

	// For now, this just looks for ROM+SAV in the same directory with the same base name.
	strcpy(romPath_out, path_in);
	strcpy(savPath_out, path_in);
	len = strlen(path_in);
	if (len > 4) {
		char *ext = romPath_out + len - 4;
		const char *basename;

		// keep the leading slash in basename for simpler concatenation
		basename = strrchr(romPath_out, '/');
		if (!basename)
			basename = romPath_out;

		if (!strcmp(ext, ".sav"))
			// Check for a .gba file in the same directory
			strcpy(romPath_out + len - 4, ".gba");
		else if (!strcmp(ext, ".gba")) {
			// Check for a .sav file in the same directory
			strcpy(savPath_out + len - 4, ".sav");
			if (access(savPath_out, F_OK) >= 0)
				return;

			// EZ-Flash IV and Omega put saves in the SAVER directory.
			strcpy(savPath_out, "/SAVER");
			strcat(savPath_out, basename);
			ext = strrchr(savPath_out, '.');
			strcpy(ext, ".sav");
			if (access(savPath_out, F_OK) >= 0)
				return;

			// GBA Exploader puts saves in the GBA_SAVE directory
			strcpy(savPath_out, "/GBA_SAVE");
			strcat(savPath_out, basename);
			ext = strrchr(savPath_out, '.');
			strcpy(ext, ".sav");
		}
	}
}

static char savPath[512] = {};
static char romPath[512] = {};

void openGameFromSD() {
	for (;;) {
		int gameId = 0;
		size_t bytesRead;
		FILE *fp;

		if (romPath[0] == 0)
			strcpy(romPath, "/");

		if (!filePicker(romPath, sizeof(romPath)))
			return;

		selectBottomConsole();
		findRomAndSav(romPath, savPath, romPath);

		fp = fopen(savPath, "rb");
		if (!fp) {
			iprintf("No save file found for:\n%s\n", romPath);
			wait_for_button();
			continue;
		}
		bytesRead = fread(saveBuffer, 1, 0x20000, fp);
		fclose(fp);

		// Save files are normally 128K, but the last 16K is unused.
		// Just in case any tools trim save files, we subtract 16K to get 0x1c000 (112K)
		if (bytesRead < 0x1c000) {
			iprintf("This isn't a valid save file.\n");
			wait_for_button();
			continue;
		}

		if (!assets_init_romfile(romPath))
			assets_init_placeholder();

		sav_load(savPath, gameId, saveBuffer);
	}
}

void openGameFromCart() {
	int gameId;

	gameId = readSlot2Save(saveBuffer);
	if (gameId < 0) {
		// Error message was written to the buffer
		iprintf("%s", saveBuffer);
		return;
	}

	const char *name = getGBAGameName(gameId);
	if (!assets_init_cart())
		assets_init_placeholder();

	sav_load(name, gameId, saveBuffer);

}

int main(int argc, char **argv) {
	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	initConsoles();
	selectBottomConsole();

	if (!fatInitDefault()) {
		iprintf("fatInitDefault failure\n");
		wait_for_button();
	}

	struct ConsoleMenuItem top_menu[] = {
		{"Slot-2 GBA Cartridge", 0},
		{"ROM/SAV file on SD card", 1}
	};

	for (;;) {
		int extra = -1;
		int selecting = 0;

		selecting = console_menu_open("Load Pokemon save data from...", top_menu,
			ARRAY_LENGTH(top_menu), NULL, &extra);
		if (!selecting)
			continue;

		selectBottomConsole();
		consoleClear();

		if (extra == 0)
			openGameFromCart();
		else if (extra == 1)
			openGameFromSD();
	}

	// Unreachable
	return 0;
}
