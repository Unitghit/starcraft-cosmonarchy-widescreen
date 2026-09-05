#ifndef COSMONARCHY_WORLD_ZOOM_H
#define COSMONARCHY_WORLD_ZOOM_H

#include <cstdint>

namespace world_zoom
{
    struct Point
    {
        int x;
        int y;
    };

    // World zoom is independent from internal resolution and presentation
    // scaling. Missing or unsupported settings leave the original path intact.
    bool Enabled();
    bool Active();
    void BeginMatch();
    unsigned Percentage();
    unsigned VisibleWidth();
    unsigned VisibleHeight();
    unsigned SourceLeft();
    unsigned SourceTop();
    unsigned SourceScreenHeight();
    struct SelectionBounds { unsigned left, top, right, bottom; };
    // Displayed crop, including the visible world beside the bottom HUD.
    // Uses the current rendered transform, never the next wheel target.
    SelectionBounds VisibleSelectionBounds(unsigned camera_x, unsigned camera_y);

    void UpdateTransform(uint32_t camera_x, uint32_t camera_y,
                         uint32_t map_width, uint32_t map_height,
                         bool advance_animation = true);

    // During an anchored transition, derives the real camera position which
    // keeps the recorded world pixel under the pointer. Returns true when the
    // caller should move the engine camera to the returned coordinates.
    bool ResolveCameraAnchor(uint32_t camera_x, uint32_t camera_y,
                             uint32_t map_width, uint32_t map_height,
                             uint32_t *adjusted_x, uint32_t *adjusted_y);

    // Scales only the indexed 8-bit battlefield. The HUD and other screen UI
    // are composed after this operation by the existing renderer.
    void ScaleBattlefield(uint8_t *frame, uint8_t *scratch);

    Point PresentedToSource(int x, int y);
    Point SourceToPresented(int x, int y);

    // Minimap world-point centering. Returns a base camera whose visible crop
    // is centered on the point, with the native eight-pixel vertical margin.
    Point CameraForMinimapPoint(uint32_t world_x, uint32_t world_y,
                               uint32_t map_width, uint32_t map_height);

    // Positive wheel deltas step inward. Negative deltas step outward. The
    // world pixel under the presented pointer remains anchored throughout the
    // transition. Returns true when the opt-in zoom subsystem owns the event.
    bool AdjustByWheel(int delta, int presented_x, int presented_y,
                       uint32_t camera_x, uint32_t camera_y);
}

#endif // COSMONARCHY_WORLD_ZOOM_H
