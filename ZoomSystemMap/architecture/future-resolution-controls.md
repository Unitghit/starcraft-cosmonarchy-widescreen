# Future GUI: independent resolution controls

## Goal

A future configuration GUI must expose the game's **internal logical
resolution** and its **external presentation size** as separate settings. A
change to one must never silently rewrite the other.

The relationship is:

```text
logical framebuffer -> renderer/compositor -> presentation scaler -> window/display
     1280x720                                     2.5x            3200x1800
```

Changing the logical framebuffer changes how much game world and UI layout the
engine renders. Changing presentation only magnifies or fits the completed
image; it must not change camera coverage, HUD geometry, hit testing, or other
logical coordinates.

## GUI control contract

### Internal resolution

Independent inputs:

- logical width;
- logical height.

Current owner: `ZoomSource\zoom_resolution.h`.

An internal-resolution change must regenerate every derived geometry value,
build the renderer, run the resolution matrix and regression checks, install
the new renderer, and restart the game. The GUI must report this as a rebuild
operation until the renderer supports runtime logical-resolution changes.

### External presentation

Independent inputs:

- presentation mode: rational scale, fit-to-display, or explicit output size;
- rational scale when that mode is selected;
- optional output width and height for explicit or fit modes;
- fullscreen/windowed preference and scaling filter.

Current owners:

- `Release\cosmonarchy_viewport.ini` for runtime output size;
- `ViewportConfigurator` for derived cnc-ddraw settings;
- `Starcraft\ddraw.ini` for the runtime presentation target.

A presentation-only change must preserve the logical width and height. For the
scale mode:

```text
output_width  = logical_width  * scale_numerator / scale_denominator
output_height = logical_height * scale_numerator / scale_denominator
```

It must also preserve physical-to-logical mouse mapping and apply the wrapper
configuration only after all previous game processes have exited.

## Required separation

The future GUI backend should maintain two explicit configuration objects:

```text
LogicalResolution { width, height }
Presentation      { mode, scale, output_width, output_height, filter,
                    fullscreen, windowed }
```

Rules:

1. Logical values are the only inputs to renderer geometry and gameplay/UI
   coordinate derivation.
2. Presentation values are the only inputs to final window/display sizing and
   raster filtering.
3. Output dimensions are derived from both objects only when the selected
   presentation mode requires that calculation.
4. Mouse coordinates cross from physical to logical space exactly once, in the
   presentation wrapper.
5. The GUI must show logical and external resolutions simultaneously so the
   user can see which layer is being changed.
6. Each Apply action must verify the resulting configuration before launching
   the game and must provide a recoverable last-known-good setting.

## Recommended GUI behavior

- Internal resolution panel: width, height, aspect-ratio presets, and a clear
  **Rebuild required** status.
- Presentation panel: rational presets plus fit, exact output, fullscreen, and
  filter controls, with **Logical resolution unchanged** shown explicitly.
- Read-only summary: `1280x720 internal -> 2.5x -> 3200x1800 external`.
- Validation should reject non-positive sizes, outputs larger than the selected
  display unless explicitly allowed, and modes whose required fields are
  missing.
- Applying presentation settings should use the isolated presentation
  synchronizer rather than editing renderer geometry.

## Current status

The ownership boundary and portable GUI are implemented. Internal profiles are
selected independently from external presentation. The initial manifest lists
only the validated 1280x720 renderer; presentation is runtime-configurable and
confirmed at 2.5x windowed and 3x borderless fullscreen.
