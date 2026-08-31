# Draw layers and structures

## Native layer table

The table begins at `0x006CEF50` and contains eight packed 20-byte records.

```text
+0x00  u8   draw / buffers enabled
+0x01  u8   refresh and unknown flags
+0x02  s16  left
+0x04  s16  top
+0x06  u16  width
+0x08  u16  height
+0x0A  u16  alignment / unknown
+0x0C  void* function parameter (often Surface*)
+0x10  function pointer
```

Important: aidebug currently overlays bytes `+0x02..+0x09` with a generic
`Rect<int16_t>` named `area`. Its `right` and `bottom` members correspond to
native layer **width** and **height**, not absolute coordinates.

## Observed layer roles

| Index | Operational role | Typical function / parameter |
|---:|---|---|
| 0 | Cursor | `0x004BDFA0` |
| 1 | Context-help surface (160x480 backing surface plus smaller live bounds) | `0x004810F0`, param `0x00655C40` |
| 2 | Dialog and native UI | `0x0041CB50` |
| 3 | Placement surface A | `0x0048D5C0`, Surface `0x64095C` |
| 4 | Placement surface B | `0x0048D5C0`, Surface `0x640964` |
| 5 | Game world | GPTP wrapper at an ASLR-dependent address |
| 6 | Usually inactive in observed states | Unknown |
| 7 | Usually inactive in observed states | Unknown |

The outer native frame retains GPTP's layer-5 wrapper. Private world passes
temporarily call raw StarCraft world rendering and then explicitly request
only GPTP coordinate mode `1` graphics through the stable build's validated
internal draw routine. This preserves rally lines without stamping
screen-space stat-res graphics into every tile. The stable GPTP QDP does not
export `DrawExpandedMapGraphics`.

Layer 1 is not the drag-selection rectangle. Its parameter at `0x00655C40`
is a **confirmed 160x480 Surface**; the prepared tooltip itself occupies the
smaller live left/top/width/height in the layer-table record at `0x006CEF64`.
Its visibility state is at `0x00655C48`. Tooltip setup at
`0x004813D0..0x0048147B` updates those bounds. The expanded renderer suppresses
this screen-space layer from native/private passes and draws it once on the
finished expanded frame.

Live frame-signature capture on 2026-08-30 showed stable building/status help
remaining visible, while multiselection help alternated visible/hidden every
frame with the same `160x480` surface hash and the same `145x35` live bounds.
The expanded renderer therefore retains the last complete backing surface and
live bounds while the status-control envelope still owns context help. The
previous inferred `160x92` cache limit rejected every native surface and was
the direct cause of the visible flicker.

## Surface

The native packed Surface used by this project is:

```text
+0x00  u16 width
+0x02  u16 height
+0x04  u8* pixels
```

This layout applies to the placement records at `0x64095C/64` and the canvas
structures used by draw routines.

## Draw parameters

The placement draw function at `0x0048D5C0` receives a layer Surface and a
native draw parameter/clip. It clips negative `left/top` values and uses fixed
640x400 native extents. That is correct inside each private pass.

## Mutation rules

- Translate `left/top` for camera-relative screen positioning.
- Preserve width, height, alignment, pointer, and function unless their exact
  owner and all consumers are audited.
- Save and restore any temporarily changed layer state inside the same pass.
- Force refresh flags when a private camera needs a complete redraw.
- Private passes may temporarily use raw layer 5 only when GPTP `ON_MAP`
  graphics are replayed explicitly afterward and the installed pointer is
  restored before leaving the pass.

## Diagnostic representation

Layer captures should log both semantic names and raw bytes when investigating
an unknown entry. Placement captures now report:

- enabled state;
- base `left/top`;
- calculated pass-local `left/top`;
- native width/height;
- Surface width/height;
- mouse point and placement tile;
- per-pass hash and nonzero count.
