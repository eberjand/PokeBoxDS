#!/usr/bin/env python3
import sys

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


with open(ifile, "r") as fp:
    for line in fp:
        (codepoint_hex, data_hex) = line.split(':', maxsplit=1)
        codepoint = int(codepoint_hex, base=16)
        if codepoint < codepoint_min or codepoint > codepoint_max:
            continue
        codepoint_off = (codepoint - codepoint_min) * glyphBytes
        data_bytes = bytearray.fromhex(data_hex)
        if len(data_bytes) > glyphBytes:
            continue
        isWide = len(data_bytes) > 16

        idx = 0
        idxOut = 0
        while idx < len(data_bytes):
            word = data_bytes[idx]
            if isWide:
                word = word << 8 | data_bytes[idx + 1]
                idx += 1
            elif isFullwidth:
                # Center any halfwidth glyph in a fullwidth slot
                word <<= 4
            idx += 1
            # The hex format encodes the leftmost pixel of a row as the
            #   highest-order (mask 0x80) bit in each byte. We need the
            #   opposite, so we reverse all the bits.
            word = reverseBits(word, 16 if isFullwidth else 8)
            # For fullwidth glyphs, each row is encoded in the input as two
            #   bytes with byte0 holding the left 8px of the row and byte1
            #   holding the right 8px.
            bytebuffer[codepoint_off + idxOut] = word & 0xFF
            idxOut += 1
            if isFullwidth:
                bytebuffer[codepoint_off + idxOut] = word >> 8
                idxOut += 1
    pass

with open(ofile, "wb") as fp:
    fp.write(headerBytes)
    fp.write(bytebuffer)
