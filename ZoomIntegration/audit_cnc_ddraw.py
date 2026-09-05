"""Read-only PE/import preflight for local, official cnc-ddraw samples.

Does not load the DLLs, modify installed files, or claim runtime compatibility.
Run with one or more paths to ddraw.dll.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def imports(binary):
    def u16(offset):
        return struct.unpack_from('<H', binary, offset)[0]

    def u32(offset):
        return struct.unpack_from('<I', binary, offset)[0]

    assert binary[:2] == b'MZ'
    pe = u32(0x3c)
    assert binary[pe:pe+4] == b'PE\0\0'
    assert u16(pe + 4) == 0x14c and u16(pe + 24) == 0x10b
    section_table = pe + 24 + u16(pe + 20)

    def at(rva, count=1):
        for i in range(u16(pe + 6)):
            entry = section_table + 40 * i
            virtual, raw_size, raw = (u32(entry + 12), u32(entry + 16),
                                      u32(entry + 20))
            if virtual <= rva and rva + count <= virtual + raw_size:
                offset = raw + rva - virtual
                assert offset + count <= len(binary)
                return offset
        raise AssertionError(f'Unbacked RVA {rva:#x}')

    def name(rva):
        offset = at(rva)
        end = binary.index(b'\0', offset)
        at(rva, end - offset + 1)
        return binary[offset:end].decode('ascii')

    import_rva, import_size = u32(pe + 24 + 104), u32(pe + 24 + 108)
    result = {}
    for i in range(import_size // 20):
        entry = at(import_rva + i * 20, 20)
        lookup, _, _, module, slots = struct.unpack_from('<IIIII', binary, entry)
        if not module:
            break
        dll = name(module).lower()
        assert lookup, 'Production named-import lookup requires OriginalFirstThunk'
        index = 0
        while True:
            value = u32(at(lookup + index * 4, 4))
            if not value:
                break
            at(slots + index * 4, 4)
            if not value & 0x80000000:
                result[dll, name(value + 2)] = slots + index * 4
            index += 1
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dll', nargs='+', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / 'ViewportConfigurator/compatibility-manifest.json'
                           ).read_text(encoding='utf-8'))
    profiles = {entry['sha256']: entry['version']
                for entry in manifest['cncDdrawProfiles']}
    for path in args.dll:
        binary = path.read_bytes()
        digest = hashlib.sha256(binary).hexdigest().upper()
        assert digest in profiles, f'Unrecognized sample: {path} ({digest})'
        slots = imports(binary)
        clip = slots['user32.dll', 'ClipCursor']
        cursor = slots['user32.dll', 'SetCursorPos']
        screen = slots['user32.dll', 'ScreenToClient']
        swap = slots['gdi32.dll', 'SwapBuffers']
        stretch = slots['gdi32.dll', 'StretchDIBits']
        print(f'PASS cnc-ddraw {profiles[digest]}: manifest hash, x86 PE, '
              f'ClipCursor={clip:#x}, SetCursorPos={cursor:#x}, '
              f'ScreenToClient={screen:#x}, SwapBuffers={swap:#x}, '
              f'StretchDIBits={stretch:#x}')
