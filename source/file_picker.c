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
#include "file_picker.h"

#include <nds.h>
#include <fat.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "asset_manager.h"
#include "list_menu.h"
#include "message_window.h"
#include "util.h"

static int path_ascend(char *path, char **prev_name_out) {
	char *slash_prev = path;
	char *slash_cur = path;
	char *pos = path;
	char c;

	// Can't ascend from the root directory
	if (!strcmp(path, "/") || !path[0])
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
		*slash_cur = 0;
		slash_cur = slash_prev;
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

static int path_descend(char *path, const char *adding, int path_max) {
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

enum FileMenuType {
	FILETYPE_PARENT = 0x00,
	FILETYPE_DIR = 0x01,
	FILETYPE_ROM_GEN3 = 0x300,
	FILETYPE_ROM_GEN4 = 0x400,
	FILETYPE_ROM_GEN5 = 0x500,
	FILETYPE_MISC = 0x600
};

static int read_rom_header(const char *path) {
	int gameid;
	gameid = read_romfile_gameid(path);
	if (gameid < 0)
		gameid = FILETYPE_MISC;
	else
		gameid = FILETYPE_ROM_GEN3 | gameid;
	return gameid;
}

static int comparator(const void *a, const void *b) {
	const struct ListMenuItem *ai = a;
	const struct ListMenuItem *bi = b;
	int res = 0;
	res = ai->extra - bi->extra;
	if (!res)
		res = strcmp(ai->str, bi->str);
	return res;
}

#include "folder.h"
#include "folderParent.h"
#include "carts_gen3_32px.h"
#include "carts_gen4_32px.h"
#include "carts_gen5_32px.h"

static int write_icon(uint8_t *gfx_out, uint8_t *pal_out, int extra) {
	int extra_msb = extra & 0xFF00;
	int extra_lsb = extra & 0x00FF;

	if (extra == FILETYPE_PARENT) {
		memcpy(gfx_out, folderParentTiles, 512);
		memcpy(pal_out, folderParentPal, 32);
	} else if (extra == FILETYPE_DIR) {
		memcpy(gfx_out, folderTiles, 512);
		memcpy(pal_out, folderPal, 32);
	} else if (extra_msb == FILETYPE_ROM_GEN3) {
		memcpy(gfx_out, (uint8_t*) carts_gen3_32pxTiles + 512 * extra_lsb, 512);
		memcpy(pal_out, carts_gen3_32pxPal, 32);
	} else if (extra_msb == FILETYPE_ROM_GEN4) {
		memcpy(gfx_out, (uint8_t*) carts_gen4_32pxTiles + 512 * extra_lsb, 512);
		memcpy(pal_out, carts_gen4_32pxPal, 32);
	} else if (extra_msb == FILETYPE_ROM_GEN5) {
		memcpy(gfx_out, (uint8_t*) carts_gen5_32pxTiles + 512 * extra_lsb, 512);
		memcpy(pal_out, carts_gen5_32pxPal, 32);
	} else {
		return 0;
	}

	return 1;
}

static int file_picker_readdir(const char *path, struct ListMenuItem *menu_items,
	int limit, int filter) {
	DIR *pdir;
	struct dirent *pent;
	char *tmp_basename;
	int num_files = 0;
	char tmp_path[512];

	pdir = opendir(path);

	if (!pdir) {
		open_message_window("Unable to open directory:\n%s", path);
		return -1;
	}

	strncpy(tmp_path, path, sizeof(tmp_path) - 2);
	tmp_basename = tmp_path + strnlen(tmp_path, sizeof(tmp_path) - 2);
	tmp_basename[0] = '/';
	tmp_basename[1] = 0;
	tmp_basename++;

	while ((pent = readdir(pdir)) != NULL) {
		int type = FILETYPE_MISC;
		int namelen;
		char *name;

		if (pent->d_name[0] == '.')
			continue;
		if (num_files >= limit)
			break;

		if (pent->d_type == DT_DIR) {
			type = FILETYPE_DIR;
		} else {
			const char *ext;
			int is_rom;
			ext = strrchr(pent->d_name, '.');
			if (ext) {
				if (!strcasecmp(ext, ".gba")) {
					type = FILETYPE_ROM_GEN3;
				} else if (!strcasecmp(ext, ".nds")) {
					type = FILETYPE_ROM_GEN4;
				}
			}
			is_rom = type >= FILETYPE_ROM_GEN3 && type <= FILETYPE_ROM_GEN5;
			if (filter == FILE_FILTER_ROM && !is_rom)
				continue;
			if (filter == FILE_FILTER_SAV) {
				if (!ext || (strcasecmp(ext, ".sav") && strcasecmp(ext, ".dat"))) {
					continue;
				}
			}
			if (is_rom) {
				strncpy(tmp_basename, pent->d_name,
					sizeof(tmp_path) - (tmp_basename - tmp_path) - 1);
				tmp_path[sizeof(tmp_path) - 1] = 0;
				type = read_rom_header(tmp_path);
				if (filter == FILE_FILTER_ROM && type == FILETYPE_MISC)
					continue;
			}
		}

		namelen = strnlen(pent->d_name, sizeof(pent->d_name));
		name = malloc(namelen + 1);
		if (!name)
			break;

		strncpy(name, pent->d_name, namelen);
		name[namelen] = 0;

		menu_items[num_files].str = name;
		menu_items[num_files].extra = type;

		num_files++;
	}

	closedir(pdir);

	qsort(menu_items, num_files, sizeof(*menu_items), &comparator);

	return num_files;
}

int file_picker(char *path, size_t path_max, int filter, const char *desc) {
	int DIRENTS_MAX = 128;
	int selected = 0;
	char *prevSelected = NULL;
	struct stat statbuf;

	// If given a file path, start at its containing directory and keep the
	// file's basename for selecting the cursor's start position
	stat(path, &statbuf);
	if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
		path_ascend(path, &prevSelected);
	}

	for (;;) {
		int num_files = 0;
		struct ListMenuItem *menu_items;

		selected = 0;

		menu_items = malloc(DIRENTS_MAX * sizeof(struct ListMenuItem));
		num_files = file_picker_readdir(path, menu_items, DIRENTS_MAX, filter);

		if (num_files < 0) {
			free(menu_items);
			selected = -1;
			break;
		}

		if (prevSelected) {
			for (int i = 0; i < num_files; i++) {
				if (!strcmp(prevSelected, menu_items[i].str)) {
					selected = i;
					break;
				}
			}
		}
		prevSelected = NULL;

		struct ListMenuConfig menuConfig = {
			.header1 = desc ? desc : "Select a file",
			.header2 = path,
			.items = menu_items,
			.size = num_files,
			.startIndex = selected,
			.icon_func = write_icon
		};

		selected = list_menu_open(&menuConfig);
		if (selected >= 0) {
			struct ListMenuItem item = menu_items[selected];
			int res;

			res = path_descend(path, item.str, path_max);

			for (int i = 0; i < num_files; i++) {
				free((void*) menu_items[i].str);
			}
			free(menu_items);

			if (!res) {
				selected = -1;
				break;
			} else if (item.extra > FILETYPE_DIR) {
				// Selected item is a regular file, not a directory
				break;
			}
		} else {
			for (int i = 0; i < num_files; i++) {
				free((void*) menu_items[i].str);
			}
			free(menu_items);

			if (!path_ascend(path, &prevSelected))
				break;
		}
	}

	return selected >= 0;
}
