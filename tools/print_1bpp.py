#!/usr/bin/env python3
# This script prints out all the glyphs from the specified 1bpp font file to
# the terminal with block drawing. You'll probably want to pipe it into less.
import sys

ifile = sys.argv[1]

try:
    with open(ifile, "rb") as fp:
        cpStart = int.from_bytes(fp.read(2), byteorder='little')
        cpLen = int.from_bytes(fp.read(2), byteorder='little')
        glyphWidth = int(fp.read(1)[0])
        glyphHeight = int(fp.read(1)[0])
        fp.read(2)
        glyphWidth = 16 if glyphWidth > 8 else 8
        glyphBytes = 2 if glyphWidth > 8 else 1
        for cpOff in range(cpLen):
            glyph = fp.read(glyphWidth * 2)
            print('U+%04X' % (cpStart + cpOff))
            print('\u250C' + ('\u2500' * glyphWidth * 2) + '\u2510')
            for row in range(16):
                glyphRow = int.from_bytes(
                    glyph[row * glyphBytes : (row + 1) * glyphBytes],
                    byteorder='little')
                print('\u2502' + ''.join([
                    '\u2588\u2588' if (glyphRow >> i & 1 != 0) else '  '
                    for i in range(glyphWidth)
                    ]) + '\u2502')
            print('\u2514' + ('\u2500' * glyphWidth * 2) + '\u2518')
except BrokenPipeError:
    pass
