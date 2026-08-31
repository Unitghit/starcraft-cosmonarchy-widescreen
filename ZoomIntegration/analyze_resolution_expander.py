import collections
import struct
import sys


path = sys.argv[1]
data = open(path, "rb").read()

# Raw PE sections from ResolutionExpander.dll. This script intentionally keeps
# the mapping explicit so audit output remains reproducible without third-party
# reverse-engineering packages.
sections = [
    (".text", 0x00000400, 0x0002AE00, 0x00015000),
    (".rdata", 0x0002B200, 0x00005600, 0x00040000),
    (".data", 0x00030800, 0x00105400, 0x00046000),
    (".idata", 0x00135C00, 0x00001200, 0x0015A000),
    (".rsrc", 0x00136E00, 0x00000600, 0x0015C000),
    (".reloc", 0x00137400, 0x00003A00, 0x0015D000),
]


def section_for_file_offset(file_offset):
    for name, raw_start, raw_size, virtual_start in sections:
        if raw_start <= file_offset < raw_start + raw_size:
            return name, 0x10000000 + virtual_start + file_offset - raw_start
    return "headers", file_offset


references = collections.defaultdict(list)
for offset in range(0, len(data) - 3):
    value = struct.unpack_from("<I", data, offset)[0]
    if 0x00400000 <= value < 0x00700000:
        references[value].append(offset)

print("StarCraft-address references embedded in ResolutionExpander.dll")
print("address    refs first-location")
for address, offsets in sorted(references.items(), key=lambda item: (-len(item[1]), item[0])):
    if len(offsets) < 2:
        continue
    section, virtual_address = section_for_file_offset(offsets[0])
    print(f"0x{address:08X} {len(offsets):4d} {section}@0x{virtual_address:08X}")

print("\nAll references in the StarCraft rendering range 0x0041C000..0x00420000")
for address, offsets in sorted(references.items()):
    if not 0x0041C000 <= address < 0x00420000:
        continue
    locations = []
    for offset in offsets[:8]:
        section, virtual_address = section_for_file_offset(offset)
        locations.append(f"{section}@0x{virtual_address:08X}")
    print(f"0x{address:08X} refs={len(offsets):3d} {' '.join(locations)}")
