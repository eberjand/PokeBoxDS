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
#pragma once

#include <stdint.h>

struct ListMenuItem {
	const char *str;
	int extra;
};

struct ListMenuConfig {
	const char *header1;
	const char *header2;
	const struct ListMenuItem *items;
	int size;
	int startIndex;
	int (*hover_func)(const char *str, int extra);
	/* icon_func should:
	 * write 512 bytes (32x32 4bpp) to gfx_out
	 * write 32 bytes (palette data) to pal_out
	 */
	int (*icon_func)(uint8_t *gfx_out, uint8_t *pal_out, int extra);
};

int list_menu_open(const struct ListMenuConfig *cfg);
