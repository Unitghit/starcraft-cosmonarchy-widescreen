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

### Scrollable settings and fixed actions

`MainForm.BuildInterface` uses a non-scrolling root with two rows: a fill-sized
`SettingsScroll` panel and an auto-sized, opaque `ActionFooter`. Only the
settings content uses vertical scrolling; its preferred height is independent
of the available viewport. Footer status, Restore Original, Save and Save &
Play remain outside that scroll surface. Layout allocates separate rectangles,
so settings cannot cover or push the actions outside the client area.

The minimum window height is 420 logical pixels, with startup height bounded
by the display working area. Native WinForms scrolling handles overflow and
keyboard focus. No game configuration semantics or renderer changes.
Configurator tests check compact/normal/wide windows, scrollbar visibility
and disappearance, absence of horizontal scrolling, final-row reachability,
footer/button containment and unchanged settings after resizing/scrolling.
The resolution summary and its warnings now occupy separate auto-sized footer
rows above status/actions, outside the scrolling surface. Text wraps to the
available width, and empty warnings collapse. Layout tests check stationary
summary bounds, long-warning containment, and no overlap with settings/buttons.

`PageScrollControls.cs` makes every dropdown a `PageScrollComboBox`. Its wheel
handler skips normal value-selection handling, marks the event handled, and
forwards it to the ancestor `SettingsScrollPanel`. Open lists close first so
they cannot float over a scrolled-away field. Click/keyboard selection and
numeric fields remain unchanged. No global message filter or game input hook.
Tests send native WM_MOUSEWHEEL in both directions to every dropdown, open
and closed, asserting no selection events, page movement and a stationary footer.

Local handoff, 2026-09-04: all 16 configurator checks pass, including rendered
compact/normal-window inspection at the host's 200% DPI. Updated the Windows
Release configurator only (SHA-256
`25A409D85156230FC242D064798D7527DF25212795109122F17B83F5EAB8BA8E`).
Viewport INI, wrapper INI and installed renderer hashes remained unchanged.
Previous GUI is backed up under `ZoomIntegration/backups/gui-scroll-20260904-203007`.
No public release was created; interactive user acceptance is pending.

| Artifact | Configurator action |
|---|---|
| `plugins/aize_debug.qdp` | Back up once, install embedded universal renderer, verify hash |
| `cosmonarchy_viewport.ini` | Write internal-profile identity and runtime presentation settings |
| `../Starcraft/ddraw.ini` | Normalize and update only owned top-level presentation keys |
| `../Starcraft/ddraw.dll` | Never write, replace, rename, or remove |
| `.cosmonarchy-widescreen/` | Store original backup and installation state |
| `plugins/gptp.qdp` | Read/hash only; never write |

The QDP loader already contains eight modules, matching MPQDraft's eight-plugin
limit. Therefore the release must replace and back up the existing
`aize_debug.qdp`; it must not install a ninth uniquely named QDP.

The configurator compares `ddraw.dll` with tested SHA-256 identities for
cnc-ddraw 6.9, 7.0, and 7.1 and includes the result in installation status.
An unknown DLL produces a nonblocking compatibility warning. Runtime support
remains capability-based because the renderer discovers the required named
imports instead of selecting a version-specific payload.

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
- Installation state records the original and last-applied value of every owned
  ddraw key. Restore reverts a key only while it still equals the last-applied
  value, preserving later user or wrapper changes.
- Active duplicates are collapsed only for owned keys. Unowned keys, comments,
  sections, and the wrapper DLL are preserved.
- cnc-ddraw detection is read-only and never changes Save or Restore ownership.
- Runtime verification records config path, internal size, output size, mode,
  compatibility, and actual client dimensions in
  `fixed_zoom_presentation.log`.
- The logged config path must be absolute; a relative path is invalid because
  Win32 profile APIs use Windows-directory lookup semantics.

## Confirmed evidence

- Self-contained .NET 8 WinForms publish produces one executable.
- Per-Monitor-V2 layout was captured and inspected at 200% display scaling.
- Isolated Save installed the payload and generated 1280x720 -> 3200x1800.
- Isolated Restore returned the renderer byte-for-byte and restored owned ddraw
  values without replacing unrelated settings.
- Borderless smoke test generated 3840x2160, `fullscreen=true`,
  `windowed=true`, borderless presentation and then restored cleanly.
- Real normal-launch runtime read `cosmonarchy_viewport.ini` with
  `compatible=1` and produced an exact 3200x1800 client.
- GPTP remained unchanged through real installation.
- The universal payload passes compile-time maximum-buffer checks and offline
  geometry checks across preset and representative custom resolutions.
