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

#include <stdio.h>

#include "ConsoleMenu.h"
#include "file_picker.h"
#include "sav_loader.h"

int main(int argc, char **argv) {
	char path[512];
	int rc;
	
	PrintConsole bottomScreen;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&bottomScreen);

	struct ConsoleMenuItem top_menu[] = {
		{"Slot-2 GBA Cartridge", 0},
		{"SAV file on SD card", 1}
	};

	for (;;) {
		int selected;
		int extra;

		selected = console_menu_open("Load Pokemon save data from...", top_menu, 2, NULL, &extra);

		if (extra == 0) {
			// TODO
			consoleSelect(&bottomScreen);
			consoleClear();
			iprintf("GBA slot loading is currently\nunsupported\n");
			selected = 0;
		} else {
			selected = filePicker(path, sizeof(path));
		}

		if (selected) {
			consoleSelect(&bottomScreen);
			consoleClear();
			sav_load(path);
		}
	}

	return !rc;
}
