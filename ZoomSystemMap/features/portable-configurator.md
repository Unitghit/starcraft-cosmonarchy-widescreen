# Portable widescreen configurator

## User contract

`Cosmonarchy Widescreen Settings.exe` is placed beside
`Cosmonarchy BW.exe`. It presents internal viewport resolution and external
presentation as separate controls, then applies settings through **Save** or
**Save & Play**. The ordinary Cosmonarchy executable remains the normal launch
path afterward.

The top-screen objective and resource text layout is independently selectable.
`centered_4_3` is the default and `screen_edges` is the optional wide layout.
The selection is persisted as `viewport.top_ui_layout` and consumed by the
same universal renderer payload.

## Ownership

| Artifact | Configurator action |
|---|---|
| `plugins/aize_debug.qdp` | Back up once, install embedded universal renderer, verify hash |
| `cosmonarchy_viewport.ini` | Write internal-profile identity and runtime presentation settings |
| `../Starcraft/ddraw.ini` | Preserve file and update only owned top-level presentation keys |
| `.cosmonarchy-widescreen/` | Store original backup and installation state |
| `plugins/gptp.qdp` | Read/hash only; never write |

The QDP loader already contains eight modules, matching MPQDraft's eight-plugin
limit. Therefore the release must replace and back up the existing
`aize_debug.qdp`; it must not install a ninth uniquely named QDP.

## Resolution separation

Internal presets come from the configurator's embedded
`compatibility-manifest.json`. A single universal payload reads the chosen
dimensions before installing geometry patches. It accepts 640x480 through
3840x2160, including 16:9 presets through 4K and a custom entry. The 1280x720
configuration is recommended and remains the most thoroughly tested target;
4:3 presets stay visibly experimental.

External presentation is independently runtime-configurable.
`mainpatch.cpp` validates and derives internal geometry first, then
`presentation.cpp` applies explicit output dimensions. cnc-ddraw owns the
corresponding raster scale and physical-to-logical mouse mapping.

## Safety and verification

- StarCraft, GPTP, and hotloader hashes must match the embedded manifest.
- Game processes must exit before Save or Restore.
- Renderer, ddraw, and config writes use same-directory temporary files and
  atomic replacement.
- A failed multi-file apply restores the pre-transaction bytes.
- Restore requires a verified original aidebug backup.
- Runtime verification records config path, internal size, output size, mode,
  compatibility, and actual client dimensions in
  `fixed_zoom_presentation.log`.
- The logged config path must be absolute; a relative path is invalid because
  Win32 profile APIs use Windows-directory lookup semantics.

## Confirmed evidence

- Self-contained .NET 8 WinForms publish produces one executable.
- Per-Monitor-V2 layout was captured and inspected at 200% display scaling.
- Isolated Save installed the payload and generated 1280x720 -> 3200x1800.
- Isolated Restore returned renderer and ddraw files byte-for-byte.
- Borderless smoke test generated 3840x2160, `fullscreen=true`,
  `windowed=true`, borderless presentation and then restored cleanly.
- Real normal-launch runtime read `cosmonarchy_viewport.ini` with
  `compatible=1` and produced an exact 3200x1800 client.
- GPTP remained unchanged through real installation.
- The universal payload passes compile-time maximum-buffer checks and offline
  geometry checks across preset and representative custom resolutions.
