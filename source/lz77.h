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
#pragma once

#include <stdint.h>

uint32_t lz77_extract(void *dest, const uint32_t *src, uint32_t dest_max);
uint32_t lz77_extracted_size(const uint32_t *src);
uint32_t lz77_compressed_size(const uint32_t *src, uint32_t src_max);
uint32_t lz77_truncate(uint32_t *lzdata, uint32_t lzdata_len, uint32_t target_extracted_len);
