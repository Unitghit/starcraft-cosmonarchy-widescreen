#include "limits.h"

#include <cstdio>
#include <cstring>
#include <intrin.h>

#include "patch/patchmanager.h"
#include "console/windows_wrap.h"
#include "draw.h"
#include "offsets_hooks.h"
#include "offsets.h"
#include "scconsole.h"
#include "runtime_diagnostics.h"
#include "yms.h"

// Every fopen call in this translation unit writes diagnostic output.
#define fopen runtime_diagnostics::Open

namespace
{
    enum class GptpPlacementPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    GptpPlacementPatchState gptp_placement_patch_state =
        GptpPlacementPatchState::WaitingForModule;

    enum class GptpCursorHoverPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    GptpCursorHoverPatchState gptp_cursor_hover_patch_state =
        GptpCursorHoverPatchState::WaitingForModule;

    enum class GptpCursorWarpPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    GptpCursorWarpPatchState gptp_cursor_warp_patch_state =
        GptpCursorWarpPatchState::WaitingForModule;

    enum class GptpMinimapBoxPatchState
    {
        WaitingForModule,
        Ready,
        Incompatible,
    };

    GptpMinimapBoxPatchState gptp_minimap_box_patch_state =
        GptpMinimapBoxPatchState::WaitingForModule;
    uint16_t *gptp_minimap_box_width;
    uint16_t *gptp_minimap_box_height;

    enum class GptpUpgradeClearPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    GptpUpgradeClearPatchState gptp_upgrade_clear_patch_state =
        GptpUpgradeClearPatchState::WaitingForModule;

    enum class GptpInitialCameraPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    GptpInitialCameraPatchState gptp_initial_camera_patch_state =
        GptpInitialCameraPatchState::WaitingForModule;

    enum class PresentationCursorPatchState
    {
        WaitingForModule,
        Installed,
        Incompatible,
    };

    PresentationCursorPatchState presentation_cursor_patch_state =
        PresentationCursorPatchState::WaitingForModule;

    uintptr_t native_control_at_mouse;
    uintptr_t native_game_menu_context;
    uintptr_t native_game_menu_update;
    void *prepared_control_lookup_event;
    void *stabilized_status_control;
    uint32_t suppress_game_menu_tooltip;

    void __declspec(naked) FilterLegacyHudTooltipControl()
    {
        __asm {
            PUSHAD
            PUSH ECX
            CALL PrepareExpandedHudControlLookup
            ADD ESP, 4
            MOV prepared_control_lookup_event, EAX
            POPAD
            MOV ECX, prepared_control_lookup_event
            TEST ECX, ECX
            JE suppress
            JMP DWORD PTR [native_control_at_mouse]
        suppress:
            XOR EAX, EAX
            RET
        }
    }

    void __declspec(naked) FilterStatusPanelTooltipControl()
    {
        __asm {
            PUSHAD
            PUSH ECX
            CALL PrepareExpandedHudControlLookup
            ADD ESP, 4
            MOV prepared_control_lookup_event, EAX
            POPAD
            MOV ECX, prepared_control_lookup_event
            TEST ECX, ECX
            JE suppress
            CALL DWORD PTR [native_control_at_mouse]
            MOV stabilized_status_control, EAX
            PUSHAD
            PUSH prepared_control_lookup_event
            PUSH EAX
            CALL StabilizeExpandedSelectionTooltip
            ADD ESP, 8
            MOV stabilized_status_control, EAX
            POPAD
            MOV EAX, stabilized_status_control
            RET
        suppress:
            XOR EAX, EAX
            RET
        }
    }

    void __declspec(naked) FilterLegacyGameMenuTooltip()
    {
        __asm {
            PUSHAD
            PUSH EDX
            CALL ShouldSuppressLegacyGameMenuTooltip
            ADD ESP, 4
            MOVZX EAX, AL
            MOV suppress_game_menu_tooltip, EAX
            POPAD
            CMP suppress_game_menu_tooltip, 0
            JNE suppress
            JMP DWORD PTR [native_game_menu_context]
        suppress:
            XOR EAX, EAX
            RET
        }
    }

    void __declspec(naked) FilterLegacyGameMenuControl()
    {
        __asm {
            PUSHAD
            PUSH ECX
            CALL PrepareGameMenuControlLookup
            ADD ESP, 4
            MOV prepared_control_lookup_event, EAX
            POPAD
            MOV ECX, prepared_control_lookup_event
            TEST ECX, ECX
            JE suppress
            CALL DWORD PTR [native_control_at_mouse]
            PUSHAD
            PUSH EAX
            CALL ShouldSuppressLegacyGameMenuTooltip
            ADD ESP, 4
            MOVZX EAX, AL
            MOV suppress_game_menu_tooltip, EAX
            POPAD
            CMP suppress_game_menu_tooltip, 0
            JNE suppress
            RET
        suppress:
            XOR EAX, EAX
            RET
        }
    }

    void __declspec(naked) SynchronizeGameMenuUpdate()
    {
        __asm {
            PUSHAD
            PUSH ECX
            CALL SynchronizeGameMenuHoverState
            ADD ESP, 4
            POPAD
            JMP DWORD PTR [native_game_menu_update]
        }
    }

    // Used only by gameplay hit tests.  Do not enlarge StarCraft's own
    // 0x005993B0 rectangle: the stock renderer also consumes that rectangle
    // and must remain 640x400 inside each compositor pass.
    Rect32 expanded_game_rect(
        0, 0,
        static_cast<int32_t>(resolution::game_width),
        static_cast<int32_t>(resolution::screen_height));
    Rect32 expanded_drag_clip_rect(
        0, 0,
        static_cast<int32_t>(resolution::game_width),
        static_cast<int32_t>(resolution::screen_height));

    typedef BOOL (WINAPI *ClipCursorFunction)(const RECT *);
    ClipCursorFunction original_clip_cursor;

    typedef BOOL (WINAPI *SetCursorPosFunction)(int, int);
    SetCursorPosFunction original_set_cursor_pos;

    BOOL WINAPI SetCursorPosExpandedHudGuard(int x, int y)
    {
        if (ShouldSuppressTranslatedHudCursorWarp())
        {
            // The translated event is already being handled at the cursor's
            // physical expanded-HUD position. Applying this native-space warp
            // would queue a second WM_MOUSEMOVE in the obsolete 4:3 HUD.
            static DWORD last_log_tick;
            const DWORD now = GetTickCount();
            if (now - last_log_tick >= 250)
            {
                FILE *log = fopen("fixed_zoom_input.log", "a");
                if (log)
                {
                    fprintf(log,
                        "%lu suppressed translated-HUD SetCursorPos "
                        "target=(%d,%d) caller=%p\n",
                        static_cast<unsigned long>(now), x, y,
                        _ReturnAddress());
                    fclose(log);
                }
                last_log_tick = now;
            }
            return TRUE;
        }
        return original_set_cursor_pos ? original_set_cursor_pos(x, y) : FALSE;
    }

    BOOL WINAPI ClipCursorExpanded(const RECT *requested)
    {
        if (ShouldSuppressTranslatedHudCursorWarp())
        {
            static DWORD last_suppressed_log_tick;
            const DWORD now = GetTickCount();
            if (now - last_suppressed_log_tick >= 250)
            {
                FILE *log = fopen("fixed_zoom_input.log", "a");
                if (log)
                {
                    fprintf(log,
                        "%lu suppressed translated-HUD ClipCursor "
                        "requested=(%ld,%ld,%ld,%ld) caller=%p\n",
                        static_cast<unsigned long>(now),
                        requested ? requested->left : 0,
                        requested ? requested->top : 0,
                        requested ? requested->right : 0,
                        requested ? requested->bottom : 0,
                        _ReturnAddress());
                    fclose(log);
                }
                last_suppressed_log_tick = now;
            }
            return TRUE;
        }
        const bool in_game = is_in_game();
        const bool left_button_down =
            (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const LONG width = requested ?
            requested->right - requested->left : 0;
        const LONG height = requested ?
            requested->bottom - requested->top : 0;
        const bool native_game_clip = requested &&
            width >= static_cast<LONG>(resolution::native_width) - 2 &&
            width <= static_cast<LONG>(resolution::native_width) + 2 &&
            height >= static_cast<LONG>(resolution::native_game_height) - 2 &&
            height <= static_cast<LONG>(resolution::native_game_height) + 2;
        const bool native_menu_clip = requested &&
            width >= static_cast<LONG>(resolution::native_width) - 2 &&
            width <= static_cast<LONG>(resolution::native_width) + 2 &&
            height >= static_cast<LONG>(resolution::native_height) - 2 &&
            height <= static_cast<LONG>(resolution::native_height) + 2;

        RECT expanded = {};
        const RECT *forwarded = requested;
        if (in_game && left_button_down && native_game_clip &&
            *bw::popup_dialog_active == 0)
        {
            // Preserve the coordinate basis and origin supplied by the
            // engine/windowing shim, but replace the legacy gameplay extent.
            // This prevents ClipCursor from synchronously warping an expanded
            // click back to x=638 while a selection drag begins.
            expanded = *requested;
            expanded.right = expanded.left +
                static_cast<LONG>(resolution::game_width);
            expanded.bottom = expanded.top +
                static_cast<LONG>(resolution::screen_height);
            forwarded = &expanded;
        }
        else if (!in_game && native_menu_clip)
        {
            // Preserve the screen-space origin supplied by StarCraft, but
            // constrain the pointer to the centered aspect-fitted menu rather
            // than the obsolete top-left 640x480 rectangle.
            expanded = *requested;
            expanded.left += static_cast<LONG>(resolution::menu_left);
            expanded.top += static_cast<LONG>(resolution::menu_top);
            expanded.right = expanded.left +
                static_cast<LONG>(resolution::menu_width);
            expanded.bottom = expanded.top +
                static_cast<LONG>(resolution::menu_height);
            forwarded = &expanded;
        }

        if (in_game && requested)
        {
            POINT screen_cursor = {};
            POINT client_cursor = {};
            GetCursorPos(&screen_cursor);
            client_cursor = screen_cursor;
            HWND hwnd = static_cast<HWND>(*bw::main_window_hwnd);
            if (hwnd)
                ScreenToClient(hwnd, &client_cursor);
            FILE *log = fopen("fixed_zoom_input.log", "a");
            if (log)
            {
                fprintf(log,
                    "%lu clip-cursor requested=(%ld,%ld,%ld,%ld %ldx%ld) "
                    "forwarded=(%ld,%ld,%ld,%ld) cursor-screen=(%ld,%ld) "
                    "cursor-client=(%ld,%ld) left=%u expanded=%u\n",
                    static_cast<unsigned long>(GetTickCount()),
                    requested->left, requested->top,
                    requested->right, requested->bottom, width, height,
                    forwarded->left, forwarded->top,
                    forwarded->right, forwarded->bottom,
                    screen_cursor.x, screen_cursor.y,
                    client_cursor.x, client_cursor.y,
                    static_cast<unsigned>(left_button_down),
                    static_cast<unsigned>(forwarded == &expanded));
                fclose(log);
            }
        }
        return original_clip_cursor ? original_clip_cursor(forwarded) : FALSE;
    }

    bool PatchClipCursor(Common::PatchContext *patch)
    {
        const uintptr_t iat_address = 0x004FE37C + patch->GetDiff();
        original_clip_cursor = *reinterpret_cast<ClipCursorFunction *>(
            iat_address);
        ClipCursorFunction replacement = ClipCursorExpanded;
        if (!original_clip_cursor || !patch->Patch(
            reinterpret_cast<void *>(0x004FE37C), &replacement,
            sizeof(replacement), PATCH_REPLACE))
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "a");
            if (log)
            {
                fprintf(log, "ClipCursor hook installation failed: old=%p\n",
                    reinterpret_cast<void *>(original_clip_cursor));
                fclose(log);
            }
            return false;
        }
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log, "drag ClipCursor expansion installed: old=%p\n",
                reinterpret_cast<void *>(original_clip_cursor));
            fclose(log);
        }
        return true;
    }

    bool PatchSetCursorPos(Common::PatchContext *patch)
    {
        // USER32!SetCursorPos is import 21 in this StarCraft build.
        const uintptr_t iat_address = 0x004FE2CC + patch->GetDiff();
        original_set_cursor_pos = *reinterpret_cast<SetCursorPosFunction *>(
            iat_address);
        SetCursorPosFunction replacement = SetCursorPosExpandedHudGuard;
        if (!original_set_cursor_pos || !patch->Patch(
            reinterpret_cast<void *>(0x004FE2CC), &replacement,
            sizeof(replacement), PATCH_REPLACE))
        {
            FILE *log = fopen("fixed_zoom_renderer.log", "a");
            if (log)
            {
                fprintf(log,
                    "SetCursorPos HUD guard installation failed: old=%p\n",
                    reinterpret_cast<void *>(original_set_cursor_pos));
                fclose(log);
            }
            return false;
        }
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                "translated-HUD SetCursorPos guard installed: old=%p\n",
                reinterpret_cast<void *>(original_set_cursor_pos));
            fclose(log);
        }
        return true;
    }

    struct BoundPatch
    {
        uintptr_t address;
        uint32_t expected;
        uint32_t replacement;
        unsigned size;
    };

    bool PatchPortraitCameraOrigins(Common::PatchContext *patch)
    {
        // The status-panel portrait updater at 0x45E3A0 receives the selected
        // unit's sprite position and records a camera origin for the clickable
        // portrait callback at 0x45E9F0. Trigger_Portrait has a parallel target
        // encoder at 0x45EE4B. Both subtract the native 320x200 playfield
        // center. Retain their negative clamps and callback; replace only the
        // four center displacements.
        constexpr uintptr_t status_sequence_address = 0x0045E3A0;
        const uint8_t expected_status_sequence[] = {
            0x55, 0x8B, 0xEC,                   // push ebp; mov ebp,esp
            0x05, 0xC0, 0xFE, 0xFF, 0xFF,       // add eax,-320
            0x33, 0xC9, 0x85, 0xC0,
            0x0F, 0x9C, 0xC1,
            0x33, 0xD2, 0x49, 0x23, 0xC8,
            0x8B, 0x45, 0x08,                   // mov eax,[ebp+0x8]
            0x05, 0x38, 0xFF, 0xFF, 0xFF,       // add eax,-200
            0x85, 0xC0, 0x0F, 0x9C, 0xC2,
            0x89, 0x0D, 0x34, 0xFD, 0x57, 0x00,
            0x4A, 0x23, 0xD0,
            0x89, 0x15, 0x38, 0xFD, 0x57, 0x00,
        };
        constexpr uintptr_t transmission_sequence_address = 0x0045EE4B;
        const uint8_t expected_transmission_sequence[] = {
            0x83, 0xFF, 0xFF,                   // cmp edi, -1
            0x74, 0x2E,                         // je 0x45EE7E
            0x33, 0xC9,                         // xor ecx, ecx
            0x8D, 0x87, 0xC0, 0xFE, 0xFF, 0xFF, // lea eax,[edi-320]
            0x85, 0xC0, 0x0F, 0x9C, 0xC1,
            0x33, 0xD2, 0x49, 0x23, 0xC8,
            0x8B, 0x45, 0x0C,                   // mov eax,[ebp+0xc]
            0x05, 0x38, 0xFF, 0xFF, 0xFF,       // add eax,-200
            0x85, 0xC0, 0x0F, 0x9C, 0xC2,
            0x89, 0x0D, 0x34, 0xFD, 0x57, 0x00,
            0x4A, 0x23, 0xD0,
            0x89, 0x15, 0x38, 0xFD, 0x57, 0x00,
        };
        const uint8_t *status_sequence =
            reinterpret_cast<const uint8_t *>(
                status_sequence_address + patch->GetDiff());
        const uint8_t *transmission_sequence =
            reinterpret_cast<const uint8_t *>(
                transmission_sequence_address + patch->GetDiff());
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (memcmp(status_sequence, expected_status_sequence,
                   sizeof(expected_status_sequence)) != 0 ||
            memcmp(transmission_sequence, expected_transmission_sequence,
                   sizeof(expected_transmission_sequence)) != 0)
        {
            if (log)
            {
                fprintf(log,
                    "portrait camera preflight failed: status=%p "
                    "transmission=%p\n",
                    status_sequence, transmission_sequence);
                fclose(log);
            }
            return false;
        }

        static_assert(resolution::maximum_screen_width <= 0x7FFFFFFF &&
                      resolution::maximum_screen_height <= 0x7FFFFFFF,
            "portrait camera center must fit signed 32-bit displacement");
        uint32_t x_displacement =
            0u - static_cast<uint32_t>(resolution::camera_center_x);
        uint32_t y_displacement =
            0u - static_cast<uint32_t>(resolution::camera_center_y);
        patch->Patch(reinterpret_cast<void *>(0x0045E3A4),
            &x_displacement, sizeof(x_displacement), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x0045E3B8),
            &y_displacement, sizeof(y_displacement), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x0045EE54),
            &x_displacement, sizeof(x_displacement), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x0045EE66),
            &y_displacement, sizeof(y_displacement), PATCH_REPLACE);

        if (log)
        {
            fprintf(log,
                "portrait camera installed: center=(%u,%u) "
                "operands=(%08X,%08X) status=%p transmission=%p\n",
                static_cast<unsigned>(resolution::camera_center_x),
                static_cast<unsigned>(resolution::camera_center_y),
                x_displacement, y_displacement,
                status_sequence, transmission_sequence);
            fclose(log);
        }
        return true;
    }

    bool PatchTriggerCenterView(Common::PatchContext *patch)
    {
        // Trigger action 10 (Center View) averages a location's bounds and
        // converts that world-space midpoint to a camera origin. StarCraft's
        // original subtraction still assumes the 640x400 viewport, so every
        // mission-start Center View lands at the old 4:3 center. Keep the
        // action and native map-edge clamping intact; change only its two
        // viewport-center operands.
        constexpr uintptr_t sequence_address = 0x004C6E64;
        const uint8_t expected_sequence[] = {
            0xD1, 0xF9,                         // sar ecx, 1
            0xD1, 0xF8,                         // sar eax, 1
            0x81, 0xE9, 0xC8, 0x00, 0x00, 0x00, // sub ecx, 200
            0x2D, 0x40, 0x01, 0x00, 0x00,       // sub eax, 320
            0xE8, 0xC8, 0x55, 0xFD, 0xFF,       // call move_screen
        };
        const uint8_t *sequence = reinterpret_cast<const uint8_t *>(
            sequence_address + patch->GetDiff());
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (memcmp(sequence, expected_sequence,
                   sizeof(expected_sequence)) != 0)
        {
            if (log)
            {
                fprintf(log,
                    "trigger Center View preflight failed: sequence=%p\n",
                    sequence);
                fclose(log);
            }
            return false;
        }

        static_assert(resolution::maximum_screen_width <= 0xFFFF,
            "camera center x must fit Center View's 32-bit operand");
        static_assert(resolution::maximum_screen_height <= 0xFFFF,
            "camera center y must fit Center View's 32-bit operand");
        uint32_t center_y = resolution::camera_center_y;
        uint32_t center_x = resolution::camera_center_x;
        patch->Patch(reinterpret_cast<void *>(0x004C6E6A),
            &center_y, sizeof(center_y), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x004C6E6F),
            &center_x, sizeof(center_x), PATCH_REPLACE);

        if (log)
        {
            fprintf(log,
                "trigger Center View installed: center=(%u,%u) "
                "battlefield=%ux%u action=%p\n",
                static_cast<unsigned>(center_x),
                static_cast<unsigned>(center_y),
                static_cast<unsigned>(resolution::game_width),
                static_cast<unsigned>(resolution::game_height),
                reinterpret_cast<const void *>(sequence_address +
                    patch->GetDiff()));
            fclose(log);
        }
        return true;
    }

    bool PatchStartLocationCameraOrigin(Common::PatchContext *patch)
    {
        // CHK start-location decoding stores the initial camera origin in
        // g_move_to_tile_x/y. InitScreenPositions later restores those tile
        // values verbatim. The installed Cosmonarchy build reaches stock
        // StarCraft 0x4CB190 here (verified from the live 0x4CD86A call), whose
        // x/y conversion subtracts the native 10x6 tile center. Derive both
        // offsets from the configured logical camera center.
        constexpr uintptr_t sequence_address = 0x004CB1DE;
        const uint8_t expected_sequence[] = {
            0x66, 0xC1, 0xE8, 0x05,             // shr ax, 5
            0x66, 0xC1, 0xE9, 0x05,             // shr cx, 5
            0x0F, 0xB7, 0xC0,
            0x83, 0xC0, 0xF6,                   // add eax, -10
            0x33, 0xD2, 0x85, 0xC0,
            0x0F, 0x9C, 0xC2, 0x4A, 0x23, 0xD0,
            0x0F, 0xB7, 0xC1,
            0x83, 0xC0, 0xFA,                   // add eax, -6
            0x33, 0xC9, 0x85, 0xC0,
            0x0F, 0x9C, 0xC1,
        };
        const uint8_t *sequence = reinterpret_cast<const uint8_t *>(
            sequence_address + patch->GetDiff());
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (memcmp(sequence, expected_sequence,
                   sizeof(expected_sequence)) != 0)
        {
            if (log)
            {
                fprintf(log,
                    "start-location camera preflight failed: sequence=%p\n",
                    sequence);
                fclose(log);
            }
            return false;
        }

        constexpr unsigned tile_pixels = 32;
        const unsigned center_x_tiles =
            resolution::camera_center_x / tile_pixels;
        const unsigned center_y_tiles =
            resolution::camera_center_y / tile_pixels;
        if (center_x_tiles > 128 || center_y_tiles > 128)
        {
            if (log)
            {
                fprintf(log,
                    "start-location camera center exceeds imm8: (%u,%u)\n",
                    center_x_tiles, center_y_tiles);
                fclose(log);
            }
            return false;
        }
        uint8_t x_subtract = static_cast<uint8_t>(0u - center_x_tiles);
        uint8_t y_subtract = static_cast<uint8_t>(0u - center_y_tiles);
        patch->Patch(reinterpret_cast<void *>(0x004CB1EB),
            &x_subtract, sizeof(x_subtract), PATCH_REPLACE);
        patch->Patch(reinterpret_cast<void *>(0x004CB1FB),
            &y_subtract, sizeof(y_subtract), PATCH_REPLACE);

        if (log)
        {
            fprintf(log,
                "start-location camera installed: center-tiles=(%u,%u) "
                "center-pixels=(%u,%u) sequence=%p\n",
                center_x_tiles, center_y_tiles,
                static_cast<unsigned>(resolution::camera_center_x),
                static_cast<unsigned>(resolution::camera_center_y),
                sequence);
            fclose(log);
        }
        return true;
    }

    bool PatchPositionalAudioViewport(Common::PatchContext *patch)
    {
        // Cosmonarchy's AudioSystem delegates both initial and continuously
        // updated positional sounds to StarCraft's native volume/pan helpers.
        // Those helpers still describe a 640x400 world viewport. Expand only
        // their semantic viewport geometry; retain the stock attenuation and
        // pan curves, DirectSound range, fog filtering, and minimum-volume
        // behavior.
        constexpr uintptr_t pan_function = 0x0048E850;
        constexpr uintptr_t volume_function = 0x0048E8D0;
        const uint8_t pan_prefix[] = {
            0x99, 0x83, 0xE2, 0x1F, 0x03, 0xC2,
            0x0F, 0xB7, 0x15, 0xD0, 0xF1, 0x57, 0x00,
            0xC1, 0xF8, 0x05, 0x53, 0x2B, 0xC2,
            0x33, 0xC9, 0x32, 0xDB, 0x83, 0xE8,
        };
        const uint8_t volume_prefix[] = {
            0x8B, 0x15, 0x8C, 0x84, 0x62, 0x00,
            0x56, 0x8B, 0xF0, 0x33, 0xC0, 0x3B, 0xCA,
            0x7D, 0x06, 0x2B, 0xD1, 0x8B, 0xC2, 0xEB, 0x16,
            0x57, 0x8D, 0xBA,
        };
        const uint8_t after_first_width[] = {
            0x3B, 0xCF, 0x5F, 0x7E, 0x0A, 0x2B, 0xCA, 0x81, 0xE9,
        };
        const uint8_t before_height[] = {
            0x8B, 0xC1, 0x8B, 0x0D, 0xA8, 0x84, 0x62, 0x00,
            0x3B, 0xF1, 0x7D, 0x06, 0x2B, 0xCE, 0x03, 0xC1,
            0xEB, 0x13, 0x8D, 0x91,
        };
        const uint8_t before_negative_height[] = {
            0x3B, 0xF2, 0x7E, 0x09, 0x2B, 0xF1, 0x8D, 0x84, 0x30,
        };

        const uint8_t *pan = reinterpret_cast<const uint8_t *>(
            pan_function + patch->GetDiff());
        const uint8_t *volume = reinterpret_cast<const uint8_t *>(
            volume_function + patch->GetDiff());
        const bool signatures_match =
            memcmp(pan, pan_prefix, sizeof(pan_prefix)) == 0 &&
            memcmp(volume, volume_prefix, sizeof(volume_prefix)) == 0 &&
            memcmp(volume + 0x1C, after_first_width,
                   sizeof(after_first_width)) == 0 &&
            memcmp(volume + 0x29, before_height,
                   sizeof(before_height)) == 0 &&
            memcmp(volume + 0x41, before_negative_height,
                   sizeof(before_negative_height)) == 0;

        constexpr uint32_t native_width = 640;
        constexpr uint32_t native_height = 400;
        constexpr uint32_t native_center_tiles = native_width / 64;
        const uint32_t expanded_center_tiles =
            resolution::game_width / 64;
        if (expanded_center_tiles > 0x7F)
            return false;
        const BoundPatch geometry[] = {
            { 0x0048E869, native_center_tiles, expanded_center_tiles, 1 },
            { 0x0048E8E8, native_width, resolution::game_width, 4 },
            { 0x0048E8F5, native_width, resolution::game_width, 4 },
            { 0x0048E90D, native_height, resolution::game_height, 4 },
            { 0x0048E91A, 0u - native_height,
              0u - resolution::game_height, 4 },
        };

        bool operands_match = true;
        uint32_t bad_value = 0;
        uintptr_t bad_address = 0;
        for (const BoundPatch &operand : geometry)
        {
            uint32_t value = 0;
            memcpy(&value, reinterpret_cast<const void *>(
                operand.address + patch->GetDiff()), operand.size);
            if (value != operand.expected)
            {
                operands_match = false;
                bad_value = value;
                bad_address = operand.address;
                break;
            }
        }

        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (!signatures_match || !operands_match)
        {
            if (log)
            {
                fprintf(log,
                    "positional audio preflight failed: signatures=%u "
                    "operand=%08lX value=%08lX\n",
                    static_cast<unsigned>(signatures_match),
                    static_cast<unsigned long>(bad_address),
                    static_cast<unsigned long>(bad_value));
                fclose(log);
            }
            return false;
        }

        for (const BoundPatch &operand : geometry)
        {
            uint32_t replacement = operand.replacement;
            patch->Patch(reinterpret_cast<void *>(operand.address),
                &replacement, operand.size, PATCH_REPLACE);
        }
        if (log)
        {
            fprintf(log,
                "positional audio viewport installed: battlefield=%ux%u "
                "pan-center=%u tiles volume=%p pan=%p operands=%u\n",
                static_cast<unsigned>(resolution::game_width),
                static_cast<unsigned>(resolution::game_height),
                static_cast<unsigned>(expanded_center_tiles),
                reinterpret_cast<const void *>(volume_function +
                    patch->GetDiff()),
                reinterpret_cast<const void *>(pan_function +
                    patch->GetDiff()),
                static_cast<unsigned>(
                    sizeof(geometry) / sizeof(geometry[0])));
            fclose(log);
        }
        return true;
    }

    bool PatchInteractionBounds(Common::PatchContext *patch)
    {
        // Runtime profiles are loaded after C++ static initialization. Keep
        // the semantic gameplay rectangle synchronized with the selected
        // internal resolution before any StarCraft or GPTP operand is pointed
        // at it. Without this, a 1920x1080 profile still uses the build-time
        // default bounds for right-click orders and cursor hover.
        expanded_game_rect.left = 0;
        expanded_game_rect.top = 0;
        expanded_game_rect.right = static_cast<int32_t>(
            resolution::game_width);
        expanded_game_rect.bottom = static_cast<int32_t>(
            resolution::screen_height);

        // StarCraft clamps WM_MOUSEMOVE and all three button handlers to
        // 640x480. It also chooses edge-scroll cursors at x=638/y=478.
        // Selection and building-placement code has additional semantic
        // 640x400 viewport bounds.  Those are safe to widen because they do
        // not index a native render surface.  Renderer-internal clip bounds
        // intentionally remain native for the multipass compositor.
        const BoundPatch bounds[] = {
            // OS-level ClipCursor rectangle built by 0x4215E0 whenever
            // StarCraft gains focus or changes UI state.  This was the
            // earliest remaining 640x480 gate: points beyond x=639 never
            // generated a WM_MOUSEMOVE until these operands were widened.
            { 0x00421603, 0x00000280, resolution::screen_width, 4 },
            { 0x0042160A, 0x000001e0, resolution::screen_height, 4 },

            { 0x004D1300, 0x0000027e, resolution::screen_width - 2, 4 },
            { 0x004D1334, 0x000001de, resolution::screen_height - 2, 4 },

            { 0x004D1963, 0x0280, resolution::screen_width, 2 },
            { 0x004D196f, 0x027f, resolution::screen_width - 1, 2 },
            { 0x004D1984, 0x01e0, resolution::screen_height, 2 },
            { 0x004D1990, 0x01df, resolution::screen_height - 1, 2 },

            { 0x004D19ef, 0x0280, resolution::screen_width, 2 },
            { 0x004D19fb, 0x027f, resolution::screen_width - 1, 2 },
            { 0x004D1a10, 0x01e0, resolution::screen_height, 2 },
            { 0x004D1a1c, 0x01df, resolution::screen_height - 1, 2 },

            { 0x004D1a7f, 0x0280, resolution::screen_width, 2 },
            { 0x004D1a8b, 0x027f, resolution::screen_width - 1, 2 },
            { 0x004D1aa0, 0x01e0, resolution::screen_height, 2 },
            { 0x004D1aac, 0x01df, resolution::screen_height - 1, 2 },

            { 0x004D24e8, 0x0280, resolution::screen_width, 2 },
            { 0x004D24f2, 0x0000027f, resolution::screen_width - 1, 4 },
            { 0x004D2506, 0x01e0, resolution::screen_height, 2 },
            { 0x004D2512, 0x000001df, resolution::screen_height - 1, 4 },

            // Visible world rectangle used to collect selectable units.
            // The first pair is the alternate/modified-selection branch;
            // the second pair is the normal branch. Both must agree or unit
            // selection fails intermittently depending on selection state.
            { 0x0046FC76, 0x00000280, resolution::game_width, 4 },
            { 0x0046FC88, 0x00000190, resolution::screen_height, 4 },
            { 0x0046FE19, 0x00000280, resolution::game_width, 4 },
            { 0x0046FE2B, 0x00000190, resolution::screen_height, 4 },

            // input_game_left_mouse_click passes the native gameplay RECT to
            // StarCraft's cursor-restriction manager. The manager stores that
            // rectangle and a later cursor update performs the x=638 warp;
            // this path does not necessarily call the imported ClipCursor.
            { 0x0046FFAA, 0x005993b0,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
                  &expanded_drag_clip_rect)), 4 },

            // Building-placement screen-point acceptance.  The nearby
            // 0x48D5F2 drawing clip stays native because it draws into a
            // 640x400 pass surface.
            { 0x0048D665, 0x0280, resolution::game_width, 2 },
            { 0x0048D670, 0x0190, resolution::screen_height, 2 },

            // Redirect gameplay-only PtInRect calls away from StarCraft's
            // renderer-owned 640x400 rectangle.
            { 0x0048468E, 0x005993b0,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
                  &expanded_game_rect)), 4 },
            { 0x004D14AA, 0x005993b0,
              static_cast<uint32_t>(reinterpret_cast<uintptr_t>(
                  &expanded_game_rect)), 4 },
        };

        for (const BoundPatch &bound : bounds)
        {
            const uint8_t *current = reinterpret_cast<const uint8_t *>(
                bound.address + patch->GetDiff());
            uint32_t value = 0;
            memcpy(&value, current, bound.size);
            if (value != bound.expected)
            {
                FILE *log = fopen("fixed_zoom_renderer.log", "a");
                if (log)
                {
                    fprintf(log,
                        "interaction bounds preflight failed at %08lX: "
                        "expected=%08lX actual=%08lX size=%u\n",
                        static_cast<unsigned long>(bound.address),
                        static_cast<unsigned long>(bound.expected),
                        static_cast<unsigned long>(value), bound.size);
                    fclose(log);
                }
                return false;
            }
        }
        for (const BoundPatch &bound : bounds)
        {
            uint32_t replacement = bound.replacement;
            patch->Patch(reinterpret_cast<void *>(bound.address),
                         &replacement, bound.size, PATCH_REPLACE);
        }
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                "expanded interaction bounds installed: input=%ux%u "
                "game=%ux%u edge=(%u,%u) patches=%u rect=%p\n",
                static_cast<unsigned>(resolution::screen_width),
                static_cast<unsigned>(resolution::screen_height),
                static_cast<unsigned>(resolution::game_width),
                static_cast<unsigned>(resolution::game_height),
                static_cast<unsigned>(resolution::screen_width - 2),
                static_cast<unsigned>(resolution::screen_height - 2),
                static_cast<unsigned>(sizeof(bounds) / sizeof(bounds[0])),
                static_cast<const void *>(&expanded_game_rect));
            fclose(log);
        }
        return true;
    }

    bool PatchLegacyHudTooltipHitTests(Common::PatchContext *patch)
    {
        struct TooltipCallsite
        {
            uintptr_t address;
            const char *name;
            void *filter;
            uintptr_t expected_target;
        };
        constexpr uintptr_t native_lookup_address = 0x00418340;
        constexpr uintptr_t game_menu_context_address = 0x004F4F70;
        constexpr uintptr_t game_menu_update_address = 0x004F4FB0;
        struct GameMenuUpdateInitializer
        {
            uintptr_t address;
            uint8_t destination_register;
        };
        const GameMenuUpdateInitializer game_menu_update_initializers[] = {
            { 0x004F50DB, 0x40 },
            { 0x004F51B3, 0x46 },
        };
        native_control_at_mouse = native_lookup_address + patch->GetDiff();
        native_game_menu_context =
            game_menu_context_address + patch->GetDiff();
        native_game_menu_update =
            game_menu_update_address + patch->GetDiff();
        const TooltipCallsite callsites[] = {
            // The status panel has two input-event lookups before its periodic
            // status_update_tooltip poll.  Filter both so the obsolete native
            // 4:3 status controls cannot remain tooltip owners after the HUD
            // has been relocated to the bottom-center of the expanded frame.
            { 0x00457E10, "status-event-validation",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            { 0x00457E50, "status-tooltip-owner",
              reinterpret_cast<void *>(&FilterStatusPanelTooltipControl),
              native_control_at_mouse },
            // status_update_tooltip owns the center unit-information panel,
            // including multiselection wireframes and the custom GPTP
            // interaction callbacks behind them. It polls independently of
            // the command-card tooltip functions below.
            { 0x00458015, "status-panel-refresh",
              reinterpret_cast<void *>(&FilterStatusPanelTooltipControl),
              native_control_at_mouse },
            { 0x00459796, "refresh",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            // refresh_button_tooltip's dialog-parent validation performs a
            // second control_at_mouse lookup and requires it to match the
            // first result.  Status-panel overlays (weapon/armor/shields)
            // depend on that equality before their custom GPTP interaction
            // callback is allowed to create context help.
            { 0x00459825, "status-parent-validation",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            { 0x00459870, "mouse-move",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            { 0x004A5459, "minimap-refresh",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            { 0x004A54BF, "minimap-mouse-move",
              reinterpret_cast<void *>(&FilterLegacyHudTooltipControl),
              native_control_at_mouse },
            // Game Menu has two independent hover-owner paths. The first can
            // tail-jump directly into its context function, while the second
            // owns its mouse-event highlight. Filter their returned control
            // using the exact relocated button rectangle.
            { 0x004F509A, "game-menu-refresh-owner",
              reinterpret_cast<void *>(&FilterLegacyGameMenuControl),
              native_control_at_mouse },
            { 0x004F511F, "game-menu-event-owner",
              reinterpret_cast<void *>(&FilterLegacyGameMenuControl),
              native_control_at_mouse },
            // Cosmonarchy gives the F10 button its own GPTP context function,
            // so it does not pass through the generic control lookups above.
            // Filter the one verified caller while preserving EDX for GPTP's
            // dialog argument and chaining to its installed function body.
            { 0x004F5142, "game-menu-context",
              reinterpret_cast<void *>(&FilterLegacyGameMenuTooltip),
              native_game_menu_context },
        };

        for (const TooltipCallsite &site : callsites)
        {
            const uint8_t *call = reinterpret_cast<const uint8_t *>(
                site.address + patch->GetDiff());
            int32_t relative = 0;
            memcpy(&relative, call + 1, sizeof(relative));
            const uintptr_t decoded_target = reinterpret_cast<uintptr_t>(
                call + 5 + relative);
            if (call[0] != 0xE8 || decoded_target != site.expected_target)
            {
                FILE *log = fopen("fixed_zoom_renderer.log", "a");
                if (log)
                {
                    fprintf(log,
                        "legacy tooltip preflight failed: site=%s "
                        "address=%p opcode=%02X target=%p expected=%p\n",
                        site.name, call, static_cast<unsigned>(call[0]),
                        reinterpret_cast<void *>(decoded_target),
                        reinterpret_cast<void *>(site.expected_target));
                    fclose(log);
                }
                return false;
            }
        }

        for (const GameMenuUpdateInitializer &site :
             game_menu_update_initializers)
        {
            const uint8_t *update_initializer =
                reinterpret_cast<const uint8_t *>(
                    site.address + patch->GetDiff());
            uint32_t decoded_update = 0;
            memcpy(&decoded_update, update_initializer + 3,
                   sizeof(decoded_update));
            if (update_initializer[0] != 0xC7 ||
                update_initializer[1] != site.destination_register ||
                update_initializer[2] != 0x2E ||
                decoded_update != native_game_menu_update)
            {
                FILE *log = fopen("fixed_zoom_renderer.log", "a");
                if (log)
                {
                    fprintf(log,
                        "game menu update preflight failed: address=%p "
                        "bytes=%02X %02X %02X target=%p expected=%p\n",
                        update_initializer,
                        static_cast<unsigned>(update_initializer[0]),
                        static_cast<unsigned>(update_initializer[1]),
                        static_cast<unsigned>(update_initializer[2]),
                        reinterpret_cast<void *>(decoded_update),
                        reinterpret_cast<void *>(native_game_menu_update));
                    fclose(log);
                }
                return false;
            }
        }

        for (const TooltipCallsite &site : callsites)
        {
            const uintptr_t call_address = site.address + patch->GetDiff();
            uint8_t replacement[5] = { 0xE8, 0, 0, 0, 0 };
            const intptr_t displacement =
                reinterpret_cast<uintptr_t>(site.filter) -
                (call_address + sizeof(replacement));
            const int32_t relative = static_cast<int32_t>(displacement);
            memcpy(replacement + 1, &relative, sizeof(relative));
            patch->Patch(reinterpret_cast<void *>(site.address), replacement,
                sizeof(replacement), PATCH_REPLACE);
        }

        void *update_filter =
            reinterpret_cast<void *>(&SynchronizeGameMenuUpdate);
        for (const GameMenuUpdateInitializer &site :
             game_menu_update_initializers)
        {
            patch->Patch(reinterpret_cast<void *>(site.address + 3),
                         &update_filter, sizeof(update_filter), PATCH_REPLACE);
        }

        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                "legacy HUD tooltip filter installed: calls=%u "
                "native-lookup=%p filter=%p\n",
                static_cast<unsigned>(
                    sizeof(callsites) / sizeof(callsites[0])),
                reinterpret_cast<void *>(native_control_at_mouse),
                reinterpret_cast<void *>(&FilterLegacyHudTooltipControl));
            fclose(log);
        }
        return true;
    }
}

void EnsureGptpPlacementBounds()
{
    if (gptp_placement_patch_state !=
        GptpPlacementPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("gptp.qdp");
    if (!module)
        module = GetModuleHandleA("CM-GPTP-Release.qdp");
    if (!module)
        return;

    // The currently distributed Cosmonarchy GPTP replaces StarCraft's
    // init_placement_box_pos with its own implementation.  Its first two
    // comparisons still hard-code inclusive 639x399 bounds, bypassing the
    // corresponding StarCraft patches above.  Patch the two operands in the
    // verified stable GPTP build, not the module on disk, so its data-file
    // compatibility and all unrelated gameplay code remain untouched.
    constexpr uintptr_t function_rva = 0x00087BF0;
    constexpr size_t x_instruction_offset = 0x04;
    constexpr size_t y_instruction_offset = 0x1C;
    constexpr size_t immediate_offset = 0x06;
    const uint8_t x_signature[] = {
        0x81, 0x3D, 0xC4, 0xDD, 0x6C, 0x00,
    };
    const uint8_t y_signature[] = {
        0x81, 0x3D, 0xC8, 0xDD, 0x6C, 0x00,
    };
    constexpr uint32_t native_x_max = 639;
    constexpr uint32_t native_y_max = 399;
    const uint32_t expanded_x_max = resolution::game_width - 1;
    const uint32_t expanded_y_max = resolution::screen_height - 1;

    uint8_t *function = reinterpret_cast<uint8_t *>(module) + function_rva;
    uint8_t *x_instruction = function + x_instruction_offset;
    uint8_t *y_instruction = function + y_instruction_offset;
    uint32_t *x_bound = reinterpret_cast<uint32_t *>(
        x_instruction + immediate_offset);
    uint32_t *y_bound = reinterpret_cast<uint32_t *>(
        y_instruction + immediate_offset);

    const bool signatures_match =
        memcmp(x_instruction, x_signature, sizeof(x_signature)) == 0 &&
        memcmp(y_instruction, y_signature, sizeof(y_signature)) == 0;
    const bool original_bounds =
        *x_bound == native_x_max && *y_bound == native_y_max;
    const bool already_patched =
        *x_bound == expanded_x_max && *y_bound == expanded_y_max;

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signatures_match || (!original_bounds && !already_patched))
    {
        if (log)
        {
            fprintf(log,
                "GPTP placement bounds preflight failed: module=%p "
                "function=%p signatures=%u bounds=(%lu,%lu)\n",
                module, function, static_cast<unsigned>(signatures_match),
                static_cast<unsigned long>(*x_bound),
                static_cast<unsigned long>(*y_bound));
            fclose(log);
        }
        gptp_placement_patch_state =
            GptpPlacementPatchState::Incompatible;
        return;
    }

    if (original_bounds)
    {
        DWORD old_protection = 0;
        if (!VirtualProtect(function, 0x28, PAGE_EXECUTE_READWRITE,
                            &old_protection))
        {
            if (log)
            {
                fprintf(log,
                    "GPTP placement bounds VirtualProtect failed: error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                fclose(log);
            }
            gptp_placement_patch_state =
                GptpPlacementPatchState::Incompatible;
            return;
        }
        *x_bound = expanded_x_max;
        *y_bound = expanded_y_max;
        FlushInstructionCache(GetCurrentProcess(), function, 0x28);
        DWORD ignored = 0;
        VirtualProtect(function, 0x28, old_protection, &ignored);
    }

    if (log)
    {
        fprintf(log,
            "GPTP placement bounds installed: function=%p "
            "mouse-max=(%lu,%lu) configured=%ux%u\n",
            function, static_cast<unsigned long>(*x_bound),
            static_cast<unsigned long>(*y_bound),
            static_cast<unsigned>(resolution::game_width),
            static_cast<unsigned>(resolution::game_height));
        fclose(log);
    }
    gptp_placement_patch_state = GptpPlacementPatchState::Installed;
}

void EnsureGptpCursorHoverBounds()
{
    if (gptp_cursor_hover_patch_state !=
        GptpCursorHoverPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("gptp.qdp");
    if (!module)
        module = GetModuleHandleA("CM-GPTP-Release.qdp");
    if (!module)
        return;

    // Stable GPTP replaces get_cursor_type at StarCraft 0x004D1460. Its
    // expanded-aware outside-screen check is followed by a second PtInRect
    // against StarCraft's renderer-owned 0x5993B0 native rectangle. Redirect
    // only that push operand to our semantic gameplay rectangle; leave the
    // shared native rectangle untouched for the renderer and native UI.
    constexpr uintptr_t sequence_rva = 0x000675B8;
    constexpr size_t rect_instruction_offset = 0x0C;
    constexpr size_t rect_immediate_offset = rect_instruction_offset + 1;
    const uint8_t prefix_signature[] = {
        0xFF, 0x35, 0xC8, 0xDD, 0x6C, 0x00,
        0xFF, 0x35, 0xC4, 0xDD, 0x6C, 0x00,
        0x68,
    };
    const uint8_t suffix_signature[] = { 0xFF, 0xD6, 0x85, 0xC0 };
    constexpr uint32_t native_rect_address = 0x005993B0;
    const uint32_t expanded_rect_address = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(&expanded_game_rect));

    uint8_t *sequence = reinterpret_cast<uint8_t *>(module) + sequence_rva;
    uint32_t *rect_operand = reinterpret_cast<uint32_t *>(
        sequence + rect_immediate_offset);
    const bool signature_matches =
        memcmp(sequence, prefix_signature, sizeof(prefix_signature)) == 0 &&
        memcmp(sequence + rect_immediate_offset + sizeof(uint32_t),
               suffix_signature, sizeof(suffix_signature)) == 0;
    const bool original_operand = *rect_operand == native_rect_address;
    const bool already_patched = *rect_operand == expanded_rect_address;

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signature_matches || (!original_operand && !already_patched))
    {
        if (log)
        {
            fprintf(log,
                "GPTP cursor-hover preflight failed: module=%p "
                "sequence=%p signature=%u rect=%08lX expected=%08lX\n",
                module, sequence, static_cast<unsigned>(signature_matches),
                static_cast<unsigned long>(*rect_operand),
                static_cast<unsigned long>(expanded_rect_address));
            fclose(log);
        }
        gptp_cursor_hover_patch_state =
            GptpCursorHoverPatchState::Incompatible;
        return;
    }

    if (original_operand)
    {
        DWORD old_protection = 0;
        if (!VirtualProtect(sequence, 0x17, PAGE_EXECUTE_READWRITE,
                            &old_protection))
        {
            if (log)
            {
                fprintf(log,
                    "GPTP cursor-hover VirtualProtect failed: error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                fclose(log);
            }
            gptp_cursor_hover_patch_state =
                GptpCursorHoverPatchState::Incompatible;
            return;
        }
        *rect_operand = expanded_rect_address;
        FlushInstructionCache(GetCurrentProcess(), sequence, 0x17);
        DWORD ignored = 0;
        VirtualProtect(sequence, 0x17, old_protection, &ignored);
    }

    if (log)
    {
        fprintf(log,
            "GPTP cursor-hover bounds installed: sequence=%p rect=%p "
            "game=%ux%u\n",
            sequence, static_cast<const void *>(&expanded_game_rect),
            static_cast<unsigned>(resolution::game_width),
            static_cast<unsigned>(resolution::game_height));
        fclose(log);
    }
    gptp_cursor_hover_patch_state = GptpCursorHoverPatchState::Installed;
}

void EnsureGptpCursorWarpGuard()
{
    if (gptp_cursor_warp_patch_state !=
        GptpCursorWarpPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("gptp.qdp");
    if (!module)
        module = GetModuleHandleA("CM-GPTP-Release.qdp");
    if (!module)
        return;

    // Stable GPTP's custom cursor updater owns a second SetCursorPos import,
    // independent of StarCraft's IAT entry. Verify both coordinate loads and
    // the indirect call before redirecting only that import slot.
    constexpr uintptr_t sequence_rva = 0x00067F70;
    constexpr uintptr_t iat_rva = 0x001D6274;
    constexpr uintptr_t cursor_x_pointer_rva = 0x00259E9C;
    constexpr uintptr_t cursor_y_pointer_rva = 0x00259EA0;
    uint8_t *base = reinterpret_cast<uint8_t *>(module);
    uint8_t *sequence = base + sequence_rva;
    SetCursorPosFunction *iat =
        reinterpret_cast<SetCursorPosFunction *>(base + iat_rva);
    const uintptr_t expected_x_pointer =
        reinterpret_cast<uintptr_t>(base) + cursor_x_pointer_rva;
    const uintptr_t expected_y_pointer =
        reinterpret_cast<uintptr_t>(base) + cursor_y_pointer_rva;
    const uintptr_t expected_iat_operand =
        reinterpret_cast<uintptr_t>(base) + iat_rva;

    uintptr_t x_pointer = 0;
    uintptr_t y_pointer = 0;
    uintptr_t iat_operand = 0;
    memcpy(&x_pointer, sequence + 1, sizeof(uint32_t));
    memcpy(&y_pointer, sequence + 8, sizeof(uint32_t));
    memcpy(&iat_operand, sequence + 0x10, sizeof(uint32_t));
    const bool signature_matches =
        sequence[0] == 0xA1 && sequence[5] == 0xFF &&
        sequence[6] == 0x30 && sequence[7] == 0xA1 &&
        sequence[0x0C] == 0xFF && sequence[0x0D] == 0x30 &&
        sequence[0x0E] == 0xFF && sequence[0x0F] == 0x15 &&
        x_pointer == expected_x_pointer &&
        y_pointer == expected_y_pointer &&
        iat_operand == expected_iat_operand;
    const SetCursorPosFunction current = *iat;
    const bool import_matches =
        current == original_set_cursor_pos ||
        current == SetCursorPosExpandedHudGuard;

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signature_matches || !import_matches)
    {
        if (log)
        {
            fprintf(log,
                "GPTP cursor-warp guard preflight failed: module=%p "
                "sequence=%p signature=%u iat=%p current=%p expected=%p\n",
                module, sequence, static_cast<unsigned>(signature_matches),
                iat, reinterpret_cast<void *>(current),
                reinterpret_cast<void *>(original_set_cursor_pos));
            fclose(log);
        }
        gptp_cursor_warp_patch_state =
            GptpCursorWarpPatchState::Incompatible;
        return;
    }

    if (current != SetCursorPosExpandedHudGuard)
    {
        DWORD old_protection = 0;
        if (!VirtualProtect(iat, sizeof(*iat), PAGE_READWRITE,
                            &old_protection))
        {
            if (log)
            {
                fprintf(log,
                    "GPTP cursor-warp IAT VirtualProtect failed: error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                fclose(log);
            }
            gptp_cursor_warp_patch_state =
                GptpCursorWarpPatchState::Incompatible;
            return;
        }
        *iat = SetCursorPosExpandedHudGuard;
        FlushInstructionCache(GetCurrentProcess(), iat, sizeof(*iat));
        DWORD ignored = 0;
        VirtualProtect(iat, sizeof(*iat), old_protection, &ignored);
    }

    if (log)
    {
        fprintf(log,
            "GPTP translated-HUD SetCursorPos guard installed: "
            "call=%p iat=%p original=%p replacement=%p\n",
            sequence + 0x0E, iat,
            reinterpret_cast<void *>(original_set_cursor_pos),
            reinterpret_cast<void *>(&SetCursorPosExpandedHudGuard));
        fclose(log);
    }
    gptp_cursor_warp_patch_state = GptpCursorWarpPatchState::Installed;
}

void EnsureGptpMinimapViewportBox()
{
    // Cosmonarchy's minimap camera outline is sized by two GPTP globals. The
    // stable distributed binary initialized them for its original 640x400
    // battlefield, so synchronize the globals with our internal battlefield
    // dimensions after each map establishes its minimap zoom level. External
    // presentation scaling deliberately does not participate in this math.
    if (gptp_minimap_box_patch_state ==
        GptpMinimapBoxPatchState::Incompatible)
        return;

    if (gptp_minimap_box_patch_state ==
        GptpMinimapBoxPatchState::WaitingForModule)
    {
        HMODULE module = GetModuleHandleA("gptp.qdp");
        if (!module)
            module = GetModuleHandleA("CM-GPTP-Release.qdp");
        if (!module)
            return;

        constexpr uintptr_t sequence_rva = 0x0002C5F0;
        constexpr uintptr_t width_rva = 0x0026DC7C;
        constexpr uintptr_t height_rva = 0x0026D9DC;
        constexpr uint32_t mouse_x_address = 0x006CDDC4;
        constexpr uint32_t mouse_y_address = 0x006CDDC8;
        uint8_t *base = reinterpret_cast<uint8_t *>(module);
        uint8_t *sequence = base + sequence_rva;

        uint32_t width_operand = 0;
        uint32_t mouse_x_operand = 0;
        uint32_t height_operand = 0;
        uint32_t mouse_y_operand = 0;
        memcpy(&width_operand, sequence + 0x09, sizeof(width_operand));
        memcpy(&mouse_x_operand, sequence + 0x13, sizeof(mouse_x_operand));
        memcpy(&height_operand, sequence + 0x20, sizeof(height_operand));
        memcpy(&mouse_y_operand, sequence + 0x2D, sizeof(mouse_y_operand));

        const uint8_t prologue[] = {
            0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08,
        };
        const bool signature_matches =
            memcmp(sequence, prologue, sizeof(prologue)) == 0 &&
            sequence[0x06] == 0x0F && sequence[0x07] == 0xB7 &&
            sequence[0x08] == 0x0D &&
            sequence[0x10] == 0x0F && sequence[0x11] == 0xB7 &&
            sequence[0x12] == 0x05 &&
            sequence[0x1D] == 0x0F && sequence[0x1E] == 0xB7 &&
            sequence[0x1F] == 0x0D &&
            sequence[0x2A] == 0x0F && sequence[0x2B] == 0xB7 &&
            sequence[0x2C] == 0x05 &&
            width_operand == reinterpret_cast<uintptr_t>(base) + width_rva &&
            height_operand == reinterpret_cast<uintptr_t>(base) + height_rva &&
            mouse_x_operand == mouse_x_address &&
            mouse_y_operand == mouse_y_address;

        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (!signature_matches)
        {
            if (log)
            {
                fprintf(log,
                    "GPTP minimap viewport preflight failed: module=%p "
                    "sequence=%p operands=(%08lX,%08lX,%08lX,%08lX)\n",
                    module, sequence,
                    static_cast<unsigned long>(width_operand),
                    static_cast<unsigned long>(mouse_x_operand),
                    static_cast<unsigned long>(height_operand),
                    static_cast<unsigned long>(mouse_y_operand));
                fclose(log);
            }
            gptp_minimap_box_patch_state =
                GptpMinimapBoxPatchState::Incompatible;
            return;
        }

        gptp_minimap_box_width = reinterpret_cast<uint16_t *>(
            base + width_rva);
        gptp_minimap_box_height = reinterpret_cast<uint16_t *>(
            base + height_rva);
        gptp_minimap_box_patch_state = GptpMinimapBoxPatchState::Ready;
        if (log)
        {
            fprintf(log,
                "GPTP minimap viewport ready: sequence=%p width=%p "
                "height=%p battlefield=%ux%u\n",
                sequence, gptp_minimap_box_width, gptp_minimap_box_height,
                static_cast<unsigned>(resolution::game_width),
                static_cast<unsigned>(resolution::game_height));
            fclose(log);
        }
    }

    constexpr uintptr_t minimap_zoom_level_address = 0x0059CC6C;
    const uint16_t zoom_level = *reinterpret_cast<const uint16_t *>(
        minimap_zoom_level_address);
    if (zoom_level == 0)
        return;

    const uint32_t calculated_width =
        (resolution::game_width + zoom_level - 1) / zoom_level;
    const uint32_t calculated_height =
        (resolution::game_height + 16 + zoom_level - 1) / zoom_level;
    const uint16_t desired_width = static_cast<uint16_t>(
        calculated_width >= 2 ? calculated_width : 2);
    const uint16_t desired_height = static_cast<uint16_t>(
        calculated_height >= 1 ? calculated_height : 1);

    if (*gptp_minimap_box_width == desired_width &&
        *gptp_minimap_box_height == desired_height)
        return;

    const uint16_t old_width = *gptp_minimap_box_width;
    const uint16_t old_height = *gptp_minimap_box_height;
    uint8_t *write_start = reinterpret_cast<uint8_t *>(
        gptp_minimap_box_height);
    const SIZE_T write_size =
        reinterpret_cast<uint8_t *>(gptp_minimap_box_width) - write_start +
        sizeof(*gptp_minimap_box_width);
    DWORD old_protection = 0;
    if (!VirtualProtect(write_start, write_size, PAGE_READWRITE,
                        &old_protection))
    {
        FILE *log = fopen("fixed_zoom_renderer.log", "a");
        if (log)
        {
            fprintf(log,
                "GPTP minimap viewport write protection failed: error=%lu\n",
                static_cast<unsigned long>(GetLastError()));
            fclose(log);
        }
        gptp_minimap_box_patch_state =
            GptpMinimapBoxPatchState::Incompatible;
        return;
    }

    *gptp_minimap_box_width = desired_width;
    *gptp_minimap_box_height = desired_height;
    DWORD ignored = 0;
    VirtualProtect(write_start, write_size, old_protection, &ignored);

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (log)
    {
        fprintf(log,
            "GPTP minimap viewport synchronized: zoom=%u box=(%u,%u)->"
            "(%u,%u) battlefield=%ux%u\n",
            static_cast<unsigned>(zoom_level),
            static_cast<unsigned>(old_width),
            static_cast<unsigned>(old_height),
            static_cast<unsigned>(desired_width),
            static_cast<unsigned>(desired_height),
            static_cast<unsigned>(resolution::game_width),
            static_cast<unsigned>(resolution::game_height));
        fclose(log);
    }
}

void EnsureGptpUpgradeResearchClear()
{
    if (gptp_upgrade_clear_patch_state !=
        GptpUpgradeClearPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("gptp.qdp");
    if (!module)
        module = GetModuleHandleA("CM-GPTP-Release.qdp");
    if (!module)
        return;

    // Stable GPTP's upgrades.dat extender allocates only the extended
    // upgrade range [46,100), but biases most code operands by -46 so native
    // upgrade IDs can index it directly. The same bias is accidentally
    // applied to the match-start bulk-clear destination at 0x4CCEC3. That
    // REP STOSB consequently writes 46 bytes before the allocation and can
    // hit a heap guard boundary. Correct only this bulk-clear destination;
    // all ID-indexed operands must retain their bias.
    constexpr uintptr_t sequence_address = 0x004CCEBD;
    constexpr uint32_t first_extended_upgrade = 46;
    constexpr uint32_t upgrade_limit = 100;
    constexpr uint32_t player_count = 12;
    constexpr uint32_t clear_count =
        (upgrade_limit - first_extended_upgrade) * player_count;
    uint8_t *sequence = reinterpret_cast<uint8_t *>(sequence_address);
    uint32_t observed_count = 0;
    uint32_t biased_destination = 0;
    memcpy(&observed_count, sequence + 1, sizeof(observed_count));
    memcpy(&biased_destination, sequence + 6,
           sizeof(biased_destination));
    const bool signature_matches =
        sequence[0] == 0xB9 && sequence[5] == 0xBF &&
        sequence[10] == 0xF3 && sequence[11] == 0xAA &&
        sequence[12] == 0xB9 && observed_count == clear_count;
    const uintptr_t corrected_destination =
        static_cast<uintptr_t>(biased_destination) +
        first_extended_upgrade;

    bool writable_range = signature_matches;
    uintptr_t cursor = corrected_destination;
    const uintptr_t end = corrected_destination + clear_count;
    while (writable_range && cursor < end)
    {
        MEMORY_BASIC_INFORMATION memory = {};
        if (VirtualQuery(reinterpret_cast<const void *>(cursor), &memory,
                         sizeof(memory)) != sizeof(memory) ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
        {
            writable_range = false;
            break;
        }
        const DWORD protection = memory.Protect & 0xFF;
        writable_range =
            protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
        const uintptr_t region_end =
            reinterpret_cast<uintptr_t>(memory.BaseAddress) +
            memory.RegionSize;
        if (region_end <= cursor)
        {
            writable_range = false;
            break;
        }
        cursor = region_end < end ? region_end : end;
    }

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signature_matches || biased_destination == 0 || !writable_range)
    {
        if (log)
        {
            fprintf(log,
                "GPTP upgrade research clear preflight failed: module=%p "
                "sequence=%p signature=%u count=%lu biased=%08lX "
                "corrected=%p writable=%u\n",
                module, sequence, static_cast<unsigned>(signature_matches),
                static_cast<unsigned long>(observed_count),
                static_cast<unsigned long>(biased_destination),
                reinterpret_cast<void *>(corrected_destination),
                static_cast<unsigned>(writable_range));
            fclose(log);
        }
        gptp_upgrade_clear_patch_state =
            GptpUpgradeClearPatchState::Incompatible;
        return;
    }

    DWORD old_protection = 0;
    uint32_t *destination_operand = reinterpret_cast<uint32_t *>(
        sequence + 6);
    if (!VirtualProtect(destination_operand, sizeof(*destination_operand),
                        PAGE_EXECUTE_READWRITE, &old_protection))
    {
        if (log)
        {
            fprintf(log,
                "GPTP upgrade research clear VirtualProtect failed: "
                "error=%lu\n",
                static_cast<unsigned long>(GetLastError()));
            fclose(log);
        }
        gptp_upgrade_clear_patch_state =
            GptpUpgradeClearPatchState::Incompatible;
        return;
    }
    *destination_operand = static_cast<uint32_t>(corrected_destination);
    FlushInstructionCache(GetCurrentProcess(), sequence, 12);
    DWORD ignored = 0;
    VirtualProtect(destination_operand, sizeof(*destination_operand),
        old_protection, &ignored);

    if (log)
    {
        fprintf(log,
            "GPTP upgrade research clear stabilized: sequence=%p "
            "destination=%08lX->%p bytes=%lu\n",
            sequence, static_cast<unsigned long>(biased_destination),
            reinterpret_cast<void *>(corrected_destination),
            static_cast<unsigned long>(clear_count));
        fclose(log);
    }
    gptp_upgrade_clear_patch_state =
        GptpUpgradeClearPatchState::Installed;
}

void EnsureGptpInitialCameraCenter()
{
    if (gptp_initial_camera_patch_state !=
        GptpInitialCameraPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("gptp.qdp");
    if (!module)
        module = GetModuleHandleA("CM-GPTP-Release.qdp");
    if (!module)
        return;

    // Stable GPTP chooses the initial camera from StarCraft's start-position
    // table after the local player is resolved. This is later than the stock
    // trigger Center View action and independently subtracts 320x200. Patch
    // all four members of the compare/subtract pair so the underflow guards
    // and the resulting origin continue to agree.
    constexpr uintptr_t sequence_rva = 0x00118C53;
    constexpr size_t x_compare_offset = 0x08;
    constexpr size_t x_subtract_offset = 0x1E;
    constexpr size_t y_compare_offset = 0x2E;
    constexpr size_t y_subtract_offset = 0x3F;
    constexpr uint32_t native_x_center = 320;
    constexpr uint32_t native_y_center = 200;
    constexpr uint32_t native_x_subtract = 0u - native_x_center;
    constexpr uint32_t native_y_subtract =
        static_cast<uint16_t>(0u - native_y_center);
    const uint32_t expanded_x_center = resolution::camera_center_x;
    const uint32_t expanded_y_center = resolution::camera_center_y;
    const uint32_t expanded_x_subtract = 0u - expanded_x_center;
    const uint32_t expanded_y_subtract =
        static_cast<uint16_t>(0u - expanded_y_center);

    const uint8_t prefix[] = {
        0x8B, 0x04, 0x85, 0x1C, 0xD7, 0x58, 0x00, 0xBA,
    };
    const uint8_t x_middle[] = {
        0x89, 0x85, 0x88, 0xFA, 0xFF, 0xFF,
        0x0F, 0xB7, 0xC0, 0x8B, 0xC8, 0x66, 0x3B, 0xC2,
        0x76, 0x08, 0x8D, 0x81,
    };
    const uint8_t before_y_compare[] = {
        0xEB, 0x02, 0x33, 0xC0,
        0x66, 0x89, 0x85, 0x88, 0xFA, 0xFF, 0xFF, 0xB9,
    };
    const uint8_t before_y_subtract[] = {
        0x66, 0x8B, 0x85, 0x8A, 0xFA, 0xFF, 0xFF,
        0x66, 0x3B, 0xC1, 0x76, 0x22, 0xB9,
    };
    const uint8_t suffix[] = {
        0x66, 0x03, 0xC1,
        0x66, 0x89, 0x85, 0x8A, 0xFA, 0xFF, 0xFF,
        0xFF, 0xB5, 0x88, 0xFA, 0xFF, 0xFF,
    };

    uint8_t *sequence = reinterpret_cast<uint8_t *>(module) + sequence_rva;
    uint32_t *x_compare = reinterpret_cast<uint32_t *>(
        sequence + x_compare_offset);
    uint32_t *x_subtract = reinterpret_cast<uint32_t *>(
        sequence + x_subtract_offset);
    uint32_t *y_compare = reinterpret_cast<uint32_t *>(
        sequence + y_compare_offset);
    uint32_t *y_subtract = reinterpret_cast<uint32_t *>(
        sequence + y_subtract_offset);
    const bool signatures_match =
        memcmp(sequence, prefix, sizeof(prefix)) == 0 &&
        memcmp(sequence + 0x0C, x_middle, sizeof(x_middle)) == 0 &&
        memcmp(sequence + 0x22, before_y_compare,
               sizeof(before_y_compare)) == 0 &&
        memcmp(sequence + 0x32, before_y_subtract,
               sizeof(before_y_subtract)) == 0 &&
        memcmp(sequence + 0x43, suffix, sizeof(suffix)) == 0;
    const bool native_operands =
        *x_compare == native_x_center &&
        *x_subtract == native_x_subtract &&
        *y_compare == native_y_center &&
        *y_subtract == native_y_subtract;
    const bool expanded_operands =
        *x_compare == expanded_x_center &&
        *x_subtract == expanded_x_subtract &&
        *y_compare == expanded_y_center &&
        *y_subtract == expanded_y_subtract;

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signatures_match || (!native_operands && !expanded_operands))
    {
        if (log)
        {
            fprintf(log,
                "GPTP initial camera preflight failed: module=%p "
                "sequence=%p signature=%u operands=(%lu,%08lX,%lu,%08lX)\n",
                module, sequence, static_cast<unsigned>(signatures_match),
                static_cast<unsigned long>(*x_compare),
                static_cast<unsigned long>(*x_subtract),
                static_cast<unsigned long>(*y_compare),
                static_cast<unsigned long>(*y_subtract));
            fclose(log);
        }
        gptp_initial_camera_patch_state =
            GptpInitialCameraPatchState::Incompatible;
        return;
    }

    if (native_operands)
    {
        DWORD old_protection = 0;
        if (!VirtualProtect(sequence, 0x53, PAGE_EXECUTE_READWRITE,
                            &old_protection))
        {
            if (log)
            {
                fprintf(log,
                    "GPTP initial camera VirtualProtect failed: error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                fclose(log);
            }
            gptp_initial_camera_patch_state =
                GptpInitialCameraPatchState::Incompatible;
            return;
        }
        *x_compare = expanded_x_center;
        *x_subtract = expanded_x_subtract;
        *y_compare = expanded_y_center;
        *y_subtract = expanded_y_subtract;
        FlushInstructionCache(GetCurrentProcess(), sequence, 0x53);
        DWORD ignored = 0;
        VirtualProtect(sequence, 0x53, old_protection, &ignored);
    }

    if (log)
    {
        fprintf(log,
            "GPTP initial camera center installed: sequence=%p "
            "center=(%lu,%lu) subtract=(%08lX,%04lX)\n",
            sequence, static_cast<unsigned long>(*x_compare),
            static_cast<unsigned long>(*y_compare),
            static_cast<unsigned long>(*x_subtract),
            static_cast<unsigned long>(*y_subtract));
        fclose(log);
    }
    gptp_initial_camera_patch_state =
        GptpInitialCameraPatchState::Installed;
}

void EnsurePresentationCursorGuards()
{
    if (presentation_cursor_patch_state !=
        PresentationCursorPatchState::WaitingForModule)
        return;

    HMODULE module = GetModuleHandleA("ddraw.dll");
    if (!module)
        return;

    // The local cnc-ddraw presentation shim owns another pair of cursor APIs.
    // A translated native HUD click can make its legacy ClipCursor rectangle
    // synchronously clamp the physical pointer to the old window corner. Keep
    // these guards scoped by ConsoleWndProc exactly like the engine imports.
    constexpr uintptr_t clip_iat_rva = 0x0003C330;
    constexpr uintptr_t cursor_iat_rva = 0x0003C34C;
    constexpr uintptr_t clip_thunk_rva = 0x000261CC;
    constexpr uintptr_t cursor_thunk_rva = 0x000261A2;
    uint8_t *base = reinterpret_cast<uint8_t *>(module);
    ClipCursorFunction *clip_iat =
        reinterpret_cast<ClipCursorFunction *>(base + clip_iat_rva);
    SetCursorPosFunction *cursor_iat =
        reinterpret_cast<SetCursorPosFunction *>(base + cursor_iat_rva);
    uint8_t *clip_thunk = base + clip_thunk_rva;
    uint8_t *cursor_thunk = base + cursor_thunk_rva;

    uintptr_t clip_operand = 0;
    uintptr_t cursor_operand = 0;
    memcpy(&clip_operand, clip_thunk + 2, sizeof(uint32_t));
    memcpy(&cursor_operand, cursor_thunk + 2, sizeof(uint32_t));
    const bool signature_matches =
        clip_thunk[0] == 0xFF && clip_thunk[1] == 0x25 &&
        cursor_thunk[0] == 0xFF && cursor_thunk[1] == 0x25 &&
        clip_operand == reinterpret_cast<uintptr_t>(clip_iat) &&
        cursor_operand == reinterpret_cast<uintptr_t>(cursor_iat);
    const ClipCursorFunction current_clip = *clip_iat;
    const SetCursorPosFunction current_cursor = *cursor_iat;
    const bool imports_match =
        (current_clip == original_clip_cursor ||
         current_clip == ClipCursorExpanded) &&
        (current_cursor == original_set_cursor_pos ||
         current_cursor == SetCursorPosExpandedHudGuard);

    FILE *log = fopen("fixed_zoom_renderer.log", "a");
    if (!signature_matches || !imports_match)
    {
        if (log)
        {
            fprintf(log,
                "presentation cursor guards preflight failed: module=%p "
                "signature=%u clip=(iat=%p current=%p expected=%p) "
                "cursor=(iat=%p current=%p expected=%p)\n",
                module, static_cast<unsigned>(signature_matches),
                clip_iat, reinterpret_cast<void *>(current_clip),
                reinterpret_cast<void *>(original_clip_cursor),
                cursor_iat, reinterpret_cast<void *>(current_cursor),
                reinterpret_cast<void *>(original_set_cursor_pos));
            fclose(log);
        }
        presentation_cursor_patch_state =
            PresentationCursorPatchState::Incompatible;
        return;
    }

    if (current_clip != ClipCursorExpanded ||
        current_cursor != SetCursorPosExpandedHudGuard)
    {
        DWORD old_protection = 0;
        uint8_t *iat_start = base + clip_iat_rva;
        constexpr size_t iat_span = cursor_iat_rva - clip_iat_rva +
            sizeof(SetCursorPosFunction);
        if (!VirtualProtect(iat_start, iat_span, PAGE_READWRITE,
                            &old_protection))
        {
            if (log)
            {
                fprintf(log,
                    "presentation cursor IAT VirtualProtect failed: "
                    "error=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
                fclose(log);
            }
            presentation_cursor_patch_state =
                PresentationCursorPatchState::Incompatible;
            return;
        }
        *clip_iat = ClipCursorExpanded;
        *cursor_iat = SetCursorPosExpandedHudGuard;
        FlushInstructionCache(GetCurrentProcess(), iat_start, iat_span);
        DWORD ignored = 0;
        VirtualProtect(iat_start, iat_span, old_protection, &ignored);
    }

    if (log)
    {
        fprintf(log,
            "presentation cursor guards installed: module=%p "
            "ClipCursor=%p SetCursorPos=%p\n",
            module, clip_iat, cursor_iat);
        fclose(log);
    }
    presentation_cursor_patch_state = PresentationCursorPatchState::Installed;
}

void PrepareExpandedDragClip()
{
    // Preserve whatever origin the active windowing shim put into the stock
    // cursor-limit rectangle. Only its legacy 640x400 extent is replaced.
    const Rect32 &stock = *reinterpret_cast<const Rect32 *>(0x005993B0);
    const int32_t left = static_cast<int32_t>(stock.left);
    const int32_t top = static_cast<int32_t>(stock.top);
    expanded_drag_clip_rect.left = left;
    expanded_drag_clip_rect.top = top;
    expanded_drag_clip_rect.right = left +
        static_cast<int32_t>(resolution::game_width);
    expanded_drag_clip_rect.bottom = top +
        static_cast<int32_t>(resolution::screen_height);

    FILE *log = fopen("fixed_zoom_input.log", "a");
    if (log)
    {
        fprintf(log,
            "%lu prepare-drag-clip stock=(%ld,%ld,%ld,%ld) "
            "expanded=(%ld,%ld,%ld,%ld)\n",
            static_cast<unsigned long>(GetTickCount()),
            static_cast<long>(stock.left),
            static_cast<long>(stock.top),
            static_cast<long>(stock.right),
            static_cast<long>(stock.bottom),
            static_cast<long>(expanded_drag_clip_rect.left),
            static_cast<long>(expanded_drag_clip_rect.top),
            static_cast<long>(expanded_drag_clip_rect.right),
            static_cast<long>(expanded_drag_clip_rect.bottom));
        fclose(log);
    }
}

Common::PatchManager *patch_mgr;

void ResetGameSpeedWaits()
{
    for (int i = 0; i < 7; i++)
    {
        bw::wait_times_msec[i] = 42;
    }
}

void PatchDraw(Common::PatchContext *patch)
{
    // Install the one-shot start-position camera correction as early as the
    // plugin load order permits. Keep BeginStockDrawScreen's call as a
    // fallback for installations where GPTP is loaded after aidebug.
    EnsureGptpInitialCameraCenter();
    PatchInteractionBounds(patch);
    PatchStartLocationCameraOrigin(patch);
    PatchTriggerCenterView(patch);
    PatchPortraitCameraOrigins(patch);
    PatchPositionalAudioViewport(patch);
    PatchClipCursor(patch);
    PatchSetCursorPos(patch);
    PatchLegacyHudTooltipHitTests(patch);
    patch->Hook(bw::IsOutsideGameScreenHook, IsOutsideExpandedGameScreen);
    patch->Hook(bw::SDrawLockSurface, SDrawLockSurface_Hook);
    patch->Hook(bw::SDrawUnlockSurface, SDrawUnlockSurface_Hook);

    // Keep StarCraft's complete native renderer intact.  Compose the fixed
    // zoom only after stock DrawScreen has produced a valid 640x480 frame.
    // Bracketing it also prevents that intermediate stock frame from being
    // presented before the completed expanded frame.
    patch->CallHook(bw::DrawScreenBegin, BeginStockDrawScreen);
    patch->CallHook(bw::DrawScreenAfter, AfterStockDrawScreen);

    // Screen-space game text belongs to the outer/base frame only. Replace
    // the call inside ScreenUpdateProc with a conditional call so recursive
    // camera passes cannot stamp it into every quadrant.
    const uintptr_t call_address = 0x004BD614 + patch->GetDiff();
    uint8_t call[5] = { 0xE8, 0, 0, 0, 0 };
    const intptr_t displacement =
        reinterpret_cast<uintptr_t>(&DrawGameTextConditional) -
        (call_address + sizeof(call));
    const int32_t relative = static_cast<int32_t>(displacement);
    memcpy(call + 1, &relative, sizeof(relative));
    patch->Patch(reinterpret_cast<void *>(0x004BD614), call,
                 sizeof(call), PATCH_REPLACE);

    // The drag-selection border is another direct ScreenUpdateProc call,
    // independent of draw layer 1. Suppress it in every stock/private pass;
    // AfterStockDrawScreen renders it once over the final expanded frame.
    const uintptr_t selection_call_address = 0x004BD619 + patch->GetDiff();
    uint8_t selection_call[5] = { 0xE8, 0, 0, 0, 0 };
    const intptr_t selection_displacement =
        reinterpret_cast<uintptr_t>(&DrawSelectionBoxConditional) -
        (selection_call_address + sizeof(selection_call));
    const int32_t selection_relative =
        static_cast<int32_t>(selection_displacement);
    memcpy(selection_call + 1, &selection_relative,
           sizeof(selection_relative));
    patch->Patch(reinterpret_cast<void *>(0x004BD619), selection_call,
                 sizeof(selection_call), PATCH_REPLACE);
}

void RemoveLimits(Common::PatchContext *patch)
{
    patch->CallHook(bw::WaitTimesSet, ResetGameSpeedWaits);
    if (UseConsole)
    {
        patch->Hook(bw::GenerateFog, GenerateFog);
    }
}
