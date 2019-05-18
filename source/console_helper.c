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
#include "console_helper.h"

#include <nds.h>
#include "font.h"

static PrintConsole topConsole;
static PrintConsole bottomConsole;
static ConsoleFont customConsoleFont = {
	(u16*) fontTiles, (u16*) fontPal, 2, 4, 0, 256, true
};

void initConsoles() {
	consoleInit(&topConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, false);
	consoleInit(&bottomConsole, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, false);
	consoleSetFont(&topConsole, &customConsoleFont);
	consoleSetFont(&bottomConsole, &customConsoleFont);
}

void selectTopConsole() {
	consoleSelect(&topConsole);
}

void selectBottomConsole() {
	consoleSelect(&bottomConsole);
}

void clearConsoles()
{
	consoleSelect(&topConsole);
	consoleClear();
	consoleSelect(&bottomConsole);
	consoleClear();
}
