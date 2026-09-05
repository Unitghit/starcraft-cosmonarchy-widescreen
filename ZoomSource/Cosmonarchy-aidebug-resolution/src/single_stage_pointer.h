#ifndef COSMONARCHY_SINGLE_STAGE_POINTER_H
#define COSMONARCHY_SINGLE_STAGE_POINTER_H
#include "pointer_layer.h"
#include "console/windows_wrap.h"
namespace single_stage
{
#ifdef SINGLE_STAGE_TEST
    inline const PointerDraw *test_pointer_override=nullptr;
#endif
    // Direct USER32 exports bypass cnc-ddraw's game-coordinate IAT shims.
    inline PointerDraw PollPointer(const PointerLayer &layer,unsigned logical_w,unsigned logical_h,
        float output_x,float output_y,float output_w,float output_h)
    {
#ifdef SINGLE_STAGE_TEST
        if(test_pointer_override)return *test_pointer_override;
#endif
        if(!layer.enabled||!layer.window)return {};
        using CursorFn=BOOL(WINAPI *)(LPPOINT);
        using ClientFn=BOOL(WINAPI *)(HWND,LPPOINT);
        static auto get_cursor=reinterpret_cast<CursorFn>(GetProcAddress(GetModuleHandleA("user32.dll"),"GetCursorPos"));
        static auto to_client=reinterpret_cast<ClientFn>(GetProcAddress(GetModuleHandleA("user32.dll"),"ScreenToClient"));
        POINT point={};HWND window=reinterpret_cast<HWND>(layer.window);
        if(!get_cursor||!to_client||!get_cursor(&point)||!to_client(window,&point))return {};
        const HWND foreground=GetForegroundWindow();
        return PointerGeometry(layer,logical_w,logical_h,output_w,output_h,
            point.x-output_x,point.y-output_y,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0,
            foreground==window||IsChild(window,foreground));
    }
}
#endif
