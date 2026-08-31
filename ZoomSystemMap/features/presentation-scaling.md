# Presentation-only rational scaling

## Symptom and success condition

The logical renderer is correct at 1280x720. This feature proved a 2.5x
physical magnification without changing world coverage, HUD geometry, buffers,
patch constants, or logical input coordinates. The configurator now defaults
to 1x while retaining 2.5x as an available presentation choice.

Success means the same 1280x720 image is presented at 3200x1800 with nearest-
neighbor scaling and correct mouse mapping.

## Ownership map

| Stage | Owner | Dimensions |
|---|---|---:|
| World, UI, input, compositor | aidebug/StarCraft/GPTP | 1280x720 logical |
| Runtime presentation setting | `cosmonarchy_viewport.ini` | 5/2, 3200x1800 |
| Compile-time fallback | `zoom_presentation.h` | 5/2 |
| Client-window enforcement | `presentation.cpp` | 3200x1800 |
| Raster scaling and mouse mapping | cnc-ddraw | physical <-> logical |

## Invariants

Must remain logical 1280x720:

- StarCraft physical-resolution operands;
- expanded framebuffer allocation and pitch;
- battlefield, HUD, popup, placement, cursor, and tooltip coordinates;
- selection and input bounds.

Presentation-only:

- target client width and height;
- nearest-neighbor filter;
- wrapper physical mouse scale.

## Implementation

`presentation.cpp` owns the only renderer-side window-size code. It reads an
explicit output size and exact numerator/denominator from
`cosmonarchy_viewport.ini`, rejects a config whose internal dimensions do not
match the compiled profile, and retains `zoom_presentation.h` only as a safe
fallback. Existing initialization and window-message paths each call one public
function from that module. `ddraw.ini` requests 3200x1800, aspect preservation, nearest-neighbor
raster, and automatic mouse adjustment. Integer boxing must remain disabled for
fractional scales: enabling it reduces 2.5x to a centered 2x raster and creates
black borders. No input division exists in aidebug because cnc-ddraw owns that
mapping.

The renderer resolves `cosmonarchy_viewport.ini` to an absolute path before
calling the Win32 profile APIs. Those APIs otherwise reinterpret a relative
filename against the Windows directory and silently return compiled fallback
values, causing the renderer to fight a correctly configured cnc-ddraw window.

The portable configurator writes the runtime config and owned cnc-ddraw keys
only after shutdown, then verifies the embedded renderer and stable GPTP hashes.
The developer synchronizer retains the compile-time fallback path. Shutdown
ordering is required: an older running cnc-ddraw instance previously restored
its saved 960x540 dimensions over a live configuration edit.

## Verification

- [x] x86 Release build succeeds.
- [x] Runtime client is 3200x1800.
- [x] Renderer log still reports logical output 1280x720 and pitch 1280.
- [x] User visually confirmed 2x presentation in-game.
- [x] User visually confirmed the active 2.5x presentation.
- [ ] Mouse reaches every logical edge and clicks map/HUD correctly.
- [ ] Tooltip, drag, placement, rally, and cursor regressions pass.
- [x] Stable GPTP hash remains unchanged.
