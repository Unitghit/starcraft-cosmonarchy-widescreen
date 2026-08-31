#ifndef OFFSETS_HOOKS_H
#define OFFSETS_HOOKS_H

#include <initializer_list>
#include <type_traits>

#include "offsets.h"
#include "patch/hook.h"

class hook_offset : public offset_base
{
    public:
        constexpr hook_offset(uintptr_t address) : offset_base(address) {}

        inline operator void*() const { return raw_pointer(); }
};

namespace bw
{
    using hook::Stdcall;
    using hook::Eax;
    using hook::Edx;
    using hook::Ecx;
    using hook::Ebx;
    using hook::Esi;
    using hook::Edi;

    const Stdcall<void()> WinMain = 0x004E0AE0;
    const Stdcall<void()> WindowCreated = 0x004E0842;

    const Stdcall<void()> GenerateFog = 0x0047FC50;

    const Stdcall<void()> DrawScreen = 0x0041E280;
    // DrawScreen has already validated its canvas at this point. The exact
    // five-byte instruction is safe for the legacy trampoline copier and lets
    // us keep the outer stock presentation off the physical display.
    const Stdcall<void()> DrawScreenBegin = 0x0041E297;
    // Common post-composition path.  The first instruction is an exact
    // five-byte `mov al, [no_draw]`, which the legacy trampoline copier can
    // relocate safely (unlike the ten-byte instruction at 0x0041E419).
    const Stdcall<void()> DrawScreenAfter = 0x0041E3FD;
    const Stdcall<int(Ecx<int>, Eax<int>)> IsOutsideGameScreenHook = 0x004D1140;
    const Stdcall<int(int, Rect32 *, uint8_t **, int *, int)> SDrawLockSurface = 0x00411E4E;
    const Stdcall<int(int, uint8_t *, int, int)> SDrawUnlockSurface = 0x00411E48;
    const Stdcall<void()> WaitTimesSet = 0x004DECB5;
}


#endif // OFFSETS_HOOKS_H

