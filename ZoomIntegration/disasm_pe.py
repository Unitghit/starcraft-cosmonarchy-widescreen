#!/usr/bin/env python3
"""Disassemble a virtual-address range from a 32-bit PE file."""

from __future__ import annotations

import argparse
import pathlib
import struct

from capstone import Cs, CS_ARCH_X86, CS_MODE_32


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def virtual_bytes(image: bytes, address: int, size: int) -> bytes:
    pe_offset = u32(image, 0x3C)
    optional_offset = pe_offset + 24
    image_base = u32(image, optional_offset + 28)
    section_count = u16(image, pe_offset + 6)
    optional_size = u16(image, pe_offset + 20)
    section_offset = optional_offset + optional_size
    rva = address - image_base
    for index in range(section_count):
        entry = section_offset + index * 40
        virtual_address = u32(image, entry + 12)
        raw_size = u32(image, entry + 16)
        raw_offset = u32(image, entry + 20)
        if virtual_address <= rva < virtual_address + raw_size:
            section_pos = rva - virtual_address
            available = raw_size - section_pos
            return image[raw_offset + section_pos : raw_offset + section_pos + min(size, available)]
    raise ValueError(f"address 0x{address:08X} is not file-backed")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=pathlib.Path)
    parser.add_argument("address", type=lambda value: int(value, 0))
    parser.add_argument("size", type=lambda value: int(value, 0))
    args = parser.parse_args()
    image = args.image.read_bytes()
    code = virtual_bytes(image, args.address, args.size)
    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    for instruction in disassembler.disasm(code, args.address):
        raw = instruction.bytes.hex(" ").upper()
        print(f"{instruction.address:08X}  {raw:<24} {instruction.mnemonic:<8} {instruction.op_str}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
