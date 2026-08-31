#!/usr/bin/env python3
"""Resolve and disassemble Cosmonarchy's live building-placement hook."""

from __future__ import annotations

import ctypes
import struct
import sys
from ctypes import wintypes

from capstone import Cs, CS_ARCH_X86, CS_MODE_32


PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
TH32CS_SNAPPROCESS = 0x00000002
TH32CS_SNAPMODULE = 0x00000008 | 0x00000010
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("cntUsage", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("th32DefaultHeapID", ctypes.c_size_t),
        ("th32ModuleID", wintypes.DWORD),
        ("cntThreads", wintypes.DWORD),
        ("th32ParentProcessID", wintypes.DWORD),
        ("pcPriClassBase", wintypes.LONG),
        ("dwFlags", wintypes.DWORD),
        ("szExeFile", wintypes.WCHAR * 260),
    ]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", wintypes.DWORD),
        ("th32ModuleID", wintypes.DWORD),
        ("th32ProcessID", wintypes.DWORD),
        ("GlblcntUsage", wintypes.DWORD),
        ("ProccntUsage", wintypes.DWORD),
        ("modBaseAddr", ctypes.POINTER(ctypes.c_byte)),
        ("modBaseSize", wintypes.DWORD),
        ("hModule", wintypes.HMODULE),
        ("szModule", wintypes.WCHAR * 256),
        ("szExePath", wintypes.WCHAR * 260),
    ]


kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
kernel32.OpenProcess.restype = wintypes.HANDLE


def find_pid() -> int:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            if entry.szExeFile.lower() == "starcraft.exe":
                return entry.th32ProcessID
            ok = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    raise RuntimeError("StarCraft.exe is not running")


def modules(pid: int) -> list[tuple[str, int, int]]:
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid)
    if snapshot == INVALID_HANDLE_VALUE:
        raise ctypes.WinError(ctypes.get_last_error())
    result = []
    try:
        entry = MODULEENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        ok = kernel32.Module32FirstW(snapshot, ctypes.byref(entry))
        while ok:
            base = ctypes.cast(entry.modBaseAddr, ctypes.c_void_p).value
            result.append((entry.szModule, base, entry.modBaseSize))
            ok = kernel32.Module32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return result


def read(process: wintypes.HANDLE, address: int, size: int) -> bytes:
    buffer = (ctypes.c_ubyte * size)()
    count = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(
            process, ctypes.c_void_p(address), buffer, size,
            ctypes.byref(count)):
        raise ctypes.WinError(ctypes.get_last_error())
    return bytes(buffer[:count.value])


def main() -> int:
    pid = find_pid()
    loaded = modules(pid)
    for name, base, size in loaded:
        if name.lower() in {"starcraft.exe", "gptp.qdp", "aize_debug.qdp"}:
            print(f"module {name:<18} base=0x{base:08X} size=0x{size:X}")

    process = kernel32.OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, pid)
    if not process:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        hook_site = 0x0048D660
        site = read(process, hook_site, 16)
        print(f"hook_site 0x{hook_site:08X}: {site.hex(' ').upper()}")
        if site[0] != 0xE9:
            raise RuntimeError("placement routine is not a near jump")
        target = hook_site + 5 + struct.unpack_from("<i", site, 1)[0]
        print(f"hook_target=0x{target:08X}")

        code = read(process, target, 768)
        dis = Cs(CS_ARCH_X86, CS_MODE_32)
        for instruction in dis.disasm(code, target):
            raw = instruction.bytes.hex(" ").upper()
            marker = ""
            operands = instruction.op_str.lower()
            if any(value in operands for value in (
                    "0x280", "0x190", "0x6cddc4", "0x6cddc8",
                    "0x640890", "0x640892", "0x62848c", "0x6284a8")):
                marker = "  <=="
            print(f"{instruction.address:08X}  {raw:<28} "
                  f"{instruction.mnemonic:<8} {instruction.op_str}{marker}")
            if instruction.mnemonic.startswith("ret"):
                break
    finally:
        kernel32.CloseHandle(process)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
