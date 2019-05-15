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
#include "box_console.h"

#include <stdio.h>
#include <nds.h>

#include "ConsoleMenu.h"
#include "sav_loader.h"

struct hover_extra {
	union pkm_t *pkm;
	PrintConsole *console;
};

int hover_callback(char *str, int extra_int) {
	struct hover_extra *extra = (struct hover_extra*) extra_int;

	consoleSelect(extra->console);
	consoleClear();
	return print_pokemon_details(extra->pkm);
}

void open_box(char *name, uint8_t *box_data) {
	char nicknames[11 * 30] = {0}; // 30 pokemon, 10 characters + NUL each
	char *cur_nick = nicknames;
	struct ConsoleMenuItem box_menu[30];
	struct hover_extra extra_data[30];
	int box_size = 0;

	PrintConsole console;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	for (int i = 0; i < 30; i++, cur_nick += 11) {
		union pkm_t *pkm = (union pkm_t*) (box_data + (i * 80));
		uint16_t checksum = decode_pkm_encrypted_data(pkm->bytes);
		// Skip anything with 0 in species field (empty space).
		// These entries may have leftover/garbage data in the other fields.
		if (pkm->species == 0) {
			continue;
		}
		string_to_ascii(cur_nick, box_data + (i * 80) + 8, 10);
		if (checksum != pkm->checksum)
			strcpy(cur_nick, "BAD EGG");
		else if (pkm->language == 0x601)
			strcpy(cur_nick, "EGG");
		extra_data[i].pkm = pkm;
		extra_data[i].console = &console;
		box_menu[box_size].str = cur_nick;
		box_menu[box_size].extra = (int) &extra_data[i];
		box_size++;
	}
	for (;;) {
		int selected;
		int extra = 0;
		selected = console_menu_open_2(name, box_menu, box_size, &extra, &hover_callback);
		if (!selected)
			break;
	}
	consoleSelect(&console);
	consoleClear();
}

void open_boxes(uint8_t *savedata, size_t *sections) {
	const int NUM_BOXES = 14;
	char box_names[9 * NUM_BOXES];
	char *box_name;
	uint8_t box_data[BOX_SIZE_BYTES];
	struct ConsoleMenuItem box_menu[NUM_BOXES];
	memcpy(box_names, savedata + sections[13] + 0x744, sizeof(box_names));
	box_name = box_names;
	// Box names can be up to 8 characters and always include a 0xFF terminator for 9 bytes
	for (int i = 0; i < NUM_BOXES; i++, box_name += 9) {
		string_to_ascii(box_name, (uint8_t*) box_name, 9);
		box_menu[i].str = box_name;
		box_menu[i].extra = i;
	}
	for (;;) {
		int selected;
		int extra = 0;
		box_name = NULL;
		selected = console_menu_open("Open PC Box", box_menu, NUM_BOXES, &box_name, &extra);
		if (!selected)
			break;

		load_box_savedata(box_data, savedata, sections, extra);
		open_box(box_name, box_data);
	}
}
