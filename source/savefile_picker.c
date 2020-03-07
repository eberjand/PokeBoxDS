/*
 * This file is part of the PokeBoxDS project.
 * Copyright (C) 2020 Jennifer Berringer
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
#include "savefile_picker.h"

#include <string.h>
#include <unistd.h>
#include <nds.h>

#include "file_picker.h"
#include "gui_util.h"
#include "list_menu.h"
//#include "text_draw.h"

#include "generalTileset.h"
#include "listHeader_map.h"
#include "listSelected_map.h"
#include "listUnselected_map.h"

enum savefile_location {
	// At /dirname/basename.sav; same directory as the ROM file
	SAVEFILE_SAMEDIR,
	// At /SAVER/basename.sav for EZ-Flash IV (and probably others)
	SAVEFILE_SAVER,
	// At /GAMESAVE/basename.dat for M3 Perfect
	SAVEFILE_GAMESAVE,
	// At /GBA_SAVE/basename.sav for GBA Exploader
	SAVEFILE_GBASAVE,
	SAVEFILE_LOC_MAX
};

static void savefileLocationToPath(char *sav_path, const char *rom_path, int loc) {
	const char *basename, *ext;

	basename = strrchr(rom_path, '/');
	if (!basename)
		basename = rom_path;
	else
		basename++;

	ext = strrchr(rom_path, '.');
	if (ext <= basename)
		ext = basename + strlen(basename);

	if (loc == SAVEFILE_SAMEDIR) {
		strcpy(sav_path, rom_path);
		strcpy(sav_path + (ext - rom_path), ".sav");
	} else if (loc == SAVEFILE_SAVER) {
		static const char dirStr[] = "/SAVER/";
		strcpy(sav_path, dirStr);
		strcpy(sav_path + sizeof(dirStr) - 1, basename);
		strcpy(sav_path + sizeof(dirStr) - 1 + (ext - basename), ".sav");
	} else if (loc == SAVEFILE_GAMESAVE) {
		static const char dirStr[] = "/GAMESAVE/";
		strcpy(sav_path, dirStr);
		strcpy(sav_path + sizeof(dirStr) - 1, basename);
		strcpy(sav_path + sizeof(dirStr) - 1 + (ext - basename), ".dat");
	} else if (loc == SAVEFILE_GBASAVE) {
		static const char dirStr[] = "/GBA_SAVE/";
		strcpy(sav_path, dirStr);
		strcpy(sav_path + sizeof(dirStr) - 1, basename);
		strcpy(sav_path + sizeof(dirStr) - 1 + (ext - basename), ".sav");
	} else {
		sav_path[0] = 0;
	}
}

int savefilePicker(char *sav_path, const char *rom_path, size_t path_max) {
	struct ListMenuItem menu_items[5];
	int filec = 0;
	int selected = 0;
	int out = 0;

	static const char *descriptions[] = {
		"SAV file in current directory",
		"SAV file in SAVER directory",
		"DAT file in GAMESAVE directory",
		"SAV file in GBA_SAVE directory"
	};

	for (int i = 0; i < SAVEFILE_LOC_MAX; i++) {
		savefileLocationToPath(sav_path, rom_path, i);
		if (access(sav_path, F_OK) >= 0) {
			menu_items[filec].str = descriptions[i];
			menu_items[filec].extra = i;
			filec++;
		}
	}

	if (!filec) {
		strcpy(sav_path, "/");
		return file_picker(sav_path, path_max, FILE_FILTER_SAV, "Select a SAV file");
	}

	// Always add a file browse option
	menu_items[filec].str = "Browse...";
	menu_items[filec].extra = -1;
	filec++;

	for (;;) {
		struct ListMenuConfig menu_cfg = {
			.header1 = "Select a save file:",
			.items = menu_items,
			.size = filec
		};
		selected = list_menu_open(&menu_cfg);

		if (selected < 0) {
			out = 0;
			break;
		}

		if (menu_items[selected].extra == -1) {
			strcpy(sav_path, "/");
			out = file_picker(sav_path, path_max, FILE_FILTER_SAV, "Select a SAV file");
			if (out)
				break;
		} else {
			savefileLocationToPath(sav_path, rom_path, menu_items[selected].extra);
			out = 1;
			break;
		}
	}
	return out;
}

