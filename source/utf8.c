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
#include "utf8.h"

uint16_t utf8_decode_next(const char *str, const char **tail_out) {
	uint32_t codepoint;
	uint8_t startByte;
	uint8_t startMask;
	int8_t contLen;
	const char *tail;

	/* Decode the start byte:
	 *  X leading ones means there's X-1 continuing bytes
	 *  0 leading ones means there's no continuing bytes
	 *  1 leading one is errorneous; reserved for continuing bytes only, not start byte
	 *  After stripping the leading ones, the remaining start byte holds the most
	 *    significant bits of the codepoint.
	 */
	startByte = (uint8_t) str[0];
	codepoint = startByte;
	contLen = -1;
	startMask = 0x7F;
	while ((startByte & 0x80)) {
		startByte <<= 1;
		startMask >>= 1;
		contLen++;
	}
	codepoint &= startMask;
	if (contLen == 0) {
		*tail_out = str + 1;
		return '?';
	}

	/* Decode the continuing bytes:
	 *  All bytes must have exactly 1 leading one (ie 10xxxxxx).
	 *  Each continuing byte holds the next 6 bits of the codepoint.
	 *  The last continuing byte holds the least sigificant codepoint bits.
	 */
	tail = str + 1;
	for (int i = 0; i < contLen; i++) {
		uint8_t contByte = (uint8_t) *tail;
		codepoint <<= 6;
		codepoint |= contByte & 0x3F;
		tail++;
		if ((contByte & 0xC0) != 0x80) {
			*tail_out = str + 1;
			return '?';
		}
	}

	// We only care about the Basic Multilingual Plane
	if (codepoint >> 16) {
		*tail_out = str + 1;
		return '?';
	}

	*tail_out = tail;
	return (uint16_t) codepoint;
}

int utf8_encode_one(char *str, uint16_t cp, int maxBytes) {
	uint8_t revBytes[8];
	uint8_t cpLen;
	int outLen;

	if (maxBytes <= 0)
		return 0;

	// Copy ASCII bytes without change
	if ((cp >> 7) == 0) {
		str[0] = (uint8_t) cp;
		return 1;
	}

	// Split the code point into bytes holding 6 bits each
	for (cpLen = 0; cp; cpLen++) {
		revBytes[cpLen] = cp & 0x3F;
		cp >>= 6;
	}

	// If adding the leading ones would overwrite any nonzero bits, increase the length
	if ((revBytes[cpLen - 1] & (0xFF << (7 - cpLen))) != 0) {
		revBytes[cpLen] = 0;
		cpLen++;
	}

	// Add leading ones to the prefix code byte
	revBytes[cpLen - 1] |= 0xFE << (7 - cpLen);

	// Copy the bytes out in reverse order
	if (cpLen > maxBytes)
		return 0;
	outLen = 0;
	while (cpLen --> 0) {
		str[outLen++] = revBytes[cpLen] | 0x80;
	}
	return outLen;
}

int utf8_encode(char *str, const uint16_t *codepoints, int maxBytes) {
	uint16_t cp;
	int outLen;

	outLen = 0;
	maxBytes--; // Make room for NUL terminator
	while ((cp = *codepoints++)) {
		int added;
		added = utf8_encode_one(str + outLen, cp, maxBytes - outLen);
		if (!added)
			break;
		outLen += added;
	}
	str[outLen] = 0;
	return outLen;
}
