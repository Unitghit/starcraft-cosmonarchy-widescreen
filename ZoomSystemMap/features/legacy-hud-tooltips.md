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
| Game-menu refresh owner | StarCraft | call at `0x004F509A` | native dialog |
| Game-menu event owner | StarCraft | call at `0x004F511F` | native dialog |
| Game-menu context caller | StarCraft | call at `0x004F5142` | native dialog |
| Game-menu context content | stable GPTP | hook at `0x004F4F70` | native dialog |
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

Cosmonarchy's Game Menu button bypasses those shared control lookups. Stable
GPTP replaces the dedicated context function at `0x004F4F70`, and one verified
StarCraft caller invokes it with the dialog pointer in `EDX`:

```text
0x004F5140  8B D6           mov edx, esi
0x004F5142  E8 29 FE FF FF  call 0x004F4F70
```

The Game Menu hover and highlight are owned by two earlier control lookups.
The refresh path can tail-jump directly into the context function, which means
filtering only `0x004F5142` does not intercept it:

```text
0x004F509A  E8 A1 32 F2 FF  call 0x00418340
0x004F50C1  E9 AA FE FF FF  jmp  0x004F4F70
0x004F511F  E8 1C 32 F2 FF  call 0x00418340
```

The button's dedicated update callback is assigned in two places. The first
creates the control and the second restores the callback during mouse-event
handling. Both assignments must point at the hover-state synchronizer:

```text
0x004F50DB  C7 40 2E B0 4F 4F 00  mov [eax+0x2e], 0x004F4FB0
0x004F51B3  C7 46 2E B0 4F 4F 00  mov [esi+0x2e], 0x004F4FB0
```

## Implemented policy

Record the unmodified physical point before HUD translation. At the eight
verified shared HUD tooltip lookup callsites, return no control when that
physical point hits solid native HUD artwork at its obsolete location but does
not hit the presented HUD. Otherwise tail-call the original lookup. At the
dedicated Game Menu caller, derive the exact control rectangle from the live
dialog and parent bounds, translate that rectangle to the bottom-centered HUD,
and return no hover owner unless the physical cursor is inside that visible
rectangle. Apply the same rectangle check to the remaining direct context
caller. Otherwise preserve the native results and chain to GPTP's installed
context function. Immediately before the button is drawn, synchronize only its
`MouseHovering` flag with the visible presented rectangle. Patch both callback
assignments so mouse-event handling cannot restore the unsynchronized native
update function.

This suppresses only the duplicate hover source. It does not move the engine
mouse, change dialog bounds, alter tooltip rendering, or affect gameplay unit
hover selection.

## Required safeguards

- Verify each call opcode and decoded target before patching.
- Patch only the eleven verified calls at `0x00457E10`, `0x00457E50`,
  `0x00458015`, `0x00459796`, `0x00459825`, `0x00459870`, `0x004A5459`, and
  `0x004A54BF`, plus the Game Menu calls at `0x004F509A`, `0x004F511F`, and
  `0x004F5142`.
- Preserve the original lookup for the presented HUD and modal dialogs.
- Keep the physical point one-to-one for gameplay cursor and world input.
- Keep runtime diagnostics disabled in release builds.
- Leave stable GPTP unchanged.

## Verification

- [x] x86 Release build succeeds.
- [x] Geometry and system-map validators pass.
- [x] Build-time inspection confirms all three Game Menu callsites and targets.
- [x] Repair, status, and other tooltips work on the centered HUD.
- [x] The same invisible 4:3 locations produce no tooltip.
- [x] The invisible native Game Menu location produces no `Game Menu (F10)`
  tooltip while the visible centered Menu button still does.
- [x] The invisible native Game Menu location does not highlight the button.
- [ ] Gameplay cursor state still changes over units in that battlefield area.
- [x] Stable GPTP hash remains unchanged.

## Implemented result

The renderer filters eight verified shared lookup calls: three status-panel
lookups, the status-parent validation call, two command-card calls, and two
minimap-button calls. It now also filters both Game Menu hover-owner lookups and
the remaining dedicated context call.
The command-card, Diplomacy/Hide Terrain, and armor/attack duplicates are
confirmed fixed. The Game Menu tooltip and highlight now respond only at the
visible bottom-centered button, confirmed by user testing.
