#!/usr/bin/env python3
import sys

def bytesTo16(inBytes, offset):
    return int.from_bytes(inBytes[offset:offset+2], byteorder='little')

def bytesTo32(inBytes, offset):
    return int.from_bytes(inBytes[offset:offset+4], byteorder='little')

orig = None
slot1 = bytearray(0xe000)
slot2 = bytearray(0xe000)

with open(sys.argv[1], "rb") as fp:
    orig = fp.read();

for (slot, slotOff) in ((slot1, 0), (slot2, 0xe000)):
    for sector in range(0xe):
        sectorOff = slotOff + sector * 0x1000
        sectorIdx = bytesTo16(orig, sectorOff + 0xff4)
        if sectorIdx < 0xe:
            slot[sectorIdx * 0x1000:(sectorIdx + 1) * 0x1000] = orig[sectorOff:sectorOff+0x1000]

with open(sys.argv[2], "wb") as fp:
    # Write the more recent save first
    if bytesTo32(slot1, 0xffc) > bytesTo32(slot2, 0xffc):
        fp.write(slot1)
        fp.write(slot2)
    else:
        fp.write(slot2)
        fp.write(slot1)
    fp.write(orig[0x1c000:])

