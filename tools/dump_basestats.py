#!/usr/bin/env python3
import struct
import sys

def printItems(vers, a, b):
    if a == 0 and b == 0:
        print('  Items%s None' % vers)
    elif a == b:
        print('  Items%s 100%% %d' % (vers, a))
    elif a != 0 and b != 0:
        print('  Items%s 50%% %d, 5%% %d' % (vers, a, b))
    elif a != 0:
        print('  Items%s 50%% %d' % (vers, a))
    elif b != 0:
        print('  Items%s 5%% %d' % (vers, b))

fp = open(sys.argv[1], 'rb')
header = fp.read(24);
if header[:8] != b'PKMBDUMP':
    print('Invalid file format')
    sys.exit(1)

version,agroup,gen,subgens = struct.unpack_from('<HBBH', header, 8)
if version != 0:
    print('Invalid file version')
    sys.exit(1)
if agroup != 4:
    print('Invalid dump type')
    sys.exit(1)

# This is incomplete, but good enough for me to test with
for idx in range(440):
    base = fp.read(32)
    evYield, = struct.unpack_from('<H', base, 10)
    items = [0] * 4
    items[0], items[1] = struct.unpack_from('<HH', base, 12)
    items[2], items[3] = struct.unpack_from('<HH', base, 28)

    print('Species %d' % idx)
    print('  Stats %d,%d,%d,%d,%d,%d' % (
        base[0], base[1], base[2],
        base[3], base[4], base[5]))
    print('  Types %d/%d' % (base[6], base[7]))
    print('  Catch Rate %d' % base[8])
    print('  Yields Exp=%d EV=%x' % (base[9], evYield))
    printItems('RSE ', items[0], items[1])
    printItems('FRLG', items[2], items[3])
    print('  FleeRate RSE=%d FRLG=%d' % (base[24], base[26]))
