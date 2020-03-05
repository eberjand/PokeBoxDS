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
#include "util.h"

void pkm_to_pkmx(uint8_t *pkmx, const uint8_t *pkm, uint16_t gameId) {
	uint8_t generation = gameId & 0xFF;

	if (gameId == 0) {
		memcpy(pkmx, pkm, PKMX_SIZE);
	} else if (generation == 3) {
		pkm3_t decoded;
		memset(pkmx, 0, PKMX_SIZE);
		// TODO Can we determine whether a space is empty without decoding?
		// A lot of the other fields seem to be garbage data, not zero.
		decode_pkm_encrypted_data(&decoded, pkm);
		if (decoded.species == 0)
			return;
		SET16(pkmx, 0) = gameId;
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

void pkmx_to_simplepkm(struct SimplePKM *pkm, const uint8_t *pkmx) {
	uint8_t generation = pkmx[0];
	memset(pkm, 0, sizeof(struct SimplePKM));

	pkm->curGameId = pkm->originGameId = GET16(pkmx, 0);
	/* The other 2 bytes in PKMX are reserved for:
	 *   originGen (keeping track of generation conversions)
	 *   originSubGen
	 */

	if (generation == 3) {
		pkm3_to_simplepkm(pkm, pkmx + 4);
	} else {
		memset(pkm, 0, sizeof(*pkm));
		if (generation != 0) {
			// This allows some level of compatibility with future versions.
			pkm->exists = 1;
			pkm->spriteIdx = 252;
		}
	}
}
