# Scene presentation

How a native scene draws a frame. Retail's per-view present (`0x0011C240`, called for each active camera slot by
`0x00033A20`) records the world, the actors, effects, the HUD and dialogue through the recording D3D boundary
(`SceneD3DDevice`), and the D3D11 presenter turns that record into PC draws. No host draw runs in a scene.

Status (2026-11-08): the whole per-view present is translated, in retail order, with its passes: the mirror, the
actor bins, the background, model sprites and the billboard flush, the screen effects (`0x000615E0`), debris
(`0x0005CF90`), dialogue, the text pools, the HUD (`0x00132290`) and the transition book and view. Still raising
when reached: an effect's mode 2 sprite (`0x0005CA10`), and any model node draw slot or background cloth hook
without a translation. Blob shadows stay off: nothing translated sets DAT_0050EF18. The project-wide count is the
completion tracker in the [README](../README.md#completion).

Read `AGENTS.md` first. The actor bins and `0x00121900` are covered in [Player scene](PLAYER_SCENE.md).

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
  The debris pool exists, so the setup and the fog hold run every view. The
  debris loop and its quad `0x0005CF90` (`draw_world_sprite_quad`, also used by
  the type-2 effect particles) are translated.
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

The passes behind a gate, and where their writers stand (2026-11-08):

| Pass | Gate | Status |
|---|---|---|
| `0x0012AB10` post models | `DAT_00512AF0` | Translated, with `0x0012A9B0` / `0x0012A9E0` / `0x0012AA70` |
| `0x000ED390` effect draw | each `DAT_00506930` word | Translated; an effect's mode 2 sprite (`0x0005CA10`) raises. The table holds 200 slots, as retail's loop to `0x00506C50` does: effect categories 0, 1 and 2 own slots 0..149, 150..179 and 180..199 (`DAT_0042A41C`, whose starts the boot sets) |
| `0x000EBCB0` trails | live type-2 trail records | Translated; the pool is native |
| `0x0011AF90` light cones | `DAT_00510560` and a background | Translated (`light_cones.cpp`); the Medusa's gaze opens it |
| `0x00119230` shadow volumes | `DAT_0050EC00`, `DAT_0050484C < 1` | Translated (`shadow_volume_pass.cpp`); 12 scenes have regions (see below) |
| `0x0011BA50` refraction | `DAT_00510580`, `DAT_00510650 == 0` | Translated, with its scene-start writer `0x0011B7A0` (`refraction_pass.cpp`) |
| `0x000615E0` screen effects | `DAT_00504478`, `DAT_005044A4`, `DAT_005044B0` | Translated (`0x00060BC0`..`0x000619D0`) |
| caption / front-end widgets | `DAT_004DE9AC`, `DAT_004DE9A0`, `DAT_004E17C8 == 6` | The app error screen's page (`DAT_004DE9AC`) is native (`app_error_screen.cpp`); the other writers were not rechecked |
| `0x00132290` HUD | `DAT_005493E0` | Translated, with its scene-start writer `0x00132040` (`scoreboard_hud.cpp`) |
| `0x0011DB50` phase overlay | `DAT_00510CF4` with `DAT_00510E30` / `E40` / `E24` | Translated (`scene_pause.cpp`) |
| `0x00048550` capture | `DAT_005462A0` | Translated, with `0x00048260` / `0x00048860` (`native_book.cpp`) |
| `0x000EC470` debris | the debris pool | Translated (`world_sprite_passes.cpp`) |

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
- `D3DDevice_SetVertexShader` (`0x001739F0`) with a fixed-function format writes the vertex colours the format
  lacks (`NV097_SET_VERTEX_DATA4UB`): diffuse white without `D3DFVF_DIFFUSE`, specular zero without
  `D3DFVF_SPECULAR`, back diffuse white and back specular zero always. A created shader with a program (flag 0x10)
  is loaded and selected without touching them. `SceneD3DDevice::set_vertex_shader` does the same. Before it, a
  fixed-function draw took whatever `SetVertexDataColor` had last set, so the point-light composite (`0x00063320`,
  `D3DFVF_XYZ | D3DFVF_TEX1` modulated by diffuse) multiplied the whole frame by the last sprite's colour: an
  attack's flash sprites tinted the room blue for a few frames (issue #34, `--scene9` frame 1720), and the pause
  menu's text left the scene about 40% as bright after resuming (issue #33, `--scene10` paused in the walk
  cutscene).
- The green that a burning ghouly's light (`0x000669D0`, inner 60, outer 120) puts on nearby walls stops at a cell
  edge: lights with +0x24 set draw one grid cell either side of their own (`0x00063770`), whatever their reach.
- `cache/player_walk_check.bmp` from `--player-opening` is still letterboxed.
  The harness reaches the display fit outside the State constructor. Not
  investigated.

## The Grim Reaper's grey (issue #41)

The Grim Reaper turned his surroundings white instead of grey. The grey is not the fade-to-grey fx (type 0x30,
`0x000770F0` / `0x000771F0`), which `--scene2a2-reaper` never creates; it is the blob-shadow pass `0x00119AD0`. The
Reaper registers a blob shadow (`0x00119A60`, inner 110, outer 140) at his middle. After the background draws, the pass
clears the point-light page (DAT_005047DC) to black, adds each shadow as a white point light (`0x00063770`, flat vertex
program, the back buffer's depth), copies the frame into DAT_005047E8 and composites the two with boot pixel shader 17
(`0x00063320`, copy mode 1):

    tex t0 / tex t1 / dp3 r0, t1, c0 (0.333) / xmma discard, discard, r0, t1, 1-t0, r0.a, t0.b

so t1 * (1 - mask) + grey * mask. ps.1.1's `dp3` writes every channel in its mask, and the NV2A's dot product is RGB
only, so XGAssembleShader sets `PS_COMBINEROUTPUT_AB_BLUE_TO_ALPHA` (0x80000) when the destination includes alpha. The
port's assembler (`xg_pixel_shader.cpp`) left it out, r0.a stayed at its initial t0.a (the mask's alpha, 1), and the
composite became t1 * (1 - mask) + mask: white. With the bit, `--scene2a2-reaper` frames 3600 to 4200 show the room
grey round him, and the door and Cooper, drawn after the pass, keep their colour.

- `xdd` still writes RGB only. The point-light shader's `xdd r0, r1, ...` result is not read through alpha.
- Not checked: the aliasing the issue's comment mentions, and the fade-to-grey fx itself (no scene driven here
  creates one).

## Pages at the window's resolution (issues #51 and #56)

Retail draws its full-screen pages at 640x480, the frame's own size: the scene target that shares the back
buffer's depth (DAT_005047DC's page, which the point-light, blob-shadow and book passes draw into and composite
back) and the back-buffer copies (`D3DDevice_CopyRects`, the book and transition captures). The port draws the frame
at the window's resolution, so a page shrunk to 640x480 and drawn back over the screen showed as a coarser copy of it:
the background round the Grim Reaper (#51) and the frozen frame in a transition (#56).

- `gfx_end_native_scene_target` now returns the frame's 4:3 area at the window's resolution, as `gfx_readback_ui`
  already did. The scene side scales it to the texture's size (each texel takes the pixel at its centre, as the
  backend did before) for the texture's memory, which native code reads.
- `show_full_resolution` then gives the texture's decoded image the full frame, with `RgbaImage::texel_width` /
  `texel_height` set to the texture's size. A linear format's texel coordinates (`native_uv_scale` and the per-stage
  scales, both backends) count in those, so the larger image covers the same area.
- Presentation only: the texture's memory still holds 640x480 texels, so native code sees what it did before.
- The test executable takes `GBTG_TEST_WINDOW=<width>x<height>` to capture at a larger size.
- Checked at 1280x960: `--scene2a2-reaper` frame 3900 shows the grey room round the reaper with smooth edges where
  they were stepped. In the `--scene3c5` death transition the frozen frame's HUD digits and rivets are sharp where they
  were blurred (frames 2895 to 3045).
- Not covered: textures the game creates and renders into with a surface of their own (`gfx_begin_native_target`)
  are still 640x480 or their own size.
- Benchmark (`tools/render_benchmark.py`, 1920x1080, hall, tutorial, transition): frame times are unchanged. Not
  measured: scenes that end a page every frame (the point-light pass), which now upload the window-sized frame.
