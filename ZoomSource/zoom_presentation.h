#ifndef COSMONARCHY_ZOOM_PRESENTATION_H
#define COSMONARCHY_ZOOM_PRESENTATION_H

// Presentation-only rational scale. This never changes StarCraft, gameplay,
// HUD, compositor, or input coordinate spaces. The logical framebuffer remains
// configured independently in zoom_resolution.h. A rational representation
// keeps fractional settings such as 2.5x exact and avoids floating-point drift.
namespace zoom_presentation_config
{
    constexpr int scale_numerator = 5;
    constexpr int scale_denominator = 2;
}

#endif // COSMONARCHY_ZOOM_PRESENTATION_H
