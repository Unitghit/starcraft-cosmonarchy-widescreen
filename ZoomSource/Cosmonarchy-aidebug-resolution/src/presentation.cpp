#include "presentation.h"

#include <cstdio>
#include <cstring>

#include "runtime_diagnostics.h"

// Every fopen call in this translation unit writes diagnostic output.
#define fopen runtime_diagnostics::Open

namespace presentation
{
    namespace
    {
        bool resizing_client;
        bool settings_loaded;

        struct RuntimeSettings
        {
            unsigned client_width = default_client_width;
            unsigned client_height = default_client_height;
            unsigned scale_numerator = default_scale_numerator;
            unsigned scale_denominator = default_scale_denominator;
            bool compatible = true;
            char mode[32] = "scale";
            char path[MAX_PATH] = "cosmonarchy_viewport.ini";
        };

        RuntimeSettings settings;

        bool FileExists(const char *path)
        {
            const DWORD attributes = GetFileAttributesA(path);
            return attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
        }

        void ResolveConfigurationPath(char *destination, size_t size)
        {
            // The Win32 profile APIs resolve a relative INI name against the
            // Windows directory, not necessarily the process working
            // directory used by GetFileAttributes. Always hand them an
            // absolute path after checking the launch directory.
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
                    reinterpret_cast<LPCSTR>(&settings_loaded), &module))
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

        void LoadSettings()
        {
            if (settings_loaded)
                return;
            settings_loaded = true;

            ResolveConfigurationPath(settings.path, sizeof(settings.path));
            if (!FileExists(settings.path))
                return;

            const unsigned configured_internal_width =
                GetPrivateProfileIntA("viewport", "internal_width",
                    resolution::screen_width, settings.path);
            const unsigned configured_internal_height =
                GetPrivateProfileIntA("viewport", "internal_height",
                    resolution::screen_height, settings.path);
            settings.compatible =
                configured_internal_width == resolution::screen_width &&
                configured_internal_height == resolution::screen_height;
            if (!settings.compatible)
                return;

            const unsigned configured_width = GetPrivateProfileIntA(
                "presentation", "output_width", default_client_width,
                settings.path);
            const unsigned configured_height = GetPrivateProfileIntA(
                "presentation", "output_height", default_client_height,
                settings.path);
            if (configured_width >= 320 && configured_width <= 16384 &&
                configured_height >= 240 && configured_height <= 16384)
            {
                settings.client_width = configured_width;
                settings.client_height = configured_height;
            }

            const unsigned numerator = GetPrivateProfileIntA(
                "presentation", "scale_numerator", default_scale_numerator,
                settings.path);
            const unsigned denominator = GetPrivateProfileIntA(
                "presentation", "scale_denominator",
                default_scale_denominator, settings.path);
            if (numerator >= 1 && numerator <= 1000 &&
                denominator >= 1 && denominator <= 1000)
            {
                settings.scale_numerator = numerator;
                settings.scale_denominator = denominator;
            }
            GetPrivateProfileStringA("presentation", "mode", "scale",
                settings.mode, sizeof(settings.mode), settings.path);
        }

        bool ClientMatches(HWND hwnd, RECT *client)
        {
            LoadSettings();
            return GetClientRect(hwnd, client) &&
                client->right - client->left ==
                    static_cast<LONG>(settings.client_width) &&
                client->bottom - client->top ==
                    static_cast<LONG>(settings.client_height);
        }

        void SetClientSize(HWND hwnd, const char *reason)
        {
            if (!hwnd || resizing_client)
                return;

            RECT client = {};
            if (ClientMatches(hwnd, &client))
                return;

            RECT window = { 0, 0,
                static_cast<LONG>(settings.client_width),
                static_cast<LONG>(settings.client_height) };
            const DWORD style = static_cast<DWORD>(GetWindowLongPtr(
                hwnd, GWL_STYLE));
            const DWORD extended_style = static_cast<DWORD>(GetWindowLongPtr(
                hwnd, GWL_EXSTYLE));
            if (!AdjustWindowRectEx(&window, style, FALSE, extended_style))
                return;

            resizing_client = true;
            SetWindowPos(hwnd, nullptr, 0, 0,
                window.right - window.left, window.bottom - window.top,
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                    SWP_FRAMECHANGED);
            resizing_client = false;

            GetClientRect(hwnd, &client);
            FILE *log = fopen("fixed_zoom_presentation.log", "a");
            if (log)
            {
                fprintf(log,
                    "%lu presentation resize reason=%s logical=%ux%u "
                    "mode=%s scale=%u/%u requested=%ux%u actual=%ldx%ld "
                    "config=%s compatible=%u\n",
                    static_cast<unsigned long>(GetTickCount()), reason,
                    static_cast<unsigned>(resolution::screen_width),
                    static_cast<unsigned>(resolution::screen_height),
                    settings.mode, settings.scale_numerator,
                    settings.scale_denominator,
                    settings.client_width, settings.client_height,
                    static_cast<long>(client.right - client.left),
                    static_cast<long>(client.bottom - client.top),
                    settings.path,
                    static_cast<unsigned>(settings.compatible));
                fclose(log);
            }
        }
    }

    void ResizeClient(HWND hwnd)
    {
        SetClientSize(hwnd, "initial");
    }

    void EnsureClient(HWND hwnd)
    {
        SetClientSize(hwnd, "correction");
    }
}
