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
#include "message_window.h"
#include <nds.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include "util.h"

#include "gui_util.h"
#include "text_draw.h"

#include "messageWindow_map.h"

#define MESSAGE_WIDTH 30
#define MESSAGE_HEIGHT 10
#define WRAP_LIMIT 10
#define DISPCNT_MASK 0x1F00 // Sprite and BG enable flags

static int message_screen = 0;

void open_message_window(const char *fmt, ...) {
	char text[MESSAGE_HEIGHT * (MESSAGE_WIDTH + 1)];
	char text_wrapped[MESSAGE_HEIGHT][MESSAGE_WIDTH + 1] = {{0}};
	va_list args;
	int cur_row = 0;
	int cur_col = 0;
	int last_space = 0;
	uint32_t dispcnt_prev;
	volatile uint32_t *dispcnt_ptr;

	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);

	/* Word wrapping */
	for (char c, *p = text; (c = *p); p++) {
		if (c == '\n') {
			cur_row++;
			cur_col = 0;
			last_space = 0;
			continue;
		}
		if (cur_row >= MESSAGE_HEIGHT) {
			break;
		}
		if (cur_col >= MESSAGE_WIDTH) {
			int prev_row = cur_row++;
			if (cur_row >= MESSAGE_HEIGHT) break;
			if (last_space > MESSAGE_WIDTH - WRAP_LIMIT) {
				// Copy the current word to the next line
				cur_col -= last_space + 1;
				memcpy(&text_wrapped[cur_row][0],
					&text_wrapped[prev_row][last_space+1],
					cur_col);
				text_wrapped[prev_row][last_space] = 0;
				last_space = 0;
			} else {
				cur_col = 0;
			}
		}
		if (c == ' ') {
			last_space = cur_col;
		}
		text_wrapped[cur_row][cur_col] = c;
		cur_col++;
	}

	/* Window display */
	dispcnt_ptr = message_screen ? &REG_DISPCNT_SUB : &REG_DISPCNT;
	dispcnt_prev = *dispcnt_ptr & DISPCNT_MASK;
	*dispcnt_ptr = (*dispcnt_ptr & ~DISPCNT_MASK) | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE;
	resetTextLabels(message_screen);
	draw_gui_tilemap(&messageWindow_map, message_screen, 0, 0);
	for (int row = 0; row < MESSAGE_HEIGHT; row++) {
		textLabel_t label = {message_screen, 1, row * 2 + 1, MESSAGE_WIDTH};
		drawText(&label, FONT_BLACK, FONT_WHITE, text_wrapped[row]);
	}
	{
		textLabel_t ok_label = {message_screen, 14, 21, 4};
		drawText(&ok_label, FONT_WHITE, FONT_BLACK, "OKAY");
	}

	for (;;) {
		KEYPAD_BITS keys;
		swiWaitForVBlank();

		scanKeys();
		keys = keysDown();
		if (keys & (KEY_A | KEY_B)) {
			break;
		}
	}

	/* Restore previous screen settings */
	resetTextLabels(message_screen);
	*dispcnt_ptr = (*dispcnt_ptr & ~DISPCNT_MASK) | dispcnt_prev;
}

void set_message_screen(int screen) {
	message_screen = screen;
}
