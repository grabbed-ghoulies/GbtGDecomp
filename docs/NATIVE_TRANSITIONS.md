# Scene transitions

The scene transitions (the chapter book with its map route, the slime "goop" wipe and the two fades) run on
translated retail code driven by the retail transition state, on the boot-owned scene stack, and present through
`SceneD3DDevice` like every native scene. The host transition is deleted.

Status (2026-11-08): the transition core, its handler table and all four handlers are translated (fade 0, book 1
with every clip and the map route and menu, slime 2, and handler 3). A handler slot outside the table raises.

Read `AGENTS.md` first.

## Where retail runs a transition

- The scene record `DAT_00510E58` is driven by `0x0011E4C0` (the frame's script
  pump) and `0x0011E6A0` (its state changes). A requested aid enters state 3,
  which starts a transition (`0x0012C5D0`) unless one is running; state 4 tears
  the scene down (`0x0011C5A0`) and loads the next; when the load finishes and
  the transition is in phase 2, `0x0012C770` starts its exit (phase 3); leaving
  state 5 ends it (`0x0012C4E0`). `0x0012C340` ticks it every frame.
- `DAT_00546328` is the transition row (the 18 rows of 0x110 bytes at
  `0x00431478`, chosen by `0x0012C2D0` from the scene name), `DAT_0054632C` the
  phase (1 entering, 2 waiting for the load, 3 leaving).
- Each row's +0 selects a handler at `0x003129AC` (4 records of 0x18 bytes):

  | Handler | +0 tick | +4 draw | +8 end | +0xC begin | +0x10 exit | +0x14 |
  |---|---|---|---|---|---|---|
  | 0 fade | `0x0012BC80` | `0x001628B0` | `0x001628B0` | `0x0012BCE0` | `0x0012BD50` | `0x0012B360` |
  | 1 book | `0x0012B370` | `0x0012B560` | `0x0012B5A0` | `0x0012B640` | `0x0012B790` | `0x0012BDC0` |
  | 2 slime | `0x0012BDD0` | `0x0012BE60` | `0x0012C1A0` | `0x0012C1D0` | `0x0012C270` | `0x001628B0` |
  | 3 | `0x0012BDD0` | `0x0012BA10` | `0x001628B0` | `0x0012BC70` | `0x001628B0` | |

  (`0x001628B0` is a bare `RET`.) The book handler drives the menu book's
  translated owner, the `0x00048260` family (`native_book.cpp`), including the
  map route (`0x00046F40`, `0x00047770`, `0x000473D0`, `0x000475B0`).

## The lifetime problem

A transition opens in the outgoing scene, survives its teardown and the next
scene's load, and closes over the new scene. Retail keeps one resource registry
(`DAT_004DF250` records, `DAT_004DF09C` loaded rows, the bundle pools at
`DAT_004DEDA8`) for the whole run. Resources are reference counted, and the
scene teardown (`0x0011C5A0`) ends with a purge: `0x00035D30` unloads every
record whose count is 0 (`0x00035C60`), `0x00036180` flushes the deferred
release lists, repeated until nothing is left, then `0x00034880` releases the
bundle pools nothing references. The transition's book model, cutscene and page
textures still hold references, so they survive.

The port builds a fresh `SceneDialogueResourceOwner` (registry, relocated
resource memory, D3D records, fonts, model loader) inside each
`SceneFactoryRuntime`, and destroys it with the scene. Nothing native can
outlive a scene.

### Direction: boot-owned like retail

The target is retail's lifetime: the object factory and actor lists, the actor
services (`SceneActorServices::State`), the cutscene runtime, the text pool
registry (`DAT_005043CC`), the model renderer and the D3D device are boot-owned,
and a scene teardown (`0x0011C5A0`) only retires what answers message 0x80
(`0x00122C70`, then `0x001232D0`) and purges unreferenced resources. Most of
`State`'s constructor is boot work in retail (renderer, boot shaders, node
callbacks, classes); the per-scene part is the markers, controls, dialogue,
background and doors. The resource owner and the D3D device are boot-owned
already (work item 1). The transition runtime below is an interim step on the
way there; prefer moving more of the scene stack to boot over growing a second
copy of it.

## Progress

- **Work item 1, the resource owner, is boot-owned.** `Game::native_resources`
  holds the `SceneDialogueResourceOwner` (registry, relocated memory, D3D
  records) and now the recording `SceneD3DDevice`, whose created vertex shaders
  the boot-owned shader cache hands out by handle. `SceneFactoryRuntime` keeps a
  reference. The unload path is translated (`anim_resources.inl`):
  `0x00035C60` (deferred through `0x000360D0`), `0x00036180` (with
  `0x00036070`, `0x0013A980`, `0x00035650`), `0x00035D30` and `0x00036210`.
  `frame_tick` flushes at the start of the present as `0x00033A90` does, and
  the scene load runs `0x0011C5A0`'s purge loop after the old scene goes.
  `0x00034880` (freeing bundle pools) has no counterpart: the host `AidStore`
  keeps its files. Each scene's `State` removes the callout hook it registers,
  since its context dies with it.
- **What stays referenced across a teardown** (releases the port does not
  translate yet): `aid_model_ghoulies_actor_dialogbat` (28), `..._buttonbat`
  (7), `aid_model_ghoulies_dialogs_backdrop1` (7), and the intro cutscene
  models (`introtablebasic`, `ambergrabbedbook`, `enteringthehouse`). Their
  counts do not grow from scene to scene.
- **The scene stack is boot-owned** (the "Direction" above). `Game::factory_scene`
  (the object factory, its address map and the scene controls) and its
  `SceneActorServices` are made once and live for the run. The services'
  constructor is the boot work; `begin_scene` copies the scene record's name
  (the spawn and player bindings read it), loads the markers (`0x0011CB80`) and,
  from the second scene on, resets the per-scene data that `0x0011C5A0`,
  `0x0011C480` and `0x00116DA0` clear (`reset_scene_data`). A scene ends in
  `0x0011C5A0`'s order (`SceneFactoryRuntime::end_scene`): an open dialogue
  closes (`0x000584D0(0)`), the scene cutscenes go (`0x0004ECE0`, released in
  place) and the doors' registrations; the scene controls are released
  (`0x00115F00`); message 0x80 goes to every active actor (`0x00122C70` over
  `DAT_00511AA4`, retiring them) and the retired actors are removed
  (`0x001232D0`); then the scene's other objects go and the services' scene
  part ends (effects, the billboard queue, the door pool). Cutscene track
  actors that a live cutscene still holds stay; the others go with the scene.
  The scene dialogue, the scene cutscene stack, the movie texture lock, the
  input devices and the controls' contexts stay bound for the run.
  `--attract` now counts the second attract script's first clip (9 clip
  starts): the cutscene stack no longer disappears between the two scripts.
  The scene's objects other than the controls are still released wholesale,
  where retail frees each kind in its own teardown routine (`0x001222A0`,
  `0x00110AB0`, `0x001143A0`, `0x00123640`, `0x001199F0`, `0x0011F610`, ...,
  not translated).
- **Sizing the transition runtime.** The book's services are the scene's whole
  actor-services stack: the cutscene context (its track actors, the book model
  among them), the model renderer, the display setup, the node states and the
  draw context. A transition runtime separate from the scene would be a second
  `State`. See "Direction" above.

- **Work item 2, the transition core, is translated** (`scene_transitions.cpp`,
  `scene_transition.h`). `RuntimeSceneTransition` is the retail block at
  `DAT_00546328`; `0x0012C340`, `0x0012C4E0`, `0x0012C5D0`, `0x0012C770`,
  `0x0012C2D0` (the row from the directory at `0x0042AC80`, through
  `0x00116220`) and `0x0012C830` dispatch through the handler table's slots
  as retail does. The loading text (`0x0005F390`, `0x000FF0A0`, `0x0005F580`,
  `0x0005F550`, `0x0005F460`) and the camera release (`0x00048F10`) are
  translated. The boot-owned services bind `SceneTransitionServices`; the
  present runs `0x0012C490` where `0x0011C240` does, so a transition draws
  between scenes too.
- **Work item 3, the handlers.** Fade (0), book (1) and slime (2) are
  translated, and so is handler 3, which only row 16 reaches. The book's clips
  0, 1, 13 and 14 (plain), 2 (the chapter title, `0x0012B810`, `0x0004B550`,
  `0x0004B5A0`) and 3 to 7 (maps) run, with the book page capture
  (`0x00048260`).
- **The book's map route is translated** (`book_map.cpp`): `0x00046F40`
  (begin), `0x00047370` (close), `0x000473D0` (release), `0x000475B0` (tick),
  `0x00047770` (draw), with `0x00046760` (the spot labels), `0x00046500` (the
  six collectible markers), `0x00046BA0` (a point along a route) and
  `0x00046E10` (its projection). The map block is the book model's block of
  type 0x16; its words are offsets in the model's CPU view. Each point is
  posed by `0x00031530` on the model's pose. The menu path (`+0xE8` set:
  `0x00141DA0`, `0x00109340`, `0x00142460`, `0x00142070`, `0x001093C0`) is
  translated too.
- **Presenter additions** for the map: the recording device takes
  `SetVertexData4f` on register 0 (`D3DVSDE_POSITION`) as well as -1, and
  vertex format 0x44 (`D3DFVF_XYZRHW | D3DFVF_DIFFUSE`); the fixed-function
  presenter draws pretransformed vertices and widens lines by
  `D3DRS_LINEWIDTH` (D3D11 lines are one pixel).
- **Work item 4, the switch, is done.** `transition_book.cpp`,
  `transition_data.h`, `draw_scene_transition` and the host branch of
  `frame_tick` are deleted, with the host cutscene props
  (`cutscene_runtime.cpp`), `g.cutscenes`, `g.models` (`load_scene_models`)
  and the host movie list (`movie_alloc`, `movie_present`, `movie_apply`).
  `parse_cutscene` stays in `src/gbtg/` as a format parser with no game
  caller. No `pcm_*` cue caller is left outside the Bink clip's own audio.
- **Still open** (raising when reached): the book's chapter title (clip 2:
  `0x0012B810`, `0x0004B550`, `0x0004B5A0` and its widgets), a book page
  texture (`0x00048260`'s third argument: `0x00035E90`, `0x000F86F0`, and its
  release in `0x00048860`), handler 3 (row 16), the map menu, and the
  widescreen page redraw (`0x0005CE80`).
- **The text pools draw every frame** as `0x0005F330` does in `0x0011C240`
  (over `DAT_005043CC`, masked by `DAT_0050EA9C`), not only while a script
  dialogue is bound. The map labels and the loading text depend on it.
