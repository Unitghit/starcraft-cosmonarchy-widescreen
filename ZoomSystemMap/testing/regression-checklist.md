# Regression checklist

## Guarded world composition

- Run `ZoomIntegration/verify_panning_native.ps1` for production crop geometry
  and synthetic fresh-frame tests, plus the normal resolution verifier.
- Live-test 1080p first: slow horizontal/vertical/diagonal middle pan, stop
  while held, release, and move into every map edge and corner.
- Check disappearing units/effects across tile joins, rally lines, ghosts,
  HUD/top text, and fractional zoom. Compare frame pacing against the backup.
- Repeat at 720p, 1600x900, an uneven custom size, and 4K when practical.
- Offline tests are not evidence of real-engine raster or timing correctness.

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

## Optional gameplay zoom

- [ ] First wheel response has no slow-start hesitation, both inward and out.
      Repeated/reversed wheel remains smooth with the same 120ms completion.
- [ ] `verify_single_stage.ps1` confirms zero warmed world-buffer growths and
      unchanged packed crop pixels. Test first activation from 100% separately:
      GPU initialization still occurs on first use in that configuration.

- [ ] No zoom-step selector or saved steps key remains. Existing old step
      settings do not restrict zoom to exact-only stops.
- [ ] At 1080p internal / 4K output, 100%, 150%, 200% remain among the stops.
      Test wheel reversal, minimap, pointer anchoring, HUD, and starting view.
- [ ] Regular includes 12.5% steps plus exact stops. At 1080p internal to
      3200x1800 output it includes 120%, with no duplicates in either direction.
      Smooth still interpolates between exact
      stops; Instant skips intermediate frames. Change output and restart to
      verify the list is recalculated. Full viewport remains an escape stop.
- [ ] Smooth reaches each target in 120ms while still drawing intermediate
      frames. Instant and repeated/reversed wheel behavior remain correct.

- [ ] For single-stage changes, run `ZoomIntegration/verify_single_stage.ps1`
      on Windows and its executable under Wine. Use `--core` for GL core and
      `--gl-only` with Mesa GL 2.1/GLSL 120 overrides for the legacy path.
- [ ] At internal 1920x1080, starting view 1280x720, output 3840x2160,
      Pixel-perfect yields uniform 3x3 world pixels with unchanged HUD.
- [ ] Fresh/missing backend selects Pixel-perfect; explicitly saved Standard
      stays Standard. Disabled zoom installs no optional rendering hooks.
- [ ] Verify all race HUDs, tooltips, cursor, minimap, palette changes, and
      popup fallback. Return from a popup restores single-stage rendering.
- [ ] Compare frame pacing; the extra UI probes must not introduce stutter.
- [ ] Repeat on cnc-ddraw 6.9/7.0/7.1. Offline adapter tests are not equivalent
      to game integration tests, and Wine is not a substitute for a Mac test.

- [ ] Existing configuration with no `[world_zoom]` section is pixel and
      input equivalent to the 100 percent build.
- [ ] The GUI defaults to `Off (100%)` and reloads every saved zoom level.
- [ ] At every 12.5 percent target from 100 through 200 percent, the world
      scales while HUD, minimap, top text, popup, tooltips, and cursor retain
      their normal dimensions.
- [ ] Wheel up steps toward 200 percent and wheel down steps toward 100
      percent without restarting the match.
- [ ] Every wheel step eases monotonically to its target without a first-frame
      jump, final-frame gap, or input/render disagreement.
- [ ] Fractional targets distribute repeated rows and columns symmetrically,
      with no left-edge or top-edge sampling bias.
- [ ] Wheel input over HUD, minimap, popup, and front-end menus is not consumed
      by gameplay zoom.
- [ ] Unit hover, selection, right-click orders, and building placement align
      with the displayed world in the center and all four quadrants.
- [ ] Drag selection renders once at the physical drag bounds.
- [ ] Edge scrolling reaches all four map boundaries, and terrain at the map
      edges remains visible rather than being cropped away.
- [ ] Minimap click and held drag retain their current behavior, and the white
      camera outline reflects the zoomed visible world extent.
- [ ] After cursor-centered zooming away from the starting camera position,
      the white outline begins at the actual cropped-world origin.
- [ ] Middle-button panning remains smooth and keeps the cursor aligned.
- [ ] World pixels in both lower side gutters use the same zoom transform as
      the battlefield above the HUD.
- [ ] Opening a translucent popup does not resize the HUD or screen-space UI.

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
- [ ] Hovering the HUD or minimap keeps the normal UI cursor even when a unit
      or building is visible underneath that screen position.
- [ ] Leaving the HUD or minimap immediately restores unit, building,
      placement, and command cursor states over the battlefield.

## Input

- [ ] Cursor reaches x=`screen_width-1` and y=`screen_height-1`.
- [ ] Edge scrolling works on all battlefield edges.
- [ ] Single-click selection works in every battlefield quadrant.
- [ ] Drag selection starts, crosses x=639 and y=399, and completes correctly.
- [ ] Modifier-clicking a unit outside the original 640x400 area selects all
      visible units of the same type.
- [ ] A selection drag that starts on the battlefield still completes when
      the left button is released over the bottom HUD.
- [ ] Right-click commands use the visible destination in every quadrant.
- [ ] Large unit selections do not activate invisible native controls.
- [ ] Minimap and command-card controls work only at their presented location.
- [ ] Ordinary HUD buttons still highlight, show help, and activate normally.
- [ ] Move the camera away from a selected unit/building, click its bottom-HUD
      portrait, and confirm the sprite lands at the configured battlefield
      center rather than the legacy 640x400 center.
- [ ] Repeat portrait centering beside all four map edges and confirm native
      eight-pixel alignment and edge clamping remain stable.
- [ ] Clicking the presented minimap moves the camera without moving the
      physical cursor into the obsolete 4:3 HUD.
- [ ] Holding left mouse and dragging across the minimap continuously moves the
      camera while the cursor stays beneath the pointer.
- [ ] Repeat minimap click, held drag, and release tests with official
      cnc-ddraw 6.9, 7.0, and 7.1. Confirm import discovery succeeds for each.
- [ ] The configurator reports 6.9, 7.0, and 7.1 as supported while leaving
      `ddraw.dll` byte-for-byte unchanged through Save and Restore.
- [ ] An unknown `ddraw.dll` produces an unverified warning without blocking
      settings or replacing the DLL.
- [ ] Repeat the held minimap drag under Wine. Confirm the pointer can traverse
      the complete relocated minimap and does not jump toward the obsolete
      native minimap when the button is released.
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

## Optional high-refresh cursor and selection

- [ ] Enable the checkbox with Pixel-perfect rendering, then restart.
- [ ] In gameplay and paused game menus, movement follows the display cadence
      while cursor animations retain their normal speed.
- [ ] Drag selection in all directions at base zoom and enlarged zoom; the
      anchor stays fixed and release selects the intended units, including over HUD.
- [ ] HUD, tooltips, minimap taps/held drags and middle-button panning retain
      their normal cursor type, visibility and interaction.
- [ ] Check window offsets, aspect-fit borders, alt-tab and return to focus.
- [ ] Disabling the option restores normal pacing; Restore original restores
      unchanged owned minfps/maxfps values without overwriting later user edits.
- [ ] Front-end menus and unsupported software presentation retain the old path.

## Stability and recovery

- [ ] `ZoomIntegration/verify_world_zoom_native.ps1` passes before packaging.
- [ ] Rapid wheel reversal retains the world point under the pointer.
- [ ] Zoom handoff near map edges remains stable after a delayed frame.
- [ ] Zooming back to 100% restores stationary hover without mouse movement.
- [ ] A zoomed minimap tap and held drag center the clicked landmark.
- [ ] Zoomed popup transparency matches the actual background without
      duplicate sprites or placement ghosts.
- [ ] Repeat zoom and minimap checks on cnc-ddraw 6.9/7.0/7.1 at 1x and
      scaled presentation, including an offset window.

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
