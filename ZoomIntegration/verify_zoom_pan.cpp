// Tests the production accumulator AND x86 call adapters in an isolated process.
#define NOMINMAX
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/zoom_pan.cpp"

static bool zoom_active = true;
static unsigned crop_width = 960, crop_height = 540;
namespace world_zoom
{
    bool Enabled() { return true; }
    bool Active() { return zoom_active; }
    unsigned VisibleWidth() { return crop_width; }
    unsigned SourceScreenHeight() { return crop_height; }
}
static void Check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void Accumulators()
{
    for (unsigned output : {720u, 1080u, 1920u, 2160u, 3840u})
        for (unsigned zoom : {100u, 125u, 150u, 200u, 350u, 600u, 1200u})
            for (int sign : {-1, 1})
            {
                zoom_pan::MotionAxis axis;
                const unsigned crop = output * 100 / zoom;
                long long total = 0, input = 0;
                for (int i = 0; i < 10000; ++i)
                {
                    const int delta = sign * (i % 17 + 1);
                    input += delta;
                    total += axis.Scale(delta, crop, output);
                    Check(std::abs(total - static_cast<double>(input) * crop / output) < 1.000001,
                          "fractional travel differs by more than one world pixel");
                }
            }
    zoom_pan::MotionAxis a;
    Check(a.Scale(1, 1, 4) == 0, "quarter first");
    Check(a.Scale(1, 1, 4) == 0, "quarter second");
    Check(a.Scale(-1, 1, 4) == 0, "reverse clears carry");
    Check(a.Scale(-3, 1, 4) == -1, "reverse carry");
    Check(a.Scale(3, 1, 4) == 0, "positive fraction");
    Check(a.Scale(7, 1, 1) == 7, "native identity");
    Check(a.Scale(1, 1, 4) == 0, "identity cleared carry");
    a.Reset();
    Check(a.Scale(1, 1, 2) == 0 && a.Scale(2, 1, 4) == 1,
          "changing displayed zoom retains fractional displacement");

    zoom_pan::Motion m;
    auto fraction = [&](int camera, uint32_t tick, int after) {
        m.Begin(camera, 0, tick);
        const int dx = m.X(1, 1, 2);
        m.End(camera, 0, dx, 0, after, 0);
        return dx;
    };
    Check(fraction(0, 1, 0) == 0 && fraction(0, 2, 1) == 1, "motion carry");
    Check(fraction(1, 3, 1) == 0 && fraction(100, 4, 100) == 0, "external jump reset");
    Check(fraction(100, 200, 100) == 0, "inactivity reset");
    Check(fraction(100, 201, 100) == 1, "blocked requested pixel");
    Check(fraction(100, 202, 100) == 0, "blocked carry reset");
    m.Reset();
    Check(fraction(0, 0xfffffff0u, 0) == 0 && fraction(0, 5, 1) == 1, "tick wrap");
}

// Minimal native callees with the real register ABI and EAX result. They live
// only in this test executable; never load or patch a running game.
__declspec(naked) void NativeUp()
{
    __asm { sub dword ptr ds:[0x6284a8], eax }
    __asm { mov eax, 1 }
    __asm { ret }
}
__declspec(naked) void NativeDown()
{
    __asm { add dword ptr ds:[0x6284a8], edx }
    __asm { mov eax, 1 }
    __asm { ret }
}
__declspec(naked) void NativeLeft()
{
    __asm { sub dword ptr ds:[0x62848c], eax }
    __asm { mov eax, 1 }
    __asm { ret }
}
__declspec(naked) void NativeRight()
{
    __asm { add dword ptr ds:[0x62848c], edx }
    __asm { mov eax, 1 }
    __asm { ret }
}
static void __fastcall NativeMiddle(int dx, int dy)
{
    *reinterpret_cast<int *>(0x62848c) += dx;
    *reinterpret_cast<int *>(0x6284a8) += dy;
}
static void Jump(uintptr_t at, uintptr_t target)
{
    *reinterpret_cast<unsigned char *>(at) = 0xe9;
    zoom_pan::WriteCall(at, target);
}
static int Invoke(uintptr_t target, int amount)
{
    int result, saved_b, saved_s, saved_d;
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 12345
        mov esi, 23456
        mov edi, 34567
        mov eax, amount
        mov edx, amount
        call target
        mov result, eax
        mov saved_b, ebx
        mov saved_s, esi
        mov saved_d, edi
        pop edi
        pop esi
        pop ebx
    }
    Check(saved_b == 12345 && saved_s == 23456 && saved_d == 34567, "nonvolatile register ABI");
    return result;
}
static void Adapters()
{
    for (uintptr_t address : {0x470000u, 0x490000u, 0x620000u})
        Check(VirtualAlloc(reinterpret_cast<void *>(address), 0x10000,
                           MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE) != nullptr,
              "isolated engine address reservation");
    Jump(0x49c360, reinterpret_cast<uintptr_t>(&NativeUp));
    Jump(0x49c280, reinterpret_cast<uintptr_t>(&NativeDown));
    Jump(0x49c1a0, reinterpret_cast<uintptr_t>(&NativeLeft));
    Jump(0x49c0c0, reinterpret_cast<uintptr_t>(&NativeRight));
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(0x400000), 0x300000);
    Check(resolution::Configure(1920, 1080), "resolution");
    auto &x = *reinterpret_cast<int *>(0x62848c);
    auto &y = *reinterpret_cast<int *>(0x6284a8);
    const uintptr_t wrappers[] = {
        reinterpret_cast<uintptr_t>(&zoom_pan::UpCall),
        reinterpret_cast<uintptr_t>(&zoom_pan::DownCall),
        reinterpret_cast<uintptr_t>(&zoom_pan::LeftCall),
        reinterpret_cast<uintptr_t>(&zoom_pan::RightCall)
    };
    for (bool active : {false, true})
    {
        zoom_active = active;
        const int distance = active ? 7 : 14;
        for (unsigned i = 0; i < 4; ++i)
        {
            x = y = 1000;
            zoom_pan::scroll_motion.Reset();
            Check(Invoke(wrappers[i], 14) == 1, "native result retained");
            Check(x == 1000 + (i == 2 ? -distance : i == 3 ? distance : 0) &&
                  y == 1000 + (i == 0 ? -distance : i == 1 ? distance : 0), "native ABI scaled axis");
        }
        zoom_pan::original_middle = &NativeMiddle;
        zoom_pan::middle_motion.Reset();
        x = y = 1000;
        zoom_pan::MiddleDelta(14, -14);
        Check(x == 1000 + distance && y == 1000 - distance, "middle fastcall axes");
    }
    zoom_pan::scroll_motion.Reset();
    x = y = 1000;
    Check(Invoke(wrappers[3], 1) == 0 && x == 1000, "subpixel redraw result");
    Check(Invoke(wrappers[3], 1) == 1 && x == 1001, "subpixel motion retained");
    *reinterpret_cast<unsigned char *>(0x47f210) = 0xe8;
    zoom_pan::WriteCall(0x47f210, 0x49c360);
    Check(zoom_pan::BranchIs(0x47f210, 0xe8, 0x49c360), "branch preflight");
    Check(!zoom_pan::BranchIs(0x47f210, 0xe8, 0x49c361) &&
          !zoom_pan::BranchIs(0x47f210, 0xe9, 0x49c360), "unknown branch rejected");
    for (uintptr_t address : {0x470000u, 0x490000u, 0x620000u})
        VirtualFree(reinterpret_cast<void *>(address), 0, MEM_RELEASE);
}
int main()
{
    Accumulators();
    Adapters();
    std::puts("Zoom pan: 700,000 fractional steps, reset cases, native/middle x86 ABI and branch guards PASS");
}
