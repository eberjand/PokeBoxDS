/*
 * This file is part of the PokeBoxDS project.
 * Copyright (C) 2020 Jennifer Berringer
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
#include "gui_util.h"
#include <nds.h>

void draw_gui_tilemap(const tilemap_t *tilemap, uint8_t screen, uint8_t x, uint8_t y) {
	uint8_t width = tilemap->width;
	uint8_t height = tilemap->height;
	uint16_t *mapRam;
	if (screen) {
		mapRam = BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS);
	} else {
		mapRam = BG_MAP_RAM(BG_MAPBASE_BUTTONS);
	}
	for (int rowIdx = 0; rowIdx < height; rowIdx++) {
		for (int colIdx = 0; colIdx < width; colIdx++) {
			uint16_t tspec = (8 << 12) | tilemap->map[rowIdx * width + colIdx];
			mapRam[(rowIdx + y) * 32 + colIdx + x] = tspec;
		}
	}
}
