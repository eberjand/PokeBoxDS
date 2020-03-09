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

/* PKMX data contains (hex offsets):
 * 00 current generation
 * 01 current sub-generation
 * 02-03 origin generation
 *
 * PKM data (only one of the following):
 * 04-38 PKM2 data + nickname + OT (52 bytes)
 * 04-54 PKM3 data (80 bytes)
 * 04-8C PKM4 data (136 bytes)
 * 04-8C PKM5 data (136 bytes)
 *
 * Data lost from backwards conversion for Gen2 to Gen1:
 * TODO 5 bytes for moves, 2 bytes for caught data, 1 byte for friendship
 *
 * Data lost from backwards conversion for Gen3 to Gen2:
 * TODO
 *
 * Data lost from backwards conversion for Gen4 to Gen3:
 * 55-8B TODO
 *
 * Data lost from backwards conversion for Gen5 to Gen4:
 * 8C-8F Personality value (regenerated in Gen4 to preserve nature)
 * 90-97 Lost moves
 * 98-9B Met locations
 * AE    Pokeball type (can be Dream Ball)
 * TODO need to verify French nicknames can be backported losslessly
 *
 * Data lost from forward conversion for Gen1/2 to Gen3 and higher:
 * 9C-A5 EVs (aka Stat Experience)
 * A6-A7 IVs (aka DVs; could be preserved but we're imitating PokeTransporter)
 * A8-A9 Caught/met data (Crystal only)
 * AA-AB Nickname adjustment (used for restoring the PK and MN glyphs)
 * AC    OT name adjustment (also for the PK and MN glyphs)
 *
 * Data lost from forward conversion for Gen4 to Gen5:
 * AD    Shiny Leaves (HGSS)
 */

#define PKMX_SIZE 176
#define BOX_SIZE_BYTES_X (176*30)

struct SimplePKM {
	uint16_t nickname[12];
	uint16_t trainerName[8];
	uint16_t stats[6];
	uint32_t IVs;
	uint8_t EVs[6];
	uint16_t dexNumber;
	uint16_t spriteIdx;
	uint16_t spriteIdxNonEgg;
	uint16_t pokeball;
	uint16_t curGameId;
	uint16_t originGameId;
	uint8_t marking;
	uint8_t form;
	uint8_t gender;
	uint8_t nature;
	uint8_t level;
	uint8_t metLevel;
	uint8_t language;
	uint8_t exists : 1;
	uint8_t isEgg : 1;
	uint8_t isBadEgg : 1;
	uint8_t isOTFemale : 1;
	uint8_t isShiny : 1;
	uint8_t isOnCart : 1;
	uint8_t unusedFlags : 2;
	uint16_t heldItem;
	const char *metLocation;
	const char *ability;
	union {
		uint16_t trainerId16[2];
		uint32_t trainerId;
	};
	uint8_t types[2];
	uint16_t moves[4];
	uint8_t movePP[4];
};

void pkm_to_pkmx(uint8_t *pkmx, const uint8_t *pkm, uint16_t gameId);
int pkmx_convert_generation(uint8_t *pkmx, int generation);
int pkmx_to_pkm(uint8_t *pkm, uint8_t *pkmx, int generation);
void pkmx_to_simplepkm(struct SimplePKM *simple, const uint8_t *pkmx, int is_cart);
