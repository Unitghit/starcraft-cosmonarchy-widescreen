#pragma once

#include "resolution.h"
#include "world_zoom.h"

namespace gameplay_input
{
    struct BattlefieldRoute
    {
        world_zoom::Point point;
        bool hidden_native_hud;
    };

    // Presented HUD ownership is decided by the caller before using this
    // route. The obsolete dialog tree instead sees the gameplay-dispatch
    // coordinate, which is in zoom-source space, not presentation space.
    template<class NativeHudHit>
    BattlefieldRoute RouteBattlefield(int x, int y, NativeHudHit native_hud_hit)
    {
        auto point = world_zoom::PresentedToSource(x, y);
        if (world_zoom::Active())
        {
            // Keep the existing native edge-scroll triggers unchanged.
            if (x <= 1)
                point.x = 0;
            else if (x >= static_cast<int>(resolution::game_width) - 2)
                point.x = static_cast<int>(resolution::game_width) - 1;
            if (y <= 1)
                point.y = 0;
        }
        return {point, native_hud_hit(point.x, point.y)};
    }
}
