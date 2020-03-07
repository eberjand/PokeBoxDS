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
#include "list_menu.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <nds.h>

#include "gui_util.h"
#include "text_draw.h"
#include "util.h"

#include "generalTileset.h"
#include "listHeader_map.h"
#include "listUnselected_map.h"
#include "listSelected_map.h"

#define MAX_LIST_ROWS 5
#define MAX_LABEL_LEN 28

struct MenuState {
	const struct ListMenuConfig *cfg;
	int cursor_pos;
	int scroll;
};

static void redraw_list(struct MenuState *state) {
	int item_max;
	const struct ListMenuItem *item;
	int (*icon_func)(uint8_t*, uint8_t*, int);

	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	resetTextLabels(1);

	icon_func = state->cfg->icon_func;

	{
		textLabel_t headLabel1 = {1, 0, 0, 32};
		textLabel_t headLabel2 = {1, 0, 2, 32};
		const char *header1, *header2;

		header1 = state->cfg->header1;
		header2 = state->cfg->header2;

		drawText(&headLabel1, FONT_WHITE, FONT_BLACK, header1 ? header1 : "");
		drawText(&headLabel2, FONT_WHITE, FONT_BLACK, header2 ? header2 : "");
		draw_gui_tilemap(&listHeader_map, 1, 0, 0);
	}

	if (state->cfg->size == 0) {
		textLabel_t label = {1, 4, 5, MAX_LABEL_LEN};
		drawText(&label, FONT_WHITE, FONT_BLACK, "(Empty list)");
	}

	item = state->cfg->items + state->scroll;
	item_max = MIN(state->cfg->size - state->scroll, MAX_LIST_ROWS);

	for (int item_idx = 0; item_idx < item_max; item_idx++, item++) {
		const char *str = item->str;
		int len;

		// Split long filenames to two text rows
		len = strnlen(item->str, MAX_LABEL_LEN * 2);
		if (len > 28) {
			textLabel_t label1 = {1, 4, 4 + item_idx * 4, MAX_LABEL_LEN};
			textLabel_t label2 = {1, 4, 6 + item_idx * 4, MAX_LABEL_LEN};
			drawText(&label1, FONT_WHITE, FONT_BLACK, str);
			drawText(&label2, FONT_WHITE, FONT_BLACK, str + MAX_LABEL_LEN);
		} else {
			textLabel_t label = {1, 4, 5 + item_idx * 4, MAX_LABEL_LEN};
			drawText(&label, FONT_WHITE, FONT_BLACK, str);
		}

		draw_gui_tilemap(
			(item_idx == state->cursor_pos) ? &listSelected_map : &listUnselected_map,
			1, 0, 4 + item_idx * 4);

		/* Draw file icon as a sprite */ {
			int has_sprite;
			SpriteEntry *entry;

			entry = &oamSub.oamMemory[item_idx];
			has_sprite = icon_func && icon_func(
				(uint8_t*) SPRITE_GFX_SUB + item_idx * 512,
				(uint8_t*) SPRITE_PALETTE_SUB + 32 * item_idx,
				item->extra);

			if (has_sprite) {
				entry->attribute[0] = OBJ_Y(item_idx * 32 + 32) | ATTR0_COLOR_16;
				entry->attribute[1] = OBJ_X(0) | ATTR1_SIZE_32;
				entry->palette = item_idx;
				entry->gfxIndex = item_idx * 4;
			} else {
				entry->isHidden = 1;
			}
		}
	}
	for (int item_idx = item_max; item_idx < MAX_LIST_ROWS; item_idx++) {
		SpriteEntry *entry;
		entry = &oamSub.oamMemory[item_idx];
		entry->isHidden = 1;
	}
	oamUpdate(&oamSub);
}

static void set_selected(struct MenuState *state, int pos) {
	int size;
	int scroll;

	size = state->cfg->size;

	if (pos < 0 || pos >= size)
		return;

	// Try to put the selected item in the middle of the screen
	scroll = pos - MAX_LIST_ROWS / 2;
	scroll = MIN(scroll, size - MAX_LIST_ROWS);
	scroll = MAX(scroll, 0);

	state->scroll = scroll;
	state->cursor_pos = pos - scroll;
}

static void move_cursor(struct MenuState *state, int rel) {
	int size = state->cfg->size;
	int cursor_pos = state->cursor_pos;
	int scroll = state->scroll;
	bool scrolling = false;

	if (size == 0)
		return;
	if (cursor_pos + rel < 0) {
		if (scroll == 0)
			return;
		scrolling = true;
	}

	if (cursor_pos + scroll + rel >= size)
		return;
	if (cursor_pos + rel >= MAX_LIST_ROWS)
		scrolling = true;

	if (scrolling) {
		state->scroll += rel;
		redraw_list(state);
	} else {
		draw_gui_tilemap(&listUnselected_map, 1, 0, 4 + cursor_pos * 4);
		state->cursor_pos = cursor_pos += rel;
		draw_gui_tilemap(&listSelected_map, 1, 0, 4 + cursor_pos * 4);
	}
}

static void move_page(struct MenuState *state, int rel) {
	int pos_before;
	int pos_after;
	int scroll;
	int scroll_max;
	int itemc;

	itemc = state->cfg->size;
	pos_before = state->cursor_pos + state->scroll;
	rel *= MAX_LIST_ROWS;
	pos_after = pos_before + rel;

	if (itemc == 0)
		return;
	if (pos_after < 0)
		pos_after = 0;
	else if (pos_after >= itemc)
		pos_after = itemc - 1;

	// Don't redraw the screen if the cursor didn't move
	if (pos_before == pos_after)
		return;

	scroll_max = MAX(itemc - MAX_LIST_ROWS, 0);
	scroll = state->scroll;
	scroll += rel;
	scroll = MAX(scroll, 0);
	scroll = MIN(scroll, scroll_max);
	state->scroll = scroll;
	state->cursor_pos = pos_after - scroll;
	redraw_list(state);
}

int list_menu_open(const struct ListMenuConfig *cfg) {
	struct MenuState state = {cfg, 0, 0};
	int out = -1;

	set_selected(&state, cfg->startIndex);

	bgInitSub(BG_LAYER_BUTTONS, BgType_Text4bpp, BgSize_T_256x256,
		BG_MAPBASE_BUTTONS, BG_TILEBASE_BUTTONS);
	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	memcpy(BG_TILE_RAM_SUB(BG_TILEBASE_BUTTONS), generalTilesetTiles, sizeof(generalTilesetTiles));
	memcpy((uint8_t*) BG_PALETTE_SUB + 32 * 8, generalTilesetPal, sizeof(generalTilesetPal));

	oamInit(&oamSub, SpriteMapping_1D_128, false);
	memset(&oamSub.oamMemory[0], 0, MAX_LIST_ROWS * sizeof(oamSub.oamMemory[0]));

	redraw_list(&state);

	keysSetRepeat(15, 5);
	for (;;) {
		KEYPAD_BITS keys;

		swiWaitForVBlank();
		scanKeys();
		keys = (KEYPAD_BITS) keysDown();
		if (keys & KEY_A) {
			if (cfg->size > 0) {
				out = state.cursor_pos + state.scroll;
			}
			break;
		} else if (keys & KEY_B) {
			break;
		}
		keys = (KEYPAD_BITS) keysDownRepeat();
		if (keys & (KEY_DOWN | KEY_UP)) {
			move_cursor(&state, (keys & KEY_DOWN) ? 1 : -1);
		}
		if (keys & (KEY_LEFT | KEY_RIGHT)) {
			move_page(&state, (keys & KEY_RIGHT) ? 1 : -1);
		}
	}

	memset(BG_MAP_RAM_SUB(BG_MAPBASE_BUTTONS), 0, 2048);
	resetTextLabels(1);
	memset(&oamSub.oamMemory[0], 0, MAX_LIST_ROWS * sizeof(oamSub.oamMemory[0]));
	for (int i = 0; i < 5; i++) {
		SpriteEntry *oam = &oamSub.oamMemory[i];
		oam->attribute[0] = 0;
		oam->attribute[1] = 0;
		oam->attribute[2] = 0;
	}
	oamDisable(&oamSub);

	return out;
}
