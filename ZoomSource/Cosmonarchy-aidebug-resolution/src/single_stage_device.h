#ifndef COSMONARCHY_SINGLE_STAGE_DEVICE_H
#define COSMONARCHY_SINGLE_STAGE_DEVICE_H
#include "console/windows_wrap.h"
#include <cstdint>
#include <cstring>

namespace single_stage
{
    // Locate the wrapper's actual device, not a dummy device's possibly
    // instance-private vtable. Match its full DrawPrimitive/EndScene sequence.
    // Read only: no wrapper instructions or on-disk files are changed.
    inline void **FindWrapperDeviceSlot(HMODULE module)
    {
        auto base = reinterpret_cast<uint8_t *>(module);
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
        if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
            return nullptr;
        const auto sections = IMAGE_FIRST_SECTION(nt);
        const uintptr_t begin = reinterpret_cast<uintptr_t>(base);
        const uintptr_t end = begin + nt->OptionalHeader.SizeOfImage;
        // mov eax,[device]; push 2; push 0; push 5; mov ecx,[eax]; push eax;
        // mov eax,[ecx+144h]; call eax; mov eax,[same device]; push eax;
        // mov ecx,[eax]; mov eax,[ecx+A8h]; call eax.
        constexpr uint8_t draw[] = {0x6a,2,0x6a,0,0x6a,5,0x8b,8,0x50,
            0x8b,0x81,0x44,1,0,0,0xff,0xd0,0xa1};
        constexpr uint8_t end_scene[] = {0x50,0x8b,8,0x8b,0x81,0xa8,0,0,0,0xff,0xd0};
        void **result = nullptr;
        for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s)
        {
            const auto &section = sections[s];
            if (!(section.Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            if (section.VirtualAddress >= nt->OptionalHeader.SizeOfImage ||
                section.Misc.VirtualSize > nt->OptionalHeader.SizeOfImage - section.VirtualAddress) return nullptr;
            const auto code = base + section.VirtualAddress;
            for (size_t i = 0; i + 38 <= section.Misc.VirtualSize; ++i)
            {
                const auto p = code + i;
                if (p[0] != 0xa1 || memcmp(p+5,draw,sizeof(draw)) ||
                    memcmp(p+27,end_scene,sizeof(end_scene))) continue;
                uint32_t device, again;
                memcpy(&device,p+1,4); memcpy(&again,p+23,4);
                if (device != again || device < begin || device > end - sizeof(void *)) continue;
                bool writable = false;
                for (unsigned t = 0; t < nt->FileHeader.NumberOfSections; ++t)
                    if ((sections[t].Characteristics & IMAGE_SCN_MEM_WRITE) &&
                        device >= begin + sections[t].VirtualAddress &&
                        device + sizeof(void *) <= begin + sections[t].VirtualAddress + sections[t].Misc.VirtualSize)
                        writable = true;
                if (!writable || result) return nullptr; // Ambiguous/unknown build: ordinary fallback.
                result = reinterpret_cast<void **>(device);
            }
        }
        return result;
    }
}
#endif
