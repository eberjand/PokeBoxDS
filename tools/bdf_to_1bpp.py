#!/usr/bin/env python3
import sys
import shlex

ifile = sys.argv[1]
ofile = sys.argv[2]
codepoint_min = int(sys.argv[3], 16)
codepoint_max = int(sys.argv[4], 16)
glyphWidth = int(sys.argv[5])

isFullwidth = glyphWidth > 8

def reverseBits(x, n):
    res = 0
    for i in range(n):
        res |= (x >> i & 1) << (n - i - 1)
    return res

def cpToUnicode(cp, charset):
    if charset == 'ISO10646':
        return cp
    if charset.startswith('JISX0208'):
        if cp & 0xFF < 0x20 or cp & 0xFF00 < 0x2000:
            return 0
        iso2022_str = b'\x1b$(B' + cp.to_bytes(2, byteorder='big')
        return int.from_bytes(
            iso2022_str.decode('iso2022_jp').encode('utf_16_le'),
            byteorder='little')
    raise Exception('Unknown charset: %s' % charset)

if codepoint_min > codepoint_max or codepoint_max > 0xFFFF or codepoint_min < 0:
    print("Invalid Unicode codepoints")
    sys.exit(1)
if glyphWidth > 16:
    print("Invalid glyph width")
    sys.exit(1)

glyphBytes = 32 if isFullwidth else 16
bytebuffer = bytearray((codepoint_max - codepoint_min + 1) * glyphBytes)
numCodepoints = codepoint_max - codepoint_min + 1

headerBytes = bytes([
    codepoint_min & 0xFF, (codepoint_min >> 8) & 0xFF,
    numCodepoints & 0xFF, (numCodepoints >> 8) & 0xFF,
    glyphWidth, 16, 0, 0])

# Preserve the existing file if it exists and matches the given arguments
try:
    with open(ofile, "rb") as fp:
        in_cpMin = int.from_bytes(fp.read(2), byteorder='little')
        in_cpNum = int.from_bytes(fp.read(2), byteorder='little')
        in_width = fp.read(4)[0]
        if in_cpMin != codepoint_min:
            pass
        elif in_width != glyphWidth:
            pass
        else:
            in_cpNum = min(in_cpNum, numCodepoints)
            print('Preserving %d glyphs from the existing file' % in_cpNum)
            copy_len = in_cpNum * [16,32][glyphWidth > 8]
            bytebuffer[:copy_len] = fp.read(copy_len)
except FileNotFoundError:
    pass

# Read the BDF file
with open(ifile, "r") as fp:
    codepoint = 0
    codepoint_off = 0
    rowIdx = 0
    byteIdx = 0
    in_bitmap = False
    charset = None
    for line in fp:
        try:
            line_split = shlex.split(line)
        except ValueError:
            line_split = line.split()
        if len(line_split) < 1:
            continue

        if line_split[0] == 'ENDCHAR':
            in_bitmap = False
            codepoint = 0

        elif in_bitmap:
            row_bytes = bytearray.fromhex(line_split[0])

            word = int.from_bytes(row_bytes, byteorder='big')
            if isFullwidth and len(row_bytes) == 1:
                # Center any halfwidth glyph in a fullwidth slot
                word <<= 4

            # The BDF format encodes the leftmost pixel of a row as the
            #   highest-order bit. We need the opposite, so we reverse
            #   all the bits.
            word = reverseBits(word, 16 if isFullwidth else 8)

            bytebuffer[byteIdx] = word & 0xFF
            byteIdx += 1
            if isFullwidth:
                bytebuffer[byteIdx] = word >> 8
                byteIdx += 1

            rowIdx += 1

        elif line_split[0] == 'CHARSET_REGISTRY':
            charset = line_split[1]

        elif line_split[0] == 'ENCODING':
            codepoint = cpToUnicode(int(line_split[1]), charset)
            if codepoint < codepoint_min or codepoint > codepoint_max:
                codepoint = 0
            else:
                codepoint_off = (codepoint - codepoint_min) * glyphBytes

        elif line_split[0] == 'BITMAP':
            if codepoint != 0:
                in_bitmap = True
                rowIdx = 0
                byteIdx = (codepoint - codepoint_min) * glyphBytes
                # Zero any existing glyph data for this codepoint
                bytebuffer[byteIdx:byteIdx+glyphBytes] = bytes(glyphBytes)

# Write output binary file
with open(ofile, "wb") as fp:
    fp.write(headerBytes)
    fp.write(bytebuffer)
