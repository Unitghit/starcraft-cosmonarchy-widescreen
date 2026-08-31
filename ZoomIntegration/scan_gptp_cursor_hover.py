#!/usr/bin/env python3
"""Locate stable-GPTP cursor selector references in the installed PE."""

from __future__ import annotations

import pathlib
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def main() -> int:
    path = pathlib.Path(sys.argv[1])
    image = path.read_bytes()
    pe = u32(image, 0x3C)
    section_count = u16(image, pe + 6)
    optional_size = u16(image, pe + 20)
    optional = pe + 24
    image_base = u32(image, optional + 28)
    section_table = optional + optional_size

    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    disassembler.skipdata = True
    instructions = []
    for index in range(section_count):
        entry = section_table + index * 40
        name = image[entry:entry + 8].rstrip(b"\0")
        if name != b".text":
            continue
        rva = u32(image, entry + 12)
        raw_size = u32(image, entry + 16)
        raw_offset = u32(image, entry + 20)
        code = image[raw_offset:raw_offset + raw_size]
        instructions.extend(disassembler.disasm(code, image_base + rva))

    refs = []
    for index, instruction in enumerate(instructions):
        if "5993b0" in instruction.op_str.lower():
            refs.append(index)

    for ref_index in refs:
        start = max(0, ref_index - 70)
        end = min(len(instructions), ref_index + 100)
        # Compiler-aligned functions are padded with int3. Prefer that
        # boundary to avoid dumping unrelated neighboring routines.
        for index in range(ref_index - 1, start, -1):
            if instructions[index].mnemonic == "int3":
                start = index + 1
                break
        for index in range(ref_index + 1, end):
            if (instructions[index].mnemonic == "int3" and
                    instructions[index - 1].mnemonic.startswith("ret")):
                end = index
                break

        print(f"candidate ref=0x{instructions[ref_index].address:08X} "
              f"rva=0x{instructions[start].address - image_base:08X}")
        for index in range(start, end):
            instruction = instructions[index]
            marker = " <== CURSOR RECT" if index == ref_index else ""
            if any(value in instruction.op_str.lower() for value in
                   ("6cddc4", "6cddc8", "62848c", "6284a8", "66ff5c")):
                marker += " <== STATE"
            raw = instruction.bytes.hex(" ").upper()
            print(f"{instruction.address:08X}  {raw:<27} "
                  f"{instruction.mnemonic:<8} {instruction.op_str}{marker}")
        print()

    print(f"image_base=0x{image_base:08X} cursor_rect_refs={len(refs)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
