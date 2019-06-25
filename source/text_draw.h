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

#define FONT_WHITE 1
#define FONT_GRAY 5
#define FONT_BLACK 8
#define FONT_PINK 9
#define FONT_BLUE 10
#define FONT_YELLOW 11

typedef struct textLabel textLabel_t;

void resetTextLabels(uint8_t screen);
const textLabel_t* prepareTextLabel(uint8_t screen, uint8_t x, uint8_t y, uint8_t len);
void popLabels(uint8_t screen, int n);

int drawText(const textLabel_t *label, uint8_t fg, uint8_t shadow, const char *text);
int drawTextFmt(const textLabel_t *label, uint8_t fg, uint8_t shadow, const char *text, ...)
	__attribute__ ((format (printf, 4, 5)));
int drawText16(const textLabel_t *label, uint8_t fg, uint8_t shadow, const uint16_t *text);
