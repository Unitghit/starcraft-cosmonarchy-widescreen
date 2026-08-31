# Legacy HUD tooltip duplicates

## Symptom and success condition

The bottom-centered HUD already produces correctly positioned command-card
tooltips. The still-live native dialog tree also occupies its original 640x480
coordinates, so hovering an invisible old command button in battlefield space
can produce the same tooltip a second time.

Success means command-card tooltips respond at the presented HUD location and
never at the obsolete native location. Gameplay cursor state and world input
must continue using the real expanded coordinate.

## Ownership map

| Stage | Owner | Function / state | Coordinate space |
|---|---|---|---|
| Physical mouse capture | aidebug | `ConsoleWndProc` | expanded physical |
| Presented HUD routing | aidebug | native-mask translation | physical -> native HUD |
| Control lookup | StarCraft | `dlgSetMouseOver` at `0x00418340` | native dialog |
| Status event validation | StarCraft | call at `0x00457E10` | native dialog |
| Status tooltip owner | StarCraft | call at `0x00457E50` | native dialog |
| Status tooltip refresh | StarCraft | call at `0x00458015` | native dialog |
| Tooltip refresh | StarCraft | call at `0x00459796` | native dialog |
| Status parent validation | StarCraft | call at `0x00459825` | native dialog |
| Command-card mouse move | StarCraft | call at `0x00459870` | native dialog |
| Minimap-button refresh | StarCraft | call at `0x004A5459` | native dialog |
| Minimap-button mouse move | StarCraft | call at `0x004A54BF` | native dialog |
| Tooltip content/rendering | StarCraft and stable GPTP | existing tooltip path | already correct |

## Installed-binary evidence

Both relevant StarCraft 1.16.1 callsites target `0x00418340`:

```text
0x00459796  E8 A5 EB FB FF  call dlgSetMouseOver
0x00459870  E8 CB EA FB FF  call dlgSetMouseOver
```

The first is inside `refresh_button_tooltip`; the second is inside
`statBtn_dlg_MouseMove`. The control lookup returns the command control under
the engine mouse point and feeds the existing tooltip creator.

User regression then isolated Diplomacy and Hide Terrain as a second dialog.
Static disassembly confirms its two control lookups also target `0x00418340`:

```text
0x004A5459  E8 E2 2E F7 FF  call dlgSetMouseOver
0x004A54BF  E8 7C 2E F7 FF  call dlgSetMouseOver
```

These are owned by `minimapPreviewMouseUpdate` and `minimap_dlg_MouseMove`.
The selected control is passed to `drawStatLBBtnsContextHelp`; control index 2
routes to the Hide/Show Terrain tooltip, while the neighboring map button
produces Diplomacy help.

## Implemented policy

Record the unmodified physical point before HUD translation. At only the eight
verified HUD tooltip lookup callsites, return no control when that physical
point hits solid native HUD artwork at its obsolete location but does not hit
the presented HUD. Otherwise tail-call the original lookup.

This suppresses only the duplicate hover source. It does not move the engine
mouse, change dialog bounds, alter tooltip rendering, or affect gameplay unit
hover selection.

## Required safeguards

- Verify each call opcode and decoded target before patching.
- Patch only the eight verified calls at `0x00457E10`, `0x00457E50`,
  `0x00458015`, `0x00459796`, `0x00459825`, `0x00459870`, `0x004A5459`, and
  `0x004A54BF`.
- Preserve the original lookup for the presented HUD and modal dialogs.
- Keep the physical point one-to-one for gameplay cursor and world input.
- Log suppression state transitions with physical coordinates.
- Leave stable GPTP unchanged.

## Verification

- [x] x86 Release build succeeds.
- [x] Geometry and system-map validators pass.
- [x] Runtime preflight reports all eight callsites installed.
- [x] Repair, status, and other tooltips work on the centered HUD.
- [x] The same invisible 4:3 locations produce no tooltip.
- [ ] Gameplay cursor state still changes over units in that battlefield area.
- [x] Stable GPTP hash remains unchanged.

## Implemented result

The installed renderer filters eight verified lookup calls: three status-panel
lookups, the status-parent validation call, two command-card calls, and two
minimap-button calls. Runtime preflight reported `calls=8` with the native
lookup still at `0x00418340`. `fixed_zoom_tooltip.log` records suppression
transitions at obsolete locations, and the user confirmed the command-card,
Diplomacy/Hide Terrain, and armor/attack duplicates are gone.
