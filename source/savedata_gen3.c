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
#include "savedata_gen3.h"
#include <nds.h>

#include <stdio.h>
#include <stdint.h>

#include "asset_manager.h"
#include "pkmx_format.h"
#include "pokemon_strings.h"
#include "string_gen3.h"
#include "utf8.h"
#include "util.h"

/* Resources for data structure:
 * https://bulbapedia.bulbagarden.net/wiki/Save_data_structure_in_Generation_III
 * For Gen4, see https://projectpokemon.org/docs/gen-4/dp-save-structure-r74/
 */

uint8_t savedata_buffer[SAVEDATA_NUM_SECTIONS * 0x1000];
uint32_t savedata_sections[SAVEDATA_NUM_SECTIONS];
int savedata_active_slot = -1;
uint32_t savedata_index;

static const char *savedata_file = NULL;

static const uint16_t section_sizes[] = {
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

union SaveSlotFooter {
	uint8_t bytes[16];
	struct {
		uint32_t unused;
		uint16_t section_id;
		uint16_t checksum;
		uint32_t signature; // Always 0x08012025
		uint32_t saveidx;
	} __attribute__((packed));
};

/**
 * Validates savedata for a single save slot and determines its section offsets.
 */
static int verify_savedata_slot(const uint8_t *savedata, uint32_t *sections_out,
	uint32_t *saveidx_out) {

	uint32_t saveidx = UINT32_MAX;
	uint16_t populated_sections = 0;
	int isAllFF = 1;
	union SaveSlotFooter footer;

	for (int sectionIdx = 0; sectionIdx < SAVEDATA_NUM_SECTIONS; sectionIdx++) {
		const uint8_t *section = NULL;
		uint32_t checksum = 0;
		size_t last_nonzero = 0;
		long section_offset = 0;
		int wasAllFF = isAllFF;

		section_offset = sectionIdx * 0x1000;
		section = savedata + section_offset;
		memcpy(footer.bytes, section + 0xFF0, 16);

		// Calculate checksum
		for (long wordIdx = 0; wordIdx < 0xFF0 / 4; wordIdx++) {
			uint32_t word = GET32(section, wordIdx * 4);
			checksum += word;
			if (word)
				last_nonzero = wordIdx;
			isAllFF = (word == 0xFFFFFFFF);
		}
		checksum = (checksum & 0xFFFF) + (checksum >> 16);
		checksum &= 0xFFFF;

		/* A game without any save starts with all FF bytes in flash.
		 * If the player starts a new game and only saves once, for example, one slot is
		 * all FF bytes and the other is actual data.
		 * If any section in the save slot is uninitialized, make sure the rest are too.
		 */
		if (sectionIdx && isAllFF != wasAllFF) {
			iprintf("%04lX Save slot has missing data\n", section_offset);
			return 0;
		}
		if (isAllFF)
			continue;

		if (footer.section_id >= SAVEDATA_NUM_SECTIONS) {
			iprintf("%04lX Invalid section ID: %hd\n", section_offset, footer.section_id);
			return 0;
		}
		if (last_nonzero * 4 >= section_sizes[footer.section_id]) {
			iprintf("%04lX Section too large\n", section_offset);
			return 0;
		}
		if (footer.checksum != checksum) {
			iprintf("%04lX Checksum mismatch\n", section_offset);
			return 0;
		}
		if (sectionIdx != 0 && footer.saveidx != saveidx) {
			iprintf("%04lX Save index mismatch\n", section_offset);
			return 0;
		}
		if ((populated_sections & (1 << footer.section_id))) {
			iprintf("%04lX Duplicate section\n", section_offset);
			return 0;
		}

		populated_sections |= 1 << footer.section_id;
		saveidx = footer.saveidx;
		sections_out[footer.section_id] = section_offset;
	}

	*saveidx_out = saveidx;
	return 1;
}

#define SIZE_64K (64 * 1024)

static inline void slot2_sendFlashCommand(uint8_t cmd) {
	SRAM[0x5555] = 0xaa;
	swiDelay(10);
	SRAM[0x2aaa] = 0x55;
	swiDelay(10);
	SRAM[0x5555] = cmd;
	swiDelay(10);
}

static void slot2_eraseFlashSector(uint16_t sector) {
	slot2_sendFlashCommand(0x80);
	SRAM[0x5555] = 0xaa;
	swiDelay(10);
	SRAM[0x2aaa] = 0x55;
	swiDelay(10);
	SRAM[sector] = 0x30;
	swiDelay(10);
}

static int readSlot2Save(uint8_t *out) {
	sysSetBusOwners(true, true);
	swiDelay(10);

	// All main GBA Pokemon games use the SRAM type: FLASH1M_V103
	// That 1 Mb (128 kB) save data is exactly 2 banks of 64k.
	for (int bankIdx = 0; bankIdx < 2; bankIdx++) {
		// Flash command for switching banks
		slot2_sendFlashCommand(0xb0);
		SRAM[0] = bankIdx;
		swiDelay(10);

		for (int i = 0; i < SIZE_64K; i++) {
			out[i] = SRAM[i];
		}
		out += SIZE_64K;
	}

	return 1;
}

static int writeSlot2Save(uint8_t *data, uint32_t seek, uint32_t size) {
	if ((seek & 0xFFF) || (size & 0xFFF))
		return 0;
	for (uint32_t sector = seek; sector < seek + size; sector += 0x1000) {
		uint16_t sectorInBank = sector & 0xFFFF;
		uint8_t *src = data + (sector - seek);
		uint8_t *dst = SRAM + sectorInBank;

		if (sector == seek || sector == SIZE_64K) {
			// Change bank
			slot2_sendFlashCommand(0xb0);
			SRAM[0] = sector >> 16;
		}

		// Erase sector
		slot2_eraseFlashSector(sectorInBank);
		while (SRAM[sectorInBank] != 0xFF)
			swiDelay(10);

		// Write bytes
		for (uint16_t pos = 0; pos < 0x1000; pos++) {
			uint8_t srcByte = *src;
			slot2_sendFlashCommand(0xa0);
			*dst = srcByte;
			swiDelay(10);
			while (*dst != srcByte)
				swiDelay(10);
			src++;
			dst++;
		}
	}
	return 1;
}


int load_savedata(const char *filename) {
	FILE *fp;
	uint8_t *flash_dump = NULL;
	uint32_t saveidx_slots[2] = {UINT32_MAX, UINT32_MAX};
	uint32_t sections_slot2[SAVEDATA_NUM_SECTIONS];
	int hasPokedex = 0;

	flash_dump = malloc(0x20000); // 128 kiB, too big for stack

	savedata_file = filename;
	if (filename) {
		fp = fopen(filename, "rb");
		if (!fp) {
			iprintf("Error opening save file:\n%s\n", filename);
			free(flash_dump);
			return 0;
		}

		// Save files are normally 128K, but the last 16K may be unused.
		// Just in case any tools trim save files, we subtract 16K to get 0x1c000 (112K)
		// 00000-0DFFF Save slot 1
		// 0E000-1BFFF Save slot 2
		// 1C000-1DFFF Hall of Fame
		// 1E000-1EFFF Mystery Gift
		// 1F000-1FFFF Vs Recorder
		if (fread(flash_dump, 1, 0x20000, fp) < 0x1c000) {
			iprintf("This isn't a valid save file.\n");
			free(flash_dump);
			fclose(fp);
			return 0;
		}
		fclose(fp);
	} else {
		if (!readSlot2Save(flash_dump)) {
			iprintf("%s", flash_dump);
			free(flash_dump);
			return 0;
		}
	}
	for (int slotIdx = 0; slotIdx < 2; slotIdx++) {
		uint8_t *savedata = flash_dump + slotIdx * sizeof(savedata_buffer);
		uint32_t *sections = slotIdx ? sections_slot2 : savedata_sections;
		if (!verify_savedata_slot(savedata, sections, &saveidx_slots[slotIdx])) {
			free(flash_dump);
			return 0;
		}
	}
	if (saveidx_slots[0] == UINT32_MAX && saveidx_slots[1] == UINT32_MAX) {
		iprintf("Save file appears to be uninitialized.\n");
		free(flash_dump);
		return 0;
	} else if (saveidx_slots[0] + 1 > saveidx_slots[1] + 1) {
		// The first savedata slot is more recent.
		// This overflow comparison makes UINT32_MAX compare less than any valid value
		memcpy(savedata_buffer, flash_dump, sizeof(savedata_buffer));
		savedata_active_slot = 0;
		savedata_index = saveidx_slots[0];
	} else {
		// The second savedata slot is more recent.
		memcpy(savedata_buffer, flash_dump + sizeof(savedata_buffer), sizeof(savedata_buffer));
		memcpy(savedata_sections, sections_slot2, sizeof(savedata_sections));
		savedata_active_slot = 1;
		savedata_index = saveidx_slots[1];
	}
	free(flash_dump);

	// Make sure the Pokedex is obtained
	if (IS_RUBY_SAPPHIRE) {
		hasPokedex = (GET_SAVEDATA_SECTION(2)[0x3A0] & 2) != 0;
	} else if (IS_EMERALD) {
		hasPokedex = (GET_SAVEDATA_SECTION(2)[0x3FC] & 2) != 0;
	} else if (IS_FIRERED_LEAFGREEN) {
		hasPokedex = (GET_SAVEDATA_SECTION(2)[0x65] & 2) != 0;
	}
	if (!hasPokedex) {
		iprintf("You need to obtain the Pokedex\n");
		return 0;
	}
	return 1;
}

void update_section_checksum(int sectionIdx) {
	uint32_t checksum = 0;
	uint8_t *section = GET_SAVEDATA_SECTION(sectionIdx);
	union SaveSlotFooter *footer;

	// Footer is the last 16 bytes of each section
	footer = ((union SaveSlotFooter*) (section + 0xFF0));

	// Calculate checksum
	for (long wordIdx = 0; wordIdx < 0xFF0 / 4; wordIdx++) {
		uint32_t word = GET32(section, wordIdx * 4);
		checksum += word;
	}
	checksum = (checksum & 0xFFFF) + (checksum >> 16);
	checksum &= 0xFFFF;
	footer->checksum = (uint16_t) checksum;
}

int write_savedata(void) {
	int success = 1;
	uint32_t seek;

	iprintf("Saving...\n");

	// Increment the save index in every section
	for (int sectionIdx = 0; sectionIdx < SAVEDATA_NUM_SECTIONS; sectionIdx++) {
		union SaveSlotFooter *footer =
			(union SaveSlotFooter*) (GET_SAVEDATA_SECTION(sectionIdx) + 0xFF0);
		footer->saveidx = savedata_index + 1;
	}

	// If the latest save data is in slot 0, write to slot 1, and vice-versa.
	seek = savedata_active_slot ? 0 : 0xE000;

	if (savedata_file) {
		FILE *fp;
		int rc;
		fp = fopen(savedata_file, "r+b");
		fseek(fp, savedata_active_slot ? 0 : 0xE000, SEEK_SET);
		rc = fwrite(savedata_buffer, 1, sizeof(savedata_buffer), fp);
		if (rc < sizeof(savedata_buffer)) {
			iprintf("Error writing save file\n");
			success = 0;
		}
		fclose(fp);
	} else {
		success = writeSlot2Save(savedata_buffer, seek, sizeof(savedata_buffer));
	}

	// Don't swap the slots or increment the index again in the same session
	return success;
}

int load_box_savedata(uint8_t *box_data, int boxIdx) {
	size_t box_offset;

	// First 4 bytes of PC buffer is the most recently viewed PC box number
	if (boxIdx < 0)
		boxIdx = GET32(GET_SAVEDATA_SECTION(5), 0);
	box_offset = boxIdx * BOX_SIZE_BYTES_3 + 4;

	// The actual save data only stores 0xf80 bytes in each section.
	size_t section = 5 + box_offset / 0xf80;
	size_t box_mod = box_offset % 0xf80;
	if (box_mod <= 0xf80 - BOX_SIZE_BYTES_3) {
		// Only need to read this box's data from one section
		memcpy(box_data, GET_SAVEDATA_SECTION(section) + box_mod, BOX_SIZE_BYTES_3);
	} else {
		// This box's data is split between two sections
		uint32_t bytesFromFirst = 0xf80 - box_mod;
		memcpy(box_data, GET_SAVEDATA_SECTION(section) + box_mod, bytesFromFirst);
		memcpy(box_data + bytesFromFirst, GET_SAVEDATA_SECTION(section + 1),
			BOX_SIZE_BYTES_3 - bytesFromFirst);
	}
	return boxIdx;
}

int load_boxes_savedata(uint8_t *box_data) {
	uint16_t activeBox;

	// First 4 bytes of PC buffer is the most recently viewed PC box number
	activeBox = GET32(GET_SAVEDATA_SECTION(5), 0);

	// After excluding the active box number, Section 5 has 0xf7c bytes of Pokemon data
	memcpy(box_data, GET_SAVEDATA_SECTION(5) + 4, 0xf7c);
	box_data += 0xf7c;

	// Sections 6-12 each have 0xf80 bytes of Pokemon data
	for (int section = 6; section <= 12; section++) {
		memcpy(box_data, GET_SAVEDATA_SECTION(section), 0xf80);
		box_data += 0xf80;
	}

	// Section 13 has the last 0x744 bytes of Pokemon data, adding up to 33600 bytes total
	memcpy(box_data, GET_SAVEDATA_SECTION(13), 0x744);

	return activeBox;
}

static int register_boxes_to_pokedex(const uint8_t *box_data) {
	uint8_t pokedex[386/8+1] = {0};
	pkm3_t pkm;
	int addedEntries = 0;
	uint32_t unownPersonality = 0;
	uint32_t spindaPersonality = 0;
	uint8_t *saveDexOwn, *saveDexSeen1, *saveDexSeen2, *saveDexSeen3;

	// Get the list of all species that exist in the PC boxes
	for (int i = 0; i < 14 * 30; i++) {
		uint16_t dexnum;
		decode_pkm_encrypted_data(&pkm, box_data + i * PKM3_SIZE);
		dexnum = gen3_index_to_pokedex(pkm.species);
		// Ignore eggs
		if (PKM3_IS_EGG(pkm))
			continue;
		if (dexnum == 201 && !unownPersonality)
			unownPersonality = pkm.personality;
		if (dexnum == 327 && !spindaPersonality)
			spindaPersonality = pkm.personality;
		if (dexnum) {
			dexnum--;
			pokedex[dexnum / 8] |= 1 << (dexnum & 7);
		}
	}

	// Get pointers to all the Own and Seen lists based on which game is in use
	saveDexOwn = GET_SAVEDATA_SECTION(0) + 0x28;
	saveDexSeen1 = GET_SAVEDATA_SECTION(0) + 0x5C;

	if (IS_RUBY_SAPPHIRE) {
		saveDexSeen2 = GET_SAVEDATA_SECTION(1) + 0x938;
		saveDexSeen3 = GET_SAVEDATA_SECTION(4) + 0xC0C;
		// Unlock the National Dex
		GET_SAVEDATA_SECTION(0)[0x19] = 1;
		GET_SAVEDATA_SECTION(0)[0x1A] = 0xDA;
		GET_SAVEDATA_SECTION(2)[0x3A6] |= 0x40;
		SET16(GET_SAVEDATA_SECTION(2), 0x44C) = 0x302;
	} else if (IS_EMERALD) {
		saveDexSeen2 = GET_SAVEDATA_SECTION(1) + 0x988;
		saveDexSeen3 = GET_SAVEDATA_SECTION(4) + 0xCA4;
	} else /* IS_FIRERED_LEAFGREEN */ {
		saveDexSeen2 = GET_SAVEDATA_SECTION(1) + 0x5F8;
		saveDexSeen3 = GET_SAVEDATA_SECTION(4) + 0xB98;
	}

	// Save the forms for Unown (#201) and Spinda (#327) if not already owned
	if (unownPersonality && (saveDexOwn[(201-1)/8] & (1 << ((201-1) & 7))) == 0)
		SET32(GET_SAVEDATA_SECTION(0), 0x1C) = unownPersonality;
	if (spindaPersonality && (saveDexOwn[(327-1)/8] & (1 << ((327-1) & 7))) == 0)
		SET32(GET_SAVEDATA_SECTION(0), 0x20) = spindaPersonality;

	// Add each Pokemon to all the Own and Seen lists
	for (int dexByteIdx = 0; dexByteIdx < sizeof(pokedex); dexByteIdx++) {
		uint8_t dexByte = pokedex[dexByteIdx];
		uint8_t addingBits = dexByte & ~(saveDexOwn[dexByteIdx]);
		if (addingBits) {
			saveDexOwn[dexByteIdx] |= dexByte;
			saveDexSeen1[dexByteIdx] |= dexByte;
			saveDexSeen2[dexByteIdx] |= dexByte;
			saveDexSeen3[dexByteIdx] |= dexByte;
			// Count how many Pokemon weren't already marked as owned
			while (addingBits) {
				if ((addingBits & 1))
					addedEntries++;
				addingBits >>= 1;
			}
		}
	}
	update_section_checksum(0);
	update_section_checksum(2);
	update_section_checksum(1);
	update_section_checksum(4);
	return addedEntries;
}

int write_boxes_savedata(uint8_t *box_data) {
	int rc;

	// Register all stored Pokemon to the Pokedex
	rc = register_boxes_to_pokedex(box_data);
	if (rc)
		iprintf("%d Pokemon added to the Pokedex\n", rc);

	memcpy(GET_SAVEDATA_SECTION(5) + 4, box_data, 0xf7c);
	box_data += 0xf7c;
	for (int section = 6; section <= 12; section++) {
		memcpy(GET_SAVEDATA_SECTION(section), box_data, 0xf80);
		box_data += 0xf80;
	}
	memcpy(GET_SAVEDATA_SECTION(13), box_data, 0x744);

	// Recalculate checksum for all box sections
	for (int section = 5; section <= 13; section++) {
		update_section_checksum(section);
	}

	return 1;
}

int pkm_is_shiny(const union pkm_t *pkm) {
	uint16_t xor = 0;
	for (int i = 0; i < 4; i++)
		xor ^= GET16(pkm->bytes, i * 2);
	return xor < 8;
}

int pkm_get_language(const union pkm_t *pkm) {
	switch (pkm->language) {
		case 0x201: return LANG_JAPANESE;
		case 0x202: return LANG_ENGLISH;
		case 0x203: return LANG_FRENCH;
		case 0x204: return LANG_ITALIAN;
		case 0x205: return LANG_GERMAN;
		case 0x207: return LANG_SPANISH;
	}
	return -1;
}

#define SPECIES_MISSINGNO 252 // The entire range of 252-276 is ?
#define SPECIES_UNOWN_A 201
#define SPECIES_UNOWN_B 413
#define SPECIES_EGG 412
#define SPECIES_MAX 439 // The last valid species is also Unown's Question Mark form
uint16_t pkm_displayed_species(const union pkm_t *pkm) {
	uint16_t species = pkm->species;
	uint32_t personality = pkm->personality;
	if (PKM3_IS_EGG(*pkm)) {
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

void pkm3_to_simplepkm(struct SimplePKM *simple, const pkm3_t *pkm) {
	const struct BaseStatEntryGen3 *baseStats;

	memset(simple, 0, sizeof(struct SimplePKM));
	if (pkm->species == 0)
		return;
	baseStats = getBaseStatEntry(pkm->species);
	decode_gen3_string16(simple->nickname, pkm->nickname, 10, pkm->language);
	decode_gen3_string16(simple->trainerName, pkm->trainerName, 7, pkm->language);
	simple->dexNumber = gen3_index_to_pokedex(pkm->species);
	simple->isShiny = pkm_is_shiny(pkm);
	simple->isEgg = PKM3_IS_EGG(*pkm);
	simple->isOTFemale = pkm->origins >> 15;
	simple->metLevel = pkm->origins & 0x7F;
	simple->marking = pkm->marking;
	simple->trainerId = pkm->trainerId;
	simple->heldItem = pkm->held_item;
	simple->IVs = pkm->IVs;
	simple->metLocation = get_location_name(pkm->met_location, pkm->origins >> 7 & 0xF);
	if (pkm->language >= 0x201 && pkm->language <= 0x207 && pkm->language != 0x206)
		simple->language = pkm->language & ~0x200;
	simple->trainerId = pkm->trainerId;
	if (baseStats->ability[1])
		simple->ability = get_ability_name(baseStats->ability[pkm->personality & 1]);
	else
		simple->ability = get_ability_name(baseStats->ability[0]);
	simple->types[0] = baseStats->type[0];
	simple->types[1] = baseStats->type[1];
	for (int i = 0; i < 4; i++) {
		simple->moves[i] = pkm->moves[i];
		simple->movePP[i] = pkm->move_pp[i];
	}

	// Calculate stats
	if (baseStats->expGrowth <= 5) {
		const uint32_t *growthTable;
		uint8_t level;
		uint8_t nature;
		uint8_t natureMods[6] = {10, 10, 10, 10, 10, 10};
		nature = pkm->personality % 25;
		simple->nature = nature;
		natureMods[nature / 5 + 1]++;
		natureMods[nature % 5 + 1]--;
		growthTable = experienceTables[baseStats->expGrowth];
		for (level = 0; level < 100; level++) {
			if (pkm->experience < growthTable[level])
				break;
		}
		simple->level = level;
		for (int statIdx = 0; statIdx < 6; statIdx++) {
			uint8_t iv = pkm->IVs >> (5 * statIdx) & 0x1F;
			uint32_t stat;
			stat = 2 * baseStats->stats[statIdx] + iv + pkm->effort[statIdx] / 4;
			stat = (stat * level / 100 + 5) * natureMods[statIdx] / 10;
			simple->stats[statIdx] = (uint16_t) stat;
			simple->EVs[statIdx] = pkm->effort[statIdx];
		}
		// HP stat uses a slightly different calculation
		if (simple->dexNumber == 292) // Shedinja
			simple->stats[0] = 1;
		else
			simple->stats[0] += 5 + level;
	}

	// Gender
	if (baseStats->genderRatio == 0xFF)
		simple->gender = 2;
	else if (baseStats->genderRatio == 0xFE)
		simple->gender = 1;
	else
		simple->gender = (pkm->personality & 0xFF) < baseStats->genderRatio;

	// Pokeball
	{
		uint16_t ball;
		ball = pkm->origins >> 11 & 0xF;
		if (ball == 0 || ball > 12)
			ball = 4; // Regular Pokeball
		simple->pokeball = ball;
	}

	// TODO also populate simple->form and simple->isBadEgg
}

void print_trainer_info() {
	char curString[16] = {0};
	uint32_t gameid;
	uint8_t *trainerInfo = GET_SAVEDATA_SECTION(0);
	gameid = GET32(trainerInfo, 0xAC);
	// Bulbapedia says 0xAC should be 0 for Ruby/Sapphire, but my legit Ruby
	// cart's save file seems to have 0xcdcdc4bf
	iprintf("Game: %s\n",
		(gameid == 0) ? "Ruby/Sapphire" :
		(gameid == 1) ? "FireRed/LeafGreen" : "Ruby/Sapphire/Emerald");
	decode_gen3_string(curString, trainerInfo, 16, 7, 0);
	iprintf("Name: %s\n", curString);
	iprintf("Gender: %s\n", trainerInfo[0x8] ? "F" : "M");
	iprintf("Trainer ID: %5d\n", (int) GET16(trainerInfo, 0xA));
	iprintf("Secret  ID: %5d\n", (int) GET16(trainerInfo, 0xC));
	iprintf("Play Time: %hd:%02hhd:%02hhd.%03d\n",
		trainerInfo[0xE], trainerInfo[0x10], trainerInfo[0x11],
		1000 * (int) trainerInfo[0x12] / 60);
}


int print_pokemon_details(const union pkm_t *pkm) {
	struct SimplePKM simple;

	pkm3_to_simplepkm(&simple, pkm);

	if (simple.dexNumber == 0) {
		return 0;
	}
	/* TODO replace this with text labels
	iprintf("OT  %-7s (%s) - %05ld [%05ld]\n",
		simple.trainerName, simple.isOTFemale ? "F" : "M",
		simple.trainerId & 0xFFFF, simple.trainerId >> 16);*/
	iprintf("OT:      %05ld-%05ld\n", simple.trainerId & 0xFFFF, simple.trainerId >> 16);
	iprintf("Met:     %-23.23s", simple.metLocation);
	const char *item_name = get_item_name(simple.heldItem);
	if (item_name)
		iprintf("Item:    %-23s", item_name);
	else
		iprintf("Item:    Invalid (%d)\n", simple.heldItem);
	iprintf("Nature:  %s\n", get_nature_name(simple.nature));
	iprintf("Ability: %s\n", simple.ability);
	if (simple.types[0] == simple.types[1])
		iprintf("Type:    %s\n", get_type_name(simple.types[0]));
	else
		iprintf("Type:    %-8s %s\n",
			get_type_name(simple.types[0]), get_type_name(simple.types[1]));
	iprintf("Moves:\n");
	for (int i = 0; i < 4; i++) {
		// Because each move is printed to exactly 16 characters, these 4 moves
		// fill 2 lines perfectly without the need for any newlines
		const char *move_name = get_move_name(simple.moves[i]);
		if (move_name)
			iprintf("  %-14s", move_name);
		else
			iprintf("  Invalid: %-3d  ", simple.moves[i]);
	}
	iprintf("\n      HP Atk Def Spd SpA SpD\nStat");
	for (int i = 0; i < 6; i++)
		iprintf(" %3d", (int) simple.stats[i]);
	iprintf("\n  EV");
	for (int i = 0; i < 6; i++)
		iprintf(" %3d", (int) simple.EVs[i]);
	iprintf("\n  IV");
	for (int i = 0; i < 6; i++)
		iprintf("  %2ld", (simple.IVs >> (5 * i)) & 0x1f);
	iprintf("\n");
	iprintf("\nContest: %3d %3d %3d %3d %3d %3d",
		(int) pkm->contest[0],  (int) pkm->contest[1], (int) pkm->contest[2],
		(int) pkm->contest[3], (int) pkm->contest[4],  (int) pkm->contest[5]);
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

uint16_t decode_pkm_encrypted_data(pkm3_t *dest, const uint8_t *src) {
	/* There are 4 pkm data sections that can be permutated in any order
	 * depending on the personality value.
	 * data_order[] encodes each possible ordering as one byte, made up of
	 * four 2-bit fields corresponding to the index of each section.
	 * For example, with 0x93 == 0b10010011
	 *   bits[1:0] == 0b11 => reordered section 0 is copied from source section 3
	 *   bits[3:2] == 0b00 => reordered section 1 is copied from source section 0
	 *   bits[5:4] == 0b01 => reordered section 2 is copied from source section 1
	 *   bits[7:6] == 0b10 => reordered section 3 is copied from source section 2
	 *
	 * When reordered, these four sections are:
	 *   0. Growth: species, held item, exp, friendship
	 *   1. Attacks: currently-learned moveset and PP limits
	 *   2. EVs and Contest Condition
	 *   3. Misc: IVs, ability, ribbons, pokerus, and met/origin data
	 */
	static const uint8_t data_order[] = {
		0xe4, 0xb4, 0xd8, 0x9c, 0x78, 0x6c,
		0xe1, 0xb1, 0xd2, 0x93, 0x72, 0x63,
		0xc9, 0x8d, 0xc6, 0x87, 0x4e, 0x4b,
		0x39, 0x2d, 0x36, 0x27, 0x1e, 0x1b
		// Same numbers as above but in binary
		//0b11100100, 0b10110100, 0b11011000, 0b10011100, 0b01111000, 0b01101100,
		//0b11100001, 0b10110001, 0b11010010, 0b10010011, 0b01110010, 0b01100011,
		//0b11001001, 0b10001101, 0b11000110, 0b10000111, 0b01001110, 0b01001011,
		//0b00111001, 0b00101101, 0b00110110, 0b00100111, 0b00011110, 0b00011011
	};
	uint16_t checksum;
	uint8_t order;
	uint32_t xor;
	uint8_t reordered[48];

	// Rearrange to a consistent order
	order = data_order[GET32(src, 0) % 24];
	for (int i = 0; i < 4; i++)
		memcpy(reordered + i * 12, src + 32 + 12 * ((order >> (i * 2)) & 3), 12);

	// "Decrypt" the reordered data
	xor = GET32(src, 0) ^ GET32(src, 4);
	for (int i = 0; i < 48; i += 4) {
		SET32(reordered, i) ^= xor;
	}

	// Calculate the checksum
	checksum = 0;
	for (int i = 0; i < 48; i += 2)
		checksum += GET16(reordered, i);

	// Output result data
	if (dest) {
		memcpy(dest->bytes, src, 32);
		memcpy(dest->bytes + 32, reordered, 48);
	}
	return checksum;
}
