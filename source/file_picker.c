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
#include <nds.h>
#include <fat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "ConsoleMenu.h"
#include "util.h"

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

int path_ascend(char *path, char **prev_name_out) {
	char *slash_prev = path;
	char *slash_cur = path;
	char *pos = path;
	char c;

	// Can't ascend from the root directory
	if (!strcmp(path, "/"))
		return 0;

	while ((c = *(++pos)) != 0) {
		if (c == '/') {
			slash_prev = slash_cur;
			slash_cur = pos;
		}
	}
	pos[1] = 0;
	// Ignore the trailing slash, use the second-to-last one instead
	if (slash_cur + 1 == pos) {
		slash_cur = slash_prev;
	} else {
		strcat(slash_cur, "/");
		pos++;
	}

	// If ascending to root directory, keep the / and clear the rest
	if (slash_cur == path) {
		slash_cur++;
		// Shift the previous directory name up
		for (; pos >= slash_cur; pos--) {
			pos[1] = pos[0];
		}
	}

	// Ascend by truncating at the last path separator
	*slash_cur = 0;
	if (prev_name_out)
		*prev_name_out = slash_cur + 1;
	return 1;
}

int path_descend(char *path, char *adding, int path_max) {
	int path_len;

	// Handle the special directory entries
	if (!strcmp(adding, ".") || !strcmp(adding, "./"))
		return 1;
	if (!strcmp(adding, "..") || !strcmp(adding, "../"))
		return path_ascend(path, NULL);

	// Don't open anything that exceeds the pwd buffer
	// The +2 accounts for the "/" separator and terminating NUL
	path_len = strlen(path);
	if (path_len + strlen(adding) + 2 > path_max)
		return 0;
	if (path[path_len - 1] != '/')
		strcat(path, "/");
	strcat(path, adding);
	return 1;
}

int comparator(const void *a, const void *b) {
	return strcmp(((struct ConsoleMenuItem*) a)->str, ((struct ConsoleMenuItem*) b)->str);
}

int filePicker(char *path, size_t path_max) {
	char *err = NULL;
	char *dirents;
	int DIRENTS_MAX = 128;
	int selected = 0;
	size_t NAME_LIMIT = sizeof(((struct dirent*)0)->d_name); // 256
	char *prevSelected = NULL;
	struct stat statbuf;

	dirents = malloc(DIRENTS_MAX * NAME_LIMIT);

	// If given a file path, start at its containing directory and keep the
	// file's basename for selecting the cursor's start position
	stat(path, &statbuf);
	if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
		path_ascend(path, &prevSelected);
	}

	for (;;) {
		DIR *pdir;
		struct dirent *pent;
		int num_files = 0;
		struct ConsoleMenuItem *menu_items;
		char *next_name = dirents;
		selected = 0;

		pdir = opendir(path);

		if (!pdir) {
			err = "opendir() failure";
			break;
		}

		menu_items = malloc(DIRENTS_MAX * sizeof(struct ConsoleMenuItem));

		while ((pent = readdir(pdir)) != NULL) {
			size_t name_len;
			int is_dir;

			if (pent->d_name[0] == '.')
				continue;
			if (num_files >= DIRENTS_MAX)
				break;

			strncpy(next_name, pent->d_name, NAME_LIMIT);
			next_name[NAME_LIMIT - 1] = 0;
			is_dir = (pent->d_type == DT_DIR);
			if (is_dir) {
				name_len = strlen(next_name);
				if (name_len < NAME_LIMIT - 1) {
					next_name[name_len] = '/';
					next_name[name_len + 1] = 0;
				}
			}
			menu_items[num_files].str = next_name;
			menu_items[num_files].extra = is_dir;

			num_files++;
			next_name += NAME_LIMIT;

		}
		closedir(pdir);
		qsort(menu_items, num_files, sizeof(*menu_items), &comparator);

		if (prevSelected) {
			for (int i = 0; i < num_files; i++) {
				if (!strcmp(prevSelected, menu_items[i].str)) {
					selected = i;
					break;
				}
			}
		}
		prevSelected = NULL;

		char *sel_name = NULL;
		int sel_dir = 0;
		struct ConsoleMenuConfig menuConfig = {
			.header = path,
			.items = menu_items,
			.size = num_files,
			.name_out = &sel_name,
			.extra_out = &sel_dir,
			.startIndex = selected
		};

		int opened = console_menu_open_cfg(&menuConfig);
		selected = 0;
		free(menu_items);
		if (opened) {
			if (!path_descend(path, sel_name, path_max)) {
				break;
			} else if (!sel_dir) {
				// Selected item is a regular file, not a directory
				selected = 1;
				break;
			}
		} else {
			if (!path_ascend(path, &prevSelected))
				break;
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
	return selected;
}
