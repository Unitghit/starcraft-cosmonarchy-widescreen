#ifndef COSMONARCHY_POINTER_LAYER_H
#define COSMONARCHY_POINTER_LAYER_H
#include <algorithm>
#include <cstdint>
#include <vector>
namespace single_stage
{
    constexpr unsigned CursorSize=128, CursorAtlasY=1920;
    struct PointerLayer
    {
        bool enabled=false, dragging=false;
        unsigned width=0,height=0;
        int hot_x=0,hot_y=0,anchor_x=0,anchor_y=0;
        uint8_t selection_color=0;
        uintptr_t window=0;
        std::vector<uint8_t> pixels;
    };
    struct PointerDraw
    {
        float cursor[4]={}; // physical left/top, output pixels per native pixel
        float shape[4]={}; // sprite width/height, selection palette index, enabled
        float selection[4]={}; // physical inclusive left/top/right/bottom
    };
    inline PointerDraw PointerGeometry(const PointerLayer &layer,unsigned logical_w,unsigned logical_h,
        float output_w,float output_h,float mouse_x,float mouse_y,bool left_down,bool focused)
    {
        PointerDraw draw;
        if(!layer.enabled||!focused||!logical_w||!logical_h||output_w<1||output_h<1)return draw;
        const float sx=output_w/logical_w,sy=output_h/logical_h;
        draw.cursor[0]=mouse_x+layer.hot_x*sx;draw.cursor[1]=mouse_y+layer.hot_y*sy;
        draw.cursor[2]=sx;draw.cursor[3]=sy;
        if(mouse_x>=0&&mouse_y>=0&&mouse_x<output_w&&mouse_y<output_h)
        { draw.shape[0]=float(layer.width);draw.shape[1]=float(layer.height); }
        if(layer.dragging&&left_down)
        {
            const float ax=std::clamp(layer.anchor_x*sx,0.0f,output_w-1);
            const float ay=std::clamp(layer.anchor_y*sy,0.0f,output_h-1);
            mouse_x=std::clamp(mouse_x,0.0f,output_w-1);mouse_y=std::clamp(mouse_y,0.0f,output_h-1);
            draw.selection[0]=std::min(ax,mouse_x);draw.selection[1]=std::min(ay,mouse_y);
            draw.selection[2]=std::max(ax,mouse_x);draw.selection[3]=std::max(ay,mouse_y);
            draw.shape[2]=float(layer.selection_color);draw.shape[3]=1;
        }
        return draw;
    }
}
#endif
