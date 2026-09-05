#include "zoom_pan.h"
#include "pan_motion.h"
#include "world_zoom.h"
#include "resolution.h"
#include "console/windows_wrap.h"
#include <cstring>

namespace zoom_pan
{
    namespace
    {
        Motion middle_motion, scroll_motion;
        using MiddleScroll = void (__fastcall *)(int, int);
        MiddleScroll original_middle = nullptr;

#ifdef ZOOM_PAN_OFFLINE_TEST
        // The isolated ABI test has no engine image at StarCraft's addresses.
        // Remap its native stubs/data without requiring free low memory on CI.
        uintptr_t offline_engine_base = 0;
#endif
        inline uintptr_t EngineAddress(uintptr_t address)
        {
#ifdef ZOOM_PAN_OFFLINE_TEST
            return offline_engine_base + address - 0x400000;
#else
            return address;
#endif
        }
        int CameraX() { return *reinterpret_cast<volatile int *>(EngineAddress(0x62848c)); }
        int CameraY() { return *reinterpret_cast<volatile int *>(EngineAddress(0x6284a8)); }

        void Scale(Motion &motion, int &dx, int &dy, int camera_x, int camera_y)
        {
            if (!world_zoom::Active())
            {
                motion.Reset();
                return;
            }
            motion.Begin(camera_x, camera_y, GetTickCount());
            dx = motion.X(dx, world_zoom::VisibleWidth(), resolution::game_width);
            dy = motion.Y(dy, world_zoom::SourceScreenHeight(), resolution::screen_height);
        }

        void __fastcall MiddleDelta(int dx, int dy)
        {
            const int x = CameraX(), y = CameraY();
            Scale(middle_motion, dx, dy, x, y);
            original_middle(dx, dy);
            middle_motion.End(x, y, dx, dy, CameraX(), CameraY());
        }

        int Scroll(int amount, int direction, uintptr_t target)
        {
            const int x = CameraX(), y = CameraY();
            int dx = direction == 2 ? -amount : direction == 3 ? amount : 0;
            int dy = direction == 0 ? -amount : direction == 1 ? amount : 0;
            Scale(scroll_motion, dx, dy, x, y);
            amount = direction == 0 ? -dy : direction == 1 ? dy :
                     direction == 2 ? -dx : dx;
            int result = 0;
            if (amount)
            {
                // The original up/left ABI uses EAX; down/right uses EDX.
                // Each original callee preserves the nonvolatile registers.
                __asm {
                    mov eax, amount
                    mov edx, amount
                    call target
                    mov result, eax
                }
            }
            scroll_motion.End(x, y, dx, dy, CameraX(), CameraY());
            return result;
        }

        int __cdecl Up(int amount) { return Scroll(amount, 0, EngineAddress(0x49c360)); }
        int __cdecl Down(int amount) { return Scroll(amount, 1, EngineAddress(0x49c280)); }
        int __cdecl Left(int amount) { return Scroll(amount, 2, EngineAddress(0x49c1a0)); }
        int __cdecl Right(int amount) { return Scroll(amount, 3, EngineAddress(0x49c0c0)); }

        __declspec(naked) void UpCall()
        {
            __asm { push eax }
            __asm { call Up }
            __asm { add esp, 4 }
            __asm { ret }
        }
        __declspec(naked) void DownCall()
        {
            __asm { push edx }
            __asm { call Down }
            __asm { add esp, 4 }
            __asm { ret }
        }
        __declspec(naked) void LeftCall()
        {
            __asm { push eax }
            __asm { call Left }
            __asm { add esp, 4 }
            __asm { ret }
        }
        __declspec(naked) void RightCall()
        {
            __asm { push edx }
            __asm { call Right }
            __asm { add esp, 4 }
            __asm { ret }
        }

        bool BranchIs(uintptr_t site, unsigned char opcode, uintptr_t target)
        {
            int32_t relative = 0;
            std::memcpy(&relative, reinterpret_cast<void *>(site + 1), 4);
            return *reinterpret_cast<unsigned char *>(site) == opcode &&
                site + 5 + relative == target;
        }
        void WriteCall(uintptr_t site, uintptr_t target)
        {
            const int32_t relative = static_cast<int32_t>(target - site - 5);
            // All targets were already verified as complete five-byte calls.
            std::memcpy(reinterpret_cast<void *>(site + 1), &relative, 4);
        }
    }

    void EnsureHooks()
    {
        static bool finished = false;
        if (finished || !world_zoom::Enabled()) return;
        const uintptr_t gptp = reinterpret_cast<uintptr_t>(GetModuleHandleA("gptp.qdp"));
        if (!gptp) return;
        // Wait until GPTP has installed its own engine entry-point replacements.
        if (!BranchIs(0x484460, 0xe9, gptp + 0x67e80)) return;
        finished = true;
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(gptp);
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(gptp + dos->e_lfanew);
        if (nt->OptionalHeader.SizeOfImage < 0x67f30) return;
        const unsigned char middle_signature[] = { 0x8b, 0xd6, 0x8b, 0xc8, 0xe8, 0x73, 0xfe, 0xff, 0xff };
        const unsigned char helper_signature[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x10 };
        if (std::memcmp(reinterpret_cast<void *>(gptp + 0x67f24), middle_signature, sizeof middle_signature) ||
            std::memcmp(reinterpret_cast<void *>(gptp + 0x67da0), helper_signature, sizeof helper_signature) ||
            !BranchIs(0x47f210, 0xe8, 0x49c360) ||
            !BranchIs(0x47f226, 0xe8, 0x49c280) ||
            !BranchIs(0x47f247, 0xe8, 0x49c1a0) ||
            !BranchIs(0x47f25b, 0xe8, 0x49c0c0)) return;

        DWORD engine_protection = 0, gptp_protection = 0, ignored = 0;
        if (!VirtualProtect(reinterpret_cast<void *>(0x47f210), 0x50,
                            PAGE_EXECUTE_READWRITE, &engine_protection)) return;
        if (!VirtualProtect(reinterpret_cast<void *>(gptp + 0x67f28), 5,
                            PAGE_EXECUTE_READWRITE, &gptp_protection))
        {
            VirtualProtect(reinterpret_cast<void *>(0x47f210), 0x50, engine_protection, &ignored);
            return;
        }
        original_middle = reinterpret_cast<MiddleScroll>(gptp + 0x67da0);
        WriteCall(gptp + 0x67f28, reinterpret_cast<uintptr_t>(&MiddleDelta));
        WriteCall(0x47f210, reinterpret_cast<uintptr_t>(&UpCall));
        WriteCall(0x47f226, reinterpret_cast<uintptr_t>(&DownCall));
        WriteCall(0x47f247, reinterpret_cast<uintptr_t>(&LeftCall));
        WriteCall(0x47f25b, reinterpret_cast<uintptr_t>(&RightCall));
        VirtualProtect(reinterpret_cast<void *>(gptp + 0x67f28), 5, gptp_protection, &ignored);
        VirtualProtect(reinterpret_cast<void *>(0x47f210), 0x50, engine_protection, &ignored);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(gptp + 0x67f28), 5);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(0x47f210), 0x50);
    }
}
