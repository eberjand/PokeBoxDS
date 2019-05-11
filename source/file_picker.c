#include <nds.h>
#include <fat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_CONSOLE_ROWS 24
#define MAX_CONSOLE_COLS 32

int print_filename(char *name, int is_dir, int pos) {
	int name_len;
	int skip_newline;
	int pos_max;

	name_len = strlen(name) + (is_dir != 0);
	skip_newline = (name_len >= MAX_CONSOLE_COLS - 2);
	pos_max = name_len - (MAX_CONSOLE_COLS - 2);
	if (pos > pos_max)
		pos = pos_max;
	if (pos > 0) {
		if (name_len - pos > MAX_CONSOLE_COLS - 2) {
			iprintf("..%.26s..", name + pos + 2);
		} else if (is_dir) {
			iprintf("..%s/", name + pos + 2);
		} else {
			iprintf("..%s", name + pos + 2);
		}
	} else if (name_len > MAX_CONSOLE_COLS - 2) {
		iprintf("%.28s..", name);
	} else if (is_dir) {
		iprintf("%s/", name);
	} else {
		iprintf("%s", name);
	}

	return skip_newline;
}

void print_dirents(char *pwd, struct dirent *dirents, int num_files, int scroll) {
	int skip_newline = 0;
	struct dirent *pent = dirents + scroll;
	num_files -= scroll;

	consoleClear();
	// Print current directory on the first line
	iprintf("%s", pwd);

	for (int file_idx = 0; file_idx < num_files; file_idx++, pent++) {
		if (file_idx >= MAX_CONSOLE_ROWS - 1)
			break;
		if (!skip_newline)
			iprintf("\n");
		iprintf("  ");
		skip_newline = print_filename(pent->d_name, pent->d_type == DT_DIR, 0);
	}
	fflush(stdout);
}

int path_ascend(char *path) {
	char *sep = strrchr(path, '/');

	if (sep == NULL || !strcmp(path, "/"))
		// Can't ascend
		return 0;

	// When ascending to root, keep the slash
	if (sep == path) {
		path[1] = 0;
	} else if (sep != NULL) {
		*sep = 0;
	}

	return 1;
}

int path_descend(char *path, char *adding, int path_max) {
	// Handle the special directory entries
	if (!strcmp(adding, "."))
		return 0;
	if (!strcmp(adding, ".."))
		return path_ascend(path);

	// Don't open anything that exceeds the pwd
	// The +2 accounts for the "/" separator and terminating NUL
	if (strlen(path) + strlen(adding) + 2 > path_max)
		return 0;
	// Don't append a '/' at the root because path already ends with '/'
	if (strcmp(path, "/"))
		strcat(path, "/");
	strcat(path, adding);
	return 1;
}

int filePicker(char *path, size_t path_max) {
	char *err = NULL;
	struct dirent *dirents;
	int DIRENTS_MAX = 128;
	int done = 0;

	consoleDemoInit();
	if (!fatInitDefault())
		err = "fatInitDefault failure";

	strncpy(path, "/", path_max);
	dirents = malloc(DIRENTS_MAX * sizeof(struct dirent));

	while (!done) {
		DIR *pdir;
		struct dirent *pent;
		int num_files = 0;
		KEYPAD_BITS keys;
		int cursor_pos = 0;
		int scroll_pos = 0;
		int filename_pos = 0;

		if (err)
			break;

		pdir = opendir(path);

		if (!pdir) {
			err = "opendir() failure";
			break;
		}

		while ((pent = readdir(pdir)) != NULL) {
			if (!strcmp(pent->d_name, "."))
				continue;
			if (num_files >= DIRENTS_MAX)
				break;

			memcpy(dirents + num_files, pent, sizeof(struct dirent));
			num_files++;

		}
		closedir(pdir);
		print_dirents(path, dirents, num_files, scroll_pos);

		// Put the cursor at 1,0
		iprintf("\x1b[1;0H*");

		for (;;) {
			swiWaitForVBlank();
			scanKeys();
			keys = keysDown();
			if (keys & KEY_A) {
				pent = dirents + cursor_pos + scroll_pos;
				if (!path_descend(path, pent->d_name, path_max))
					continue;
				if (pent->d_type != DT_DIR) {
					done = 1;
				}
				break;
			} else if (keys & KEY_B) {
				if (!path_ascend(path))
					continue;
				break;
			}
			keys = keysDownRepeat();
			if (keys & (KEY_DOWN | KEY_UP)) {
				int rel = (keys & KEY_DOWN) ? 1 : -1;
				int scrolling = 0;
				if (cursor_pos + rel < 0) {
					if (scroll_pos == 0)
						continue;
					scrolling = 1;
				}
				if (cursor_pos + scroll_pos + rel >= num_files)
					continue;
				if (cursor_pos + rel >= MAX_CONSOLE_ROWS - 1)
					scrolling = 1;

				if (scrolling) {
					scroll_pos += rel;
					print_dirents(path, dirents, num_files, scroll_pos);
				} else {
					pent = dirents + cursor_pos + scroll_pos;
					// Overwrite the old indicator with a space
					iprintf("\x1b[%d;0H  ", cursor_pos + 1);
					print_filename(pent->d_name, pent->d_type == DT_DIR, 0);
					cursor_pos += rel;
				}

				// Write the new indicator
				iprintf("\x1b[%d;0H*", cursor_pos + 1);
				filename_pos = 0;
			}
			if (keys & (KEY_LEFT | KEY_RIGHT)) {
				int rel = (keys & KEY_RIGHT) ? 1 : -1;
				int pos_before = cursor_pos + scroll_pos;
				int pos_after;
				int scroll_max;
				rel *= MAX_CONSOLE_ROWS - 1;
				pos_after = pos_before + rel;
				if (pos_after < 0)
					pos_after = 0;
				else if (pos_after >= num_files)
					pos_after = num_files - 1;

				// Don't refresh the screen if the cursor didn't move
				if (pos_before == pos_after)
					continue;
				scroll_pos += rel;
				scroll_max = num_files - (MAX_CONSOLE_ROWS - 1);
				if (scroll_pos < 0)
					scroll_pos = 0;
				if (scroll_pos > scroll_max)
					scroll_pos = scroll_max;
				cursor_pos = pos_after - scroll_pos;
				print_dirents(path, dirents, num_files, scroll_pos);
				iprintf("\x1b[%d;0H*", cursor_pos + 1);
				filename_pos = 0;
			}
			if (keys & (KEY_L | KEY_R)) {
				int rel = (keys & KEY_R) ? 1 : -1;
				int pos_max;
				int is_dir;
				filename_pos += rel;
				pent = dirents + cursor_pos + scroll_pos;
				is_dir = pent->d_type == DT_DIR;
				pos_max = strlen(pent->d_name) + is_dir - (MAX_CONSOLE_COLS - 2);
				if (pos_max < 0)
					pos_max = 0;
				if (filename_pos > pos_max)
					filename_pos = pos_max;
				if (filename_pos < 0)
					filename_pos = 0;
				iprintf("\x1b[%d;2H", cursor_pos + 1);
				print_filename(pent->d_name, is_dir, filename_pos);
			}
		}
	}

	free(dirents);

	if (err) {
		consoleClear();
		iprintf("%s; terminating\n", err);

		for (;;) {
			swiWaitForVBlank();
			scanKeys();
			if (keysDown()) break;
		}
		return 0;
	}
	return 1;
}
