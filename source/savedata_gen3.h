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
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

// 30 Pokemon per box, 80 bytes per Pokemon
#define BOX_SIZE_BYTES_3 (30 * 80)
#define PKM3_SIZE 80

#define SAVEDATA_NUM_SECTIONS 14

#define GET_SAVEDATA_SECTION(idx) (savedata_buffer + savedata_sections[idx])

union pkm_t {
	uint8_t bytes[80];
	struct {
		uint32_t personality;
		uint32_t trainerId;
		uint8_t nickname[10];
		uint16_t language;
		uint8_t trainerName[7];
		uint8_t marking;
		uint16_t checksum;
		uint16_t unknown;
		// Decrypted section: Growth
		uint16_t species;
		uint16_t held_item;
		uint32_t experience;
		uint8_t ppUp;
		uint8_t friendship;
		uint16_t unknown_growth;
		// Decrypted section: Attacks
		uint16_t moves[4];
		uint8_t move_pp[4];
		// Decrypted section: EVs and Contest Condition
		uint8_t effort[6];
		uint8_t contest[6];
		// Decrypted section: Miscellaneous
		uint8_t pokerus;
		uint8_t met_location;
		uint16_t origins;
		uint32_t IVs;
		uint32_t ribbons;
	} __attribute__((packed));
};
typedef union pkm_t pkm3_t;

struct SimplePKM;

#define PKM3_IS_EGG(pkm) ((pkm).IVs >> 30 & 1)

extern uint8_t savedata_buffer[SAVEDATA_NUM_SECTIONS * 0x1000]; // 56 kiB
extern uint32_t savedata_sections[SAVEDATA_NUM_SECTIONS];
extern int savedata_active_slot;

void pkm3_to_simplepkm(struct SimplePKM *simple, const pkm3_t *pkm);
int decode_gen3_string(char *out, const uint8_t *str, int len, uint16_t lang);
int pkm_is_shiny(const union pkm_t *pkm);
uint16_t pkm_displayed_species(const union pkm_t *pkm);
void print_trainer_info(void);
int print_pokemon_details(const union pkm_t *pkm);
uint16_t decode_pkm_encrypted_data(pkm3_t *dest, const uint8_t *src);
int load_box_savedata(uint8_t *box_data, int boxIdx);
int load_boxes_savedata(uint8_t *box_data);
int write_boxes_savedata(uint8_t *box_data);
int load_savedata(const char *filename);
int write_savedata(void);
