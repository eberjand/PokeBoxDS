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
#include <stdio.h>
#include <stdint.h>
#include "languages.h"

extern uint8_t wallpaperTiles[0x1000];
extern uint16_t wallpaperTilemap[0x2d0];
extern uint16_t wallpaperPal[16 * 4];
extern const char *activeGameName;
extern const char *activeGameNameShort;
extern int activeGameId;
extern int activeGameLanguage;

#define IS_RUBY_SAPPHIRE (activeGameId == 0 || activeGameId == 1)
#define IS_FIRERED_LEAFGREEN (activeGameId == 2 || activeGameId == 3)
#define IS_EMERALD (activeGameId == 4)

struct BaseStatEntryGen3 {
	uint8_t stats[6];
	uint8_t type[2];
	uint8_t catchRate;
	uint8_t expYield;
	uint16_t evYield;
	uint16_t heldItem[2];
	uint8_t genderRatio;
	uint8_t eggCycles;
	uint8_t baseFriendship;
	uint8_t expGrowth;
	uint8_t eggGroup[2];
	uint8_t ability[2];
	uint8_t safariFleeRate;
	uint8_t bodyColor;
	uint8_t padding[2];
};

void assets_init_placeholder();
_Bool assets_init_cart();
_Bool assets_init_romfile(const char *file);
void assets_free();

uint8_t getIconPaletteIdx(uint16_t species);
// Returned pointer is only valid until the next call of either function
const uint16_t* getIconImage(uint16_t species);
const uint16_t* getIconPaletteColors(int index);
void readFrontImage(uint8_t *tiles_out, uint8_t *palette_out, uint16_t species, int shiny);
int loadWallpaper(int index);
const struct BaseStatEntryGen3* getBaseStatEntry(uint16_t species);
