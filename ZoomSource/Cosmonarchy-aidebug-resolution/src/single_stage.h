#ifndef COSMONARCHY_SINGLE_STAGE_H
#define COSMONARCHY_SINGLE_STAGE_H
#include <cstdint>
namespace ui_scale { struct Geometry; }
namespace single_stage
{
    // Optional D3D9/OpenGL/GDI adapters. The ordinary indexed frame is always
    // submitted too and remains the fallback on unsupported presentation.
    void Begin(bool eligible, unsigned width, unsigned height);
    bool Capturing();
    bool Filtering();
    bool HighRefreshPointer();
    bool PointerCapturing();
    void PointerBackground(const uint8_t *pixels,unsigned width,unsigned height,uintptr_t window,bool flat);
    void PointerCursor(const uint8_t *zero,const uint8_t *full,unsigned width,unsigned height,int hot_x,int hot_y);
    void PointerSelection(int anchor_x,int anchor_y,uint8_t color);
    bool NativeUiCapturing();
    bool NativeUiPixel(unsigned layer, const ui_scale::Geometry &geometry,
                       unsigned x, unsigned y, uint8_t color);
    void World(const uint8_t *pixels, unsigned pitch, unsigned left,
               unsigned top, unsigned width, unsigned height);
    void Opaque(unsigned x, unsigned y, unsigned width = 1, unsigned height = 1);
    void Reject();
    void Submit(const uint8_t *completed);
}
#endif
