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
#include "game_menu.h"

#include "asset_manager.h"
#include "box_console.h"
#include "box_gui.h"
#include "ConsoleMenu.h"
#include "console_helper.h"
#include "savedata_gen3.h"
#include "util.h"

void game_menu_open(const char *filename) {
	struct ConsoleMenuItem top_menu[] = {
		{"Open PC boxes", 0},
		{"Show trainer info", 1},
		{"Open item bag", 2},
		{"Open PC boxes (Debug)", 3}
	};
	int selected;
	int extra = 0;

	selectTopConsole();

	for (;;) {
		selected = console_menu_open(filename ? filename : activeGameNameShort,
			top_menu, ARRAY_LENGTH(top_menu), NULL, &extra);
		if (!selected)
			break;
		if (extra == 0) {
			open_boxes_gui();
		} else if (extra == 1) {
			selectTopConsole();
			consoleClear();
			print_trainer_info();
			wait_for_button();
		} else if (extra == 2) {
		} else if (extra == 3) {
			open_boxes();
		}
	}
}
