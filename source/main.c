#include <nds.h>

#include <stdio.h>

#include "file_picker.h"

/* Resources for data structure:
 * https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_in_Generation_III
 * https://bulbapedia.bulbagarden.net/wiki/Character_encoding_in_Generation_III
 */

int string_to_ascii(char *out, char *str, int len) {
	for (int i = 0; i < len; i++) {
		char c = str[i];
		if (c == 0xFF) {
			out[i] = 0;
			return i;
		} else if (c >= 0xBB && c <= 0xD4)
			c = 'A' + (c - 0xBB);
		else if (c >= 0xD5 && c <= 0xEE)
			c = 'a' + (c - 0xD5);
		else
			c = '.';
		out[i] = c;
	}
	return len;
}

uint16_t get16(char *bytes, long pos) {
	return *((uint16_t*) (bytes + pos));
}

uint32_t get32(char *bytes, long pos) {
	return *((uint32_t*) (bytes + pos));
}

// FILE: open file handle for the sav
// slot_out: address of int where the most recent slot index (0 or 1) will be written
// sections_out: size_t[14] array that will hold the list of section offsets by ID
int verify_sav(FILE *fp, int *slot_out, size_t *sections_out) {
	long section_offset = 0;
	int success = 1;
	uint16_t saveidx = 0;
	char *section = NULL;
	uint32_t *words;
	const uint16_t NUM_SECTIONS = 14;
	size_t sections[NUM_SECTIONS];
	union {
		char bytes[32];
		struct {
			char empty[16];
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
			if (slot_out)
				*slot_out = slot;
			if (sections_out)
				memcpy(sections_out, sections, sizeof(sections));
		}
	}
	free(section);
	return success;
}

void print_file_info(char *path) {
	FILE *fp = fopen(path, "rb");
	char buf[32];
	char curString[16] = {0};
	size_t sections[16];
	if (!verify_sav(fp, NULL, sections))
		return;
	fseek(fp, sections[0], SEEK_SET);
	fread(buf, 1, 32, fp);
	string_to_ascii(curString, buf, 7);
	iprintf("Trainer: %s\n", curString);
	iprintf("Gender: %s\n", buf[0x8] ? "F" : "M");
	iprintf("Trainer ID: %5d\n", (int) get16(buf, 0xA));
	iprintf("Secret  ID: %5d\n", (int) get16(buf, 0xC));
	iprintf("Play Time: %hd:%02hhd:%02hhd (+%02hhdf)\n",
		get16(buf, 0xE), buf[0x10], buf[0x11], buf[0x12]);
}

int main(int argc, char **argv) {
	char path[512];
	int rc;
	
	// Initialise the console, required for printf
	consoleDemoInit();

	for (;;) {
		rc = filePicker(path, sizeof(path));
		if (!rc) break;

		consoleClear();
		iprintf("File: %s\n", path);
		print_file_info(path);
		for (;;) {
			swiWaitForVBlank();
			scanKeys();
			if (keysDown()) break;
		}
	}

	return !rc;
}
