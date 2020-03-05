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
#include "tilemapdefs.h"

/* VRAM layout:
 * 5000000-50001FF (512B) BG Palettes A (Top Screen)
 * 5000200-50003FF (512B) OBJ Palettes A (Top Screen)
 * 5000400-50005FF (512B) BG Palettes B (Bottom Screen)
 * 5000600-50007FF (512B) OBJ Palettes B (Bottom Screen)
 * 6000000-607FFFF (512k) BG VRAM A (Top Screen)
 * 6200000-621FFFF (128k) BG VRAM B (Bottom Screen)
 * 6400000-643FFFF (256k) OBJ VRAM A (Top Screen)
 * 6600000-661FFFF (128k) OBJ VRAM B (Bottom Screen)
 * 7000000-70003FF (  1k) OAM A (Top Screen)
 * 7000400-70007FF (  1k) OAM B (Bottom Screen)
 *
 * BG data for each screen:
 * 00000-007FF console tile map
 * 00800-00FFF console tile map (next box)
 * 01000-017FF wallpaper tile map
 * 01800-01FFF wallpaper tile map (next box)
 * 02000-027FF UI overlays tile map
 * 04000-05FFF console tile data (8x8 font, 256 tiles)
 * 06000-0BFFF text drawing (768 tiles)
 * 0C000-0CFFF wallpaper tile data
 * 0D000-0DFFF wallpaper tile data (next box)
 * 0E000-0FFFF wallpaper tile data (unused)
 * 10000-13FFF UI overlays tile data (512 tiles)
 * 14000-1FFFF unused
 *
 * BG palettes for each screen:
 * 000-01F (00)    Console text
 * 020-07F (01-03) unused
 * 080-0FF (04-07) Current box wallpaper
 * 100-11F (08)    UI overlays
 * 120-1FF (09-15) unused
 *
 * OAM entries for each screen: (limit 0x80)
 * 00    Cursor
 * 10    Large front sprite
 * 20-3D Pokemon in holding
 * 40-5D Pokemon in current box
 * 60-7D Pokemon in next box
 *
 * OBJ data for each screen:
 * 00000-001FF Cursor
 * 04000-047FF Large front sprite (double buffered)
 * 08000-0FFFF Pokemon in holding
 * 10000-17FFF Pokemon in current box
 * 18000-1FFFF Pokemon in next box
 *
 * OBJ palettes for each screen: (each palette is 32 bytes)
 * 000-05F (00-02) Box icon sprites (only 3 palettes are needed total for every species)
 * 080-0BF (04-05) Large front sprite (double buffered)
 * 100-11F (08)    Cursor
 * 120-13F (09)    Cartridge icon
 * 140-1FF (10-15) unused
 *
 * All the "next box" sections are currently unused, but reserved for
 * implementing the sliding animation in changing between boxes
 */

#define BG_LAYER_TEXT 0
#define BG_LAYER_BUTTONS 1
#define BG_LAYER_WALLPAPER 2
#define BG_LAYER_BACKGROUND 3

// Map offset = VRAM + MAPBASE * 0x800
#define BG_MAPBASE_WALLPAPER 2
#define BG_MAPBASE_BUTTONS 4

// Tileset offset = BG_GFX + TILEBASE * 0x4000
#define BG_TILEBASE_WALLPAPER 3
#define BG_TILEBASE_BUTTONS 4

#define OAM_INDEX_CURSOR 0
#define OAM_INDEX_BIGSPRITE 0x10
#define OAM_INDEX_HOLDING 0x20
#define OAM_INDEX_CURBOX 0x40

// Sprite gfx = SPRITE_GFX + GFXIDX * 128
// The boundary size is 128 because we pass SpriteMapping_1D_128 to oamInit
#define OBJ_GFXIDX_BIGSPRITE 0x80
#define OBJ_GFXIDX_HOLDING 0x100
#define OBJ_GFXIDX_CURBOX 0x200

void draw_gui_tilemap(const tilemap_t *tilemap, uint8_t screen, uint8_t x, uint8_t y);
