#!/usr/bin/env python3
"""Recover executable-byte patches from a loaded-module memory snapshot."""

from __future__ import annotations

import argparse
import pathlib
import struct


IMAGE_SCN_MEM_EXECUTE = 0x20000000


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def executable_sections(image: bytes):
    pe_offset = u32(image, 0x3C)
    if image[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    section_count = u16(image, pe_offset + 6)
    optional_size = u16(image, pe_offset + 20)
    optional_offset = pe_offset + 24
    image_base = u32(image, optional_offset + 28)
    section_offset = optional_offset + optional_size
    for index in range(section_count):
        entry = section_offset + index * 40
        name = image[entry : entry + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        virtual_size = u32(image, entry + 8)
        virtual_address = u32(image, entry + 12)
        raw_size = u32(image, entry + 16)
        raw_offset = u32(image, entry + 20)
        characteristics = u32(image, entry + 36)
        if characteristics & IMAGE_SCN_MEM_EXECUTE:
            yield (
                name,
                image_base,
                virtual_address,
                virtual_size,
                raw_offset,
                raw_size,
            )


def changed_runs(original: bytes, patched: bytes):
    start = None
    for index, (left, right) in enumerate(zip(original, patched)):
        if left != right and start is None:
            start = index
        elif left == right and start is not None:
            yield start, index
            start = None
    if start is not None:
        yield start, min(len(original), len(patched))


def describe_jump(address: int, data: bytes) -> str:
    if len(data) >= 5 and data[0] in (0xE8, 0xE9):
        displacement = struct.unpack_from("<i", data, 1)[0]
        kind = "call" if data[0] == 0xE8 else "jmp"
        return f" {kind}->0x{address + 5 + displacement:08X}"
    if len(data) >= 6 and data[:2] == b"\x68":
        return f" push=0x{u32(data, 1):08X}"
    return ""


def compare(module_path: pathlib.Path, dump_path: pathlib.Path) -> int:
    image = module_path.read_bytes()
    dump = dump_path.read_bytes()
    change_count = 0
    for name, image_base, rva, virtual_size, raw_offset, raw_size in executable_sections(image):
        compare_size = min(raw_size, virtual_size, len(dump) - rva)
        if compare_size <= 0:
            continue
        original = image[raw_offset : raw_offset + compare_size]
        patched = dump[rva : rva + compare_size]
        print(f"[{name}] RVA 0x{rva:08X}, {compare_size} file-backed executable bytes")
        for start, end in changed_runs(original, patched):
            address = image_base + rva + start
            before = original[start:end]
            after = patched[start:end]
            suffix = describe_jump(address, after)
            print(
                f"0x{address:08X} +{end - start:3d}: "
                f"{before.hex(' ').upper()} -> {after.hex(' ').upper()}{suffix}"
            )
            change_count += 1
    print(f"{change_count} contiguous executable patch runs")
    return change_count


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("module", type=pathlib.Path)
    parser.add_argument("dump", type=pathlib.Path)
    args = parser.parse_args()
    compare(args.module, args.dump)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
