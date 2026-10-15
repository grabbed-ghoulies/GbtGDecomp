# Milestone: native front end (game select)

Goal: game select (`aid_script_ghoulies_gameselect_gameselect`) runs through its
translated scene control, factory type 0x9A, and the front-end widget modes that
control owns. It draws through the native text pools, screen sprites and the
native button bat into `SceneD3DDevice`, and plays its sounds on the native
event voices. The host front end then retires: `frontend.cpp`,
`frontend_slots.cpp`, `frontend_contents.cpp`, `frontend_keyboard.cpp`,
`frontend_text.cpp`, `MenuCursor` in `frontend_types.h` (with the last
`pcm_*_owned_cue` callers), the `g.*` front-end state, and `gameselect_tick` /
`draw_ui` in `frame_tick`.

This is step 5 of `docs/LEGACY_HOST_CODE.md` (the book and slime transitions are
the other half of that step). Read `AGENTS.md` first.

## Where retail runs game select

- Start in the attract (`0x00080CB0`) requests the game-select script. Its scene
  objects include a scene control of factory type 0x9A. The factory table row
  at `0x0030DB54` (table `0x0030D41C`, stride 12) gives parent 3, getter
  `0x00083610` and name `0x003265F4`.
- The getter fills vtable `0x00505620`:

  | Slot | Address | Role |
  |---|---|---|
  | +0x0 | `0x000835F0` | sizes: instance 300 (0x12C), parameters 8 |
  | +0x4 | `0x00083660` | constructor |
  | +0x8 | `0x00083850` | destructor |
  | +0xC | `0x00115EE0` | message (type 3's) |
  | +0x10 | `0x000838D0` | tick |
  | +0x14 | `0x000839E0` | draw: the control's camera view, then the widget's draw |

- The constructor resets the profile flags (`DAT_00506C74`), refreshes the
  save-game summary (`0x000F2F00`), sets `DAT_0050EA9C` bit 0x100000, creates a
  640x480 texture (`0x000533A0`), loads `aid_loctext_ghoulies_gameselect_gameselecttext`
  into `DAT_005462C4`, picks the widget mode (`0x000E5AE0`), creates and opens
  it, links the "gameselect" camera view and raises the pause count.
- **The widget** (control +0x88 vtable, +0x8C instance) is a row of the mode
  table at `0x0030A000` (19 rows of 0x18 bytes: create, destroy, open(owner,
  widget), close, tick, draw). `0x000E5AE0` picks row `DAT_00429BC0` when music
  stream 2 is idle, else row 18 (wait for the stream). Boot (`0x000E5A80`)
  stores 1. A mode asks for another by writing the control's +0x94; the tick
  switches when `0x00083E20` allows (the menu book's page threshold), closing
  and destroying the old widget first.

  | Row | Create | Open | Close | Tick | Draw | Screen |
  |---|---|---|---|---|---|---|
  | 1 | `0x000DD730` | `0x000DD740` | `0x000DDC90` | `0x000DDD80` | `0x000DDF20` | slot select |
  | 2 | `0x000DB720` | `0x000DB730` | `0x000DB950` | `0x000DB9E0` | `0x000DBB90` | |
  | 3 | `0x000D8AB0` | `0x000D8B10` | `0x000D9300` | `0x000D9450` | `0x000D9640` | slot contents |
  | 4 | `0x000DCEB0` | `0x000DCEC0` | `0x000DD030` | `0x000DD0A0` | `0x000DD170` | naming (virtual keyboard) |
  | 5 | `0x000DC0B0` | `0x000DC0C0` | `0x000DC640` | `0x000DC6F0` | `0x000DC9A0` | |
  | 6 | `0x000D5850` | `0x000D5880` | `0x000D62A0` | `0x000D6410` | `0x000D67F0` | |
  | 7 | `0x000D4EA0` | `0x000D4EB0` | `0x000D5160` | `0x000D51E0` | `0x000D5300` | play (from contents) |
  | 8 | `0x000D4EA0` | `0x000D7A60` | `0x000D7DB0` | `0x000D7E40` | `0x000D7F80` | |
  | 9 | `0x000E1E00` | `0x000E1E10` | `0x000E21A0` | `0x000E2230` | `0x000E4DB0` | |
  | 10 | `0x000E1E00` | `0x000E2B90` | `0x000E2F20` | `0x000E2FB0` | `0x000E4DB0` | |
  | 11 | `0x000E1E00` | `0x000E39F0` | `0x000E3D80` | `0x000E3E10` | `0x000E4DB0` | |
  | 12 | `0x000E5BD0` | `0x000E5BE0` | `0x000E6120` | `0x000E6210` | `0x000E62F0` | |
  | 13 | `0x000E7E00` | `0x000E7E10` | `0x000E81F0` | `0x000E8270` | `0x000E83A0` | |
  | 14 | `0x000E7A00` | `0x000E7A10` | `0x000E7C10` | `0x000E7C70` | `0x000E7CA0` | |
  | 15 | `0x000E89A0` | `0x000E89B0` | `0x000E8C30` | `0x000E8CB0` | `0x000E8CF0` | |
  | 16 | `0x000E1E00` | `0x000E48A0` | `0x000E4BF0` | `0x000E4C80` | `0x000E4DB0` | |
  | 17 | `0x000E0B60` | `0x000E0B70` | `0x000E0F50` | `0x000E1000` | `0x000E1130` | |
  | 18 | `0x000E0650` | `0x000E0660` | `0x000E0830` | `0x000E08C0` | `0x000E0A30` | music stream still busy |

  Every row destroys with `0x000D8AC0` (HeapFree on the widget's thread heap).

## Scope

A read-only callee walk (`TreeExport.java` in the session scratchpad: functions
created inside a rolled-back transaction, the project unchanged) from the
control's vtable and all 19 mode rows reaches about 511 untranslated functions,
about 138 KB of code. Of those:

- about 180 are the widget modes (`0x000D4xxx`–`0x000E9xxx`);
- about 90 are the virtual keyboard used by mode 4 (`0x000110D0`–`0x00018xxx`);
- about 130 are XDK library code (`0x0014xxxx`–`0x0018xxxx`: string, save-game
  and memory-unit calls), most of which are host boundaries (file IO, heap);
- the rest are shared engine code (camera view, menu book `0x00048xxx`,
  texture creation `0x000533A0`, save summaries `0x0010Dxxx`).

The control and row 1 alone reach about 129 untranslated functions (23 KB).
The host front end covers rows 1, 3, 4 and 7 only.

## Current state

- **Host.** The native start stops at factory type 154 and the host front end
  (`gameselect_tick`, `draw_ui`) runs game select: rows 1, 3, 4 and 7 as host
  re-implementations, drawn with `gfx_sprite_submit`, with the cursor bat's
  sounds through `pcm_play_owned_cue` (`MenuCursor`).
- **Native.** See "Progress" below.

## Reference captures

Taken before any change in this milestone (copied to `cache/gameselect_ref/`):

- `gbtg-transition-tests OriginalFiles --title` and `--title-a` write
  `cache/title_check_N.bmp` (`title_N.bmp`, `title-a_N.bmp` in the reference
  folder): frames 30, 60 and 119 are the attract's first clip with "Press
  START"; frame 300 is the page turn into game select (Start or A at frame 120).
- The new `--gameselect` mode presses Start at frame 120 and waits until the
  game-select script is loaded and no scene transition runs (frame 410 on the
  host). From there it presses Right at +90, A at +180 and B at +360, and writes
  `cache/gameselect_check_N.bmp` at:

  | +N | Screen (host) |
  |---|---|
  | 60 | Slot select, slot 1 raised, "Empty", "Choose A Game", Back / Select / Move |
  | 150 | Slot 2 raised after Right |
  | 300 | Naming keyboard ("My Game", the key grid and its prompts) after A on the empty slot |
  | 480 | Slot select again after B, slot 2 raised |

## Work

1. Translate the control (sizes, constructor, destructor, tick, draw) and its
   direct callees.
2. Row 1 (slot select) and row 18 (stream wait), with the slot movie, the
   plates and the cursor bat (`0x0006D6C0` is native) and its sounds.
3. Rows 3, 4 (with the virtual keyboard) and 7, then any other row reachable
   from them.
4. Switch: install type 0x9A for every run, remove the gate, and delete the host
   front end.

Until step 4, type 0x9A is installed only when `g_native_game_select` is set
(the `--gameselect-native` transition test). Without it the native start stops
at type 154 exactly as before, so game select stays wholly on the host.

## Acceptance

- `--title`, `--title-a` and `--gameselect` pass on the native control, and the
  `--gameselect` captures match the references or are better (compare with
  PIL; state every difference).
- The cursor bat's sounds and the menu music play on the native voices.
- No host front-end code remains: no `gfx_sprite_submit` callers in
  `src/engine/frontend`, no `pcm_*_owned_cue` callers, no `g.slot*`,
  `g.contents*`, `g.naming`, `g.frontend_mode`.
- Release and Debug build; ctest; the six transition suites plus
  `--player-opening`, `--title`, `--title-a`, `--attract`, `--goop`, `--ui`,
  `--audio-preparation`; both 900-frame headless runs; `git diff --check`.

## Progress

Status: **in progress, gated.** Game select still runs on the host by default.
The native control runs only under `--gameselect-native`, which passes the same
Start, Right, A and B sequence as `--gameselect`. The host path's
`--title`, `--title-a` and `--gameselect` captures stay pixel-identical to the
references.

Translated and live under the gate:

- **The control** (`src/engine/scene/gameselect_control.cpp`): sizes, ctor,
  dtor, tick and draw (`0x000835F0`–`0x000839E0`), `0x00083C40`, `0x00083E20`
  (book states 2 and 3 raise), `0x000E5AE0`, `0x001234B0`, `0x00116170`. Its
  services (`GameSelectControlServices`, `src/engine/frontend/gameselect_rows.h`)
  are bound by `SceneActorServices::State::bind_gameselect_services`.
- **Row 1, slot select** (`gameselect_slots.cpp`, `0x000DD730`–`0x000E05D0`):
  the disc scan, the plates, the slot movies, "Empty", "Choose A Game", the
  prompts and the cursor bat, Right and Left, A on an empty slot, B back to the
  attract. Occupied slots raise (`0x000DEB20`'s occupied path, `0x000F3020`).
- **Row 4, naming** (`gameselect_naming.cpp`, `0x000DCEB0`–`0x000DD170`,
  `0x000DD550`, `0x000DD6C0`): the dialog page cutscene, the "gameslotnaming"
  camera and the default name.
- **The virtual keyboard** (`virtual_keyboard.cpp`, `0x000110D0`–`0x000176E0`):
  the English, symbol and accent boards, key labels, the name box and caret, the
  key and caret bats, the "VirtualKeyboardMenu" prompts (callback `0x001628B0`,
  a bare `RET`), pad and stick input with repeat, every key but Done, and the
  `RIGHTBUTTON` sound on the native event voices. The Japanese layout
  (`0x00012920`) raises.
- Shared pieces: `0x0005F970` and `0x0005FB10` (text pools), `0x00061360` (the
  rounded dialogue panel, D3D immediate mode through `SceneD3DDevice`),
  `0x0006DA00`, `0x000ED450`, `0x00039D50`, `0x000E5A80`, `0x000F2F00`,
  `0x0010D3B0`, `0x0010D710`, `0x0010D7A0`, `0x0010F620`.
- Presenter: fixed-function line lists move half a pixel (the NV2A samples
  pixel centres on whole coordinates). Without it the panel's cap lines left a
  gap row and a doubled row across every name box.

Differences from the host captures under `--gameselect-native`:

- +300: the key bat and the caret bat are drawn (the host draws neither), the
  name sits at (130, 54) as `0x00015A10` places it (about 5 pixels right of the
  host's), and the unedited name has its highlight panel (`0x000170A0`).
- +480: B returns to slot 1, not slot 2. `0x000DD740` reloads the cursor from
  `DAT_00428FA0`, which nothing writes (only one read in the image), and the
  slot scan runs again, so "Empty" appears later than on the host.

Not translated yet (each raises when reached):

- **Done on the keyboard:** `0x001411C0` (the name check: `0x000FBE50`,
  `0x0010F530`, `0x00141190`), the error dialogs `0x000582A0`/`0x000582E0`,
  `0x00142C20`, `0x00017490`, `0x000DD210`, `0x000DD270`.
- **Saving the new game:** `0x0010D430` and its tree: `0x000E9700`,
  `0x000E97B0`, `0x000EB220`, `0x000E9640`/`0x000E96A0` (the app error screen
  `0x00032990`), `0x0010F320` (the save image through
  `XGWriteSurfaceOrTextureToXPR`), `0x0010F290`, `0x0010E9D0`, `0x0010E270`,
  `0x0010EB60` with the overlapped writes `0x000E9A00`/`0x000E9AF0`/
  `0x000EB180`. The XDK save calls (`XCreateSaveGame`, `WriteFile`, the
  signature calls) are file-IO boundaries; `src/engine/runtime/xapi_runtime.cpp`
  has translations of several of them, but nothing maps `U:` to a host folder
  yet, and a test run needs its own empty save folder.
- **Occupied slots and the menu book:** `0x000F3020` (load the save into the
  profile), `0x00048260`, `0x000483E0`, `0x00048860`, `0x00083C70`,
  `0x00083D10`, `0x00083D90`, `0x000DEB20`'s occupied path.
- **Rows 3 (contents) and 7 (play)**, then whatever they reach.

The switch (removing the gate and deleting `frontend.cpp`, `frontend_slots.cpp`,
`frontend_contents.cpp`, `frontend_keyboard.cpp`, `frontend_text.cpp`,
`MenuCursor` and the `g.*` front-end state) waits for all of the above: a new
game is started from an occupied slot's contents, so the host cannot go before
the save and contents paths are native.
