#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Export a native RGBA diagnostic capture as PNG without extra dependencies."""
import argparse
from pathlib import Path
import struct
import zlib


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=480)
    args = parser.parse_args()
    pixels = args.capture.read_bytes()
    if args.width <= 0 or args.height <= 0 or len(pixels) != args.width * args.height * 4:
        parser.error("capture size does not match the supplied RGBA dimensions")
    stride = args.width * 4
    rows = b"".join(b"\0" + pixels[y * stride:(y + 1) * stride] for y in range(args.height))
    header = struct.pack(">IIBBBBB", args.width, args.height, 8, 6, 0, 0, 0)
    output = args.capture.with_suffix(".png")
    output.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))
    print(output)


if __name__ == "__main__":
    main()
