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

struct ConsoleMenuItem {
	char *str;
	int extra;
};

#ifdef __cplusplus
extern "C" {
#endif
#include <nds.h>

int console_menu_open(
	char *header, struct ConsoleMenuItem *items, int size,
	char **name_out, int *extra_out);

int console_menu_open_2(
	char *header, struct ConsoleMenuItem *items, int size,
	int *extra_out, int (*func)(char*, int));

#ifdef __cplusplus
}

class ConsoleMenu {
	public:
	typedef int (*callback_type)(char *str, int extra);
	ConsoleMenu(char *header, ConsoleMenuItem *items, int size);
	void setHoverCallback(callback_type func);
	void initConsole();
	bool openMenu(char **selected_out, int *extra_out);

	private:
	bool printItem(char *name, int scroll_x);
	void printItems();
	void updateCursor();
	void moveCursor(int rel);
	void movePage(int rel);
	void scrollName(int rel);
	PrintConsole console;
	char *header;
	ConsoleMenuItem *items;
	int itemc;
	int scroll_x;
	int scroll_y;
	int cursor_pos;
	callback_type callback;
};

#endif
