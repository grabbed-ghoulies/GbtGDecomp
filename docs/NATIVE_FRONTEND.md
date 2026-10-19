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
  table at `0x0030A000` (20 rows of 0x18 bytes: create, destroy, open(owner,
  widget), close, tick, draw). `0x000E5AE0` picks row `DAT_00429BC0` when music
  stream 2 is idle, else row 18 (wait for the stream). Boot (`0x000E5A80`)
  stores 1. A mode asks for another by writing the control's +0x94; the tick
  switches when `0x00083E20` allows (the menu book's page threshold), closing
  and destroying the old widget first.

  | Row | Create | Open | Close | Tick | Draw | Screen |
  |---|---|---|---|---|---|---|
  | 1 | `0x000DD730` | `0x000DD740` | `0x000DDC90` | `0x000DDD80` | `0x000DDF20` | slot select |
  | 2 | `0x000DB720` | `0x000DB730` | `0x000DB950` | `0x000DB9E0` | `0x000DBB90` | delete a game ("GameSlotDeletionMenu") |
  | 3 | `0x000D8AB0` | `0x000D8B10` | `0x000D9300` | `0x000D9450` | `0x000D9640` | slot contents (Play starts the game) |
  | 4 | `0x000DCEB0` | `0x000DCEC0` | `0x000DD030` | `0x000DD0A0` | `0x000DD170` | naming (virtual keyboard) |
  | 5 | `0x000DC0B0` | `0x000DC0C0` | `0x000DC640` | `0x000DC6F0` | `0x000DC9A0` | |
  | 6 | `0x000D5850` | `0x000D5880` | `0x000D62A0` | `0x000D6410` | `0x000D67F0` | |
  | 7 | `0x000D4EA0` | `0x000D4EB0` | `0x000D5160` | `0x000D51E0` | `0x000D5300` | bonus book collection ("BonusBookCollectionMenu") |
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
  | 19 | `0x000D79A0` | `0x000D79B0` | `0x000D79F0` | `0x000D7A00` | `0x00082F70` | final score (`0x000D3E30`.., "finalscore"), contents Info when Play is unavailable |

  Every row destroys with `0x000D8AC0` (HeapFree on the widget's thread heap).

  Each prompt row set (0x310 bytes) holds the menu name, then entries of 0x48
  bytes from +0x40: the button code (0 A, 1 B, 2 X, 3 Y, 4 Black, 5 White,
  6 and 7 the sticks, 8 the triggers; masks in `0x00142460`), the label key and
  the callback (+0x84 for the first). A code of -1 ends the set.

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

- **Native.** Game select runs on the translated control for every run; the
  host front end is deleted. See "Progress" below.

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

Step 4 is done: type 0x9A is installed for every scene and the gate is gone.

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

Status: **switched.** Game select runs natively in every run. The host front
end (`frontend.cpp`, `frontend_slots.cpp`, `frontend_contents.cpp`,
`frontend_keyboard.cpp`, `frontend_text.cpp`, `frontend_types.h` with
`MenuCursor`, `frontend_progress.inl`, the host naming background and menu book
in `transition_book.cpp`, the host movie list's front-end openers, `load_ui` and
`g.ui`, and every `g.*` front-end field) is deleted. Two helpers it shared
moved: `script_chapter_index` (0x0004B4D0) to `scripts/script_chapter.cpp`
and `menu_color` (0x00101730) to `graphics/color_blend.cpp`, and the map label
the host transition draws now lives in `transition_book.cpp`.
`--title`, `--title-a` and `--gameselect` pass on the native control;
`--gameselect-native` is the same run. Their captures match the native results
listed under "Differences" below (the +60 scan and prompts come up later, and B
returns to slot 1 as `0x000DD740` does).

Translated and live:

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

Differences from the host captures:

- +300: the key bat and the caret bat are drawn (the host draws neither), the
  name sits at (130, 54) as `0x00015A10` places it (about 5 pixels right of the
  host's), and the unedited name has its highlight panel (`0x000170A0`).
- +480: B returns to slot 1, not slot 2. `0x000DD740` reloads the cursor from
  `DAT_00428FA0`, which nothing writes (only one read in the image), and the
  slot scan runs again, so "Empty" appears later than on the host.

Saves (`src/engine/saves/`):

- **The save folder.** The Xbox's `U:` (the title's UDATA folder) is the
  `saves` folder beside the executable (`save_root.cpp`, created on first use;
  `set_save_root` swaps it, and the transition tests use an emptied
  `cache/test_saves`). That path mapping (`xbox_host_path`) is the only step
  that leaves the retail flow: `XCreateSaveGame`, `XFindFirstSaveGame` /
  `XFindNextSaveGame` / `XFindClose`, the volume queries, the signature calls
  (`xdk_saves.cpp`, on the `xapi_runtime.cpp` helpers) and the asynchronous
  file rows (`movie.cpp`'s `CreateFileA` / `WriteFile` boundaries) all go
  through it.
- **The save manager** (`DAT_005462E4`, `save_scan.h`, `save_names.cpp`,
  `save_games.cpp`): the boot scan `0x0010F1C0`, create `0x0010D430` (with
  `0x000E9700`, `0x000E97B0`, the save image `0x0010F320`, `0x0010E9D0`,
  `0x0010EB60`'s signed `progress.xsv` write through `0x000E9A00` /
  `0x000EB180` / `0x0010ED20`), load `0x0010D910` (`0x0010ED70`, `0x000E9880`,
  `0x0010EF10` with the signature check `0x000E9B70`), unload `0x0010D9B0`,
  the write queue `0x0010DA80` / `0x0010F2D0` / `0x0010F440` (the frame tick's
  `0x00033BF2` step) and the record accessors `0x0010DB10`..`0x0010E270`,
  `0x0010F290`, `0x0010F530`, `0x0010F620`.
- **The profile:** `0x000F2F00`, `0x000F2AC0` and `0x000F3020` (the challenge
  block `DAT_00506C54` and the progress words `DAT_004E18F8` / `DAT_004E194C`
  / `DAT_004E1740` / `DAT_004E1744`).

A new save is the retail layout: a 12-hex-digit directory (the name's hash)
holding `SaveMeta.xbx` (`Name=My Game`, `NoCopy=1`), `SaveImage.xbx` (one of
the three savegame textures as XPR0) and `progress.xsv` (the 0x268-byte
record, the progress, challenge and unlock blocks and the signature, padded
to 0x400). The record's three block pointers are written as 0.

Keyboard Done and slot select with saves: `0x001411C0` (with `0x000FBE50`,
`0x00141190`), the Done key's `0x00142C20` / `0x00017490` / `0x00142070`,
`0x000DD210`, `0x000DD270`, `0x000DD6C0`'s save state; slot select's
`0x000DEB20` (statuses 2, 3 and 4: the chapter plate, movie and name, the
no-write and broken discs), `0x000DE9E0` and `0x000DF140` (the chapter's
movie and background through `0x0004B4D0`), `0x000DF5D0` / `0x000DF650`
loading with `0x000E0640`, `0x000DF790`, `0x000DF7E0`, `0x000DF880`.

`--gameselect-new-game` (native) presses A on the empty first slot, A on Done
with the default name, A on the new game's slot and A on Play. It passes: the
save is written to `cache/test_saves`, slot select shows "My Game" in slot 1
(its chapter plate, movie, background and the Delete prompt), the menu book
turns to the contents (three chapters, "Play" over "Chapter 1, Scene 1 : Grand
Hallway", Back / Select / Info / Move), and Play starts
`aid_script_ghoulies_chapter1_scene0_1book` natively ("Game Intro.").

The progress line (`0x000DA040`) once stopped after "Chapter 1": its chapter
number was converted from the begin and end of two different temporary
strings, which copied stray memory (a NUL among it) into the line. Both
numbers now go through one `wsprintfW("%s %d")` helper.

The native transition book (`src/engine/transitions/native_book.cpp`):
`0x00048260` (create: the clip row at `0x00422078`, its cutscene and
"transition" camera, the book model's four page shaders `0x00048110` /
`0x000F8540` / `0x0015B2B0`, the camera pair `0x000481E0`), `0x000483E0`
(tick, with `0x000475B0`'s no-map case), `0x00048860` / `0x000473D0` /
`0x000F85D0` (release) and `0x00048550` (draw: the page captures, the view
clears `0x00061B40`, the camera and the cutscene). It is the capture book
(`DAT_005462A0`) each view's present draws. The page textures `DAT_005047D8`
and `DAT_005047E4` (boot `0x000620C0`) are made in each native scene's
resource memory (the host has no native boot arena, and `DAT_005047E4`
aliases retail's depth buffer); `D3DDevice_CopyRects` from the back buffer is
a recorded command (`SceneD3DCopy`) the presenter reads back into the
texture. The game-select control drives it: `0x00083D90`, `0x00083E20`,
`0x00083C70` / `0x00083D10` (the start), and the slot and contents
callbacks open and release it.

Row 3, contents (`gameselect_contents.cpp`): `0x000D8AB0`..`0x000D9640`, the
state switch `0x000DA580`, the chapter thumbnails, movies and backgrounds
(`0x000D9C20`, `0x000D9F10`, `0x000D9F60`, `0x000DA1B0`..`0x000DA530`,
`0x000D9D10`, `0x000DACE0`), the progress line (`0x000DA040` with
`0x0004B4D0`, `0x0004B530`, `0x0011EBC0`, `0x0011EC70`, `0x0011ED30`,
`0x0011ED80`), the completed splat (`0x000DAD50`, `0x000DAED0`,
`0x000DAF10`), the unplayed-challenge mark (`0x0004A470`), the secret button
sequence (`0x00142E50`, `0x00142F00`, `0x000DB1A0`, with `0x00108500`), and
the prompt callbacks `0x000DB450` (move), `0x000DB510` (back, book clip 1),
`0x000DB570` (Play: `0x00083B10`, the port claim `0x00107F50` /
`0x00109780`), `0x000DB620` (row 19), `0x000DB670` (challenges, row 6) and
`0x000DB6D0` (features, row 8).

Row 2, delete (`gameselect_delete.cpp`): `0x000DB720`..`0x000DBB90`, the
state switch `0x000DBDC0` and the "GameSlotDeletionMenu" callbacks `0x000DBD50`
(B, back) and `0x000DBC20` (A, delete). The save manager's delete is
`0x0010D5E0` with `0x000E9CA0`, `0x0010E1A0`, `0x0010F100` (XDeleteSaveGame,
`0x0014C972`, removing the save's directory under the save folder) and the
name pool's `0x000663B0` / `0x00066380`; GetFileAttributes (`0x0014B280`) is a
file-IO boundary through the save folder. `--gameselect-delete` names and saves
a game, presses X (Delete) on it and A on "Yes": the save folder is empty and
slot select shows "Empty" again.

The stats dialog (`0x000DB5E0`, Y on Play): "pause_stats" through the scene
dialogue's `0x000582E0`. `--gameselect-stats` opens a new game's contents and
presses Y; the native dialogue shows the time, the four counters and the A bat.

Engine fixes the front end needed:

- **Stale texture decodes** (the random corruption of glyphs, plates and
  backgrounds). The presenter's decoded images (`texture_images`) were keyed by
  the host address of each texture record and never dropped; the upload cache
  (`g.sprites`) is keyed by the image's address and serial. A freed scene
  resource block's bytes, or a later scene's image, could reuse an address and
  draw with another texture's pixels, depending on allocation timing.
  `SceneResourceMemory::on_free` now drops the decodes inside a freed block
  (and their uploads), and the scene's teardown releases every upload it made.
  Four `--gameselect-stats` runs now give pixel-identical captures.

- **The input stack is boot-owned** (`src/engine/frontend/input_stack.cpp`):
  the entry pool, the selected entry and the port masks persist across
  scenes, and boot pushes the "Scene Master Pad" entry (`0x0011C3F0`). The
  native scenes used to rebuild an empty stack each time, so the first
  dialogue after game select read no device.
- **The scene teardown removes retired actors** (`0x0011C5A0`'s
  `0x001232D0` after the controls go): the contents bat, retired by the
  control's widget at teardown, otherwise stayed in the active list of the
  next scene.

Not translated yet (each raises when reached):

- **The other rows**: 6 (challenges, from contents), 8 (features, from
  contents: its menu `0x000D8840` / `0x000D85A0` and the item callbacks
  `0x000D80E0` (row 17), `0x000D8130` (unlockable art), `0x000D81D0` (row 10),
  `0x000D8290` (row 9), `0x000D8360` (row 16), `0x000D8400` (`cdx.xbe`) and
  `0x000D8420` (credits)), 19 (final score, from contents Info when Play is
  unavailable), 7 (bonus book collection) and 5, 9..17 (also requested on the
  way back from gameplay, `0x0011CFB0` and `0x0004A470`). A read-only walk from
  these rows and callbacks reaches about 226 untranslated functions (61 KB).
- **The secret sequence's save** (`0x000DB380`'s `0x000F2A50`, `0x0010E4F0`,
  `0x0010DA80`) and the book's map routes (`0x00046F40`, `0x00047770`) and
  page texture (the third `0x00048260` argument).
- **The app error screen** (`0x00032990` / `0x00032BA0`, ticked by
  `0x00032AB0` and drawn from `0x0011C240`; its flag `0x40000` holds the
  scene): the save errors (`0x000DD2F0`, `0x000DD3A0`, `0x000DD420`,
  `0x000DD4A0`, `0x000DF6B0`) and the delete failure (`0x000DBCD0`). Only
  host approximations of `0x00032AB0` / `0x00033480` exist, in
  `gfx_d3d11.cpp` (step 6).
- The name errors (`0x000582A0`'s keyboard messages) and the Japanese keyboard
  (`0x00012920`).
