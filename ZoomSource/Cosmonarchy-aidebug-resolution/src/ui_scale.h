#pragma once
#include <algorithm>

// Presentation geometry only. Native engine surfaces and dialog coordinates
// stay 640x480. Rendering and pointer ownership share these exact samples.
namespace ui_scale
{
    inline int FloorDiv(int value, int divisor)
    {
        const int quotient = value / divisor;
        return quotient - (value % divisor < 0 ? 1 : 0);
    }
    struct Geometry
    {
        int left = 0, top = 0, width = 640, height = 480;
        int x_edges[641] = {}, y_edges[481] = {};
        bool Scaled() const { return width != 640 || height != 480; }
        int NativeX(int x) const { return FloorDiv((2 * (x - left) + 1) * 640, 2 * width); }
        int NativeY(int y) const { return FloorDiv((2 * (y - top) + 1) * 480, 2 * height); }
        void Configure(int screen_width, int screen_height, int reference, int anchor)
        {
            if (reference < 480 || reference > 2160) reference = screen_height;
            width = std::min(screen_width, (640 * screen_height + reference / 2) / reference);
            height = std::min(screen_height, (480 * width + 320) / 640);
            left = anchor == 1 ? 0 : anchor == 2 ? screen_width - width : (screen_width - width) / 2;
            top = anchor == 3 ? screen_height - height : 0;
            for (int x = 0; x <= 640; ++x)
                x_edges[x] = left - FloorDiv(640 - 2 * x * width, 1280);
            for (int y = 0; y <= 480; ++y)
                y_edges[y] = top - FloorDiv(480 - 2 * y * height, 960);
        }
    };
    inline Geometry hud, objectives, resources;
    inline void Configure(int width, int height, int hud_reference, int top_reference, bool edges)
    {
        hud.Configure(width, height, hud_reference, 3);
        objectives.Configure(width, height, top_reference, edges ? 1 : 0);
        resources.Configure(width, height, top_reference, edges ? 2 : 0);
    }
}
