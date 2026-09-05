"""Offline stable-GPTP selection callsite and ABI checks. Never modifies a binary."""
import argparse
import re
import struct
from pathlib import Path
from disasm_pe import virtual_bytes


def verify(image, source):
    pe = struct.unpack_from('<I', image, 0x3c)[0]
    base = struct.unpack_from('<I', image, pe + 24 + 28)[0]
    body = source.split('void EnsureGptpSelectionBounds()', 1)[1].split(
        'void EnsureGptpCursorWarpGuard()', 1)[0]
    target = int(re.search(r'search_rva = (0x[0-9A-Fa-f]+)', body)[1], 16)
    calls = [int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+',
        re.search(r'search_calls\[\] = \{([^}]+)', body)[1])]
    assert len(calls) == 2
    for call in calls:
        code = virtual_bytes(image, base + call, 5)
        assert code[0] == 0xe8 and call + 5 + struct.unpack_from('<i', code, 1)[0] == target
    prefix = bytes(int(v, 16) for v in re.findall(r'0x[0-9A-Fa-f]+',
        re.search(r'helper_prefix\[\] = \{([^}]+)', body)[1]))
    assert virtual_bytes(image, base + target, len(prefix)) == prefix
    assert b'\x89\x4d\xfc' in prefix  # The single bounds argument arrives in ECX.
    assert '!search_matches' in body and 'FlushInstructionCache' in body
    assert 'search_calls[1] + 5' in body  # Protection covers both complete calls.


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    source = (root / 'ZoomSource/Cosmonarchy-aidebug-resolution/src/limits.cpp').read_text()
    image = args.binary.read_bytes()
    verify(image, source)
    # A changed helper destination must fail even when the CALL opcode matches.
    try:
        verify(image, source.replace('search_rva = 0x0000D020', 'search_rva = 0x0000D021'))
    except AssertionError:
        pass
    else:
        raise AssertionError('Modified search target unexpectedly accepted')
    print('Same-type selection calls, ECX ABI, protected range and changed-target rejection: PASS')
