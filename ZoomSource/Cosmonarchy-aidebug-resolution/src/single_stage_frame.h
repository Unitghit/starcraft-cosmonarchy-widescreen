#ifndef COSMONARCHY_SINGLE_STAGE_FRAME_H
#define COSMONARCHY_SINGLE_STAGE_FRAME_H
#include <algorithm>
#include <cstdint>
#include <vector>
#include "pointer_layer.h"
namespace single_stage
{
    constexpr unsigned NativeLayers = 4, NativeWidth = 640, NativeHeight = 480;
    constexpr unsigned AtlasWidth = 1024, AtlasHeight = 2048;
    // The native console owns every pixel below the 640x400 battlefield.
    // Video portraits may advance between coverage passes. Different colors
    // in that opaque band are not evidence of background-dependent blending.
    inline bool NativeUiNeedsFallback(unsigned y, uint8_t zero, uint8_t full)
    { return y < 400 && zero != full && (zero != 0 || full != 255); }
    struct UiRect { int left = 0, top = 0, width = 0, height = 0; };
    struct Frame
    {
        bool valid = false;
        bool smooth_world_edges = false;
        unsigned width = 0, height = 0, world_width = 0, world_height = 0;
        uint64_t serial = 0;
        std::vector<uint8_t> world, mask, ui;
        std::vector<uint8_t> native_ui;
        UiRect native_rects[NativeLayers];
        PointerLayer pointer;
        uint32_t palette[256] = {};
    };
    // Pixel centers, not a composition of two rounded logical samples.
    inline unsigned Sample(unsigned p, unsigned input, unsigned output)
    { return static_cast<unsigned>((uint64_t(2) * p + 1) * input / (uint64_t(2) * output)); }
    inline int NativeSample(unsigned p, unsigned logical, unsigned output, int origin, int extent, unsigned native)
    {
        const int64_t relative = (int64_t(2) * p + 1) * logical - int64_t(2) * output * origin;
        const int64_t span = int64_t(2) * output * extent;
        return extent <= 0 || relative < 0 || relative >= span ? -1 : static_cast<int>(relative * native / span);
    }
    inline uint8_t IndexAt(const Frame &f, unsigned x, unsigned y, unsigned width, unsigned height)
    {
        const size_t ui = (static_cast<size_t>(Sample(y, f.height, height)) * f.width + Sample(x, f.width, width)) * 2;
        if (f.ui[ui + 1]) return f.ui[ui];
        for (int layer = NativeLayers - 1; layer >= 0; --layer)
        {
            const auto &r = f.native_rects[layer];
            if (r.width <= 0 || r.height <= 0 || f.native_ui.empty()) continue;
            const int nx = NativeSample(x, f.width, width, r.left, r.width, NativeWidth);
            const int ny = NativeSample(y, f.height, height, r.top, r.height, NativeHeight);
            if (nx < 0 || ny < 0) continue;
            const size_t index = ((layer * NativeHeight + ny) * NativeWidth + nx) * 2;
            if (f.native_ui[index + 1]) return f.native_ui[index];
        }
        return f.world[static_cast<size_t>(Sample(y, f.world_height, height)) * f.world_width + Sample(x, f.world_width, width)];
    }
    // The callback runs with a coherent published frame held on the render
    // thread. It must not call back into Storm or acquire its surface lock.
    bool WithFrame(bool (*callback)(const Frame &, void *), void *context);
    bool InstallPortableAdapters(void *wrapper);
}
#endif
