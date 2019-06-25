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

#include "text_draw.h"

#include <stdarg.h>
#include <stdio.h>
#include <nds.h>

#include "colorFont.h"
#include "font0000_half_bin.h"
#include "font2000_half_bin.h"
#include "font2190_half_bin.h"
#include "font2460_full_bin.h"
#include "font2600_half_bin.h"
#include "font2700_full_bin.h"
#include "font3000_full_bin.h"
#include "fontFF00_full_bin.h"

#include "utf8.h"
#include "util.h"

struct textLabel {
	uint8_t screen;
	uint8_t x, y;
	uint8_t length;
	uint16_t tileIdx;
};
typedef struct textLabel textLabel_t;

static struct textLabel textLabelsTop[128];
static struct textLabel textLabelsBot[128];
static uint8_t textLabelTopCount = 0;
static uint8_t textLabelBotCount = 0;
static uint16_t tileIdxTop = 256;
static uint16_t tileIdxBot = 256;

struct glyphBlockHeader {
	uint16_t cpStart;
	uint16_t cpLen;
	uint8_t glyphWidth;
	uint8_t glyphHeight;
	uint16_t padding;
};

static const uint8_t *glyphBlocks[] = {
	// U+0000 - U+00FF Basic Latin (ASCII) and Latin-1 Supplement
	font0000_half_bin,
	// U+2000 - U+205F General Punctuation
	font2000_half_bin,
	// U+2190 - U+21FF Arrows
	font2190_half_bin,
	// U+2460 - U+24FF Enclosed Alphanumerics
	font2460_full_bin,
	// U+25A0 - U+25FF Geometric Shapes
	font25A0_half_bin,
	// U+2600 - U+26FF Miscellaneous Symbols
	font2600_half_bin,
	// U+2700 - U+27BF Dingbats
	font2700_full_bin,
	// U+2B00 - U+2BFF Miscellaneous Symbols and Arrows
	font2B00_full_bin,
	// U+3000 - U+30FF CJK Punctuation, Hiragana, and Katakana
	font3000_full_bin,
	// U+5186          Yen Symbol (Kanji)
	font5186_full_bin,
	// U+FF00 - U+FF64 Fullwidth Forms
	fontFF00_full_bin,
};

/* Private Use Area: U+E000
 * E000 PK
 * E001 MN
 * E002 PO
 * E003 KE
 * E004 Pokedollar
 */

static const uint8_t* getGlyph(uint16_t codepoint, uint8_t *isWide_out) {
	for (int i = 0; i < ARRAY_LENGTH(glyphBlocks); i++) {
		const uint8_t *block;
		const struct glyphBlockHeader *header;
		uint16_t offset;
		header = (const struct glyphBlockHeader*) glyphBlocks[i];
		block = (uint8_t*) (header + 1);
		offset = codepoint - header->cpStart;
		if (offset < header->cpLen) {
			int isWide;
			isWide = *isWide_out = header->glyphWidth > 8;
			return block + (isWide ? 32 : 16) * offset;
		}
	}
	*isWide_out = 0;
	return font0000_half_bin + sizeof(struct glyphBlockHeader) + 16 * '?';
}

static uint16_t* drawTextPrepare(const textLabel_t *label) {
	uint16_t *mapRam;
	uint16_t *tileRam;
	if (label->screen) {
		mapRam = BG_MAP_RAM_SUB(0);
		tileRam = BG_TILE_RAM_SUB(1);
	} else {
		mapRam = BG_MAP_RAM(0);
		tileRam = BG_TILE_RAM(1);
	}
	tileRam += label->tileIdx * 16;
	memset(tileRam, 0, label->length * 64);
	for (int i = 0; i < label->length; i++) {
		mapRam[32 * label->y       + label->x + i] = label->tileIdx + i * 2;
		mapRam[32 * (label->y + 1) + label->x + i] = label->tileIdx + i * 2 + 1;
	}
	return tileRam;
}

void resetTextLabels(uint8_t screen) {
	uint16_t *mapRam;
	if (screen) {
		textLabelBotCount = 0;
		tileIdxBot = 256;
		mapRam = BG_MAP_RAM_SUB(0);
		memcpy(BG_PALETTE_SUB, colorFontPal, sizeof(colorFontPal));
	}
	else {
		textLabelTopCount = 0;
		tileIdxTop = 256;
		mapRam = BG_MAP_RAM(0);
		memcpy(BG_PALETTE, colorFontPal, sizeof(colorFontPal));
	}
	memset(mapRam, 0, 32 * 32 * 2);
}

const textLabel_t* prepareTextLabel(uint8_t screen, uint8_t x, uint8_t y, uint8_t len) {
	textLabel_t *out;
	if (screen) {
		out = &textLabelsBot[textLabelBotCount++];
		out->tileIdx = tileIdxBot;
		tileIdxBot += len * 2;
	}
	else {
		out = &textLabelsTop[textLabelTopCount++];
		out->tileIdx = tileIdxTop;
		tileIdxTop += len * 2;
	}
	out->screen = screen;
	out->x = x;
	out->y = y;
	out->length = len;
	drawTextPrepare(out);
	return out;
}

void popLabels(uint8_t screen, int n) {
	uint8_t *countPtr;
	uint16_t *tileIdxPtr;
	struct textLabel *labelsPtr;
	if (screen) {
		countPtr = &textLabelBotCount;
		labelsPtr = textLabelsBot;
		tileIdxPtr = &tileIdxBot;
	} else {
		countPtr = &textLabelTopCount;
		labelsPtr = textLabelsTop;
		tileIdxPtr = &tileIdxTop;
	}
	for (int i = 0; i < n; i++) {
		if (*countPtr <= 0)
			break;
		(*countPtr)--;
		*tileIdxPtr -= labelsPtr[*countPtr].length * 2;
	}
}

static void drawTextTile(uint32_t *tileData, const uint8_t *glyphBits, int isWide,
	uint8_t fg, uint8_t shadow) {
	int width;
	width = isWide ? 2 : 1;
	uint16_t prevBits = 0;
	for (int i = 0; i < 16; i++) {
		uint16_t bits;
		uint16_t shadowBits;
		bits = glyphBits[i * width];
		if (isWide)
			bits |= glyphBits[i * width + 1] << 8;
		// Draw shadow below, right, and below-right of any glyph pixel.
		shadowBits = ((bits | prevBits) << 1) | prevBits;
		prevBits = bits;
		for (int x = 0; x < width; x++) {
			uint32_t fourBpp;
			fourBpp = 0;
			for (int j = 0; j < 8; j++) {
				if ((bits & 1))
					fourBpp |= fg << (j * 4);
				else if ((shadowBits & 1))
					fourBpp |= shadow << (j * 4);
				bits >>= 1;
				shadowBits >>= 1;
			}
			tileData[x * 16 + i] = fourBpp;
		}
	}
}

int drawText(const textLabel_t *label, uint8_t fg, uint8_t shadow, const char *text) {
	uint16_t *tileRam;
	const uint8_t *glyphBits;
	uint16_t codepoint;
	int outLen;
	tileRam = drawTextPrepare(label);
	outLen = 0;
	while ((codepoint = utf8_decode_next(text, &text))) {
		uint32_t tileData[32];
		uint8_t isWide = 0;
		glyphBits = getGlyph(codepoint, &isWide);
		if (outLen + isWide >= label->length)
			return outLen;
		drawTextTile(tileData, glyphBits, isWide, fg, shadow);
		memcpy(tileRam + outLen * 32, tileData,
			isWide ? sizeof(tileData) : sizeof(tileData) / 2);
		outLen += 1 + isWide;
	}
	return outLen;
}

int drawTextFmt(const textLabel_t *label, uint8_t fg, uint8_t shadow, const char *fmt, ...) {
	char text[128];
	va_list args;

	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);
	return drawText(label, fg, shadow, text);
}

int drawText16(const textLabel_t *label, uint8_t fg, uint8_t shadow, const uint16_t *text) {
	uint16_t *tileRam;
	const uint8_t *glyphBits;
	uint16_t codepoint;
	int outLen;
	tileRam = drawTextPrepare(label);
	outLen = 0;
	for (int cpIdx = 0; ((codepoint = text[cpIdx])); cpIdx++) {
		uint32_t tileData[32];
		uint8_t isWide = 0;
		glyphBits = getGlyph(codepoint, &isWide);
		if (outLen + isWide >= label->length)
			return outLen;
		drawTextTile(tileData, glyphBits, isWide, fg, shadow);
		memcpy(tileRam + outLen * 32, tileData,
			isWide ? sizeof(tileData) : sizeof(tileData) / 2);
		outLen += 1 + isWide;
	}
	return outLen;
}
