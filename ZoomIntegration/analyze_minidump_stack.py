import struct
import sys

from minidump.minidumpfile import MinidumpFile
from minidump.streams.ContextStream import WOW64_CONTEXT


def module_for(modules, address):
    for module in modules:
        if module.baseaddress <= address < module.endaddress:
            return module
    return None


dump_path = sys.argv[1]
dump = MinidumpFile.parse(dump_path)
exception = dump.exception.exception_records[0]

with open(dump_path, "rb") as dump_file:
    dump_file.seek(exception.ThreadContext.Rva)
    context = WOW64_CONTEXT.parse(dump_file)

print(
    f"exception=0x{exception.ExceptionRecord.ExceptionCode_raw:08X} "
    f"address=0x{exception.ExceptionRecord.ExceptionAddress:08X} "
    f"thread=0x{exception.ThreadId:X}"
)
for register in ("Eax", "Ebx", "Ecx", "Edx", "Esi", "Edi", "Ebp", "Esp", "Eip"):
    print(f"{register.lower()}=0x{getattr(context, register):08X}")

thread = next(thread for thread in dump.threads.threads if thread.ThreadId == exception.ThreadId)
stack_end = thread.Stack.StartOfMemoryRange + thread.Stack.DataSize
stack_size = stack_end - context.Esp
reader = dump.get_reader()
stack = reader.read(context.Esp, stack_size)
modules = dump.modules.modules

print(f"stack=0x{context.Esp:08X}..0x{stack_end:08X}")
for offset in range(0, len(stack) - 3, 4):
    value = struct.unpack_from("<I", stack, offset)[0]
    module = module_for(modules, value)
    if module is None and not (0x00400000 <= value < 0x01000000):
        continue
    location = f"[esp+0x{offset:03X}] 0x{value:08X}"
    if module:
        name = module.name.rsplit("\\", 1)[-1]
        print(f"{location} {name}+0x{value - module.baseaddress:X}")
    else:
        print(f"{location} unmapped/allocated-code")
