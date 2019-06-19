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

/* PKMX data contains (hex offsets):
 * 00 current generation
 * 01 current sub-generation
 * 02-03 origin generation
 *
 * PKM data (only one of the following):
 * 04-38 PKM2 data + nickname + OT (52 bytes)
 * 04-54 PKM3 data (80 bytes)
 * 04-8C PKM4 data (136 bytes)
 * 04-8C PKM5 data (136 bytes)
 *
 * Data lost from backwards conversion for Gen2 to Gen1:
 * TODO 5 bytes for moves, 2 bytes for caught data, 1 byte for friendship
 *
 * Data lost from backwards conversion for Gen3 to Gen2:
 * TODO
 *
 * Data lost from backwards conversion for Gen4 to Gen3:
 * 55-8B TODO
 *
 * Data lost from backwards conversion for Gen5 to Gen4:
 * 8C-8F Personality value (regenerated in Gen4 to preserve nature)
 * 90-97 Lost moves
 * 98-9B Met locations
 * AE    Pokeball type (can be Dream Ball)
 * TODO need to verify French nicknames can be backported losslessly
 *
 * Data lost from forward conversion for Gen1/2 to Gen3 and higher:
 * 9C-A5 EVs (aka Stat Experience)
 * A6-A7 IVs (aka DVs; could be preserved but we're imitating PokeTransporter)
 * A8-A9 Caught/met data (Crystal only)
 * AA-AB Nickname adjustment (used for restoring the PK and MN glyphs)
 * AC    OT name adjustment (also for the PK and MN glyphs)
 *
 * Data lost from forward conversion for Gen4 to Gen5:
 * AD    Shiny Leaves (HGSS)
 */
#pragma once

#define PKMX_SIZE 176
#define BOX_SIZE_BYTES_X (176*30)
#include <stdint.h>

void pkm_to_pkmx(uint8_t *pkmx, const uint8_t *pkm, int generation);
int pkmx_convert_generation(uint8_t *pkmx, int generation);
int pkmx_to_pkm(uint8_t *pkm, uint8_t *pkmx, int generation);
