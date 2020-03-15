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
#include <stdint.h>

extern const uint32_t experienceTables[6][100];

const char* get_pokemon_name_by_dex(unsigned index);
uint16_t gen3_index_to_pokedex(unsigned index);
uint16_t gen3_pokedex_to_index(unsigned species);
const char* get_location_name(unsigned index, unsigned origin_game);
const char* get_item_name(unsigned index);
const char* get_move_name(unsigned index);
const char* get_type_name(unsigned index);
const char* get_egg_group_name(unsigned index);
const char* get_nature_name(unsigned index);
const char* get_ability_name(unsigned index);
uint8_t gen3_tmhm_type(unsigned item_index);
