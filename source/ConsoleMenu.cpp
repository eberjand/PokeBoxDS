extern "C" {
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
}

#include "ConsoleMenu.h"

#define MAX_CONSOLE_ROWS 24
#define MAX_CONSOLE_COLS 32

// Number of rows that the menu header takes up
#define HEADER_SIZE 1

extern "C" {
int console_menu_open(
		char *header, ConsoleMenuItem *items, int size,
		char **selected_out, int *extra_out) {
	ConsoleMenu menu(header, items, size);
	menu.initConsole();
	return menu.openMenu(selected_out, extra_out);
}
}

ConsoleMenu::ConsoleMenu(char *header, ConsoleMenuItem *items, int size) :
	header(header),
	items(items),
	itemc(size),
	scroll_x(0),
	scroll_y(0),
	cursor_pos(0),
	callback(NULL) {}

void ConsoleMenu::setHoverCallback(ConsoleMenu::callback_type func) {
	this->callback = func;
}

void ConsoleMenu::initConsole() {
	videoSetMode(MODE_0_2D);
	vramSetBankA(VRAM_A_MAIN_BG);
	consoleInit(&this->console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleSelect(&this->console);
}

bool ConsoleMenu::printItem(char *name, int scroll_x) {
	int name_len = strlen(name);
	int skip_newline = (name_len >= MAX_CONSOLE_COLS - 2);

	int scroll_x_max = name_len - (MAX_CONSOLE_COLS - 2);
	if (scroll_x > scroll_x_max)
		scroll_x = scroll_x_max;

	if (scroll_x > 0) {
		if (name_len - scroll_x > MAX_CONSOLE_COLS - 2) {
			iprintf("..%.26s..", name + scroll_x + 2);
		} else {
			iprintf("..%s", name + scroll_x + 2);
		}
	} else if (name_len > MAX_CONSOLE_COLS - 2) {
		iprintf("%.28s..", name);
	} else {
		iprintf("%s", name);
	}

	return skip_newline;
}

void ConsoleMenu::printItems() {
	int item_max = this->itemc - this->scroll_y;
	bool skip_newline = false;
	ConsoleMenuItem *item = this->items + this->scroll_y;

	consoleClear();

	iprintf("%s", this->header);

	for (int item_idx = 0; item_idx < item_max; item_idx++, item++) {
		if (item_idx >= MAX_CONSOLE_ROWS - HEADER_SIZE)
			break;
		if (!skip_newline)
			iprintf("\n");
		iprintf("  ");
		skip_newline = printItem(item->str, 0);
	}
	fflush(stdout);
}

void ConsoleMenu::moveCursor(int rel) {
	int scrolling = 0;
	if (this->cursor_pos + rel < 0) {
		if (this->scroll_y == 0)
			return;
		scrolling = 1;
	}
	if (this->cursor_pos + this->scroll_y + rel >= this->itemc)
		return;
	if (this->cursor_pos + rel >= MAX_CONSOLE_ROWS - HEADER_SIZE)
		scrolling = 1;

	if (scrolling) {
		this->scroll_y += rel;
		printItems();
	} else {
		ConsoleMenuItem *item = this->items + this->cursor_pos + this->scroll_y;
		// Overwrite the old indicator with a space
		iprintf("\x1b[%d;0H  ", this->cursor_pos + HEADER_SIZE);
		// Redraw the item name without a horizontal scroll
		printItem(item->str, 0);
		cursor_pos += rel;
	}

	// Write the new indicator
	iprintf("\x1b[%d;0H*", this->cursor_pos + HEADER_SIZE);
	// Reset the horizontal scroll when selecting a different item
	this->scroll_x = 0;
}

void ConsoleMenu::movePage(int rel) {
	int pos_before = this->cursor_pos + this->scroll_y;
	int pos_after;
	int scroll_max;
	rel *= MAX_CONSOLE_ROWS - HEADER_SIZE;
	pos_after = pos_before + rel;
	if (pos_after < 0)
		pos_after = 0;
	else if (pos_after >= this->itemc)
		pos_after = this->itemc - 1;

	// Don't refresh the screen if the cursor didn't move
	if (pos_before == pos_after)
		return;
	this->scroll_y += rel;
	scroll_max = this->itemc - (MAX_CONSOLE_ROWS - HEADER_SIZE);
	if (scroll_max < 0)
		scroll_max = 0;
	if (this->scroll_y < 0)
		this->scroll_y = 0;
	if (this->scroll_y > scroll_max)
		this->scroll_y = scroll_max;
	this->cursor_pos = pos_after - this->scroll_y;
	printItems();
	iprintf("\x1b[%d;0H*", this->cursor_pos + HEADER_SIZE);
	this->scroll_x = 0;
}

void ConsoleMenu::scrollName(int rel) {
	int pos_max;
	ConsoleMenuItem *item = this->items + this->cursor_pos + this->scroll_y;
	this->scroll_x += rel;
	pos_max = strlen(item->str) - (MAX_CONSOLE_COLS - 2);
	if (pos_max < 0)
		pos_max = 0;
	if (this->scroll_x > pos_max)
		this->scroll_x = pos_max;
	if (this->scroll_x < 0)
		this->scroll_x = 0;
	iprintf("\x1b[%d;2H", this->cursor_pos + HEADER_SIZE);
	printItem(item->str, this->scroll_x);
}

bool ConsoleMenu::openMenu(char **selected_out, int *extra_out) {
	bool selecting = false;
	bool done = false;

	while (!done) {
		printItems();

		// Draw the initial cursor
		iprintf("\x1b[%d;0H*", this->cursor_pos + HEADER_SIZE);

		for (;;) {
			KEYPAD_BITS keys;

			swiWaitForVBlank();
			scanKeys();
			keys = (KEYPAD_BITS) keysDown();
			if (keys & KEY_A) {
				done = true;
				selecting = true;
				break;
			} else if (keys & KEY_B) {
				done = true;
				break;
			}
			keys = (KEYPAD_BITS) keysDownRepeat();
			if (keys & (KEY_DOWN | KEY_UP)) {
				moveCursor((keys & KEY_DOWN) ? 1 : -1);
			}
			if (keys & (KEY_LEFT | KEY_RIGHT)) {
				movePage((keys & KEY_RIGHT) ? 1 : -1);
			}
			if (keys & (KEY_L | KEY_R)) {
				scrollName((keys & KEY_R) ? 1 : -1);
			}
		}
	}

	if (selecting) {
		ConsoleMenuItem *item = this->items + this->cursor_pos + this->scroll_y;
		if (selected_out)
			*selected_out = item->str;
		if (extra_out)
			*extra_out = item->extra;
	}
	return selecting;
}
