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
  | 8 | `0x000D4EA0` | `0x000D7A60` | `0x000D7DB0` | `0x000D7E40` | `0x000D7F80` | features ("FeaturesMenu") |
  | 9 | `0x000E1E00` | `0x000E1E10` | `0x000E21A0` | `0x000E2230` | `0x000E4DB0` | |
  | 10 | `0x000E1E00` | `0x000E2B90` | `0x000E2F20` | `0x000E2FB0` | `0x000E4DB0` | scene replay ("ReplayChapterMenu") |
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
numbers now go through one `wsprintfW("%s %d")` helper. The completed splat's
height (`0x000DAD50`) now uses the font's line height (+0x10, `drop`) instead
of its cell (+0xC).

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

Row 8, features (`gameselect_features.cpp`): `0x000D4EA0`..`0x000D7F80`, the
state changes `0x000D8840`, the items `0x000D85A0` and the prompt callbacks
`0x000E4E40` (A: the selected item's action), `0x000D8080` (the stick: up and
down) and `0x000D8030` (B: back to the contents). It shows the book page, the
game's name, the "Features" banner and a button-bat menu of the features:
Artwork (when a challenge is finished or the progress bit 0x3D is set),
Options (row 17), Replay Scene (row 10, past scene one), Replay Chapter (row 9,
past chapter one), Story Books (row 16, not a new game), Demos (when
`D:\cdxmedia\cdx.inx` exists) and Credits (a finished game). The menu lists
them last-added first. Replay Scene, Replay Chapter and Story Books show an
error page from `aid_loctext_ghoulies_dialog_dialogs` when their test fails.
Artwork queues `aid_script_ghoulies_gameselect_unlockableart` through
`0x00083B90` with its scene order held by `0x001234B0`. Demos (the `cdx.xbe`
launch, `0x00032570`) and Credits (its scene order, `0x000F71F0`) raise.

The button-bat menu (`button_bat_menu.cpp`, `DAT_005461A4`): create
`0x00143550`, destroy `0x00143680`, items `0x00143320` / `0x001434E0`, add
`0x001439C0` (each item linked first), open `0x00143B10`, close `0x00143B80`,
tick `0x00143790`, draw `0x00143840`, up and down `0x00143BF0` / `0x00143CB0`,
the paging `0x00144080`, `0x00144180`, `0x00144270`, the layout `0x00144480`,
the records `0x00144360`, the hide `0x001443F0`, the bat's flight `0x00144320`,
the arrows `0x001446F0` / `0x00144690` and their slide-in sprites
`0x00144780` / `0x00144820` / `0x00144880`, with `0x00144030` and the text
pool's `0x0005FFA0` (a record still revealing). Rows 9, 10 and 16 use it too.

`--gameselect-features` names and saves a game, opens its contents, Right to
Features and A. In the menu it presses Down twice (the bat moves to Replay
Scene), A (a new game shows "Sir must first complete Scene One!"), A to close
the page and B, and checks the control is back on row 3. It passes. The page
behind the menu (and behind the delete and naming screens) was black until the
attract's fog was reset at scene teardown (`0x000FD710`, see
NATIVE_CUTSCENES.md, "Fog between scenes").

`--gameselect-replay-scene` checks Replay Scene on a save past scene one. A
child `--gameselect-new-game` run saves a game; the test sets its
`progress.xsv` script (record +0x102) to
`aid_script_ghoulies_chapter1_scene2_1viewcam` and signs the file again
(`0x000E9AF0`'s signature, flags 0), and the native scan and load read it back.
The contents show "Chapter 1, Scene 2 : Trophy Room". In the features menu,
A on Replay Scene opens row 10, which lists one scene ("Chapter 1 : Grand
Hallway", ordinal 0). A on it holds the order game select,
`chapter1_scene0_1book`, `chapter1_scene1_1book`, `chapter1_scene1_2playcam`,
game select and starts `chapter1_scene0_1book`. The test checks the list, the
held order and the started scene. It passes.

Row 10, scene replay (`gameselect_scene_replay.cpp`): create `0x000E1E00`
(0x49C bytes, shared with rows 9, 11 and 16), open `0x000E2B90`, close
`0x000E2F20`, tick `0x000E2FB0`, draw `0x000E4DB0` (shared too), the state
changes `0x000E3760` and the scene list `0x000E3560`. The list has one item per
scene before the save's: its ordinal for an unfinished game in a chapter
numbered below 5, 100 for a finished game with save word +0x260
(`0x0010E120`), otherwise the save's +0x25D byte (`0x0010E0E0`) plus 0x57. Each
item reads "Chapter N : <scene name>" (`replayscene_chapter`, the chapter
table at `0x002FE698`, and `0x0011ED80`, now `gameselect_scene_name` in
`gameselect_progress.cpp`). The item's action `0x000E3130` builds a heap scene
order of game select, every script of that scene in the held order and game
select, releases the old order, holds the new one with the heap release
`0x000358F0`, sets the next row to 10 and queues the scene's first script
(`0x00083B90`, mode 2). The prompts ("ReplayChapterMenu", `0x003095D0`) use
the features menu's A (`0x000E4E40`) and stick (`0x000D8080`) and their own B
(`0x000E30E0`, back to row 8). Page Up (Y, `0x000E4EC0`) and Page Down (X,
`0x000E4ED0`) jump to the button-bat menu's `0x00143D60` and `0x00143F30`
(`button_bat_menu.cpp`). With every item on one page (and wrap) they select the
first or the last item. Otherwise Page Down turns to the page below its visible
rows through `0x00144080` and selects its first item. Page Up turns to the page
above (`0x00144180`) and selects that page's first item, leaving +0x4C at 1. An
item's context is released through `0x000663E0` (HeapFree, the heap boundary);
a host item holds the block's value, so the release only clears it.

`--gameselect-replay-paging` is the replay test on a Chapter 1 Scene 10 save:
nine scenes, more than the menu's eight rows (`0x00429A50`). X from the first
selects the ninth, alone on its page; Y goes back to the first. It passes.

`--gameselect-retail-save` loads a save the retail game wrote: a finished game
("Paul", at `chapter5_scene13_3book`, +0x188 and +0x260 set). The test copies
the title's U: folder from `GBTG_RETAIL_SAVES` (default
`../Saves/Xbox/4d530053`, outside the repository) into the empty test save
folder. The native scan verifies its `progress.xsv` signature, so the host's
keyed signature (`0x0014CEF5`..`0x0014CF57`) matches the console's. The contents
show the COMPLETED splat on Play and the features list View Staff Credits. The
scene replay lists all 100 scenes, from "Chapter 1 : Grand Hallway" to
"Chapter 5 : Front Gate". X, X and Y page to items 8, 16 and back to 8, and B
returns to the features (row 8). It passes.

The script VM (`0x00117140`) picks the next scene from the held order:
`0x00035960` (`next_scene_order_name`, `runtime/name_lookup.cpp`) with EAX
`&DAT_00511B64`, EBX the current scene AID (`DAT_00510E58`) and wrap 1. Boot
holds the game's order (`*DAT_00430CF8`, `aid_aidlist_ghoulies_sceneorder_game`)
the way `0x0011C3F0` -> `0x0011C480` -> `0x001234B0` does
(`SceneActorServices::hold_boot_scene_order`), so the attract reel, the title
and game select run on that order; game select (`0x00083660`) holds it again.
After a replay the held order is the replay's, so the replayed scene returns
to game select. The artwork's script now follows its own held order
(`aid_sceneorder_gameselect_unlockableart`; no test runs it on).
`--gameselect-replay-scene` runs the replay on: the books advance through the
VM (game select -> `chapter1_scene0_1book` -> `chapter1_scene1_1book` ->
`chapter1_scene1_2playcam`). With no player driver for the opening's fight,
the test sets the VM's done latch (`DAT_0050E788`) 60 frames into the opening,
and the VM requests game select, which opens on the Replay Scene page.
`--player-fight` plays the opening to its real end on the boot order and
reaches `chapter1_scene2_1viewcam`. Still
host-side: the boot's first request, the `--scene` select and the debug UI
read `Game::scene_order`, which game.cpp parses from the same list. The VM's
`DAT_0050E784` branch and its timer's second `0x00035960` / `0x0011EB30` are not
translated.

Row 6, challenges (`gameselect_challenges.cpp`): create `0x000D5850` (0x1E0
bytes), open `0x000D5880`, close `0x000D62A0`, tick `0x000D6410`, draw
`0x000D67F0`, the state changes `0x000D70E0`, the selection `0x000D7640` and
its helpers (`0x000D5860`, `0x000D6A30`, `0x000D6EF0`, `0x000D6F40`,
`0x000D6F90`, `0x000D6FE0`, `0x0004A3B0`, `0x0004A3D0`, `0x00049B60`). The open
reads `aid_aidlist_ghoulies_challenges_challengeorder`: 21 params resources
(+0 the scenenames key, +0x40 the script, +0xC0 the picture, +0x140 the four
trophy scores). Challenge n (from 1) unlocks with 5n bonus books (the bits of
`DAT_00506C54`); challenge 21 is always unlocked, shown once the progress bit
0x25 is set, and plays the `DAT_004E1740`-th script of
`aid_aidlist_ghoulies_sceneorder_challenge21`. The menu is a carousel of five
(`0x00305C48`) over the selected one's picture, each with its number (the
scoreboard counter `0x0012F000`, drawn at once: `0x0012F8F0`'s EAX branch is
now translated) and its trophy (`DAT_004E194C`: 0 unplayed, 3..6 bronze to
platinum), the bonus-book count and the "challengeMenu_basic" prompts
(`0x00305CD8`): A `0x000D7770` (play), the stick `0x000D77C0`, B `0x000D7890`
(back to row 3), Y `0x000D78E0` (the "challengeintros_" page) and Black
`0x000D78F0` (row 11, untranslated). Playing sets the trophy scores
(`DAT_004E17DC`), the key (`DAT_004E17EC`) and the game's name
(`DAT_004E19B4`), and queues the challenge's script (`0x00083B90`, mode 3).
The selection globals `DAT_00506578` / `DAT_0050657C` / `DAT_00506580` and
`DAT_004E17DC` are BSS that game select never resets
(`g_challenge_selection`); the dialogue's trophy and score tags read them
live (`0x00057330`, `0x000573A0`).

`--gameselect-challenges` loads the retail save (as `--gameselect-retail-save`),
A on its slot, Right twice and A on Challenges: the menu opens on challenge 1
of 20 ("Billiard Room Bust-up!", 76 bonus books), Right turns to challenge 2,
Y shows its Rules page and A closes it, and A plays it: game select starts
`aid_script_ghoulies_challenges_bringouttheimps` (challengeId_Survive). Cooper
is left idle, so the imps hit him after 3 seconds and the run fails: the
results page reads "Sir failed to win a medal!", the restart menu ("Back to
Menu" / "Restart Challenge") shows over the scene, and B returns to the
challenges on challenge 2, whose best score (3) and trophy word (2, none) are
in memory and in the save's `progress.xsv` (+0x1B4, +0x208). It passes.
`--gameselect-challenge-restart` presses A on the restart menu instead: the
challenge runs again in the same scene, and B on its second menu quits. It
passes.

The challenge runtime (`scene/challenge_runtime.cpp`, `g_challenge_run` for
`DAT_004E17C8`..`DAT_004E1A34`, bound per scene as `ChallengeRuntimeServices`):

- **Set-up** (script handler `0x001251E0`): type 0x7A names the challenge
  (`0x000FBFB0` over the nine challengeId_* names) and makes its requirement
  pool (`0x0004A100`: Attacks, Buttons, Challenge21, Damage, Health, Kills,
  KillsBOB, Survive, Time, each with its default requirements and the
  player-alive rule `0x0004A0E0`); 0x7B starts it (`0x0004A240`); 0x7C..0x86
  add requirements (`0x0004ADD0`, the 19 challengeReqId_* ops of
  `0x004231C8`, each with its scoreboard message `0x0012FCA0`). Challenge 21
  starts from the scene start instead (`0x00049BA0`).
- **States** (`0x0004A4D0`, ticked by `0x00049DE0`): 2 starting, 3 running
  (the messages' counts `0x0004AC00`, the tests `0x00049790`..`0x00049B50`
  through `0x0004AB40`: a passed requirement goes, a failed one ends the run),
  4 finished (the pause, the score and trophy `0x0004AE80` with
  `0x0004A400`, the trophy's ditty, a 2.5 or 6.5 second wait), 5 the awards
  page (`0x0004B070`: the best score and trophy kept, the save written
  `0x000F2E20` with the new `0x0010E680` / `0x0010E740`, "challengeawards_" +
  key + trophy, and the challenge-21, first-platinum and platinum-21 pages),
  6 the restart menu ("challengeMenu_restart" `0x002FE2E0`, its own input
  device on the input stack, the players' message 0x4D reactions
  `0x0004A990`; A `0x0004B350` restarts `0x0004A310` / `0x00049C80`, B
  `0x0004B360` quits to game select row 6, `0x00049C00`). The view present
  draws its menu (`0x0011C240`'s `0x00142720(0)`). The scene teardown ends a
  run (`0x00049FF0`), and the scene start's reset (`0x00116E00`) clears it.
- The damage challenge's captions are native: the caption pool (`0x0012DCD0`),
  the refresh `0x0012DDB0` with its "caption: $points" lines (`0x0012DF30`,
  `0x0012DFD0`) and "Total Damage: $" (coreterms), and the grouped number
  `0x000FBD30`. Still untranslated (raising when reached): the save error
  screen (`0x000F32F0`).

Every challenge has been played from the menu. `GBTG_CHALLENGE=n` makes
`--gameselect-challenges` turn the carousel to challenge n and play it without
its Rules page; `GBTG_CHALLENGE_FIGHT` hands the run to the scene driver
(`play_scene7_run`, fighting at any range, which now presses B on a restart
menu); `GBTG_CHALLENGE_BOOKS` marks every bonus book and the challenge-21 bit
in the copied save's blocks in memory (a driver cheat: the retail save's 76
books unlock challenges 1..15). Results (2026-10-30):

- 1, 2, 3, 5..13 and 15 end by themselves with Cooper idle (a time limit, a
  hit, or a requirement passed) and pass: results page, restart menu, B back
  to the challenges, the best score and trophy in `progress.xsv`.
- 9 (Smash the Study Buddy, challengeId_Damage) passes since the captions
  above.
- 4 (Disco Divas) and 14 (Kick 'em Quick) have no time limit; they pass with
  the driver, which fights until Cooper faints and the run fails.
- 16..20 pass with the driver and the books cheat.
- 21 (scene order `aid_aidlist_ghoulies_sceneorder_challenge21`, started by the
  scene start `0x00049BA0`) plays its first scene,
  `chapter1_scene1_2playcam`, and moves on to `chapter1_scene2_1viewcam`. The
  story scenes it chains need their own story drivers (the trophy heads'
  Super Scary answers here), so no test plays it to its end, and the
  `DAT_004E1740` step count it resumes from is not checked.

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

- **The other rows**: 19 (final score, from
  contents Info when Play is unavailable), 7 (bonus book collection) and 5,
  9..17 (9, 10, 16 and 17 from the features menu; also requested on the way
  back from gameplay, `0x0011CFB0` and `0x0004A470`). Row 19 was missing from
  the host table, so Info raised "past the table"; it is now listed and raises
  as untranslated. The features menu's Demos (`0x000D8400`, the `cdx.xbe`
  launch) and Credits (`0x000D8420`). A read-only walk from
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
