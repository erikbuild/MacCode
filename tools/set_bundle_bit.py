#!/usr/bin/env python3
# ABOUTME: Sets the classic-Mac "hasBundle" Finder flag on a Retro68 MacBinary app so the
# ABOUTME: Finder uses its BNDL/ICN# icon, then fixes the MacBinary header CRC. Idempotent.
import sys

def crc16_xmodem(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if (crc & 0x8000) else (crc << 1) & 0xFFFF
    return crc

def main(path):
    d = bytearray(open(path, "rb").read())
    if len(d) < 128 or d[0] != 0 or d[74] != 0 or d[82] != 0:
        sys.exit(f"{path}: not a MacBinary file; refusing to patch")
    d[73] |= 0x20                          # kHasBundle is bit 13 (0x2000); high byte = 0x20
    crc = crc16_xmodem(bytes(d[0:124]))    # MacBinary CRC covers the 124-byte header
    d[124], d[125] = (crc >> 8) & 0xFF, crc & 0xFF
    open(path, "wb").write(d)
    print(f"{path}: bundle bit set (flags-hi=0x{d[73]:02X}), header CRC=0x{crc:04X}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: set_bundle_bit.py <MacBinary-app.bin>")
    main(sys.argv[1])
