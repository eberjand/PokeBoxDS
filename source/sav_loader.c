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
#include "console_helper.h"
#include "box_console.h"
#include "box_gui.h"
#include "pokemon_strings.h"
#include "util.h"

/* Resources for data structure:
 * https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_in_Generation_III
 * https://bulbapedia.bulbagarden.net/wiki/Character_encoding_in_Generation_III
 */

// Look at gfx/font.png to see which glyph these character mappings correspond to.
// My custom 1-byte charset is a superset of printable ascii with
// Japanese Katakana and some other glyphs added. Note that there's not enough
// space for Hiragana, so all Hiragana appears as the corresponding Katakana.
static const uint8_t charmap_latin[] = {
' ',  0,    0,    0,    0x80, 0,    0x90, 0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x82, 0,    0,    0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    '&',  '+',  0,
0,    0,    0,    0,    0,    '=',  ';',  0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    0x09, 0x0a, 0x14, 0x15, 0x16, 0x17, 0,    0,    0,    0,    '%',  '(',  ')',  0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0x08, 0x09, 0x0b, 0x0a, 0,    0,    0,
0,    0,    0,    0,    '<',  '>',  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
0,    '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '!',  '?',  '.',  '-',  0xf1,
0x07, 0x08, '"',  '`',  '\'', 0x0b, 0x0c, 0xf9, ',',  0xfd, '/',  'A',  'B',  'C',  'D',  'E',
'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  'U',
'V',  'W',  'X',  'Y',  'Z',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',
'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  0x10,
':',  0x8e, 0x94, 0x9a, 0x84, 0x94, 0x9a, 0,    0,    0,    0,    0,    0,    0,    0,    0};

static const uint8_t charmap_jp[] = {
' ',  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE,
0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
0xEF, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE,
0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
0xEF, '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '!',  '?',  0xF0, '-',  0xF1,
0x07, 0xF2, 0xF3, 0xF4, 0xF5, 0x0B, 0x0C, 0xF8, ',',  0xFD, '/',  'A',  'B',  'C',  'D',  'E',
'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  'U',
'V',  'W',  'X',  'Y',  'Z',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',
'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  0x10,
':',  0x8e, 0x94, 0x9a, 0x84, 0x94, 0x9a, 0,    0,    0,    0,    0,    0,    0,    0,    0};

// This conversion is for arbitrary names that we don't know the language of.
// It's a "good enough" mapping that pretty much covers any valid nickname.
// There are just a few quirks:
//   * The Japanese brackets (bytes B1-B4 ingame) will appear as double/single quotes
//   * The French double angle quotes (bytes B1-B2 ingame) will appear as double ticks
//   * The Japanese period ("kuten" or "maru") will appear as the latin period
static const uint8_t charmap_mixed[] = {
' ',  0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE,
0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
0xEF, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE,
0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE,
0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
0xEF, '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '!',  '?',  '.',  '-',  0xf1,
0x07, 0x08, '"',  '`',  '\'', 0x0b, 0x0c, 0xf9, ',',  0xfd, '/',  'A',  'B',  'C',  'D',  'E',
'F',  'G',  'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  'U',
'V',  'W',  'X',  'Y',  'Z',  'a',  'b',  'c',  'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',
'l',  'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',  'x',  'y',  'z',  0x10,
':',  0x8e, 0x94, 0x9a, 0x84, 0x94, 0x9a, 0,    0,    0,    0,    0,    0,    0,    0,    0};

#define LANG_JAPANESE 0x201
#define LANG_ENGLISH 0x202
#define LANG_FRENCH 0x203
#define LANG_ITALIAN 0x204
#define LANG_GERMAN 0x205
#define LANG_SPANISH 0x207
int decode_gen3_string(char *out, const uint8_t *str, int len, uint16_t lang) {
	const uint8_t *map;
	if (lang == 0x201)
		map = charmap_jp;
	else if (lang >= 0x202 && lang <= 0x207 && lang != 0x206)
		map = charmap_latin;
	else
		map = charmap_mixed;

	int i;
	for (i = 0; i < len; i++) {
		uint8_t c = (uint8_t) str[i];
		if (c == 0xFF)
			break;
		// Only French has double angle quotes in place of double tickmark quotes
		if (lang == LANG_FRENCH && (c == 0xB1 || c == 0xB2))
			c = 0xFA + (c - 0xB1);
		else
			c = map[c];
		if (c)
			out[i] = c;
		else
			out[i] = '.';
	}
	out[i] = 0;
	return i;
}

// sections_out: size_t[14] array that will hold the list of section offsets by ID
int verify_sav(const uint8_t *savedata, size_t *sections_out) {
	long section_offset = 0;
	int success = 1;
	uint32_t saveidx = UINT32_MAX;
	const uint16_t NUM_SECTIONS = 14;
	size_t sections[NUM_SECTIONS];
	union {
		uint8_t bytes[32];
		struct {
			uint8_t empty[16];
			uint32_t unused;
			uint16_t section_id;
			uint16_t checksum;
			uint32_t signature; // Always 0x08012025
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
		uint32_t slot_saveidx = UINT32_MAX;
		uint16_t populated_sections = 0;
		int isAllFF = 1;
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
				if (word != 0xFFFFFFFF) {
					if (sectionIdx && isAllFF) {
						iprintf("%04lX Save slot has missing data\n", section_offset);
						success = 0;
						break;
					}
					isAllFF = 0;
				}
			}
			if (!success) break;
			// A new game starts with a save data of all FF bytes.
			// If the player has only used SAVE once, one slot is all FF and the other has data.
			if (isAllFF)
				continue;

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

		if (slot_saveidx != UINT32_MAX && slot_saveidx >= saveidx + 1) {
			saveidx = slot_saveidx;
			if (sections_out)
				memcpy(sections_out, sections, sizeof(sections));
		}
	}
	if (saveidx == UINT32_MAX) {
		iprintf("Save file appears to be empty.\n");
		success = 0;
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
	decode_gen3_string(curString, buf, 7, 0);
	iprintf("Name: %s\n", curString);
	iprintf("Gender: %s\n", buf[0x8] ? "F" : "M");
	iprintf("Trainer ID: %5d\n", (int) GET16(buf, 0xA));
	iprintf("Secret  ID: %5d\n", (int) GET16(buf, 0xC));
	iprintf("Play Time: %hd:%02hhd:%02hhd (+%02hhdf)\n",
		GET16(buf, 0xE), buf[0x10], buf[0x11], buf[0x12]);
}

int pkm_is_shiny(const union pkm_t *pkm) {
	uint16_t xor = 0;
	for (int i = 0; i < 4; i++)
		xor ^= GET16(pkm->bytes, i * 2);
	return xor < 8;
}

#define SPECIES_MISSINGNO 252 // The entire range of 252-276 is ?
#define SPECIES_UNOWN_A 201
#define SPECIES_UNOWN_B 413
#define SPECIES_EGG 412
#define SPECIES_MAX 439 // The last valid species is also Unown's Question Mark form
uint16_t pkm_displayed_species(const union pkm_t *pkm) {
	uint16_t species = pkm->species;
	uint32_t personality = pkm->personality;
	if (pkm->language == 0x601) {
		species = SPECIES_EGG;
	} else if (species == SPECIES_UNOWN_A) {
		uint32_t letterDet = 0;
		for (int i = 0; i < 4; i++)
			letterDet |= (personality >> (i * 6)) & (3 << (i * 2));
		letterDet %= 28;
		// Unown A is the default sprite, others are after Deoxys
		if (letterDet > 0)
			species = SPECIES_UNOWN_B - 1 + letterDet;
	} else if (species > SPECIES_MAX) {
		species = SPECIES_MISSINGNO;
	}
	return species;
}

int print_pokemon_details(const union pkm_t *pkm) {
	char nickname[12];
	char trainer[12];

	nickname[decode_gen3_string(nickname, pkm->nickname, 10, pkm->language)] = 0;
	trainer[decode_gen3_string(trainer, pkm->trainerName, 7, pkm->language)] = 0;
	uint16_t pokedex_no = get_pokedex_number(pkm->species);
	if (pkm->species == 0) {
		iprintf("  0              (Empty Space)\n");
		return 0;
	}
	const char *species_name;
	species_name = get_species_name_by_index(pkm->species);
	int is_shiny = pkm_is_shiny(pkm);
	if (pkm->language == 0x0601) {
		iprintf("%3d %cEGG for a %s\n", pokedex_no, is_shiny ? '*' : ' ', species_name);
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
		iprintf("%3d %c%-10s  %-10s  %3s",
			pokedex_no, is_shiny ? '*' : ' ', nickname, species_name, lang_str);
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


void sav_load(const char *name, int gameId, uint8_t *savedata) {
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	initConsoles();
	selectTopConsole();

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
			selectTopConsole();
			consoleClear();
			print_trainer_info(savedata, sections[0]);
			wait_for_button();
		} else if (extra == 2) {
		} else if (extra == 3) {
			open_boxes(savedata, sections);
		}
	}
}
