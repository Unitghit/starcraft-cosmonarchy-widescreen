// Offline only: production geometry, no engine or diagnostic hooks.
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/ui_scale.h"
#include <cstdio>
#include <cstdlib>

static void Check(bool condition)
{
    if (!condition) { std::puts("FAIL HUD geometry contract"); std::exit(1); }
}
int main()
{
    unsigned cases = 0;
    for (int w : {640, 800, 1024, 1280, 1365, 1920, 2560, 3840})
    for (int h : {480, 600, 720, 769, 1080, 1440, 2160})
    for (int reference : {0, -1, 480, 600, 720, 900, 1080, 1440, 2160, 9999})
    for (int anchor = 0; anchor != 4; ++anchor)
    {
        ui_scale::Geometry g;
        g.Configure(w, h, reference, anchor);
        Check(g.left >= 0 && g.top >= 0 && g.left + g.width <= w && g.top + g.height <= h);
        Check(g.x_edges[0] == g.left && g.x_edges[640] == g.left + g.width);
        Check(g.y_edges[0] == g.top && g.y_edges[480] == g.top + g.height);
        for (int x = 0; x < 640; ++x)
            for (int p = g.x_edges[x]; p < g.x_edges[x + 1]; ++p)
                Check(g.NativeX(p) == x);
        for (int y = 0; y < 480; ++y)
            for (int p = g.y_edges[y]; p < g.y_edges[y + 1]; ++p)
                Check(g.NativeY(p) == y);
        Check(g.NativeX(g.left - 1) < 0 && g.NativeX(g.left + g.width) >= 640);
        Check(g.NativeY(g.top - 1) < 0 && g.NativeY(g.top + g.height) >= 480);
        if (reference == 0)
        {
            Check(g.width == 640 && g.height == 480);
            for (int p = 0; p < 640; ++p) Check(g.NativeX(g.left + p) == p);
        }
        if (anchor == 3) Check(g.top + g.height == h);
        ++cases;
    }
    ui_scale::Configure(1920, 1080, 720, 1080, false);
    Check(ui_scale::hud.width == 960 && ui_scale::hud.height == 720);
    Check(ui_scale::hud.left == 480 && ui_scale::hud.top == 360);
    Check(ui_scale::objectives.width == 640 && ui_scale::objectives.left == 640);
    ui_scale::Configure(1920, 1080, 720, 720, true);
    Check(ui_scale::objectives.left == 0 && ui_scale::resources.left == 960);
    std::printf("PASS HUD sizing: %u geometry configurations, exact render/input sample ownership\n", cases);
}
