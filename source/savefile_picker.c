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
#include "text_draw.h"

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

static void update_savefile_preview(int loc) {
	// TODO
}

int savefilePicker(char *sav_path, const char *rom_path, size_t path_max) {
	int files[4];
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
			files[filec++] = i;
		}
	}

	if (!filec) {
		return filePicker(sav_path, path_max);
	}

	bgInit(BG_LAYER_BUTTONS, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_BUTTONS, BG_TILEBASE_BUTTONS);
	bgInitSub(BG_LAYER_BUTTONS, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_BUTTONS, BG_TILEBASE_BUTTONS);
	memset(BG_MAP_RAM(BG_MAPBASE_BUTTONS), 0, 2048);
	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	memcpy(BG_TILE_RAM(BG_TILEBASE_BUTTONS), generalTilesetTiles, sizeof(generalTilesetTiles));
	memcpy((uint8_t*) BG_PALETTE + 32 * 8, generalTilesetPal, sizeof(generalTilesetPal));
	memcpy(BG_TILE_RAM_SUB(BG_TILEBASE_BUTTONS), generalTilesetTiles, sizeof(generalTilesetTiles));
	memcpy((uint8_t*) BG_PALETTE_SUB + 32 * 8, generalTilesetPal, sizeof(generalTilesetPal));

	resetTextLabels(0);
	resetTextLabels(1);

	{
		textLabel_t headLabel = {1, 0, 0, 32};
		drawText(&headLabel, FONT_WHITE, FONT_BLACK, "Select a save file:");
		draw_gui_tilemap(&listHeader_map, 1, 0, 0);
	}

	for (int i = 0; i < filec; i++) {
		textLabel_t label = {1, 2, 2 + i * 4, 30};
		drawText(&label, FONT_WHITE, FONT_BLACK, descriptions[files[i]]);
		draw_gui_tilemap(&listUnselected_map, 1, 0, 2 + i * 4);
	}

	{
		textLabel_t browseLabel = {1, 2, 2 + filec * 4, 30};
		drawText(&browseLabel, FONT_WHITE, FONT_BLACK, "Browse...");
		draw_gui_tilemap(&listUnselected_map, 1, 0, 2 + filec * 4);
	}

	draw_gui_tilemap(&listSelected_map, 1, 0, 2 + selected * 4);
	update_savefile_preview(selected < filec ? files[selected] : -1);

	for (;;) {
		KEYPAD_BITS keys;
		swiWaitForVBlank();

		scanKeys();
		keys = keysDown();
		if (keys & KEY_A) {
			out = 1;
			break;
		} else if (keys & KEY_B) {
			out = 0;
			break;
		} else if (keys & KEY_DOWN) {
			draw_gui_tilemap(&listUnselected_map, 1, 0, 2 + 4 * selected);
			selected++;
			if (selected >= filec + 1) selected = 0;
			update_savefile_preview(selected < filec ? files[selected] : -1);
			draw_gui_tilemap(&listSelected_map, 1, 0, 2 + 4 * selected);
		} else if (keys & KEY_UP) {
			draw_gui_tilemap(&listUnselected_map, 1, 0, 2 + 4 * selected);
			selected--;
			if (selected < 0) selected = filec;
			update_savefile_preview(selected < filec ? files[selected] : -1);
			draw_gui_tilemap(&listSelected_map, 1, 0, 2 + 4 * selected);
		}
	}

	memset(BG_MAP_RAM(BG_MAPBASE_BUTTONS), 0, 2048);
	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	if (out) {
		if (selected >= filec) {
			out = filePicker(sav_path, path_max);
		} else {
			savefileLocationToPath(sav_path, rom_path, files[selected]);
		}
	}
	return out;
}

