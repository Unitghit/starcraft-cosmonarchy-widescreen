#include "mainpatch.h"

#include <time.h>
#include <stdio.h>
#include <cstring>
#include "console/windows_wrap.h"
#include "patch/patchmanager.h"

#include "offsets_hooks.h"
#include "offsets.h"
#include "memory.h"
#include "unit.h"
#include "text.h"
#include "scconsole.h"
#include "order.h"
#include "limits.h"
#include "draw.h"
#include "resolution.h"
#include "presentation.h"
#include "runtime_diagnostics.h"
#include "yms.h"

// Every fopen call in this translation unit writes diagnostic output.
#define fopen runtime_diagnostics::Open

namespace
{
    bool fixed_renderer_enabled;

    bool FileExists(const char *path)
    {
        const DWORD attributes = GetFileAttributesA(path);
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    void ResolveViewportConfigurationPath(char *destination, size_t size)
    {
        char launch_path[MAX_PATH] = {};
        const DWORD launch_length = GetFullPathNameA(
            "cosmonarchy_viewport.ini", MAX_PATH, launch_path, nullptr);
        if (launch_length != 0 && launch_length < MAX_PATH &&
            FileExists(launch_path))
        {
            strcpy_s(destination, size, launch_path);
            return;
        }

        strcpy_s(destination, size, "cosmonarchy_viewport.ini");
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&fixed_renderer_enabled), &module))
            return;

        char module_path[MAX_PATH] = {};
        if (!GetModuleFileNameA(module, module_path, MAX_PATH))
            return;
        char *slash = strrchr(module_path, '\\');
        if (!slash)
            return;
        *slash = '\0'; // plugins
        slash = strrchr(module_path, '\\');
        if (!slash)
            return;
        *slash = '\0'; // directory containing Cosmonarchy BW.exe
        strcat_s(module_path, "\\cosmonarchy_viewport.ini");
        if (FileExists(module_path))
            strcpy_s(destination, size, module_path);
    }

    bool ConfigureRuntimeResolution()
    {
        char path[MAX_PATH] = "cosmonarchy_viewport.ini";
        ResolveViewportConfigurationPath(path, sizeof(path));
        unsigned width = static_cast<unsigned>(resolution::screen_width);
        unsigned height = static_cast<unsigned>(resolution::screen_height);
        if (FileExists(path))
        {
            width = GetPrivateProfileIntA(
                "viewport", "internal_width", width, path);
            height = GetPrivateProfileIntA(
                "viewport", "internal_height", height, path);
        }
        if (!resolution::Configure(width, height))
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "w");
            if (log)
            {
                fprintf(log,
                    "runtime resolution rejected: requested=%ux%u "
                    "allowed=640x480..%ux%u config=%s\n",
                    width, height, resolution::maximum_screen_width,
                    resolution::maximum_screen_height, path);
                fclose(log);
            }
            return false;
        }
        presentation::default_client_width =
            width * presentation::default_scale_numerator /
            presentation::default_scale_denominator;
        presentation::default_client_height =
            height * presentation::default_scale_numerator /
            presentation::default_scale_denominator;
        return true;
    }

    bool PatchPhysicalResolution(Common::PatchContext *patch)
    {
        const uint32_t stock_height = 480;
        const uint32_t stock_width = 640;
        const uintptr_t height_address = 0x0041DA3E + patch->GetDiff();
        const uintptr_t width_address = 0x0041DA43 + patch->GetDiff();
        if (*reinterpret_cast<const uint32_t *>(height_address) != stock_height ||
            *reinterpret_cast<const uint32_t *>(width_address) != stock_width)
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "a");
            if (log)
            {
                fprintf(log, "physical resolution preflight failed\n");
                fclose(log);
            }
            return false;
        }

        uint32_t height = resolution::screen_height;
        uint32_t width = resolution::screen_width;
        patch->Patch(reinterpret_cast<void *>(0x0041DA3E), &height,
                     sizeof(height), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x0041DA43), &width,
                     sizeof(width), PATCH_REPLACE);

        FILE *log = fopen("fixed_zoom_renderer.log", "w");
        if (log)
        {
            fprintf(log,
                    "fixed multipass renderer installed: output=%ux%u "
                    "battlefield=%ux%u native=%ux%u grid=%ux%u "
                    "tiles=%ux%u safe-height=%u\n",
                    static_cast<unsigned>(resolution::screen_width),
                    static_cast<unsigned>(resolution::screen_height),
                    static_cast<unsigned>(resolution::game_width),
                    static_cast<unsigned>(resolution::game_height),
                    static_cast<unsigned>(resolution::native_width),
                    static_cast<unsigned>(resolution::native_height),
                    static_cast<unsigned>(resolution::tile_columns),
                    static_cast<unsigned>(resolution::tile_rows),
                    static_cast<unsigned>(resolution::tile_width),
                    static_cast<unsigned>(resolution::tile_height),
                    static_cast<unsigned>(
                        resolution::native_safe_game_height));
            fclose(log);
        }
        return true;
    }
}

namespace bw
{
    namespace storm
    {
        intptr_t base_diff;
    }
}

void WindowCreatedPatch()
{
    HWND hwnd = static_cast<HWND>(*bw::main_window_hwnd);
    presentation::ResizeClient(hwnd);
    Common::console->HookWndProc(hwnd);
    ScConsole::HookWndProc(hwnd);
}

void WinMainPatch()
{
    Common::PatchContext patch = patch_mgr->BeginPatch(0, bw::base::starcraft);
    if (fixed_renderer_enabled)
        PatchDraw(&patch);
    PatchConsole();
    Common::console->HookTranslateAccelerator(&patch, bw::base::starcraft);
}

void InitialPatch()
{
    InitSystemInfo();

    patch_mgr = new Common::PatchManager;
    InitBwFuncs_1161(patch_mgr, (uintptr_t)GetModuleHandle(NULL));
    InitStormFuncs_1161(patch_mgr, (uintptr_t)GetModuleHandle("storm"));
    Common::PatchContext patch = patch_mgr->BeginPatch(nullptr, bw::base::starcraft);

    fixed_renderer_enabled = ConfigureRuntimeResolution() &&
        PatchPhysicalResolution(&patch);

    patch.CallHook(bw::WinMain, WinMainPatch);
    patch.CallHook(bw::WindowCreated, WindowCreatedPatch);

    RemoveLimits(&patch);
}
