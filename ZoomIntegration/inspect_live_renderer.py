#!/usr/bin/env python3
"""Read the live StarCraft 1.16.1 draw-layer table without injecting code."""

from __future__ import annotations

import argparse
import ctypes
import struct


PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pid", type=int)
    args = parser.parse_args()
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = ctypes.c_void_p
    handle = kernel32.OpenProcess(
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, args.pid
    )
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())

    def read(address: int, size: int) -> bytes:
        output = ctypes.create_string_buffer(size)
        transferred = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(
            handle, ctypes.c_void_p(address), output, size,
            ctypes.byref(transferred)
        ):
            raise ctypes.WinError(ctypes.get_last_error())
        return output.raw[: transferred.value]

    try:
        surface = struct.unpack("<HHI", read(0x006CEFF0, 8))
        screen_x = struct.unpack("<I", read(0x0062848C, 4))[0]
        screen_y = struct.unpack("<I", read(0x006284A8, 4))[0]
        is_ingame = read(0x006556E0, 1)[0]
        print(
            f"game_screen={surface[0]}x{surface[1]} image=0x{surface[2]:08X} "
            f"camera=({screen_x},{screen_y}) is_ingame={is_ingame}"
        )
        layer_data = read(0x006CEF50, 20 * 8)
        for index in range(8):
            fields = struct.unpack_from("<BBhhhhHII", layer_data, index * 20)
            draw, flags, left, top, right, bottom, unknown, param, function = fields
            print(
                f"layer[{index}] draw={draw} flags=0x{flags:02X} "
                f"area=({left},{top},{right},{bottom}) unknown=0x{unknown:04X} "
                f"param=0x{param:08X} function=0x{function:08X}"
            )
    finally:
        kernel32.CloseHandle(handle)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
