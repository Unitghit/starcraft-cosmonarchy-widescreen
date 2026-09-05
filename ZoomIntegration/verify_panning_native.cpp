// Offline tests of the actual geometry used by both production world paths.
#define NOMINMAX
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/resolution.h"

static void Check(bool ok, const char *message)
{
    if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

static void Geometry(unsigned width, unsigned height, unsigned map_width,
                     unsigned camera)
{
    Check(resolution::Configure(width, height), "configure");
    const unsigned native_max = map_width - 640;
    unsigned end = 0;
    for (unsigned col = 0; col < resolution::tile_columns; ++col)
    {
        const auto p = resolution::PlanWorldPassX(col, camera, map_width);
        Check(p.destination == end && p.width > 0, "gap or empty span");
        Check(p.source + p.width <= 640, "native read bounds");
        Check(p.camera + p.source == camera + p.destination, "world coordinate mismatch");
        Check(p.camera <= native_max, "camera bounds");
        if (p.camera < native_max)
            Check(p.source + p.width <= 632, "unguarded native right edge");
        else
            Check(p.camera % 8 == 0, "map edge camera must be aligned");
        if (col + 1 < resolution::tile_columns)
            Check(p.source + p.width <= 632, "unguarded internal join");
        end += p.width;
    }
    Check(end == width, "incomplete coverage");
}

static unsigned Fresh(unsigned x, unsigned y, unsigned frame)
{
    // Changes every frame, including the stationary-camera case. This models
    // disappearing units/effects/fog, not just an immutable terrain image.
    return (frame << 24) | ((y & 4095) << 12) | (x & 4095);
}

static void FreshFrames(unsigned width, unsigned height)
{
    Check(resolution::Configure(width, height), "configure");
    std::vector<unsigned> output(width, 0xdeadbeef);
    constexpr unsigned map_width = 8192;
    for (unsigned frame = 1; frame <= 120; ++frame)
    {
        // Vertical-only, horizontal, diagonal, stop with the button held,
        // release, then both map edges. Pan state is not a renderer input.
        const unsigned camera = frame < 30 ? 1003 : frame < 60 ? 1000 + frame :
            frame < 80 ? 1059 : frame < 100 ? map_width - width : 0;
        const unsigned y = frame < 45 ? 1000 + frame : 1044;
        for (unsigned col = 0; col < resolution::tile_columns; ++col)
        {
            const auto p = resolution::PlanWorldPassX(col, camera, map_width);
            for (unsigned x = 0; x < p.width; ++x)
            {
                const unsigned sx = p.source + x;
                // Poison the known unsafe right edge in unaligned native
                // passes. A valid crop must never import these old pixels.
                output[p.destination + x] = p.camera % 8 && sx >= 636 ?
                    0xdeadbeef : Fresh(p.camera + sx, y, frame);
            }
        }
        for (unsigned x = 0; x < width; ++x)
            Check(output[x] == Fresh(camera + x, y, frame), "stale or displaced pixel");
    }
    const unsigned old_columns = (width + 639) / 640;
    std::printf("%ux%u: world passes %u -> %u; fresh-frame sequence PASS\n",
        width, height, old_columns * resolution::tile_rows,
        resolution::tile_columns * resolution::tile_rows);
}

int main()
{
    unsigned cases = 0;
    for (unsigned width = 640; width <= 3840; ++width)
        for (unsigned height : {480u, 720u, 1080u, 1440u, 2160u})
            for (unsigned phase = 0; phase < 32; ++phase)
                for (unsigned map_width : {((width + 31) / 32) * 32, 8192u})
                {
                    const unsigned max = map_width - width;
                    for (unsigned camera : {std::min(phase, max),
                                            std::min(1000 + phase, max),
                                            max - std::min(phase, max)})
                    {
                        Geometry(width, height, map_width, camera);
                        ++cases;
                    }
                }
    std::printf("Native production crop geometry: %u cases PASS\n", cases);
    for (auto size : {std::pair{640u,480u}, {1280u,720u}, {1600u,900u},
                      {1919u,1079u}, {1920u,1080u}, {2560u,1440u}, {3840u,2160u}})
        FreshFrames(size.first, size.second);
}
