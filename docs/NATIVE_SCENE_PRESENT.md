# Milestone: native scene present

Goal: a natively started scene presents only what retail's per-view present
(`0x0011C240`, called for each active camera slot by `0x00033A20`) records:
the world, the actors, effects, HUD and dialogue. All of it goes through the
recording D3D boundary (`SceneD3DDevice`) and the D3D11 presenter. No host draw
(`g.models`, `draw_cutscene_props`, `draw_ui`, the body camera) runs in a
native scene.

This is step 2 of `docs/LEGACY_HOST_CODE.md`. Read `AGENTS.md` first. The actor
bins and `0x00121900` are covered in `docs/PLAYER_SCENE.md`.

## Retail order

`0x0011C240` per view:

1. `0x0011F690`, `0x00121DC0`, then `DAT_0051120C = 0` and `0x0012AB10`.
2. The actor bins: `0x00121730`, `0x00121880`, `0x00121900` (passes 0/1 and
   the world helpers), `0x00121D40` (pass 2).
3. `0x00121EC0`, the control list `DAT_0050E438` (vtable +0x14 on each
   entry), and `0x000ED390` for each nonzero word in `DAT_00506930..0x00506C50`.
4. `0x000EC470`, `0x000EBCB0`, `0x0005C250`, `0x0011AF90`, `0x00119230`,
   `0x0011BA50`, `0x000615E0`.
5. `0x00058150` (dialogue). When `DAT_004DE9AC` is set: `0x00048BC0` and
   `0x0004DBD0`. `0x00142720(0)` when `DAT_004DE9A0` is set or
   `DAT_004E17C8 == 6`.
6. `0x00122230`, `0x00132290`, then `0x0011DB50`, `0x00048550(DAT_005462A0)`,
   `0x0012C490` and `0x0005F330`. `DAT_0050EA9C & 0x2000` moves `0x0005F330`
   from first to last of these.

## Reference screenshots

Taken before this step with
`gbtg --headless --frames N --scene chapter1_scene1_2playcam --width 640 --height 480 --screenshot …`
(`cache/native_world_ref/scene_N.bmp`), plus a default boot run
(`cache/native_world_ref/boot_900.bmp`):

| Frame | Screen |
|---|---|
| 120 | Slime transition into the hall, a strip of the hall visible at the top |
| 180 | Hall, Cooper and the imps, camera further back |
| 240 | Hall, play camera |
| 300 | Hall, play camera (Cooper, five imps) |
| 450, 600, 750 | Native tutorial page (backdrop, head, corner bats, text) |
| 900 | Tutorial page, A-button bat |
| boot 900 | Attract: bookshelf backdrop, "Press START" |

## Current state (2026-10-15)

- **The background draws natively.** The background's vtable +0x14 is
  `0x00042C10`: nothing while `DAT_004EE1EC` (the dialogue mode of
  `0x000584D0`) is set, otherwise a tail jump to the actor draw `0x0003C9E0`
  (`draw_runtime_background`, `entities/actor_frame_render.cpp`). Its model
  tree reaches two new node slots:
  - `0x001571B0` (ndShaderParam2 while `0x001572C0` selects the background
    variant): `0x00157140` classifies the child `'B'` (ndBGPushBuffer) boxes
    against the render state +0x64 frustum (set by `0x00156350` from
    `0x000F5FD0`) with `0x00159BA0`, marks each one's +0x3F, and skips the
    group through `0x001589C0` when none is visible. Otherwise it is
    `0x00157090`.
  - `0x001580E0` (ndBGPushBuffer): a push buffer whose instance key (copy +4,
    -1 from `0x001580C0`'s copy slot) differs from render state +0x18C (only
    `0x00155A10` writes it, -1) only walks its group; a +0x3F box skips to its
    sibling; otherwise it is `0x001582A0`.
- **No host draw remains in a native scene.** `present_views` records each
  view's bins and the dialogue and presents them. Its host fallback for frames
  with no active camera slot is gone (it never ran in the opening or a
  3000-frame boot). The host camera upload (`gfx_set_camera`) is gone from the
  native view. The native scene start releases `g.models`. `frame_tick`
  draws `g.models`, `draw_cutscene_props`, `draw_ui` and the host dialogue
  only when no native scene runs (the legacy front end).

## Results against the references

- Frames 450–900 and the boot run are pixel-identical.
- Frames 180, 240 and 300 differ only in hall dressing (about 2,600 pixels
  each). The host drew every mesh of `aid_model_ghoulies_background_centralhallway_all`
  at once. The native tree draws busts on the stair posts and wall lamps, and
  leaves out the Ghoulies signs and rope barriers. After the tutorial
  (`--player-opening`, `cache/player_walk_check.bmp`) the native hall shows
  the signs and ropes and drops the busts. These parts follow scene state,
  which the host stand-in ignored. Neither the frustum test nor the node
  masks remove them (checked with temporary probes, removed). This has not
  been compared with xemu.
- Frame 120 differs in a 90×40 strip of hall behind the slime, for the same
  reason.

## The full per-view present (2026-10-15)

`SceneActorServices::State::present_view` is now the translated `0x0011C240`
(`present_runtime_scene_view`, `scene/scene_view_present.cpp`). Every callee is
called in retail order, including the `DAT_0050EA9C & 0x2000` reordering of
the tail. The opening runs 900 frames through it.

Translated and live in the opening:

- **Mirror pass `0x0011F690`** (`scene/scene_mirror.cpp`). The hall's
  background carries one mirror (its model's runtime header +0x18). Scene start
  now runs `0x0011F450` after the navigation build to read it, and the port's
  `0x001287F0` now sets `DAT_005110B4 = 1`, which it used to omit. The pass picks
  the nearest mirror box in the frustum and reflects the view across the
  mirror's first triangle. It renders the bins mirrored through `0x00120E80`
  (`draw_runtime_view_bins`), with `0x00121900`'s mirror branch: pass 1 only,
  the plane-side and sphere tests, and node pairs 0x1C to 0x1E. The frustum
  comes from `0x0013E520` / `0x0013E070`. It then restores the view, clears
  depth and stencil, and writes the mirror polygon's depth with color writes
  off. New D3DX translations are in `graphics/d3dx_view.cpp`. New display
  helpers are `0x000F5CC0`, `0x000F61F0` and `0x000F6E60`. The floor now shows
  the reflected hall.
- **`0x00121DC0`** (vtable +0x38 for each background and view-bin actor). It is
  0 for every class in the opening, so no calls are made. Other classes raise.
- **`0x00121EC0`** (model sprites). The hall's 35 candle and lamp sprites
  queue billboards. Their textures are headers inside the model's resource
  block, at the scene address plus the host copy's offset.
- **`0x0005C250` billboard flush** (`graphics/billboard_queue.cpp`, with
  `0x0005C390`, `0x0005C410`, `0x0005C580`, `0x0005C7F0` and `0x0005C8C0`). It
  sorts far to near and draws each billboard as a boot-row-16 quad. The flame
  sprites need X_D3DFMT_A8B8G8R8 (0x3A), which the texture decoder now handles,
  along with LIN_A8B8G8R8 (0x3F).
- **`0x000EC470`** (`graphics/world_sprite_passes.cpp`, with `0x0005CF20`).
  The debris pool exists, so the setup and the fog hold run every view. A live
  debris record still raises: its quad `0x0005CF90` is translated
  (`draw_world_sprite_quad`, used by the type-2 effect particles), but the
  debris loop's size and colour set-up around it is not.
- **World sprite quads `0x0005CF90`** (`graphics/world_sprite_passes.cpp`, with
  the depth mode `0x0005DFF0`). Immediate-mode `Begin(8)` quads under the
  `DAT_0050DD70` world matrix, with `SetVertexData2f` register 9 texture
  coordinates. The recording device records them as fixed format `0x102`
  (`D3DFVF_XYZ | D3DFVF_TEX1`) with the stage-0 texture, and the presenter
  samples it under the current diffuse.
- **Control list** (`DAT_0050E438`, vtable +0x14). Control types 3 and 0x91
  have the empty `0x001628B0` there.
- **Dialogue.** `0x00058150`, `0x00122230` and `0x0005F330` are called
  separately (`draw_scene_dialogue_page`, `flush_scene_dialogue_sprites` and
  `draw_scene_text_pools`).
- **Boot aspect.** `0x000FC310` sets `DAT_00509704` for a 640-wide mode from
  `DAT_0042A54C == 0`. The port never did, so native views were letterboxed to
  436 lines. The mirror pass's viewport restore (`0x000F5CC0` writes the fitted
  rect back as the origin) shrank the view every frame without it. The State
  constructor now does this store before `0x000FC190`. Native views fill
  640×480 as on retail 4:3.

Gates translated, bodies raise when they open (none open in the opening):

| Pass | Gate | Writer status |
|---|---|---|
| `0x0012AB10` post models | `DAT_00512AF0` | `0x0012A9B0` / `0x0012A9E0` / `0x0012AA70` not called natively |
| `0x000ED390` effect draw | each `DAT_00506930` word | The port's table has 150 entries; retail loops to `0x00506C50` (200) |
| `0x000EBCB0` trails | live type-2 trail records | Pool is native |
| `0x0011AF90` light cones | `DAT_00510560` and a background | Translated (`light_cones.cpp`); the Medusa's gaze opens it |
| `0x00119230` shadow volumes | `DAT_0050EC00`, `DAT_0050484C < 1` | Translated (`shadow_volume_pass.cpp`); 12 scenes have regions (see below) |
| `0x0011BA50` refraction | `DAT_00510580`, `DAT_00510650 == 0` | `0x0011B7A0` (scene start) not run |
| `0x000615E0` screen effects | `DAT_00504478`, `DAT_005044A4`, `DAT_005044B0` | `0x00060BC0`..`0x000619D0` not translated |
| caption / front-end widgets | `DAT_004DE9AC`, `DAT_004DE9A0`, `DAT_004E17C8 == 6` | Front-end writers not native |
| `0x00132290` HUD | `DAT_005493E0` | `0x00132040` (scene start) not run |
| `0x0011DB50` phase overlay | `DAT_00510CF4` with `DAT_00510E30` / `E40` / `E24` | Legacy scene-phase pointers |
| `0x00048550` capture | `DAT_005462A0` | `0x00048260` / `0x00048860` not translated |

**Shadow volumes `0x00119230`** draw each background region (`DAT_0050EAA0`, count `DAT_0050EC00`, filled by the
background construction) through the volume vertex shader (`DAT_0042735C`) with the depth ramp `0x00119160` creates
at boot: the region's stencil, then its back faces into book page 0 and its front faces into page 1, and a screen
quad (`DAT_00430B00`, FVF `0xA00404`) that compares the pages where the stencil is set, in the first region's colour.
On screen it is a translucent volume (chapter3c_scene12_1's lab, on its right). Twelve scenes have regions:
chapter2a scenes 6_2, 7_2, 8_1, 10_1, 11_1 and 12_2, chapter2b 3_2 and 5_1, chapter3a 8_1 and chapter3c 3_2, 12_1
and 14_1. Before 2026-11-01 the gate read a stale copy of `DAT_0050EC00` and the pass never ran.

The pages are drawn with the device's own depth surface (`DAT_0050EC0C`), so the stencil marks the volumes made are
tested. The port's texture targets otherwise get a depth buffer of their own; a book page set with the device's depth
surface opens a scene target instead (`gfx_begin_native_scene_target`): a colour surface shaped like the back buffer,
bound with the back buffer's depth-stencil under the fitted viewport, scaled down into the page when it closes.
Dialogue targets (`0x000629D0`) and the point-light surface (`0x00062B00`) also pair a texture with the device's depth
on retail; they still get their own depth buffer.

**Transition `0x0012C490`** is translated up to its dispatch. With a transition
running (`DAT_0054632C`) and no dialogue page, it reads the slot's handler
record. The empty handler `0x001628B0` draws nothing. The book and slime draws
(`0x0012B560`, `0x0012BE60`, `0x0012BA10`) and the transition camera
(`0x00048BC0`, ESI `DAT_0054937C`) are step 5. The legacy host transition
still draws them, after the views (`game.cpp`). Any other handler raises.

### Results against the references

The new references are the `cache/view_present/scene_N.png` captures.

- Every hall and tutorial frame differs from the old references because the
  letterbox is gone. Retail 640×480 4:3 fills the screen.
- Frames 180, 240 and 300 now show the hall reflected in the floor (portraits,
  arches, Cooper's legs) and the stair-post lamp flames (model sprites).
- Frames 450–900: the tutorial page is unchanged apart from the full-height
  framing.
- Frame 120: the slime still comes from the host transition; the strip of hall
  above it now fills the full width and height.
- The default boot run (attract, host front end) is pixel-identical.
- Not compared with xemu.

### Notes found on the way

- Headless runs read the host's real keyboard unless `--render-profile` is
  set (`runtime/platform.cpp`). Keys pressed during a run (IJKL is the right
  stick) make Cooper attack and reach the untranslated `entityStateHit`.
  Leave the keyboard alone during captures.
- The port's `0x000F5E50` (`rebuild_display_view`) stops before the retail
  function's frustum rebuild. Nothing on this path depends on it.
- Native vertices carry each stage's whole oT register (`GfxNativeVertex::uv_zw` holds z and w), so programs run
  on the CPU feed the dot-product stages as GPU-compiled ones do. PROJECT2D samples at (s / q, t / q), and a NONE
  stage's texture register is (0, 0, 0, 1) (xemu's `psh.c`).
- `cache/player_walk_check.bmp` from `--player-opening` is still letterboxed.
  The harness reaches the display fit outside the State constructor. Not
  investigated.

## Fade to grey (issue #41, open)

The Grim Reaper's fade-to-grey actor turns its surroundings white instead of grey (seen in `--scene3c9`'s captures:
a soft-edged white region round him, characters drawn normally over it).

- Its draw callbacks bracket the actor's own model: `0x000770F0` (`begin_runtime_fade_to_grey`) sets the glow page
  surface as the render target with the glow depth surface and clears it (0xF3, colour 0); `0x000771F0` composites
  the page texture back over the view with boot pixel shader 3, "Black and White Constant Alpha":
  grey = 0.33 * (r + g + b) of t0 (xdd / xmma / dp3), then `lerp(t0, grey, c0)`, c0 the fade level.
- t0 is `glow_targets.page_texture`, which the port backs with book page 0 (`scene_actor_services_bindings.cpp`).
  Between the two callbacks only the fade actor's model draws into it, so t0 holds that model on a cleared page
  rather than the scene, and the lerp shows white.
- Not settled: what retail's glow page and depth surface alias (boot `0x000620C0`, DAT_005047E0 / DAT_005047E4 /
  DAT_005047FC), and so where the scene image the shader greys comes from. Next: export `0x000620C0` and the fade
  actor's draw, and check in xemu.

## Work

1. **HUD and effect bodies.** Translate the scene-start writers (`0x00132040`,
   `0x0011B7A0`, `0x00060C80`) and the bodies above as their gates open in
   later scenes or the fight.
2. **Debris quads**: the `0x000EC470` loop body around the translated quad
   `0x0005CF90`, needed once debris is live in the fight.
3. **Transition draws** (step 5): the slot record at `DAT_00546328`, the
   back-buffer copy (`CopyRects`) in the recording device and presenter, and
   `0x0012B560` / `0x0012BE60` / `0x0012BA10`.
4. **Host cutscene queue.** `script_queue_scene_start` still starts the host
   `g.cutscenes` queue and `frame_tick` ticks it, moving `g.camera`. Nothing in
   a native scene draws from it. Retiring it is step 4.

## Acceptance

- `frame_tick` draws nothing on the host for a native scene, except the
  step-5 transition. Done.
- `present_view` is the translated `0x0011C240`, with every callee called in
  order. Done.
- Screenshots at the frames above match or are better, with differences
  named. Done.
- Release and Debug build. `ctest` 6/6 in each. The six transition suites and
  `--player-opening` pass, except Debug `--actor-loading`, which fails at
  `check_boot_pixel_shaders` on the unmodified tree too. `gbtg --headless
  --frames 900` exits 0 with and without `--scene chapter1_scene1_2playcam` in
  both builds. Done.
- `git diff --check` is clean; no probes remain. Done.
