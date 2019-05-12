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
	char **selected_out, int *extra_out);

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
