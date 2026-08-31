#!/usr/bin/env python3
"""Locate GPTP code using StarCraft mouse globals and native bounds."""

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
    sections = optional + optional_size
    disassembler = Cs(CS_ARCH_X86, CS_MODE_32)
    disassembler.skipdata = True

    instructions = []
    for index in range(section_count):
        entry = sections + index * 40
        name = image[entry:entry + 8].rstrip(b"\0")
        if name != b".text":
            continue
        virtual_address = u32(image, entry + 12)
        raw_size = u32(image, entry + 16)
        raw_offset = u32(image, entry + 20)
        code = image[raw_offset:raw_offset + raw_size]
        instructions.extend(disassembler.disasm(
            code, image_base + virtual_address))

    mouse_addresses = ("6cddc4", "6cddc8")
    placement_addresses = ("640890", "640892", "64088a")
    native_bounds = ("0x280", "0x190")

    mouse_indices = [
        index for index, instruction in enumerate(instructions)
        if any(address in instruction.op_str.lower()
               for address in mouse_addresses)
    ]
    placement_indices = [
        index for index, instruction in enumerate(instructions)
        if any(address in instruction.op_str.lower()
               for address in placement_addresses)
    ]
    bound_indices = [
        index for index, instruction in enumerate(instructions)
        if any(bound in instruction.op_str.lower() for bound in native_bounds)
    ]

    matches = []
    for mouse_index in mouse_indices:
        nearest_bound = min(bound_indices,
                            key=lambda item: abs(item - mouse_index))
        nearest_place = min(placement_indices,
                            key=lambda item: abs(item - mouse_index))
        bound_distance = abs(nearest_bound - mouse_index)
        place_distance = abs(nearest_place - mouse_index)
        if bound_distance <= 120 or place_distance <= 120:
            matches.append((mouse_index, nearest_bound, nearest_place,
                            bound_distance, place_distance))

    seen = set()
    for mouse_index, bound_index, place_index, bound_distance, place_distance in matches:
        center_min = min(mouse_index, bound_index, place_index)
        center_max = max(mouse_index, bound_index, place_index)
        start = max(0, center_min - 12)
        end = min(len(instructions), center_max + 20)
        key = (instructions[start].address, instructions[end - 1].address)
        if key in seen:
            continue
        seen.add(key)
        print(f"candidate mouse=0x{instructions[mouse_index].address:08X} "
              f"bound_distance={bound_distance} place_distance={place_distance}")
        for index in range(start, end):
            instruction = instructions[index]
            raw = instruction.bytes.hex(" ").upper()
            marker = ""
            if index == mouse_index:
                marker += " MOUSE"
            if index == bound_index:
                marker += " BOUND"
            if index == place_index:
                marker += " PLACE"
            print(f"{instruction.address:08X}  {raw:<24} "
                  f"{instruction.mnemonic:<8} {instruction.op_str}{marker}")
        print()
    print(f"image_base=0x{image_base:08X} mouse_refs={len(mouse_indices)} "
          f"placement_refs={len(placement_indices)} "
          f"bound_refs={len(bound_indices)} matches={len(seen)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
