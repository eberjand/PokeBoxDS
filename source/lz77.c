/*
 * This file is part of the PokeBoxDS project.
 * Copyright (C) 2020 Jennifer Berringer
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
#include "lz77.h"

#include <stddef.h>
#include <nds.h>

#include "util.h"

uint32_t lz77_extract(void *dest, const uint32_t *src, uint32_t dest_max) {
	uint32_t len;

	if (src == NULL || (len = src[0] >> 8) > dest_max)
		return 0;

	swiDecompressLZSSWram((void*) src, dest);
	return len;
}

uint32_t lz77_extracted_size(const uint32_t *src) {
	return src[0] >> 8;
}

uint32_t lz77_compressed_size(const uint32_t *src, uint32_t max) {
	const uint8_t *data = (const uint8_t*) src;
	uint32_t dec = 0;
	uint32_t dec_limit = 0;
	uint32_t size = 0;

	dec_limit = src[0] >> 8;
	size += 4;

	while (size < max && dec < dec_limit) {
		uint8_t flags;
		flags = data[size];
		size++;
		for (int i = 0; i < 8; i++, flags <<= 1) {
			if (size >= max || dec >= dec_limit)
				break;

			if ((flags & 0x80) == 0) {
				// One byte is copied from input to output when decompressing.
				dec++;
				size++;
			} else {
				// 3-18 bytes are copied from existing output to current position.
				// The location and size of this copy is denoted by 2 bytes of input.
				uint16_t meta;
				meta = data[size] | (uint16_t) data[size+1] << 8;
				dec += ((meta >> 4) & 0xF) + 3;
				size += 2;
			}
		}
	}

	// Align the size up to 4 bytes
	size = (size + 3) & ~3;
	if (size > max)
		size = max;

	return size;
}

uint32_t lz77_truncate(uint32_t *lzdata, uint32_t lzdata_len, uint32_t target_extracted_len) {
	uint8_t *data = (uint8_t*) lzdata;
	uint32_t dec = 0;
	uint32_t dec_limit = 0;
	uint32_t size = 0;
	uint32_t last_flags = 0;

	dec_limit = lzdata[0] >> 8;
	dec_limit = MIN(dec_limit, target_extracted_len);
	lzdata[0] = dec_limit << 8 | data[0];
	size += 4;

	while (size < lzdata_len && dec < dec_limit) {
		uint8_t flags;
		flags = data[size];
		last_flags = size;
		size++;
		for (int i = 0; i < 8; i++, flags <<= 1) {
			if (size >= lzdata_len || dec >= dec_limit) {
				if (flags) {
					data[last_flags] &= 0xFF00 >> i;
				}
				break;
			}

			if ((flags & 0x80) == 0) {
				// One byte is copied from input to output when decompressing.
				dec++;
				size++;
			} else if (dec + 3 > dec_limit) {
				/* Any back-reference copy will pass the limit and we need to
				 * change it to one or two single-byte copies.
				 *
				 * If it's just one byte, that's simple: change the current flag
				 * to copy a single byte and assume that byte is zero.
				 *
				 * If it's two bytes, change both the current and next flag to
				 * copy a single byte and assume those bytes are zero. Touching
				 * the next flag means we might need to add another flags byte
				 * depending on the current flag position.
				 */
				data[last_flags] &= 0xFF00 >> i;
				data[size] = 0;
				data[size + 1] = 0;
				size += dec_limit - dec;
				if (dec + 2 == dec_limit && i == 7 && size < lzdata_len) {
					data[size] = 0;
					size++;
				}
				dec = dec_limit;
				break;
			} else {
				// 3-18 bytes are copied from existing output to current position.
				// The location and size of this copy is denoted by 2 bytes of input.
				uint8_t meta;
				meta = data[size];
				dec += (meta >> 4) + 3;
				size += 2;
				if (dec > dec_limit) {
					meta = ((meta >> 4) - (dec - dec_limit)) << 4 | (meta & 0xF);
					dec = dec_limit;
				}
			}
		}
	}

	// Align the size up to 4 bytes
	while ((size & 3) != 0 && size < lzdata_len) {
		data[size++] = 0;
	}
	if (size > lzdata_len)
		size = lzdata_len;
	return size;
}
