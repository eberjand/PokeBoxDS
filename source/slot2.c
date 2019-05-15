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
#include "slot2.h"

#include <nds.h>
#include <stdio.h>
#include "util.h"

// All main-series Pokemon games use FLASH1M_V103

// GBA Flash ROM uses 64KB banks
#define SIZE_64K (64 * 1024)

const char* getGBAGameName(int gameId) {
	const char *game_names[] = {
		"Pokemon Ruby Version",
		"Pokemon Sapphire Version",
		"Pokemon FireRed Version",
		"Pokemon LeafGreen Version",
		"Pokemon Emerald Version"
	};
	if (gameId < 0 || gameId >= ARRAY_LENGTH(game_names))
		return NULL;
	return game_names[gameId];
}

static int detectGame(char *gamecode) {
	uint32_t known_games[] = {
		0x565841, // AXV: Ruby
		0x505841, // AXP: Sapphire
		0x525042, // BPR: FireRed
		0x475042, // BPG: LeafGreen
		0x455042, // BPE: Emerald
	};

	uint32_t code_u32 = *((uint32_t*) gamecode) & 0xFFFFFF;

	for (int i = 0; i < ARRAY_LENGTH(known_games); i++) {
		if (code_u32 == known_games[i]) {
			return i;
		}
	}
	return -1;
}

int readSlot2Save(uint8_t *out) {
	int gameId;
	sysSetBusOwners(true, true);
	swiDelay(10);
	gameId = detectGame(GBA_HEADER.gamecode);

	if (gameId < 0) {
		if (GBA_HEADER.gamecode[0] <= 0 || GBA_HEADER.gamecode[0] > 0x80)
			strcpy((char*) out, "Error: No GBA cartridge found\n");
		else
			sprintf((char*) out,
				"Error: Unsupported game cart\n"
				"Title: %.12s\n"
				"Code:  %.4s\n",
				GBA_HEADER.title, GBA_HEADER.gamecode);
		return -1;
	}

	// All main Pokemon games have 1Mb savedata; exactly 2 banks
	for (int bankIdx = 0; bankIdx < 2; bankIdx++) {
		// Flash commands for switching banks
		SRAM[0x5555] = 0xaa;
		swiDelay(10);
		SRAM[0x2aaa] = 0x55;
		swiDelay(10);
		SRAM[0x5555] = 0xb0;
		swiDelay(10);
		SRAM[0] = bankIdx;
		swiDelay(10);

		for (int i = 0; i < SIZE_64K; i++) {
			out[i] = SRAM[i];
		}
		out += SIZE_64K;
	}
	return gameId;
}
