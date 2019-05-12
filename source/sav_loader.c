#include <nds.h>

#include <stdio.h>

#include "ConsoleMenu.h"

/* Resources for data structure:
 * https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_in_Generation_III
 * https://bulbapedia.bulbagarden.net/wiki/Character_encoding_in_Generation_III
 */

typedef uint8_t u8;

int string_to_ascii(char *out, u8 *str, int len) {
	const u8 map[] =
		" ..............................."
		"................................"
		"................................"
		"................................"
		"................................"
		".0123456789!?.-....''...,./ABCDE"
		"FGHIJKLMNOPQRSTUVWXYZabcdefghijk"
		"lmnopqrstuvwxyz................\0";
	for (int i = 0; i < len; i++) {
		u8 c = map[(uint8_t) str[i]];
		out[i] = c;
		if (!c)
			return i;
	}
	return len;
}

uint16_t get16(u8 *bytes, long pos) {
	return *((uint16_t*) (bytes + pos));
}

uint32_t get32(u8 *bytes, long pos) {
	return *((uint32_t*) (bytes + pos));
}

// FILE: open file handle for the sav
// slot_out: address of int where the most recent slot index (0 or 1) will be written
// sections_out: size_t[14] array that will hold the list of section offsets by ID
int verify_sav(FILE *fp, size_t *sections_out) {
	long section_offset = 0;
	int success = 1;
	uint16_t saveidx = 0;
	u8 *section = NULL;
	uint32_t *words;
	const uint16_t NUM_SECTIONS = 14;
	size_t sections[NUM_SECTIONS];
	union {
		u8 bytes[32];
		struct {
			u8 empty[16];
			uint32_t unknown_a;
			uint16_t section_id;
			uint16_t checksum;
			uint32_t saveidx;
		};
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
	section = malloc(0x1000); // 4K
	words = (uint32_t*) section;
	rewind(fp);
	for (int slot = 0; slot < 2; slot++) {
		uint32_t slot_saveidx = 0;
		uint16_t populated_sections = 0;
		for (int sectionIdx = 0; sectionIdx < NUM_SECTIONS; sectionIdx++) {
			uint32_t checksum = 0;
			size_t last_nonzero = 0;
			fread(section, 1, 0x1000, fp);
			for (long wordIdx = 0; wordIdx < 0xFE0 / 4; wordIdx++) {
				uint32_t word = words[wordIdx];
				checksum += word;
				if (word)
					last_nonzero = wordIdx;
			}
			checksum = (checksum & 0xFFFF) + (checksum >> 16);
			checksum &= 0xFFFF;
			memcpy(footer.bytes, section + 0xFE0, 32);
			if (footer.section_id >= NUM_SECTIONS) {
				iprintf("%04lX Invalid section ID: %hd", section_offset, footer.section_id);
				success = 0;
				break;
			}
			if (last_nonzero * 4 >= section_sizes[footer.section_id]) {
				iprintf("%04lX Section too large", section_offset);
				success = 0;
				break;
			}
			if (footer.checksum != checksum) {
				iprintf("%04lX Checksum mismatch", section_offset);
				success = 0;
				break;
			}
			if (sectionIdx != 0 && footer.saveidx != slot_saveidx) {
				iprintf("%04lX Save index mismatch", section_offset);
				success = 0;
				break;
			}
			if ((populated_sections & (1 << footer.section_id))) {
				iprintf("%04lX Duplicate section", section_offset);
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
	free(section);
	return success;
}

void print_trainer_info(FILE *fp, size_t section_offset) {
	u8 buf[256];
	char curString[16] = {0};
	uint32_t gameid;
	fseek(fp, section_offset, SEEK_SET);
	fread(buf, 1, sizeof(buf), fp);
	gameid = get32(buf, 0xAC);
	iprintf("Game: %s\n",
		(gameid == 0) ? "Ruby/Sapphire" :
		(gameid == 1) ? "FireRed/LeafGreen" : "Emerald");
	string_to_ascii(curString, buf, 7);
	iprintf("Name: %s\n", curString);
	iprintf("Gender: %s\n", buf[0x8] ? "F" : "M");
	iprintf("Trainer ID: %5d\n", (int) get16(buf, 0xA));
	iprintf("Secret  ID: %5d\n", (int) get16(buf, 0xC));
	iprintf("Play Time: %hd:%02hhd:%02hhd (+%02hhdf)\n",
		get16(buf, 0xE), buf[0x10], buf[0x11], buf[0x12]);
}

void wait_for_button() {
	iprintf("Press any button to continue...");
	for (;;) {
		swiWaitForVBlank();
		scanKeys();
		if (keysDown()) break;
	}
}

struct hover_extra {
	u8 *pkm;
	PrintConsole *console;
};

#include "species.h"

int hover_callback(char *str, int extra_int) {
	struct hover_extra *extra = (struct hover_extra*) extra_int;
	char nickname[12];
	char trainer[12];
	union pkm_t {
		u8 bytes[80];
		struct {
			uint32_t personality;
			uint32_t trainerId;
			u8 nickname[10];
			uint16_t language;
			u8 trainerName[7];
			uint8_t marking;
			uint16_t checksum;
			uint16_t unknown;
			u8 encrypted[48];
		} __attribute__((packed));
	};
	union pkm_t *pkm = (union pkm_t*) extra->pkm;
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
	uint8_t order = data_order[pkm->personality % 24];
	
	uint8_t decrypted[48];
	uint32_t xor = pkm->personality ^ pkm->trainerId;

	string_to_ascii(nickname, pkm->nickname, sizeof(pkm->nickname));
	string_to_ascii(trainer,  pkm->trainerName, sizeof(pkm->trainerName));
	memcpy(decrypted, pkm->encrypted, sizeof(decrypted));
	for (int i = 0; i < sizeof(decrypted); i += sizeof(uint32_t)) {
		*((uint32_t*) (decrypted + i)) ^= xor;
	}
	int off_growth = 12 * (order & 0x3);
	//int off_attack = 12 * ((order >> 2) & 3);
	//int off_condit = 12 * ((order >> 4) & 3);
	int off_misc   = 12 * ((order >> 6) & 3);
	uint16_t index_no = get16(decrypted, off_growth);
	uint16_t pokedex_no = pokemon_index_to_dex(index_no);
	uint16_t origins = get16(decrypted, off_misc + 2);
	consoleSelect(extra->console);
	consoleClear();
	if (index_no > sizeof(pokemon_species) / sizeof(char*)) index_no = 0;
	iprintf("%3d  %-10s  %-10s\n", pokedex_no, nickname, pokemon_species[index_no]);
	iprintf("OT  %-7s (%s) - %5ld [%5ld]\n",
		trainer, (origins & 0x8000) ? "F" : "M",
		pkm->trainerId & 0xFFFF, pkm->trainerId >> 16);
	// TODO Met location from decrypted[off_misc + 1];
	// TODO ability and nature
	// TODO held item
	// TODO ball and game of origins (from origins & 0x7800, 0x780)
	// TODO print stats with lines like "HP  255 255 31  Atk 255 255 31"
	// TODO contest stats format: "Cool  255  Beaut 255  Cute 255" and Smart,Tough,Feel
	// TODO print moveset (2 lines, 2 moves per line)
	return 0;
}

void open_box(char *name, u8 *box_data) {
	char nicknames[11 * 30] = {0}; // 30 pokemon, 10 characters + NUL each
	char *cur_nick = nicknames;
	struct ConsoleMenuItem box_menu[30];
	struct hover_extra extra_data[30];

	PrintConsole console;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	for (int i = 0; i < 30; i++, cur_nick += 11) {
		string_to_ascii(cur_nick, box_data + (i * 80) + 8, 10);
		box_menu[i].str = cur_nick;
		box_menu[i].extra = (int) &extra_data[i];
		extra_data[i].pkm = box_data + (i * 80);
		extra_data[i].console = &console;
	}
	for (;;) {
		int selected;
		int extra = 0;
		selected = console_menu_open_2(name, box_menu, 30, &extra, &hover_callback);
		if (!selected)
			break;
	}
}

void open_boxes(FILE *fp, size_t *sections) {
	char box_names[126];
	const int NUM_BOXES = 14;
	char *box_name;
	u8 box_data[30 * 80]; // 30 pokemon per box, 80 bytes per pokemon
	struct ConsoleMenuItem box_menu[NUM_BOXES];
	fseek(fp, sections[13] + 0x744, SEEK_SET);
	fread(box_names, 1, sizeof(box_names), fp);
	box_name = box_names;
	for (int i = 0; i < NUM_BOXES; i++, box_name += 9) {
		string_to_ascii(box_name, (u8*) box_name, 9);
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

		// First 4 bytes of PC buffer is the most recently viewed PC box number
		// 30 Pokemon per Box, 80 bytes per Pokemon
		size_t box_offset = extra * sizeof(box_data) + 4;
		size_t section = 5 + box_offset / 0xf80;
		size_t box_mod = box_offset % 0xf80;
		fseek(fp, sections[section] + box_mod, SEEK_SET);
		if (box_mod <= 0xf80 - sizeof(box_data)) {
			// Only need to read this box's data from one section
			fread(box_data, 1, sizeof(box_data), fp);
		} else {
			// This box's data is split between two sections
			uint32_t bytesFromFirst = 0xf80 - box_mod;
			fread(box_data, 1, bytesFromFirst, fp);
			fseek(fp, sections[section + 1], SEEK_SET);
			fread(box_data + bytesFromFirst, 1, sizeof(box_data) - bytesFromFirst, fp);
		}
		open_box(box_name, box_data);
	}
}

void sav_load(char *path) {
	PrintConsole console;
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	consoleInit(&console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleSelect(&console);

	FILE *fp = fopen(path, "rb");
	size_t sections[14];
	uint32_t key;
	if (!verify_sav(fp, sections)){
		wait_for_button();
		return;
	}
	fseek(fp, sections[0] + 0xAC, SEEK_SET);
	fread(&key, sizeof(uint32_t), 1, fp);
	if (key == 1) {
		fseek(fp, sections[0] + 0xAF8, SEEK_SET);
		fread(&key, sizeof(uint32_t), 1, fp);
	}
	rewind(fp);

	struct ConsoleMenuItem top_menu[] = {
		{"Show trainer info", 0},
		{"Open PC boxes", 1},
		{"Open item bag", 2}
	};
	int selected;
	int extra = 0;
	for (;;) {
		selected = console_menu_open(path, top_menu, 3, NULL, &extra);
		if (!selected)
			break;
		if (extra == 0) {
			consoleSelect(&console);
			consoleClear();
			print_trainer_info(fp, sections[0]);
			wait_for_button();
		} else if (extra == 1) {
			open_boxes(fp, sections);
		}
	}
}
