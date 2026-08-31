# Building-placement subsystem

This case study records both the working design and the diagnostic mistake that
revealed why structure semantics must be proven before modification.

## Active flow

1. GPTP's replacement `init_placement_box_pos` reads mouse globals
   `0x6CDDC4/0x6CDDC8`.
2. It subtracts half the selected building dimensions.
3. It combines the screen point with camera globals and converts world pixels
   to map tiles.
4. It writes placement tile x/y at `0x640890/0x640892`.
5. StarCraft's placement setup prepares layer 3 and/or 4 positions and surface
   records.
6. The multipass renderer translates each active placement layer's `left/top`
   into private-camera coordinates and draws it with the native layer routine
   `0x0048D5C0`.
7. On a placement click, `input_place_building` (`0x0048E5D0`) consumes both
   the dialog event point and, through `0x0048DDC0`, the global mouse point.
   Invisible-native-HUD bypasses must temporarily correct both to the physical
   expanded point before validation and command construction.

## GPTP 4:3 gate

The installed stable GPTP function is at module RVA `0x87BF0`. Its original
instructions compare mouse x and y against inclusive maxima 639 and 399. That
bypasses the equivalent StarCraft patch because GPTP replaced the function.

`EnsureGptpPlacementBounds` verifies instruction signatures referencing the
mouse globals, then changes only the two immediate maxima to
`game_width - 1` and `game_height - 1`. For 1280x720 output, these are 1279
and 639. Unknown GPTP builds are refused.

## Placement structures

Layers 3 and 4 begin at draw-layer table entries based at `0x6CEF50`.

Their `func_param` pointers refer to:

- layer 3: `0x64095C`;
- layer 4: `0x640964`.

Each is an 8-byte native Surface record:

```text
+0x00  u16 width
+0x02  u16 height
+0x04  u8* pixels
```

The placement draw position is in the layer structure's `left/top` fields.
The following two fields are width/height even though aidebug's generic Rect
type names them `right/bottom`.

## Failure and correction

An early pass translation treated `0x64095C/64` as position records. Applying
camera deltas to the first two words therefore changed the ghost surface's
dimensions. For example, a valid 128x128 surface could become negative or
truncated in non-first passes. The observed result was a ghost visible only in
part of the original 4:3 area.

Disassembly of `0x0048D700`, `0x0048D7F0`, and `0x0048DAxx`, plus live layer
captures, proved the true layout. The corrected renderer leaves the surface
record immutable and translates only layer `left/top`. The user confirmed the
ghost then worked across the expanded view.

### Green ghost but rejected bottom-left click

The obsolete native HUD still occupies x=0..639, y=400..479 for dialog input.
Clicks on opaque pixels in that hidden copy are routed through an expanded-only
decoy so invisible controls cannot consume them. The general gameplay wrapper
initially corrected only event fields `+0x0E/+0x10`. Disassembly showed that
`input_place_building` reads those fields for `IsOutsideGameScreen`, but then
calls `0x0048DDC0`, which independently recalculates the placement tile from
mouse globals `0x006CDDC4/0x006CDDC8`. The preview could therefore remain green
at the physical point while command validation used the decoy x coordinate.

For the placement callback only, the wrapper temporarily exposes the same
corrected physical point through the event and both mouse globals, then restores
the caller's temporary globals. This keeps preview, validity, and command tile
ownership synchronized without weakening invisible-HUD filtering.

## Required regression tests

- Move the ghost across x=639/640 and throughout the right half.
- Move across each vertical pass boundary and near y=639.
- Test small and large building footprints.
- Test valid and invalid placement coloring.
- Test an addon and refinery-like placement if available.
- Click to place in every quadrant and confirm the command uses the visible
  tile.
- Scroll the camera while the ghost is active.
- Confirm surface dimensions stay constant in
  `fixed_zoom_placement_passes.txt`.
