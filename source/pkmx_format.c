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

#include "pkmx_format.h"

#include <string.h>
#include "savedata_gen3.h"

void pkm_to_pkmx(uint8_t *pkmx, const uint8_t *pkm, int generation) {
	if (generation == 0) {
		memcpy(pkmx, pkm, PKMX_SIZE);
	} else if (generation == 3) {
		pkm3_t decoded;
		memset(pkmx, 0, PKMX_SIZE);
		// TODO Can we determine whether a space is empty without decoding?
		// A lot of the other fields seem to be garbage data, not zero.
		decode_pkm_encrypted_data(&decoded, pkm);
		if (decoded.species == 0)
			return;
		pkmx[0] = 3;
		memcpy(pkmx + 4, pkm, PKM3_SIZE);
	}
}

int pkmx_convert_generation(uint8_t *pkmx, int generation) {
	if (pkmx[0] == 0 || pkmx[0] == generation || generation == 0)
		return 1;
	return 0;
}

int pkmx_to_pkm(uint8_t *pkm, uint8_t *pkmx, int generation) {
	if (!pkmx_convert_generation(pkmx, generation))
		return 0;
	if (generation == 0) {
		memcpy(pkm, pkmx, PKMX_SIZE);
	} else if (generation == 3) {
		memcpy(pkm, pkmx + 4, PKM3_SIZE);
	} else {
		return 0;
	}
	return 1;
}
