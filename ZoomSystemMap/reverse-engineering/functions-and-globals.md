# Functions and globals

Addresses in the StarCraft tables are absolute addresses for the supported
1.16.1 executable. GPTP addresses are RVAs unless explicitly labeled live.

## Rendering functions

| Address | Working name | Evidence / use |
|---:|---|---|
| `0x0041E280` | `DrawScreen` | Confirmed hook target and private-pass entry |
| `0x0041E297` | DrawScreen begin hook site | Exact five-byte trampoline-safe instruction |
| `0x0041E3FD` | DrawScreen post hook site | Common post-composition path |
| `0x0041CB50` | Native dialog/UI layer update | Live layer-2 function |
| `0x004810F0` | Native context layer update | Live layer-1 function |
| `0x004BD580` | Raw native world draw | Bypasses GPTP map graphics; do not use for private passes |
| `0x004BD3A0` | Rebuild visible-sprite rows | Behavioral name, confirmed necessary per camera |
| `0x004BDFA0` | Cursor layer update | Live layer-0 function |
| `0x004BD614` | Screen-space game-text call | Replaced with no-op during native/private draw |
| `0x004BD619` | Drag-selection call | Replaced; expanded box drawn once later |

## Placement functions

| Address | Working name | Notes |
|---:|---|---|
| `0x0048D5C0` | Placement-surface layer draw | Native per-pass clip; Surface param |
| `0x0048D5F2` | Native placement clip constants | 640x400; must remain native |
| `0x0048D660` | Stock placement-position initializer | Entry replaced by stable GPTP |
| `0x0048D700` | Initialize placement layers | Assigns Surface params and draw function |
| `0x0048D7B0` | Disable/clear placement layer | Source helper name is uncertain |
| `0x0048D7F0` | Update placement layer position/pixels | Writes layer left/top and placement bitmap |
| `0x0048D9A0` | Refresh placement layers 3/4 | Confirmed from source and call behavior |
| `0x0048DDC0` | Stock draw-placement-box update | Calls placement position and layer setup |
| GPTP RVA `0x87BF0` | Stable replacement placement initializer | Confirmed by static disassembly and runtime signature |

## Input and patch functions

| Address | Working name | Use |
|---:|---|---|
| `0x00421603/0A` | Focus-time cursor rectangle operands | Physical 640x480 gate |
| `0x0046FC76/88` | Alternate visible-unit bounds | Expanded selection collection |
| `0x0046FE19/2B` | Normal visible-unit bounds | Expanded selection collection |
| `0x0046FFAA` | Stored drag-clip rectangle pointer | Redirected to expanded rectangle |
| `0x0048468E` | Gameplay rectangle pointer consumer | Redirected from native rectangle |
| `0x004D14AA` | Gameplay rectangle pointer consumer | Redirected from native rectangle |
| `0x004D1140` | Outside-game-screen predicate | Hooked with expanded-aware function |
| `0x004D1460` | Cursor-type selector entry | Replaced by stable GPTP |
| `0x004D14D0` | Cursor update caller | Stores selected type and swaps current GRP |
| `0x00418340` | Native control-at-point lookup | Returns dialog child under an event coordinate |
| `0x00459770` | Refresh command-card tooltip | Calls control lookup at `0x00459796` |
| `0x00459860` | Command-card mouse-move handler | Calls control lookup at `0x00459870` |
| `0x004A5110` | Map-button context help | Dispatches Diplomacy and Hide/Show Terrain help by control index |
| `0x004A5440` | Minimap-button refresh | Calls control lookup at `0x004A5459` |
| `0x004A5490` | Minimap-dialog mouse move | Calls control lookup at `0x004A54BF` |
| GPTP `minimap_game_mouse_update` | Minimap camera movement | Reads shared `g_mouse`, calls `get_minimap_cursor_pos`, then `move_screen` |
| GPTP RVA `0x2C5F0` | Minimap camera update sequence | Version signature exposes the shared camera-box width/height operands |
| `0x0048E850` | Positional-sound stereo pan | World x in EAX; native center was 10 tiles |
| `0x0048E8D0` | Positional-sound volume attenuation | World x in ECX and y in EAX; reads camera x/y |
| `0x0045E3A0` | Native status-portrait target writer | Encodes a sprite position minus the native 320x200 center into `0x0057FD34/38` |
| `0x0045E9F0` | Native portrait target callback | Reads `0x0057FD34/38` and calls `move_screen`; not the live Cosmonarchy bottom-HUD portrait callback |
| `0x0045EE4B` | Native transmission-portrait target writer | Parallel 320x200 target encoder used by talking/transmission portraits |
| `0x004CB190` | Encode CHK start location | Stores player start position and derives initial camera-origin tiles |
| `0x004CB1E9/F9` | Start-location center subtraction | Native x=10/y=6 tile offsets; patched from configured camera center |
| `0x004C6DE0` | Trigger `Center View` action | Averages the selected location bounds, converts midpoint to camera origin, then calls `move_screen` |
| `0x004C6E68/6E` | Trigger Center View center subtraction | Native y=200/x=320 immediates; patched from configured camera center |
| `0x004CCEC3` | Extended upgrade-research clear destination | GPTP-patched pointer; bulk clear must use allocation base, not ID-biased base |
| `0x004BD3F0` | `InitScreenPositions` | Restores `g_move_to_tile_x/y` as pixel camera origin through `move_screen` |

## Core globals

| Address | Type / role | Confidence |
|---:|---|---|
| `0x0051BFB0` | Main window handle | Confirmed |
| `0x0057F1D4/1D6` | Map width/height in tiles | Confirmed |
| `0x005993B0` | Native renderer/input game rectangle | Confirmed shared consumer |
| `0x00596B70` | Current cursor type | Confirmed by disassembly and live transitions |
| `0x00597394` | Current cursor GRP pointer | Confirmed by disassembly and live transitions |
| `0x0059CC6C` | Minimap zoom level (`u16`) | Source-confirmed; initialized per map |
| `0x0057FD34/38` | Native portrait camera target x/y | Used by `0x0045E9F0`; live capture proved the Cosmonarchy bottom-HUD portrait path bypasses these globals |
| `0x00597248` | Active portrait unit pointer | Used by the resolution-aware compatibility correction to obtain the live sprite world position |
| `0x0057F1D0/D2` | Initial/current camera origin in tiles | Written by start-location decoding and `move_screen`; restored by `InitScreenPositions` |
| `0x0062848C` | Camera x in world pixels | Confirmed |
| `0x006284A8` | Camera y in world pixels | Confirmed |
| `0x006284B0` | Native camera-y maximum | Confirmed |
| `0x00640880` | Is placing building | Confirmed |
| `0x0064088A` | Building type being placed | Confirmed |
| `0x00640890/92` | Placement tile x/y | Confirmed |
| `0x00640958` | Placement result/message state | Source-confirmed |
| `0x0064095C/64` | Placement Surface records | Confirmed by disassembly and behavior |
| `0x006556EC/F0` | Left/right gameplay click procedure | Confirmed |
| `0x0066FF50` | Inclusive drag-selection rectangle | Confirmed |
| `0x0066FF5C` | Drag-selection active flag | Confirmed |
| `0x006CDDC4/C8` | Engine mouse x/y | Confirmed |
| `0x006CEF50` | Eight-entry draw-layer table | Confirmed |
| `0x006CEFF0` | Native game Surface | Confirmed |
| `0x006CEFF8` | Native redraw tile grid | Confirmed |
| `0x006CF4A8` | Current canvas Surface pointer | Confirmed |
| `0x006D1214` | Popup dialog active | Confirmed |
| `0x006D5BF4` | Popup dialog pointer | Confirmed |
| `0x006D5E14` | STrans list | Confirmed |
| `0x006D5E18` | Game redraw STrans | Confirmed |
| `0x006D5E1C` | Full redraw flag | Confirmed |
| GPTP RVA `0x26DC7C` | Minimap camera-box width (`u16`) | Shared by local and multiplayer camera outlines |
| GPTP RVA `0x26D9DC` | Minimap camera-box height (`u16`) | Shared by local and multiplayer camera outlines |

## Update rule

For a new address, record the module, supported version, exact access width,
structure offset, callers, and the evidence used to name it. If it came from a
live ASLR module, record an RVA rather than the observed virtual address.
