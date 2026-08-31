#ifndef COSMONARCHY_PRESENTATION_H
#define COSMONARCHY_PRESENTATION_H

#include "console/windows_wrap.h"
#include "resolution.h"
#include "../../zoom_presentation.h"

namespace presentation
{
    constexpr unsigned default_scale_numerator =
        zoom_presentation_config::scale_numerator;
    constexpr unsigned default_scale_denominator =
        zoom_presentation_config::scale_denominator;
    inline unsigned default_client_width =
        resolution::screen_width * default_scale_numerator /
        default_scale_denominator;
    inline unsigned default_client_height =
        resolution::screen_height * default_scale_numerator /
        default_scale_denominator;

    static_assert(default_scale_numerator >= 1 &&
                  default_scale_denominator >= 1,
                  "presentation scale must be a positive rational number");
    void ResizeClient(HWND hwnd);
    void EnsureClient(HWND hwnd);
}

#endif // COSMONARCHY_PRESENTATION_H
