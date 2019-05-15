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
#include "sav_loader.h"
#include <nds.h>

#include <stdio.h>
#include <stdint.h>

#include "ConsoleMenu.h"
#include "box_console.h"
#include "box_gui.h"
#include "pokemon_strings.h"
#include "util.h"

/* Resources for data structure:
 * https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_in_Generation_III
 * https://bulbapedia.bulbagarden.net/wiki/Character_encoding_in_Generation_III
 */

int string_to_ascii(char *out, const uint8_t *str, int len) {
	const uint8_t map[] =
		" ..............................."
		"................................"
		"................................"
		"................................"
		"................................"
		".0123456789!?.-....''...,./ABCDE"
		"FGHIJKLMNOPQRSTUVWXYZabcdefghijk"
		"lmnopqrstuvwxyz................\0";
	for (int i = 0; i < len; i++) {
		uint8_t c = map[(uint8_t) str[i]];
		out[i] = c;
		if (!c)
			return i;
	}
	return len;
}

// FILE: open file handle for the sav
// slot_out: address of int where the most recent slot index (0 or 1) will be written
// sections_out: size_t[14] array that will hold the list of section offsets by ID
int verify_sav(const uint8_t *savedata, size_t *sections_out) {
	long section_offset = 0;
	int success = 1;
	uint16_t saveidx = 0;
	const uint16_t NUM_SECTIONS = 14;
	size_t sections[NUM_SECTIONS];
	union {
		uint8_t bytes[32];
		struct {
			uint8_t empty[16];
			uint32_t unknown_a;
			uint16_t section_id;
			uint16_t checksum;
			uint32_t saveidx;
		} __attribute__((packed));
	} footer;
	const uint16_t section_sizes[] = {
		0xf2c, // Trainer info
		0xf80, // Team/items
		0xf80, // Game state
		0xf80, // Misc data
		0xf08, // Rival info
		0xf80, // PC A
		0xf80, // PC B
		0xf80, // PC C
		0xf80, // PC D
		0xf80, // PC E
		0xf80, // PC F
		0xf80, // PC G
		0xf80, // PC H
		0x7d0, // PC I
	};
	//assert(sizeof(section_sizes) / sizeof(uint16_t) == NUM_SECTIONS);
	for (int slot = 0; slot < 2; slot++) {
		uint32_t slot_saveidx = 0;
		uint16_t populated_sections = 0;
		for (int sectionIdx = 0; sectionIdx < NUM_SECTIONS; sectionIdx++) {
			const uint8_t *section = NULL;
			uint32_t checksum = 0;
			size_t last_nonzero = 0;
			section = savedata + (slot * NUM_SECTIONS + sectionIdx) * 0x1000;
			for (long wordIdx = 0; wordIdx < 0xFE0 / 4; wordIdx++) {
				uint32_t word = GET32(section, wordIdx * 4);
				checksum += word;
				if (word)
					last_nonzero = wordIdx;
			}
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
			checksum &= 0xFFFF;
			memcpy(footer.bytes, section + 0xFE0, 32);
			if (footer.section_id >= NUM_SECTIONS) {
				iprintf("%04lX Invalid section ID: %hd\n", section_offset, footer.section_id);
				success = 0;
				break;
			}
			if (last_nonzero * 4 >= section_sizes[footer.section_id]) {
				iprintf("%04lX Section too large\n", section_offset);
				success = 0;
				break;
			}
			if (footer.checksum != checksum) {
				iprintf("%04lX Checksum mismatch\n", section_offset);
				success = 0;
				break;
			}
			if (sectionIdx != 0 && footer.saveidx != slot_saveidx) {
				iprintf("%04lX Save index mismatch\n", section_offset);
				success = 0;
				break;
			}
			if ((populated_sections & (1 << footer.section_id))) {
				iprintf("%04lX Duplicate section\n", section_offset);
				success = 0;
				break;
			}
			populated_sections |= 1 << footer.section_id;
			slot_saveidx = footer.saveidx;
			sections[footer.section_id] = section_offset;
			section_offset += 0x1000;
		}
		if (!success) break;
		if (slot == 0 || slot_saveidx > saveidx) {
			saveidx = slot_saveidx;
			if (sections_out)
				memcpy(sections_out, sections, sizeof(sections));
		}
	}
	return success;
}

int load_box_savedata(uint8_t *box_data, uint8_t *savedata, size_t *sections, int boxIdx) {
	size_t box_offset;

	// First 4 bytes of PC buffer is the most recently viewed PC box number
	if (boxIdx < 0)
		boxIdx = GET32(savedata, sections[5]);
	box_offset = boxIdx * BOX_SIZE_BYTES + 4;

	// The actual save data only stores 0xf80 bytes in each section.
	size_t section = 5 + box_offset / 0xf80;
	size_t box_mod = box_offset % 0xf80;
	if (box_mod <= 0xf80 - BOX_SIZE_BYTES) {
		// Only need to read this box's data from one section
		memcpy(box_data, savedata + sections[section] + box_mod, BOX_SIZE_BYTES);
	} else {
		// This box's data is split between two sections
		uint32_t bytesFromFirst = 0xf80 - box_mod;
		memcpy(box_data, savedata + sections[section] + box_mod, bytesFromFirst);
		memcpy(box_data + bytesFromFirst, savedata + sections[section + 1],
			BOX_SIZE_BYTES - bytesFromFirst);
	}
	return boxIdx;
}

void print_trainer_info(uint8_t *savedata, size_t section_offset) {
	uint8_t buf[256];
	char curString[16] = {0};
	uint32_t gameid;
	memcpy(buf, savedata + section_offset, sizeof(buf));
	gameid = GET32(buf, 0xAC);
	iprintf("Game: %s\n",
		(gameid == 0) ? "Ruby/Sapphire" :
		(gameid == 1) ? "FireRed/LeafGreen" : "Emerald");
	string_to_ascii(curString, buf, 7);
	iprintf("Name: %s\n", curString);
	iprintf("Gender: %s\n", buf[0x8] ? "F" : "M");
	iprintf("Trainer ID: %5d\n", (int) GET16(buf, 0xA));
	iprintf("Secret  ID: %5d\n", (int) GET16(buf, 0xC));
	iprintf("Play Time: %hd:%02hhd:%02hhd (+%02hhdf)\n",
		GET16(buf, 0xE), buf[0x10], buf[0x11], buf[0x12]);
}

int print_pokemon_details(const union pkm_t *pkm) {
	char nickname[12];
	char trainer[12];

	nickname[string_to_ascii(nickname, pkm->nickname, 10)] = 0;
	trainer[string_to_ascii(trainer,  pkm->trainerName, 7)] = 0;
	uint16_t pokedex_no = get_pokedex_number(pkm->species);
	if (pkm->species == 0) {
		iprintf("  0              (Empty Space)\n");
		return 0;
	}
	const char *species_name;
	species_name = get_species_name_by_index(pkm->species);
	if (pkm->language == 0x0601) {
		iprintf("%3d  EGG for a %s\n", pokedex_no, species_name);
	}
	else {
		unsigned lang = pkm->language;
		char *lang_str =
			(lang == 0x201) ? "JPN" :
			(lang == 0x202) ? "ENG" :
			(lang == 0x203) ? "FRE" :
			(lang == 0x204) ? "ITA" :
			(lang == 0x205) ? "GER" :
			(lang == 0x206) ? "KOR" :
			(lang == 0x207) ? "SPA" : "???";
		iprintf("%3d  %-10s  %-10s  %3s", pokedex_no, nickname, species_name, lang_str);
	}
	iprintf("OT  %-7s (%s) - %5ld [%5ld]\n",
		trainer, (pkm->origins & 0x8000) ? "F" : "M",
		pkm->trainerId & 0xFFFF, pkm->trainerId >> 16);
	const char *location_name = get_location_name(pkm->met_location);
	if (location_name)
		iprintf("Met: %-27s", location_name);
	else
		iprintf("Met: Invalid Location (%d\n", pkm->met_location);
	// TODO add level, experience, PP, friendship, ability, ribbons
	// TODO add origins info (ball, game, level)
	const char *item_name = get_item_name(pkm->held_item);
	if (item_name)
		iprintf("Item: %-26s", item_name);
	else
		iprintf("Item: Invalid (%d)\n", pkm->held_item);
	iprintf("\nMoves:\n");
	for (int i = 0; i < 4; i++) {
		// Because each move is printed to exactly 16 characters, these 4 moves
		// fill 2 lines perfectly without the need for any newlines
		const char *move_name = get_move_name(pkm->moves[i]);
		if (move_name)
			iprintf("  %-14s", move_name);
		else
			iprintf("  Invalid: %-3d  ", pkm->moves[i]);
	}
	iprintf("\nStat  HP Atk Def Spd SpA SpD\n  EV");
	for (int i = 0; i < 6; i++)
		iprintf(" %3d", (int) pkm->effort[i]);
	iprintf("\n  IV");
	for (int i = 0; i < 6; i++)
		iprintf("  %2ld", (pkm->IVs >> (5 * i)) & 0x1f);
	iprintf("\n");
	iprintf("\nContest stats:\n");
	iprintf(
		"Cool  %3d  Beaut %3d  Cute %3d\n"
		"Smart %3d  Tough %3d  Feel %3d\n",
		(int) pkm->contest[0],  (int) pkm->contest[1], (int) pkm->contest[2],
		(int) pkm->contest[3], (int) pkm->contest[4],  (int) pkm->contest[5]);
	iprintf("\nTechnical Data:\n");
	iprintf(" PID=%08lx  TID=%08lx\n", pkm->personality, pkm->trainerId);
	for (int i = 0; i < 4; i++) {
		if (i != 0) iprintf("\n");
		for (int j = 0; j < 3; j++) {
			iprintf(" ");
			for (int k = 0; k < 4; k++)
				iprintf("%02x", (int) pkm->bytes[32 + i * 12 + j * 4 + k]);
		}
	}
	return 0;
}

uint16_t decode_pkm_encrypted_data(uint8_t *pkm) {
	// There are 4 pkm sections that can be permutated in any order depending on the
	// personality value.
	// Encoded values: each byte in this list represents a possible ordering for the
	// data sections. Each of the 4 sections has a 2-bit index in the byte.
	const uint8_t data_order[] = {
		0xe4, 0xb4, 0xd8, 0x9c, 0x78, 0x6c,
		0xe1, 0xb1, 0xd2, 0x93, 0x72, 0x63,
		0xc9, 0x8d, 0xc6, 0x87, 0x4e, 0x4b,
		0x39, 0x2d, 0x36, 0x27, 0x1e, 0x1b
	};
	uint16_t checksum = 0;
	// The entire contents of the 48-byte encrypted section is xored with this
	uint32_t xor = GET32(pkm, 0) ^ GET32(pkm, 4);
	uint8_t reordered[48];

	for (int i = 32; i < 80; i += 4) {
		*((uint32_t*) (pkm + i)) ^= xor;
	}

	uint8_t order = data_order[GET32(pkm, 0) % 24];

	// Rearrange to a consistent order: Growth, Attacks, EVs/Condition, then Misc
	for (int i = 0; i < 4; i++)
		memcpy(reordered + i * 12, pkm + 32 + 12 * ((order >> (i * 2)) & 3), 12);
	memcpy(pkm + 32, reordered, 48);

	for (int i = 0; i < 48; i += 2)
		checksum += GET16(reordered, i);

	return checksum;
}


void sav_load(char *name, int gameId, uint8_t *savedata) {
	PrintConsole console;
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	consoleInit(&console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleSelect(&console);

	size_t sections[14];
	uint32_t key;
	if (!verify_sav(savedata, sections)){
		wait_for_button();
		return;
	}
	key = GET32(savedata, sections[0] + 0xAC);
	if (key == 1) {
		key = GET32(savedata, sections[0] + 0xAF8);
	}

	struct ConsoleMenuItem top_menu[] = {
		{"Open PC boxes", 0},
		{"Show trainer info", 1},
		{"Open item bag", 2},
		{"Open PC boxes (Debug)", 3}
	};
	int selected;
	int extra = 0;
	for (;;) {
		selected = console_menu_open(name, top_menu, ARRAY_LENGTH(top_menu), NULL, &extra);
		if (!selected)
			break;
		if (extra == 0) {
			open_boxes_gui(savedata, sections);
		} else if (extra == 1) {
			consoleSelect(&console);
			consoleClear();
			print_trainer_info(savedata, sections[0]);
			wait_for_button();
		} else if (extra == 2) {
		} else if (extra == 3) {
			open_boxes(savedata, sections);
		}
	}
}
