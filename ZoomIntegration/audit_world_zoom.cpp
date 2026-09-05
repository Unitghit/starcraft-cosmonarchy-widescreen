// Offline audit only. Includes the production translation unit so these tests
// exercise its state transitions, not a separately maintained geometry model.
// No code from this executable is linked into the renderer.
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <vector>

static DWORD audit_tick = 1000;
static DWORD AuditGetTickCount() { return audit_tick; }
#define GetTickCount AuditGetTickCount
#include "../ZoomSource/Cosmonarchy-aidebug-resolution/src/world_zoom.cpp"
#undef GetTickCount

static int failures = 0;
static void Check(bool passed, const char* contract)
{
    std::printf("%s: %s\n", passed ? "PASS" : "FAIL", contract);
    if (!passed) ++failures;
}

static void Reset(unsigned width = 1600, unsigned height = 900)
{
    resolution::Configure(width, height);
    world_zoom::settings.loaded = true;
    world_zoom::settings.enabled = true;
    world_zoom::settings.smooth = true;
    world_zoom::settings.start_zoom_units = 10000;
    world_zoom::settings.extra_zoom_percent = 0;
    world_zoom::pixel_levels.clear();
    world_zoom::wheel_remainder = 0;
    world_zoom::current_zoom_units = 10000;
    world_zoom::target_zoom_units = 10000;
    world_zoom::animation_start_units = 10000;
    world_zoom::anchor = {};
    world_zoom::transform = {};
    world_zoom::lookup_valid = false;
    audit_tick = 1000;
}

static void Settle(unsigned units, uint32_t camera_x = 2000,
                   uint32_t camera_y = 2000)
{
    world_zoom::current_zoom_units = units;
    world_zoom::target_zoom_units = units;
    world_zoom::UpdateTransform(camera_x, camera_y, 8192, 8192);
}

int main()
{
    bool selection_good=true;
    for(unsigned width : {1280u,1920u,3840u})
    for(unsigned zoom : {10000u,12500u,15000u,20000u,40000u})
    for(unsigned camera : {0u,2000u,7000u})
    {
        Reset(width,width*9/16);
        Settle(zoom,camera,camera);
        const auto bounds=world_zoom::VisibleSelectionBounds(camera,camera);
        const auto first=world_zoom::PresentedToSource(0,0);
        const auto last=world_zoom::PresentedToSource(width-1,width*9/16-1);
        selection_good &= bounds.left==camera+world_zoom::SourceLeft() &&
            bounds.top==camera+world_zoom::SourceTop() &&
            bounds.right-bounds.left==world_zoom::VisibleWidth() &&
            bounds.bottom-bounds.top==world_zoom::SourceScreenHeight() &&
            unsigned(first.x)+camera>=bounds.left && unsigned(last.x)+camera<bounds.right &&
            unsigned(first.y)+camera>=bounds.top && unsigned(last.y)+camera<bounds.bottom;
        // A pending wheel target must not widen/narrow the displayed query.
        world_zoom::target_zoom_units=50000;
        const auto pending=world_zoom::VisibleSelectionBounds(camera,camera);
        selection_good &= pending.left==bounds.left && pending.top==bounds.top &&
            pending.right==bounds.right && pending.bottom==bounds.bottom;
    }
    Check(selection_good,"same-type selection uses displayed crop, full-height side strips and not pending zoom target");
    Check(world_zoom::Settings{}.extra_zoom_percent == 100 &&
          world_zoom::NormalizeExtraZoomPercent(0) == 0,
          "extra zoom defaults to 100 percent more and explicit Off is preserved");
    // A wheel target does not itself change the displayed scale. Therefore
    // the current frame's input transform must survive the target change.
    Reset();
    Settle(15000);
    const auto before = world_zoom::PresentedToSource(1200, 200);
    world_zoom::AdjustByWheel(120, 1200, 200, 2000, 2000);
    const auto after = world_zoom::PresentedToSource(1200, 200);
    std::printf("wheel mapping before=(%d,%d) after=(%d,%d)\n",
        before.x, before.y, after.x, after.y);
    Check(before.x == after.x && before.y == after.y,
          "wheel retarget preserves current input mapping");

    // Model the two clock reads in ConsoleWndProc -> UpdateTransform ->
    // AdjustByWheel, separated by a legitimate one-millisecond tick.
    Reset();
    Settle(15000);
    world_zoom::AdjustByWheel(120, 1200, 200, 2000, 2000);
    audit_tick += 60;
    world_zoom::UpdateTransform(2000, 2000, 8192, 8192);
    const auto displayed = world_zoom::PresentedToSource(1200, 200);
    ++audit_tick;
    world_zoom::AdjustByWheel(120, 1200, 200, 2000, 2000);
    std::printf("rapid wheel displayed=(%d,%d) anchor=(%lld,%lld)\n",
        displayed.x, displayed.y,
        static_cast<long long>(world_zoom::anchor.world_x - 2000),
        static_cast<long long>(world_zoom::anchor.world_y - 2000));
    Check(world_zoom::anchor.world_x == 2000 + displayed.x &&
          world_zoom::anchor.world_y == 2000 + displayed.y,
          "rapid wheel anchors the displayed world point");

    // At an edge the default crop depends on camera position. Validate the
    // actual update/resolve/update sequence used by BeginStockDrawScreen.
    Reset();
    uint32_t cx = 50, cy = 2000;
    world_zoom::UpdateTransform(cx, cy, 8192, 8192);
    const auto original = world_zoom::PresentedToSource(1200, 200);
    const int64_t anchored_world = cx + original.x;
    world_zoom::AdjustByWheel(120, 1200, 200, cx, cy);
    audit_tick += 180;
    world_zoom::UpdateTransform(cx, cy, 8192, 8192);
    uint32_t nx = cx, ny = cy;
    if (world_zoom::ResolveCameraAnchor(cx, cy, 8192, 8192, &nx, &ny))
    {
        cx = nx; cy = ny;
        world_zoom::UpdateTransform(cx, cy, 8192, 8192);
    }
    const auto final_point = world_zoom::PresentedToSource(1200, 200);
    std::printf("edge handoff world before=%lld after=%lld camera=%u\n",
        static_cast<long long>(anchored_world),
        static_cast<long long>(cx + final_point.x), cx);
    Check(cx + final_point.x == anchored_world,
          "edge anchor handoff retains the same reachable world point after a delayed frame");

    // Integration test of the new camera solver used by the guarded GPTP
    // input callback. Binary verification separately checks that call site.
    Reset();
    Settle(15000);
    const unsigned divisor = 32, clicked_world_x = 4096;
    const auto minimap_camera = world_zoom::CameraForMinimapPoint(
        clicked_world_x, 4096, 8192, 8192);
    const uint32_t clicked_camera = minimap_camera.x;
    world_zoom::UpdateTransform(clicked_camera, minimap_camera.y, 8192, 8192);
    const auto center = world_zoom::PresentedToSource(800, 350);
    const int center_error = static_cast<int>(clicked_camera + center.x) - clicked_world_x;
    std::printf("modeled minimap center error=%d world pixels\n", center_error);
    Check(std::abs(center_error) <= static_cast<int>(divisor),
          "GPTP minimap input model centers the visible crop within minimap rounding");

    bool matrix_ok = true;
    const unsigned sizes[][2] = {
        {640,480}, {1280,720}, {1600,900}, {1920,1080},
        {2560,1440}, {3840,2160}, {1365,769}
    };
    for (const auto& size : sizes)
    {
        for (unsigned zoom : {10000u,11250u,12500u,13750u,15000u,16250u,
                              17500u,18750u,20000u,25000u,30000u,40000u,60000u,90000u,120000u})
        {
            Reset(size[0], size[1]);
            Settle(zoom);
            const size_t count = static_cast<size_t>(size[0]) * size[1];
            std::vector<uint8_t> frame(count + 16, 0xA5);
            std::vector<uint8_t> scratch(count + 16, 0x5A);
            for (size_t i = 0; i < count; ++i)
                frame[i] = static_cast<uint8_t>((i * 37 + i / size[0]) % 251);
            const auto original_frame = frame;
            world_zoom::ScaleBattlefield(frame.data(), scratch.data());
            for (unsigned y = 0; y < size[1]; y += 19)
                for (unsigned x = 0; x < size[0]; x += 17)
                {
                    const auto source = world_zoom::PresentedToSource(x, y);
                    matrix_ok &= source.x >= 0 && source.y >= 0 &&
                        source.x < static_cast<int>(size[0]) &&
                        source.y < static_cast<int>(size[1]);
                    if (matrix_ok)
                        matrix_ok &= frame[static_cast<size_t>(y)*size[0]+x] ==
                            original_frame[static_cast<size_t>(source.y)*size[0]+source.x];
                }
            for (size_t i = count; i < count + 16; ++i)
                matrix_ok &= frame[i] == 0xA5 && scratch[i] == 0x5A;
        }
    }
    Check(matrix_ok, "actual scaler/input mapping and buffer sentinels, 105 combinations through 1200%");

    bool transitions_ok = true;
    unsigned transitions = 0;
    for (unsigned map_size : {2000u, 8192u})
    for (uint32_t start_x : {0u, 50u, 200u, map_size - 1600u})
    for (int pointer_x : {0, 400, 1200, 1599})
    for (DWORD step : {1u, 16u, 60u, 250u})
    for (int direction : {-120, 120})
    {
        Reset();
        uint32_t x = start_x, y = 50;
        Settle(15000, x, y);
        world_zoom::UpdateTransform(x, y, map_size, map_size);
        const auto source = world_zoom::PresentedToSource(pointer_x, 200);
        const int64_t world_x = x + source.x;
        world_zoom::AdjustByWheel(direction, pointer_x, 200, x, y);
        for (DWORD elapsed = step; elapsed <= 180 + step; elapsed += step)
        {
            audit_tick = 1000 + elapsed;
            world_zoom::UpdateTransform(x, y, map_size, map_size);
            uint32_t ax = x, ay = y;
            if (world_zoom::ResolveCameraAnchor(x, y, map_size, map_size, &ax, &ay))
            {
                x = ax; y = ay;
                world_zoom::UpdateTransform(x, y, map_size, map_size, false);
            }
            transitions_ok &= x <= map_size - 1600 && y <= map_size - 820;
        }
        const unsigned sw = world_zoom::transform.source_width;
        const int64_t sample = world_zoom::SampleOffset(pointer_x, sw, 1600);
        const int64_t maximum = map_size - sw + sample;
        const int64_t expected = std::max(sample, std::min(world_x, maximum));
        const auto actual = world_zoom::PresentedToSource(pointer_x, 200);
        transitions_ok &= std::abs(static_cast<int64_t>(x) + actual.x - expected) <= 1;
        ++transitions;
    }
    std::printf("transition cases=%u\n", transitions);
    Check(transitions_ok, "edge/overlapping-edge handoffs across frame delays and wheel directions");

    bool expanded_anchors_ok = true;
    for (const auto &size : {std::pair{1280u,720u}, {1920u,1080u}, {3840u,2160u}})
    for (bool smooth : {false,true})
    for (int direction : {-120,120})
    for (const auto &pointer : {std::pair{640,400}, {int(size.first*3/4),int(size.second*3/4)},
                               {int(size.first-1),int(size.second-1)}})
    {
        Reset(size.first,size.second);
        world_zoom::settings.smooth=smooth;
        uint32_t x=4000,y=4000;
        Settle(15000,x,y);
        world_zoom::UpdateTransform(x,y,16384,16384);
        auto source=world_zoom::PresentedToSource(pointer.first,pointer.second);
        const int64_t wx=x+source.x,wy=y+source.y;
        world_zoom::AdjustByWheel(direction,pointer.first,pointer.second,x,y);
        expanded_anchors_ok &= world_zoom::anchor.presented_x==pointer.first &&
                               world_zoom::anchor.presented_y==pointer.second;
        for(DWORD elapsed=0;elapsed<=160;elapsed+=16)
        {
            audit_tick=1000+elapsed;
            world_zoom::UpdateTransform(x,y,16384,16384);
            uint32_t ax=x,ay=y;
            if(world_zoom::ResolveCameraAnchor(x,y,16384,16384,&ax,&ay))
            {
                x=ax;y=ay;world_zoom::UpdateTransform(x,y,16384,16384,false);
            }
            source=world_zoom::PresentedToSource(pointer.first,pointer.second);
            expanded_anchors_ok &= std::abs(int64_t(x)+source.x-wx)<=1 &&
                                   std::abs(int64_t(y)+source.y-wy)<=1;
        }
    }
    Check(expanded_anchors_ok,"full-viewport logical pointer anchors in both axes, instant/smooth, in/out");

    bool extended_range_ok = true;
    for (unsigned width : {640u,1280u,1920u,3840u})
    for (unsigned extra : {0u,50u,100u,25u,200u,0xffffffffu})
    for (bool close_start : {false,true})
    for (bool smooth : {false,true})
    {
        Reset(width,width==640?480:width*9/16);
        const unsigned start=close_start?std::max(20000u,width*10000/640):10000u;
        world_zoom::settings.start_zoom_units=start;
        world_zoom::settings.extra_zoom_percent=extra;
        world_zoom::settings.smooth=smooth;
        const unsigned expected=std::max(20000u,start)*(100+(extra==50||extra==100?extra:0))/100;
        extended_range_ok &= world_zoom::MaximumZoomUnits()==expected;
        world_zoom::pixel_levels=pixel_zoom_steps::Build(width,resolution::screen_height,
            width*2,resolution::screen_height*2,expected);
        for(auto stop:world_zoom::pixel_levels)extended_range_ok &= stop<=expected;
        world_zoom::BeginMatch();
        world_zoom::UpdateTransform(4000,4000,16384,16384);
        extended_range_ok &= world_zoom::current_zoom_units==start;
        const int px=width*3/4,py=resolution::screen_height*3/4;
        const auto before=world_zoom::PresentedToSource(px,py);
        world_zoom::AdjustByWheel(120*128,px,py,4000,4000);
        extended_range_ok &= world_zoom::target_zoom_units==expected;
        uint32_t x=4000,y=4000;
        audit_tick+=160;
        world_zoom::UpdateTransform(x,y,16384,16384);
        uint32_t ax=x,ay=y;
        if(world_zoom::ResolveCameraAnchor(x,y,16384,16384,&ax,&ay))
        { x=ax;y=ay;world_zoom::UpdateTransform(x,y,16384,16384,false); }
        const auto after=world_zoom::PresentedToSource(px,py);
        extended_range_ok &= std::abs(int64_t(x)+after.x-4000-before.x)<=1 &&
                             std::abs(int64_t(y)+after.y-4000-before.y)<=1;
        extended_range_ok &= world_zoom::current_zoom_units==expected &&
            world_zoom::transform.source_width==width*10000/expected &&
            world_zoom::transform.source_height==resolution::screen_height*10000/expected;
        world_zoom::AdjustByWheel(-120*128,px,py,x,y);
        extended_range_ok &= world_zoom::target_zoom_units==10000;
        world_zoom::BeginMatch();
        extended_range_ok &= world_zoom::current_zoom_units==start;
    }
    Check(extended_range_ok,"optional +50/+100 zoom cap, exact stops, anchor, reset and reverse through 1200%");

    Reset();
    world_zoom::settings.enabled = false;
    auto identity = world_zoom::PresentedToSource(1200, 200);
    Check(!world_zoom::AdjustByWheel(120, 1200, 200, 0, 0) &&
          identity.x == 1200 && identity.y == 200,
          "disabled zoom retains identity and does not consume wheel input");
    Reset();
    world_zoom::settings.start_zoom_units = 25000;
    world_zoom::settings.smooth = false;
    world_zoom::BeginMatch();
    world_zoom::UpdateTransform(2000, 2000, 8192, 8192);
    Check(world_zoom::VisibleWidth() == 640 && world_zoom::transform.source_height == 360,
          "configured start is applied immediately at match entry");
    world_zoom::AdjustByWheel(-120, 800, 400, 2000, 2000);
    world_zoom::UpdateTransform(2000, 2000, 8192, 8192);
    Check(world_zoom::current_zoom_units == 23750,
          "instant mode reaches target on next draw without interpolation");
    world_zoom::BeginMatch();
    Check(world_zoom::current_zoom_units == 25000 && !world_zoom::anchor.active,
          "next match restores configured starting zoom");
    world_zoom::settings.smooth = true;
    world_zoom::UpdateTransform(2000, 2000, 8192, 8192);
    world_zoom::AdjustByWheel(-120, 800, 400, 2000, 2000);
    audit_tick += 90;
    world_zoom::UpdateTransform(2000, 2000, 8192, 8192);
    Check(world_zoom::current_zoom_units > 23750 && world_zoom::current_zoom_units < 25000,
          "smooth mode retains the interpolated transition");
    Reset(1365, 769);
    world_zoom::settings.start_zoom_units = 21328;
    world_zoom::BeginMatch();
    world_zoom::AdjustByWheel(-120, 600, 200, 0, 0);
    Check(world_zoom::target_zoom_units == 21250,
          "non-grid starting zoom steps to its nearest neighbor");
    auto crisp = pixel_zoom_steps::Build(1920,1080,3840,2160,20000);
    Check(crisp == std::vector<unsigned>{10000,15000,20000}, "1080p to 4K exact 2x/3x/4x stops");
    Check(pixel_zoom_steps::Build(1920,1080,3840,2160,30000) ==
        std::vector<unsigned>{10000,15000,20000,25000,30000}, "closer starting view extends exact stops through 6x");
    bool exact=true, complete=true;
    for (auto internal : {std::pair{1920u,1080u}, {1280u,720u}, {1600u,900u}, {1280u,960u}, {1919u,1079u}})
    for (auto output : {std::pair{3840u,2160u}, {2560u,1440u}, {1920u,1080u}, {1365u,767u}, {2560u,2048u}})
    for (bool fit : {false,true})
    {
        unsigned ow=output.first, oh=output.second;
        pixel_zoom_steps::Fit(internal.first,internal.second,ow,oh,fit);
        auto levels=pixel_zoom_steps::Build(internal.first,internal.second,ow,oh,30000);
        for (auto z:levels)
        {
            if(z==10000) continue; // Full viewport escape is explicitly not guaranteed.
            const unsigned cw=internal.first*10000/z, ch=internal.second*10000/z;
            exact &= !(ow%cw) && !(oh%ch) && ow/cw==oh/ch;
        }
        for(unsigned z=10001;z<=30000;++z)
        {
            const unsigned cw=internal.first*10000/z, ch=internal.second*10000/z;
            if(ow%cw || oh%ch || ow/cw!=oh/ch) continue;
            bool found=false;
            for(auto stop:levels) found |= internal.first*10000/stop==cw && internal.second*10000/stop==ch;
            complete &= found;
        }
    }
    Check(exact && complete,"all exact crops, no fractional stops across 50 output/aspect configurations");
    Reset(1920,1080);
    world_zoom::settings.smooth=false;
    world_zoom::pixel_levels=pixel_zoom_steps::Build(1920,1080,3200,1800,20000);
    world_zoom::BeginMatch();world_zoom::UpdateTransform(2000,2000,8192,8192);
    std::vector<unsigned> visited{world_zoom::target_zoom_units};
    for(int i=0;i<10;++i)
    {
        world_zoom::AdjustByWheel(120,960,400,2000,2000);
        if(visited.back()!=world_zoom::target_zoom_units) visited.push_back(world_zoom::target_zoom_units);
    }
    Check(visited==std::vector<unsigned>{10000,11250,12000,12500,13750,15000,16250,17500,18750,20000},
        "regular steps automatically include off-grid exact 120 percent stop");
    bool reverse=true;
    for(auto it=visited.rbegin()+1;it!=visited.rend();++it)
    {
        world_zoom::AdjustByWheel(-120,960,400,2000,2000);
        reverse &= world_zoom::target_zoom_units==*it;
    }
    Check(reverse,"merged regular/exact sequence reverses without skips or duplicates");
    world_zoom::settings.start_zoom_units=13750;world_zoom::BeginMatch();
    Check(world_zoom::current_zoom_units==13750,"regular mode keeps its chosen non-perfect starting view");
    world_zoom::pixel_levels=pixel_zoom_steps::Build(1920,1080,3840,2160,20000);
    world_zoom::AdjustByWheel(120,960,400,2000,2000);
    world_zoom::AdjustByWheel(120,960,400,2000,2000);
    Check(world_zoom::target_zoom_units==16250,"existing regular exact stop is not duplicated");
    Reset(1920,1080);
    Settle(15000);
    world_zoom::AdjustByWheel(120,960,400,2000,2000);
    unsigned previous=15000;bool intermediate=true;
    for(DWORD elapsed: {20u,40u,60u,80u,100u})
    {
        audit_tick=1000+elapsed;
        world_zoom::UpdateTransform(2000,2000,8192,8192);
        intermediate &= world_zoom::current_zoom_units>previous && world_zoom::current_zoom_units<16250;
        previous=world_zoom::current_zoom_units;
    }
    Check(intermediate,"faster smooth zoom preserves advancing intermediate frames");
    audit_tick=1120;world_zoom::UpdateTransform(2000,2000,8192,8192);
    Check(world_zoom::current_zoom_units==16250,"smooth zoom completes at 120ms instead of 180ms");
    Reset(1920,1080);Settle(15000);
    world_zoom::AdjustByWheel(120,960,400,2000,2000);
    audit_tick=1016;world_zoom::UpdateTransform(2000,2000,8192,8192);
    std::printf("Zoom response after 16ms: %u/1250 units\n",world_zoom::current_zoom_units-15000);
    Check(world_zoom::current_zoom_units>=15300,"first rendered zoom frame has no slow-start ramp");
    std::printf("Audit completed: %d failed contracts\n", failures);
    return failures ? 1 : 0;
}
