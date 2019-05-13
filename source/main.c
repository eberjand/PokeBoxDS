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
	char *saveBuffer = NULL;
	
	PrintConsole bottomScreen;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&bottomScreen);

	struct ConsoleMenuItem top_menu[] = {
		{"Slot-2 GBA Cartridge", 0},
		{"SAV file on SD card", 1}
	};

	// DS only has 4MB RAM. This takes 128K (1/32) of it.
	// If RAM becomes a problem later, we should read on demand instead.
	saveBuffer = malloc(1024 * 128);

	for (;;) {
		int selected;
		int extra;
		FILE *fp = NULL;

		selected = console_menu_open("Load Pokemon save data from...", top_menu, 2, NULL, &extra);

		consoleSelect(&bottomScreen);
		consoleClear();

		if (extra == 0) {
			// Slot-2 GBA Cart
			const char* name = readSlot2Save((uint8_t*) saveBuffer);
			if (name) {
				strcpy(path, "GBA: ");
				strcat(path, name);
				fp = fmemopen(saveBuffer, 0x20000, "r");
			} else {
				iprintf("%s", saveBuffer);
			}
			selected = (name != NULL);
		} else {
			// SD card
			selected = filePicker(path, sizeof(path));
			if (selected)
				fp = fopen(path, "rb");
		}

		if (selected) {
			consoleSelect(&bottomScreen);
			consoleClear();
			sav_load(path, fp);
		}
		if (fp) {
			fclose(fp);
			fp = NULL;
		}
	}

	// Unreachable
	free(saveBuffer);
	return 0;
}
