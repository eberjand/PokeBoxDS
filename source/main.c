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

#include "ConsoleMenu.h"
#include "file_picker.h"
#include "sav_loader.h"
#include "slot2.h"
#include "util.h"

int main(int argc, char **argv) {
	char path[512];
	uint8_t *saveBuffer = NULL;
	
	PrintConsole bottomScreen;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&bottomScreen);

	if (!fatInitDefault()) {
		iprintf("fatInitDefault failure\n");
		wait_for_button();
	}

	struct ConsoleMenuItem top_menu[] = {
		{"Slot-2 GBA Cartridge", 0},
		{"SAV file on SD card", 1},
		{"Test", 2},
	};

	// DS only has 4MB RAM. This takes 128K (1/32) of it.
	// If RAM becomes a problem later, we should read on demand instead.
	saveBuffer = malloc(1024 * 128);

	for (;;) {
		int opening_save = 0;
		int extra;
		int gameId = -1;

		console_menu_open("Load Pokemon save data from...", top_menu,
			ARRAY_LENGTH(top_menu), NULL, &extra);

		consoleSelect(&bottomScreen);
		consoleClear();

		if (extra == 0) {
			// Slot-2 GBA Cart
			gameId = readSlot2Save(saveBuffer);
			if (gameId < 0) {
				iprintf("%s", saveBuffer);
			} else {
				const char *name = getGBAGameName(gameId);
				strcpy(path, "GBA: ");
				strcat(path, name);
				opening_save = 1;
			}
		} else if (extra == 1) {
			// SD card
			strcpy(path, "/");
			opening_save = filePicker(path, sizeof(path));
			if (opening_save) {
				FILE *fp = fopen(path, "rb");
				fread(saveBuffer, 1, 0x20000, fp);
				fclose(fp);
			}
		} else if (extra == 2) {
		}

		if (opening_save) {
			consoleSelect(&bottomScreen);
			consoleClear();
			sav_load(path, gameId, saveBuffer);
		}
	}

	// Unreachable
	free(saveBuffer);
	return 0;
}
