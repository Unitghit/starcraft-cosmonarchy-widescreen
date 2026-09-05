#!/usr/bin/env python3
"""Check the C++ minimap patch guards against an actual stable GPTP binary."""

import argparse
import re
import struct
from pathlib import Path


def verify(binary: bytes, source: str) -> None:
    pe = struct.unpack_from('<I', binary, 0x3c)[0]
    sections = pe + 24 + struct.unpack_from('<H', binary, pe + 20)[0]
    image_base = struct.unpack_from('<I', binary, pe + 24 + 28)[0]

    def at(rva: int, size: int) -> bytes:
        for index in range(struct.unpack_from('<H', binary, pe + 6)[0]):
            entry = sections + index * 40
            _, virtual, raw_size, raw = struct.unpack_from(
                '<IIII', binary, entry + 8)
            if virtual <= rva and rva + size <= virtual + raw_size:
                return binary[raw + rva - virtual:raw + rva - virtual + size]
        raise AssertionError(f'RVA {rva:#x} is not backed by file bytes')

    function = source.split('void EnsureGptpMinimapViewportBox()', 1)[1]
    function = function.split('void EnsureGptpUpgradeResearchClear()', 1)[0]
    constants = {name: int(value, 16) for name, value in re.findall(
        r'constexpr (?:uintptr_t|uint32_t) (\w+) = (0x[0-9A-Fa-f]+);',
        function)}
    blocks = {
        'sequence': at(constants['sequence_rva'], 0x50),
        'draw_sequence': at(constants['draw_sequence_rva'], 0x90),
    }
    checks = re.findall(
        r'\b(sequence|draw_sequence)\[(0x[0-9A-Fa-f]+)\] == '
        r'(0x[0-9A-Fa-f]+)', function)
    assert len(checks) >= 16, 'Expected both native routine byte guards'
    for block, offset, expected in checks:
        actual = blocks[block][int(offset, 16)]
        assert actual == int(expected, 16), (
            f'{block}[{offset}]: expected {expected}, binary has {actual:#04x}')
    assert blocks['sequence'][:6] == bytes.fromhex('55 8b ec 83 ec 08')
    assert constants['sequence_rva'] + 0x4d + struct.unpack_from(
        '<i', blocks['sequence'], 0x49)[0] == constants['minimap_converter_rva']
    for block, offset, key, relocated in (
        ('sequence', 0x09, 'width_rva', True),
        ('sequence', 0x13, 'mouse_x_address', False),
        ('sequence', 0x20, 'height_rva', True),
        ('sequence', 0x2d, 'mouse_y_address', False),
        ('draw_sequence', 0x62, 'camera_y_pointer_rva', True),
        ('draw_sequence', 0x84, 'camera_x_pointer_rva', True),
    ):
        actual = struct.unpack_from('<I', blocks[block], offset)[0]
        assert actual == constants[key] + (image_base if relocated else 0)
    for key, target in (('camera_y_pointer_rva', 0x6284a8),
                        ('camera_x_pointer_rva', 0x62848c)):
        assert struct.unpack('<I', at(constants[key], 4))[0] == target
    # The draw code dereferences each pointer cell once more before division.
    assert blocks['draw_sequence'][0x71:0x75] == bytes.fromhex('8b 00 f7 f7')
    assert blocks['draw_sequence'][0x8b:0x8f] == bytes.fromhex('8b 00 f7 f7')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gptp', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / 'ZoomSource/Cosmonarchy-aidebug-resolution/src/limits.cpp'
              ).read_text(encoding='utf-8')
    binary = args.gptp.read_bytes()
    verify(binary, source)
    # Ensure this check catches the incorrect encoding from the last build.
    broken = source.replace('draw_sequence[0x66] == 0x33',
                            'draw_sequence[0x66] == 0x31')
    assert broken != source
    try:
        verify(binary, broken)
    except AssertionError:
        pass
    else:
        raise AssertionError('Incorrect XOR encoding was not rejected')
    print('Minimap byte guards, camera pointers, and regression check: PASS')
