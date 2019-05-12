#include <nds.h>

#include <stdio.h>

#include "ConsoleMenu.h"
#include "file_picker.h"
#include "sav_loader.h"

int main(int argc, char **argv) {
	char path[512];
	int rc;
	
	PrintConsole bottomScreen;
	videoSetModeSub(MODE_0_2D);
	vramSetBankC(VRAM_C_SUB_BG);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	consoleSelect(&bottomScreen);

	struct ConsoleMenuItem top_menu[] = {
		{"Slot-2 GBA Cartridge", 0},
		{"SAV file on SD card", 1}
	};

	for (;;) {
		int selected;
		int extra;

		selected = console_menu_open("Load Pokemon save data from...", top_menu, 2, NULL, &extra);

		if (extra == 0) {
			// TODO
			consoleSelect(&bottomScreen);
			consoleClear();
			iprintf("GBA slot loading is currently\nunsupported\n");
			selected = 0;
		} else {
			selected = filePicker(path, sizeof(path));
		}

		if (selected) {
			consoleSelect(&bottomScreen);
			consoleClear();
			sav_load(path);
		}
	}

	return !rc;
}
