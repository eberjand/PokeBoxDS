#include <nds.h>

#include <stdio.h>

#include "file_picker.h"

int main(int argc, char **argv) {
	char path[512];
	int rc;
	
	// Initialise the console, required for printf
	consoleDemoInit();

	rc = filePicker(path, sizeof(path));

	if (rc) {
		consoleClear();
		iprintf("File: %s\n", path);
		for (;;) {
			swiWaitForVBlank();
			scanKeys();
			if (keysDown()) break;
		}
	}

	return !rc;
}
