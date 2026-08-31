# Regression checklist

Run the smallest applicable subset during iteration and the full checklist
before declaring a resolution build stable.

## Startup and menus

- [ ] Launcher reaches the Cosmonarchy menu without a data-file error.
- [ ] Starting a match passes the extended-upgrade research clear without an
      access violation at `0x004CCEC7`.
- [ ] A local-player CHK start location appears at the configured expanded
      camera center rather than the legacy 4:3 center.
- [ ] A later trigger `Center View` action also uses the expanded center.
- [ ] Center View near each map edge still clamps without wrapping or jumping.
- [ ] Menu artwork and text render.
- [ ] Front-end menus fill the derived centered 4:3 fit and buttons remain
      aligned with hover/click input at all four menu edges.
- [ ] Window client is exactly the configured output size.
- [ ] Presentation log records an absolute config path and the selected scale,
      requested output, and actual client agree, especially at 1x.
- [ ] Opening and closing the in-game menu does not move the HUD.
- [ ] Popup is centered and its buttons are clickable.
- [ ] Popup transparency shows the terrain and units beneath its centered
      presented rectangle, not imagery from the obsolete 4:3 source location.
- [ ] No obsolete invisible popup/HUD controls respond in battlefield space.

## World rendering

- [ ] Terrain covers the entire battlefield with no black tile gaps.
- [ ] Units, bullets, shadows, overlays, and selection circles render in every
      horizontal and vertical pass.
- [ ] Sprites crossing each tile seam remain continuous.
- [ ] Camera works at all four map edges and corners.
- [ ] Bottom map edge does not expose the native HUD-shaped terrain mask.
- [ ] Rally lines and other GPTP map graphics render in every pass.
- [ ] Sustained battle does not crash or progressively corrupt rendering.

## HUD and screen-space UI

- [ ] HUD is centered horizontally and flush with the bottom.
- [ ] Portrait, command card, minimap, resources, and status text are intact.
- [ ] Protoss and Zerg upper minimap/corner artwork joins the bottom HUD;
      Terran's rusty x=0..22 corner ornament remains discarded.
- [ ] Top game text appears once without flicker.
- [ ] Objectives and resources remain flush with the top and occupy opposite
      sides of the selected top-UI layout (`centered_native_box` or
      `screen_edges`).
- [ ] The configurator defaults to `centered_4_3`, saves either top-UI layout,
      and reloads the saved selection without affecting internal resolution or
      external presentation.
- [ ] Drag-selection rectangle appears once at the physical cursor bounds.
- [ ] Cursor graphic appears once and remains aligned over HUD and battlefield.

## Input

- [ ] Cursor reaches x=`screen_width-1` and y=`screen_height-1`.
- [ ] Edge scrolling works on all battlefield edges.
- [ ] Single-click selection works in every battlefield quadrant.
- [ ] Drag selection starts, crosses x=639 and y=399, and completes correctly.
- [ ] Right-click commands use the visible destination in every quadrant.
- [ ] Large unit selections do not activate invisible native controls.
- [ ] Minimap and command-card controls work only at their presented location.
- [ ] Move the camera away from a selected unit/building, click its bottom-HUD
      portrait, and confirm the sprite lands at the configured battlefield
      center rather than the legacy 640x400 center.
- [ ] Repeat portrait centering beside all four map edges and confirm native
      eight-pixel alignment and edge clamping remain stable.
- [ ] Clicking the presented minimap moves the camera without moving the
      physical cursor into the obsolete 4:3 HUD.
- [ ] Holding left mouse and dragging across the minimap continuously moves the
      camera while the cursor stays beneath the pointer.
- [ ] The minimap's white camera outline represents the full internal
      battlefield viewport and has the correct aspect at map edges.
- [ ] Starting a match on a differently sized map recalculates the minimap
      camera outline without retaining the previous map's dimensions.
- [ ] Transparent HUD gaps continue to accept battlefield clicks.

## Building placement

- [ ] Ghost remains visible across x=639/640.
- [ ] Ghost remains visible across every vertical tile boundary.
- [ ] Ghost reaches the right and lower battlefield edges.
- [ ] Ghost footprint and valid/invalid coloring are not stretched or clipped.
- [ ] Placement click creates the building at the visible map tile.
- [ ] Camera scrolling while placing keeps the ghost aligned.
- [ ] Surface dimensions remain constant across all private-pass diagnostics.

## Positional audio

- [ ] Equivalent combat sounds are audible at the left, center, right, and
      lower edges of the expanded battlefield.
- [ ] Stereo direction crosses smoothly through the physical battlefield
      center rather than the legacy x=320 center.
- [ ] Sounds beyond the expanded viewport still attenuate instead of becoming
      global.
- [ ] A sustained or looping positional sound updates correctly while the
      camera scrolls.

## Stability and recovery

- [ ] `verify_fixed_zoom.py` passes.
- [ ] Runtime log reports all patch preflights successful.
- [ ] Runtime log reports `GPTP upgrade research clear stabilized` before the
      match begins.
- [ ] Installed renderer hash matches its build artifact.
- [ ] Installed GPTP hash matches the known-good backup.
- [ ] No new Windows application error is present after the run.
- [ ] Exiting and relaunching works twice consecutively.

Record the map, game mode, resolution, and any unchecked item when reporting a
test result.
