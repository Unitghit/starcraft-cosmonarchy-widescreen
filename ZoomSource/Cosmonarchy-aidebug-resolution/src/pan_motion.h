#ifndef COSMONARCHY_PAN_MOTION_H
#define COSMONARCHY_PAN_MOTION_H

#include <cstdint>

namespace zoom_pan
{
    // Input is native world-pixel motion; output is world-pixel motion reduced
    // to keep displayed speed constant. Carry belongs to an input source/axis.
    class MotionAxis
    {
        double remainder = 0;
        int direction = 0;
    public:
        void Reset() { remainder = 0; direction = 0; }
        int Scale(int delta, unsigned source_extent, unsigned output_extent)
        {
            if (!output_extent || source_extent >= output_extent)
            {
                Reset();
                return delta;
            }
            if (!delta) return 0;
            const int sign = delta < 0 ? -1 : 1;
            if (sign != direction) remainder = 0;
            direction = sign;
            const double motion = remainder +
                static_cast<double>(delta) * source_extent / output_extent;
            const int whole = static_cast<int>(motion); // symmetric truncation
            remainder = motion - whole;
            return whole;
        }
    };

    class Motion
    {
        MotionAxis x, y;
        bool valid = false;
        int expected_x = 0, expected_y = 0;
        uint32_t last_tick = 0;
    public:
        void Reset() { x.Reset(); y.Reset(); valid = false; }
        void Begin(int camera_x, int camera_y, uint32_t tick)
        {
            if (!valid || tick - last_tick > 150 ||
                camera_x != expected_x || camera_y != expected_y)
            {
                x.Reset();
                y.Reset();
            }
            last_tick = tick;
        }
        int X(int delta, unsigned source, unsigned output)
            { return x.Scale(delta, source, output); }
        int Y(int delta, unsigned source, unsigned output)
            { return y.Scale(delta, source, output); }
        void End(int before_x, int before_y, int requested_x, int requested_y,
                 int after_x, int after_y)
        {
            // Do not accumulate pressure against a map edge or native guard.
            if (after_x - before_x != requested_x) x.Reset();
            if (after_y - before_y != requested_y) y.Reset();
            expected_x = after_x;
            expected_y = after_y;
            valid = true;
        }
    };
}
#endif
