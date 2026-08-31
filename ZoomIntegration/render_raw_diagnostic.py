#!/usr/bin/env python3
"""Render an indexed8 renderer dump as diagnostic grayscale and false color."""

from __future__ import annotations

import argparse
import pathlib

from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("raw", type=pathlib.Path)
    parser.add_argument("width", type=int)
    parser.add_argument("height", type=int)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    pixels = args.raw.read_bytes()
    expected = args.width * args.height
    if len(pixels) != expected:
        raise ValueError(f"expected {expected} bytes, got {len(pixels)}")

    # A deterministic false-color palette makes boundaries, repeated regions,
    # and zero-filled areas obvious without requiring the live game palette.
    palette = []
    for value in range(256):
        palette.extend(((value * 73) & 0xFF,
                        (value * 151) & 0xFF,
                        (value * 199) & 0xFF))
    image = Image.frombytes("P", (args.width, args.height), pixels)
    image.putpalette(palette)
    image.save(args.output)
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
