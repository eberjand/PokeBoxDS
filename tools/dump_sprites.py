#!/usr/bin/env python3
import array
import os
import png
import struct
import sys

def color_16bit_to_tuple(halfword):
    colors_5bit = [
        0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x31, 0x39,
        0x41, 0x4a, 0x52, 0x5a, 0x62, 0x6a, 0x73, 0x7b,
        0x83, 0x8b, 0x94, 0x9c, 0xa4, 0xac, 0xb4, 0xbd,
        0xc5, 0xcd, 0xd5, 0xde, 0xe6, 0xee, 0xf6, 0xff]

    r = (halfword      ) & 0x1f
    g = (halfword >> 5 ) & 0x1f
    b = (halfword >> 10) & 0x1f
    return colors_5bit[r], colors_5bit[g], colors_5bit[b]

def write_4bpp_png(path, tiledata, palette):
    palette_16bit = array.array('H')
    palette_16bit.frombytes(palette)
    palette_tuple = [color_16bit_to_tuple(c) for c in palette_16bit]
    palette_tuple[0] = (0, 0, 0, 0)

    num_tiles = int(len(tiledata) / 32)
    if num_tiles >= 64:
        tile_width = 8
    elif num_tiles >= 16:
        tile_width = 4
    elif num_tiles >= 4:
        tile_width = 2
    elif num_tiles >= 1:
        tile_width = 1
    else:
        print('Invalid sprite image')
        sys.exit(1)
    tile_height = int(num_tiles / tile_width)

    image_array = [[0 for col in range(tile_width * 8)] for row in range(tile_height * 8)]
    for ty in range(tile_height):
        for tx in range(tile_width):
            for py in range(8):
                for px in range(4):
                    b = tiledata[(ty * tile_width + tx) * 32 + py * 4 + px]
                    image_array[ty * 8 + py][tx * 8 + px * 2] = b & 0xF
                    image_array[ty * 8 + py][tx * 8 + px * 2 + 1] = b >> 4
            pass

    w = png.Writer(tile_width * 8, tile_height * 8, palette=palette_tuple, bitdepth=8)
    f = open(path, 'wb')
    w.write(f, image_array)
    f.close()

def lz77_decompress(data):
    header, = struct.unpack_from('<I', data, 0)
    if header & 0xF0 != 0x10:
        print('Invalid LZ77 data stream')
        sys.exit(1)
    out = bytearray(header >> 8)
    in_idx = 4
    out_idx = 0

    while in_idx < len(data) and out_idx < len(out):
        flags = data[in_idx]
        in_idx += 1
        flag_mask = 0x80
        while flag_mask != 0 and in_idx < len(data) and out_idx < len(out):
            if (flags & flag_mask) == 0:
                out[out_idx] = data[in_idx]
                out_idx += 1
                in_idx += 1
            else:
                lsb = data[in_idx]
                in_idx += 1
                if in_idx == len(data):
                    break
                msb = data[in_idx]
                in_idx += 1
                disp = ((lsb & 0xF) << 8) | msb
                copylen = (lsb >> 4) + 3
                out_start = out_idx - disp - 1
                for i in range(copylen):
                    out[out_idx+i] = out[out_start+i]
                out_idx += copylen
            flag_mask >>= 1
        pass

    return out

class DumpReader:
    def __init__(self, path):
        self.fp = fp = open(path, 'rb')
        header = fp.read(24)

        # Magic check
        if header[:8] != b'PKMBDUMP':
            print('Error: specified input file is not a PokeBoxDS asset dump')
            sys.exit(1)

        # Python's array type doesn't seem to have fixed width or endianness typecodes,
        # so make sure it works as expected.
        arrtest = array.array('I')
        arrtest.frombytes(b'\x01\x02\x03\x04\xFF\xFF\xFF\xFF')
        if arrtest[0] != 0x04030201:
            print('Error: This platform is not supported.')
            sys.exit(1)

        flags = header[14]
        version, = struct.unpack_from('<H', header, 8)
        self.num_items,self.item_size = struct.unpack_from('<HH', header, 16)

        is_sprite           = flags & 0x0001 != 0
        self.has_shared_pal = flags & 0x0008 != 0
        other_flags         = flags & 0xFFF6 != 0

        if version != 0:
            print('Error: Unrecognized dump version. This script may be out of date.')
            sys.exit(1)

        if other_flags:
            print('Error: unrecognized dump flags. This script may be out of date.')
            sys.exit(1)

        if not is_sprite:
            print('Error: The given dump file doesn\'t contain sprites.')
            sys.exit(1)

        self.offsets = None
        self.palettes = None
        self.pal_indices = None

        if self.item_size == 0:
            self.offsets = array.array('I')
            self.offsets.frombytes(fp.read(self.num_items * 4))
        if self.has_shared_pal:
            self.num_pals, = struct.unpack('<I', fp.read(4))
            self.palettes = [fp.read(32) for i in range(self.num_pals)]
            self.pal_indices = fp.read(self.num_items)

    def __iter__(self):
        self.item_iterator = iter(range(self.num_items))
        self.pal_iterator = iter(range(0))
        return self

    def next_sprite(self):
        self.sprite_idx = next(self.item_iterator)
        fp = self.fp

        if self.offsets:
            fp.seek(self.offsets[self.sprite_idx])

        size = self.item_size
        num_pals = 1
        num_sprites = 1
        is_compressed = False

        if self.item_size == 0:
            size,num_pals,num_sprites = struct.unpack('<HBB', fp.read(4))
            is_compressed = num_sprites & 0x80 != 0
            num_sprites &= 0x7F

        if self.has_shared_pal:
            self.item_pals = [self.palettes[self.pal_indices[self.sprite_idx]]]
        else:
            self.item_pals = [fp.read(32) for i in range(num_pals)]

        self.pal_iterator = iter(enumerate(self.item_pals))

        self.data = fp.read(size)
        if is_compressed:
            self.data = lz77_decompress(self.data)

    def next_pal(self):
        self.pal_idx, self.pal_data = next(self.pal_iterator)

    def __next__(self):
        try:
            self.next_pal()
        except StopIteration:
            self.next_sprite()
            self.next_pal()
        return self.sprite_idx, self.pal_idx, self.data, self.pal_data

def main():
    try:
        os.mkdir(sys.argv[2])
    except FileExistsError:
        if not os.path.isdir(sys.argv[2]):
            print('Error: destination is not a directory')
            sys.exit(1)

    reader = DumpReader(sys.argv[1])

    for idx,pal_idx,data,palette in reader:
        outpng = os.path.join(sys.argv[2], 'sprite%03d-%d.png' % (idx, pal_idx))
        write_4bpp_png(outpng, data, palette)

if __name__ == '__main__':
    main()
