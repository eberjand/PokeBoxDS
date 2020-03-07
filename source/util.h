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

// General-purpose macros
#define ARRAY_LENGTH(array) (sizeof((array))/sizeof((array)[0]))

#define MAX(a, b) ({ \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a > _b ? _a : _b; })
#define MIN(a, b) ({  \
	__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

// Shorter alternatives to uint*_t
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;

// Utility functions for reading binary data
#define GET16(arr, offset) (*((const u16*) ((u8*) (arr) + (offset))))
#define GET32(arr, offset) (*((const u32*) ((u8*) (arr) + (offset))))

// Same as above, but valid lvalue for assignment
#define SET16(arr, offset) (*((u16*) ((u8*) (arr) + (offset))))
#define SET32(arr, offset) (*((u32*) ((u8*) (arr) + (offset))))

void wait_for_button();
