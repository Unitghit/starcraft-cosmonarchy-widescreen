# Feature status

Status here refers to the current 1280x720 build unless noted otherwise.

| Feature | Status | Evidence |
|---|---|---|
| 1280x720 client and presentation | Confirmed | Runtime log reports exact client and pitch |
| 2.5x presentation magnification | Confirmed | User confirmed full-client 3200x1800 at exact 5/2 scale; logical renderer remains 1280x720 with pitch 1280 |
| Portable widescreen configurator | Implemented, integration-confirmed | Single-file WinForms app includes 4:3/16:9/custom selection through 4K, selectable centered or screen-edge top text, 1x presentation default, one embedded universal renderer, transactional Save/Restore, borderless generation, real normal-launch runtime config, and unchanged-GPTP checks |
| 1280x640 expanded map | Confirmed | User test and six-pass captures |
| Resolution-dependent tile seams | Confirmed | Camera-aligned source crops removed the duplicated horizontal band and also restored continuous rally lines |
| Units and sprites across full width | Confirmed | User test after visible-row rebuild per pass |
| Stable HUD bottom-centered | Confirmed | User test; popup no longer shifts it |
| Modal popup centered | Confirmed | User test and logged popup bounds |
| Modal popup transparency source | Expanded, confirmed | User test; native STrans popup pass uses camera offset matching centered relocation |
| Top-screen objectives/resources layout | Both modes confirmed; runtime-selectable; centered default | `centered_native_box` and resolution-derived `screen_edges` policies passed user tests and are exposed independently in the configurator |
| Duplicate/flickering text UI | Fixed, confirmed | Text drawn once after composition |
| Duplicate selection rectangle | Fixed, confirmed | Direct once-per-frame expanded draw |
| Middle-mouse pan rendering | Fixed, confirmed | User test; gesture-time UI extraction uses a fresh current-camera pass paired with the fresh game-only reference, preventing stale stock-map pixels from becoming ghost UI bands |
| Cursor can traverse expanded client | Confirmed | Input trace reaches x=1279 |
| Click and drag beyond native 4:3 | Confirmed | User test and raw/forwarded command traces |
| Same-type modifier selection | Expanded, confirmed | Stable-GPTP Ctrl and Ctrl+Shift visible-unit rectangles derive from runtime viewport dimensions; user test outside the original 4:3 area |
| Battlefield drag released over HUD | Fixed, confirmed | Battlefield-originated left-button sequences retain gameplay ownership through release; user test |
| Invisible native HUD hit targets | Fixed, confirmed | User test of menu/minimap collision path |
| Minimap camera viewport outline | Dynamic runtime override, confirmed | User test; GPTP globals derived from internal battlefield dimensions and per-map zoom level |
| Match-start camera | Expanded, confirmed | User test; CHK start-location encoder derives `20x8` camera-center tiles from configured `(640,260)` pixels before `InitScreenPositions` restores them |
| Bottom-HUD portrait camera centering | Expanded, confirmed | User test; exact legacy sprite-minus-320x200 result is replaced with the configured profile center and native 8-pixel clamping |
| Positional combat audio | Expanded runtime patch, confirmed | User test; native pan/volume helpers use 1280x640 battlefield geometry |
| GPTP extended-upgrade match clear | Stabilized, confirmed | User passed match start after runtime correction of source-confirmed -46 bulk-clear pointer bias |
| Race HUD protrusion/corner art | Expanded, confirmed for Protoss and Zerg | y=293..313 follows bottom HUD; Zerg's measured 14-pixel cap at x=0..5/y=290..292 also follows it; x=0..22 is kept for Zerg/Protoss and discarded for Terran |
| Aspect-fitted front-end menus | Installed, interactive user test pending | Native 640x480 frame scales into derived centered 4:3 rectangle with inverse mouse mapping |
| Rally-point line | Expanded, confirmed | Stable-GPTP coordinate mode `1` replayed in every private pass |
| Building-placement ghost | Expanded, confirmed | User test after GPTP bound and layer-position fixes |
| Gameplay hover cursor states | Expanded, runtime-confirmed | GPTP selector emits ally/neutral states beyond x=639 and y=399; visual regression pending |
| Invisible native tooltip hitboxes | Expanded, confirmed | Eight verified lookup filters cover command card, minimap buttons, and status-panel armor/attack descriptions; user confirmed obsolete 4:3 descriptions are gone |
| Multiple internal resolutions | Universal runtime renderer implemented | Startup derives all mapped geometry from config within 640x480..3840x2160; 16:9 presets extend through 4K and custom dimensions share the same payload |
| Locally rebuilt GPTP | Incompatible | Data-file errors and crashes; not deployed |
| Legacy Resolution Expander | Incompatible | DLL initialization failures and crashes |

## Current limitations

- Only 1280x720 has full interactive user regression coverage.
- Very high internal resolutions require many native render passes and may be
  substantially slower.
- The system depends on exact StarCraft 1.16.1 and stable GPTP instruction
  signatures.
- The HUD remains native-size rather than being artistically extended.
- Some diagnostic names preserve historical terminology even when a field's
  semantics were later refined; this map is the authoritative interpretation.
