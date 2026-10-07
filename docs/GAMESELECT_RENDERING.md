# Game select rendering reconstruction

## Live menu integration

The live slot and contents widgets now own their retail button-bat mesh,
animation resources, controller and skeleton pose. `gameselect_tick` advances
stand/flight/hop playback and `draw_ui` calls `0x0006D6C0` after the plate/text
pass. Constructor parameters come from the embedded normal template: radius
scale 1, menu scale 2, flight duration 0.4. Flight anchors use the menu tables'
center plus -20 Y and `0x0006DA00`'s scale * (2,21) adjustment. Navigation
requires cursor state 1 or 3 (`0x000E0210` / `0x000DB450`).

Occupied-slot Select (`0x000E02D0`) queues widget 3 and book clip 0; contents
Back (`0x000DB510`) queues widget 1 and clip 1. `0x00083E20`'s strict first
threshold comparison controls the widget switch. The independent menu book
advances and releases according to `0x00083D90`, and `0x00083C40` blocks input
while a book or scene transition is live. Page captures preserve `0x00048110`'s
two shared texture bindings: pages 1/4 share target 0, pages 2/3 target 1.
The CopyRects host boundary flushes pending UI quads and captures the fitted
4:3 game viewport without host letterboxing. Copying a widescreen backbuffer
directly into a page incorrectly compressed the game image as the page turned.
The named `colour0` index is retained from the material name table and used
when replacing a page texture (`0x00048110` / `0x0015B2B0`). In the retail
book it is stage 1; stage 0 remains the original ramp texture.

Camera draws now use the fitted viewport submitted by `0x000F6170`, through
the existing `0x000F5760` / `0x000F56C0` translations. With the current retail
aspect settings the logical rectangle is (0,22), 640x436. Previously the
projection included the 1.1 aspect multiplier but drew into 640x480, stretching
the camera image vertically. HUD and CopyRects continue to use the full game
area. The book material's literal UV scale is 640x480, with no additional inset
in that matrix. The actual inset is a separate authored surface, selected by
`0x00158240`: ndPushBuffer +0x3D selects a two-bit instance state, compared with
+0x3E; -1 bypasses the check. The host previously submitted both the inset and
full-page alternatives, allowing the full-page screen to cover the paper border.
The parser retains these bytes from the node, separately from its index payload,
and D3D checks the current instance state before submitting the range. Instance
states start at zero, and book setup now applies `0x000481E0` / `0x000F96F0`
(table +0x118 selects the slot, +0x11C supplies the value in AL). Forward and
reverse captures now retain the inset borders at the open-book phase. Exact
shader parity still requires retail comparison; the host material fallback
remains in use for generated vertex-light texture coordinates.

Naming now owns `aid_cutscene_ghoulies_dialogs_dialogpage1`, loaded by
`0x000DCEC0` with initial time 0. `0x000DD0A0` rewinds at the clip's duration
before advancing; `0x000DD170` draws that cutscene behind the keyboard. Its
retail backdrop and four animated dialog bats are no longer omitted.

Book setup also loads the optional `aid_cutsceneevents_<clip>_camera` resource
named by `0x0004E020` / `0x0004C640`. Normal playback dispatches its sound
records using `0x0004C6C0`'s strict time crossings through `0x0004DA50` /
`0x0004D370`: harp at 0.03 seconds, wind from time zero, and page turn at
3.16096 seconds forward or 0.941951 seconds backward. Type 0xC retains its
acquired native voice handle and stops that handle after the window ends;
cutscene teardown releases the three sound-window slots. Book voices have
independent ownership from the scene cutscene, so its audio sync cannot cancel
them. The cues and timing come from the retail event resources.

Validation uses the actual `Game::tick` and `draw_ui` path with retail assets and
D3D readback: standing and flying bats, occupied-slot forward page turn,
contents page, reverse turn back to slots, and naming background. A 960x540
run checks that the 720x540 page copy matches every pixel of the centered game
viewport, without rescaling or sidebars. This replaces the earlier
offscreen helper-only rendering check. Generic actor factory/frame integration,
sound queue playback, save IO and the separate Play/Features/Challenges widgets
remain distinct unfinished work; this integration does not claim those bodies
are translated. The notes below record the supporting reconstruction history.

The mode table at `0x0030A000` selects `0x000DDF20` for slot selection,
`0x000D9640` for occupied-slot contents, and `0x000DD170` for naming.
`0x000E62F0` belongs to a different widget and is not the slot-selection draw.

The current changes reconstruct these parts of the plate and movie passes:

- Static textures draw for all three plates; only the selected plate receives a
  movie overlay. The background uses a texture independently of that overlay.
- Plate backgrounds use the `0x00061360` central quad and mirrored scanlines,
  with the menu callers' corner fraction of `0.1`. Rest padding is 12 pixels,
  selected padding is 20, and dimensions are truncated before centering.
- Rest color is `0x80069764`; selected color is `0xC43EF7B6`. The retail byte
  order is preserved when submitting BGRA pixels to the host graphics boundary.
- Slot movies copy at counter >= 10 and draw at counter > 10. Their alpha ramps
  over 20 frames (`DAT_00429004`). Contents movies copy at counter >= 7, latch a
  successful copy, and fade using `(counter - 7) * 0.05 * 255`.
- `0x00107360` consumes a pending decoded frame and reports copy success. Menu
  textures retain the copied image instead of displaying the decoder's live
  image before the widget requests a copy.
- Selection changes release the previous movie owner and reset its widget
  counters, following `0x000DE9E0`, `0x000DB450`, and `0x000DACE0`.
- `0x0004B4D0` returns the matched suffix index. The separate `0x002FE698`
  table maps that index to the chapter used for artwork and movies. Missing
  occupied-slot save data raises an explicit error instead of choosing chapter 1.
- Naming no longer receives the slot widget's empty-slot background draw.

The additional rendering-state translations include:

- `0x000DF330` / `0x000DA300` count down the selected plate's zoom timer;
  `0x000DE540` / `0x000D9D10` interpolate its size, padding and color. Both
  screen constructors (`0x000DE990`, `0x000D9F10`) explicitly override the
  button-bat duration with 0.4 seconds. These calls were checked against assembly.
- `0x000DF140` / `0x000DA1E0` retain old/current backdrops. Draw order is current
  then old, with complementary truncated alpha bytes when their textures differ.
  Equal textures retain the original 0/255 alpha pair.
- Menu glyphs retain separate delay, life, age and frame arrays from
  `0x0005F5C0` / `0x0005FC30`. Crossing the delay boundary does not spend the
  remainder of that draw's time on life. Frame calculation follows
  `0x00060070` / `0x000602E0` using each glyph texture's frame count and rate.
- `0x0005F970` / `0x0005F7E0` clear sticky state and shorten remaining life
  using random duration and glyph age. Departing strings finish that exit pass;
  replacement slot names and contents labels start after zoom completes.
- The `0x000DEB20` broken/read-only overlays use 0.7 texture-pixel scale,
  alpha 127, zero positional offsets and the selected plate's zoom multiplier.
- Empty and damaged slot labels resolve `supportsavedgame__emptyslotname` and
  `supportsavedgame__damagedgamename`, rather than the gameselect text namespace.
- Progress text uses the `0x002FE670` chapter counts, `0x00311E88` scene-name
  indices and `0x00430C28` key table. Scene parsing and chapter-relative scene
  numbering follow `0x0011EBC0`, `0x0011EC70`, `0x0004B530` and `0x0011ED30`;
  `0x0011ED80` resolves the actual localized scene name.
- Slot, contents and keyboard glyph arrays are owned by their `Game` widgets.
  Reopening a widget clears its previous text pool. Zoom and glyph countdowns
  now use the scene's original freeze/scale clock.
- Naming key dimensions truncate rather than round. Their backgrounds use
  the rectangle/scanline raster boundary with a corner fraction of 0.25.
- Save-name enumeration `0x0010D810` / `0x0010D8A0` uses the existing save-name
  pool and owns/releases its enumeration index.
- Animation event dispatch follows `0x0002DFC0`, including exclusive current
  time, inclusive previous time, reverse/wrapped crossings and phases 0/1/2.
- Quaternion blending follows `0x00154030`, with its actual 0.99999 threshold
  and scalar polynomial helpers (`0x00152C40`, `0x00153320`, `0x00153450`,
  `0x001534C0`, `0x00153530`). The normalized linear interpolation shortcut
  has been removed from the shared sampler.
- Root-channel decoding and cycle displacement support the cursor's linear
  sampling mode. Playback time and root displacement follow `0x00031B50` /
  `0x00031D00`, preserving loop, clamp, ping-pong and cached-root branches.
- Player replacement follows the borrowed-resource path of `0x0002D8E0` and
  initializes the pose as `0x0002D370` does, including its 64-float trailer.
  `0x0002DBB0` preserves the previous clock, pause state, first-sample state,
  crossfade allocation, tick count and age. `0x0002D450` blends all joints and
  the first 48 auxiliary channels. `0x00031760` samples the 49 defined channels.
- Event-list passes follow `0x0002DE10`, `0x0002DF30` and `0x0002DE70` with the
  actual 60-entry size/flags table at `0x00339B50`. Startup and shutdown only
  replay flagged records and restore the player clock afterward.
- Retiming and completion follow `0x00030DB0` / `0x0003B290`. Absolute value
  and quaternion decoding use the `0x00152BB0` / `0x00152B30` helpers.
- Animation record and alias searches follow `0x00032180`, `0x00032210` and
  `0x00032150`. Mix parameters follow `0x000322A0`. The button-bat table uses
  the four actual sorted records, with looping stand and clamped fly/hop/doh.
  Mix's automatic-weight sentinel is the executable's -10, not -1.
- The vector callback now reads all four compressed neighbours and performs
  `0x0002E130` integer-space interpolation before scaling. Constant and absent
  components return directly. The quaternion callback follows `0x0002E3A0`'s
  early returns. Frame selection follows `0x0002EB50` without extra time clamps
  or silent empty-pose fallbacks; exponent shifts preserve Xbox signed divisors.
- Model-bound and shape-mode setup follows `0x0003D5F0`, `0x0003D9A0` and
  `0x0003C680` using the actual block directory. Shape classification follows
  `0x000559F0`. Sound-slot defaults follow `0x00041BE0` and the eight records
  at `0x002FD1C0`, including block-9 bone lookup and the 0xFF/0xFFFF distinction.
- Cursor draw `0x0006D6C0` follows the 0.2/0.92 flight thresholds, directional
  coordinate comparisons, model-bound rectangle and screen-mesh submission.
  Its D3D boundary clears depth and stencil within that rectangle, restores
  graphics state and preserves queued sprite ordering. The live menu now calls
  this helper through its owned cursor view.
- Playback initialization follows `0x00031A30` and `0x00031AC0`, preserving
  signed rate factors separately from absolute speed. Controller linear pose
  mixing follows `0x0002CE70`, `0x000312C0`, `0x00031950` and `0x00031FC0`,
  including nullable roots, directory presence and the retained auxiliary
  trailer. Auxiliary callbacks follow `0x0002D6E0`, `0x0002D700` and `0x0002D740`.
- Actor animation lookup follows `0x0003AAA0`: variants have no fallback,
  wildcard numbering stops at the first gap, text after `?` is discarded and
  only a successful wildcard set advances the caller-owned original RNG state.
- Controller allocation, ticks, fades, combination and slot release follow
  `0x00030740`, `0x00030BA0`, `0x00030E30`, `0x000310F0` and `0x00031240`.
  Mode dispatch follows `0x00031040`; additive and masked pose operations follow
  `0x00032020`, `0x0002CFB0` and `0x0002D1A0`. The two additive quaternion flags
  are distinct in the exported assembly, despite an aliased local in Ghidra's C.
  Owned-resource release follows `0x00035C00` / `0x0002D850` against explicit
  runtime registry records. Registry acquisition and bundle providers are now
  translated; Game's format-loader allocation and host bindings remain required.
- Skeleton evaluation follows `0x0002FAC0` and bind subtraction `0x0002F720`,
  including quaternion normalization, the w-equals-one shortcut, parent-scale
  compensation, callbacks and joints beyond the animation's count. Pose-buffer
  setup and auxiliary application follow `0x000314C0`, `0x00030F90`,
  `0x0002CD80` and `0x0002CE00`. Player allocation follows `0x0002D780`.
  Playback restart retains the root at +0xB8 when no root channel overwrites it;
  tick's guard tests the resource at +0xB0, not the pose buffer at +0x10.

This is still not a complete translation of either widget's state machine.
The following dependencies remain untranslated and block full acceptance:

- Button-bat factory/vtable registration and Game's provider allocation
  and D3D bindings for model resource preparation. The base actor constructor
  (`0x0003E940`) is translated and its non-model path is checked natively.
  Actor model replacement `0x00040C00` and instance construction `0x000F9840`
  are translated; the model hook (`0x00052F00`) now relocates prepared CPU/GPU
  allocations and populates the typed views on post-load. The original format
  loader calls relocation before exposing the allocation through its provider;
  acquisition calls post-load. Game must provide those allocations and bindings
  before menu integration. Generic factory allocation,
  parent construction and rollback (`0x00107520`, `0x00107620`, `0x001076B0`)
  are translated, but actual type-vtable initialization and these callbacks
  remain dependencies. Animation
  startup/application helpers `0x0003A9E0`, `0x0003ADA0`, `0x00030880` and
  `0x00030C70` are translated, but the factory and specific actor message
  handler still block their integration into the menu.
  `0x000420D0` also owns attachment and animation-event passes. The embedded
  normal/gold objparams records point to templates at `0x00377CC8` / `0x00378008`;
  these are executable data, not common-bundle entries. No cursor substitute
  has been added.
- Initial slot-save scan (`0x000DF5D0`, `0x000DF650` and save IO callees) and its
  disc-spin/fade states. Contents' initial mode-1 fade also remains untranslated.
  Live navigation now applies the callbacks' cursor-state gates.
- Play widget `0x000D5300`, including camera/environment setup, its cutscene,
  prompts and `0x00143840`. The earlier contents fallback still exists.
- Contents save-state flags at blob +0x184/+0x188, challenge-state icon, complete
  prompt-widget lifecycle and original text-pool routines. Progress formatting
  and widget-owned glyph storage do not accept these remaining dependencies.

Validation: Release `gbtg` build passed. A separate fixture checked zero-duration
and overrun timers, equal/different backdrops, midpoint alpha/color truncation,
glyph delay crossings, per-frame update ownership, sticky/exit frames and
empty-string removal. Those checks passed. A local integration fixture exercised
empty-slot navigation, the 10-frame slot counter, movie copying, naming/back,
chapter3c-to-chapter3 lookup, contents copying, and release/reset on contents-row
changes. An offscreen 400-frame slot-select capture was inspected. These checks
do not establish parity with xemu or accept the untranslated widget routines.

Further checks passed for chapter2c/chapter3c scene numbering, localized scene
names, empty-save enumeration, quaternion interpolation and event boundaries.
The registered `animation-event-boundaries` CTest also exercises reverse and
wrapped playback plus a synthetic compressed root track whose displacement is
known. `model-render-inputs` passed. The real flight root channel was decoded
at 0, 0.2, 0.5, 0.92 and 1; its loop displacement is 22.9375 along x. This is a
decoder check, not a cursor-state or xemu parity acceptance.

Player lifecycle fixtures also passed for replacing a sampled pose, paused
ticks, skip-pose crossfade release, aliasing output during blending, auxiliary
track wrap/clamp interpolation, and startup/shutdown event clocks. All four
retail button-bat animations were initialized and sampled through the new
player; each produced 18 joints. The menu integration fixture passed again
after the shared animation changes. These helpers are prerequisites; the
menu's cursor startup is now integrated through the resource view; the generic entity factory remains separate. The shared
model-instance application helpers are translated.

The GPU rectangle fixture checked every depth/stencil pixel after clearing a
20-by-20 region, and verified restored vertex/pixel shaders and render target.
The retail button-bat mesh fixture passed flight-threshold, absent-animation
and standing-position checks. Registered animation checks also cover signed
initialization, nullable/aliased root blending, all auxiliary presence branches,
absent directories, retained trailer, variant aliases and wildcard gaps. These
checks do not accept the missing cursor factory or complete startup path.

Controller integration checks passed for multi-slot pose/root selection,
additive and masked channel presence, fade-in, the first sound event, endpoint
voice-stop event, priority-0x32 fade-out, owned-resource reference release and
idle detection. Skeleton checks passed for non-unit quaternions, the w==1
shortcut, unanimated tail callbacks, parent-scale compensation and four-joint
bind-subtraction groups with a remainder. The retail flight animation rendered
at all five threshold positions through this evaluator. No complete menu
cursor factory, provider-load or xemu parity acceptance is implied.

Further translations cover resource acquisition `0x00035E90`, provider
registration `0x00035540`, hash/name lookup, deferred-pool promotion and loaded
record allocation/reuse. Bundle callbacks `0x00034A30`, `0x00034A70`,
`0x00034AF0`, `0x00034B30` and executable-data callbacks `0x00034E90`,
`0x00034EC0` use their original match/gate/reference-count branches. Executable
pool registration and lookup follow `0x00034E00`, `0x00034D00`, `0x00034CB0`.
Complete provider initialization, bundle relocation and demand-file IO are
still missing; no fabricated provider success has been installed in Game.
Template cloning `0x00107720` translates the inactive-pool lookup, recorded
16-bit allocation size, heap-failure test and copy into caller-owned storage.

Named/borrowed player startup now covers both paths in `0x0002D8E0` and the
named wrappers `0x0002DB50` / `0x00035B60`. Controller startup follows its
original retain/random position, reverse duration, default weight, fade,
owned/borrowed event and failure-cleanup branches. Actor startup/event-message
helpers cover `0x0003A9E0`, `0x0003ADA0`, `0x0003B3C0`, `0x0003B2E0` and
`0x0003B290`. The virtual event handler remains a required dependency;
the retail button-bat's `0x0006D8A0` handler and its sound callees are not
accepted by tests of the message bridge.

Model application covers `0x00030C70`, callback selection `0x00030FF0`, plain
skeleton evaluation `0x00030100` and packed node flags `0x000320F0` /
`0x000F96F0`. Assembly retains compensation and unanimated child translations
that Ghidra drops from the plain evaluator. The callback path preserves local
zero-scale substitutions; the plain path mutates the parent pose's zero scale.
Cursor initialization/state/flight helpers `0x0006D4F0`, `0x0006D8D0` and
`0x0006DA00` use verified stand/flybetween/hop/doh names, a 4.5-second idle
timer, duration retiming and the 0.2 restart position. Its full frame tick
`0x0006D5C0` still requires the complete actor tick `0x000420D0`.

The resource-error raster/fatal helpers follow `0x00056D80`, `0x00032770`
and `0x00032700`, with explicit original file/section/D3D boundary interfaces.
The menu does not yet bind those interfaces. The native performance-counter
boundary was explicitly authorized by the user; `0x001077D0` / `0x00032680`
preserve signed counter differences and the float deadline before polling.
Registered checks cover error bitmap expansion, boundary call order,
last-error gates, acquisition failures, deferred promotion, provider payloads,
actor message fields, alternate pose selection, node masks and skip/idle gates.
The retail cursor fixture also passes through translated bundle callbacks,
actor/controller startup, ticking and model application before drawing all
five flight-threshold positions. Real type-29/type-31 flight cues arrive in
order; standing snaps to the arrival point, scaled flight anchors subtract
2/21, and an interrupted flight restarts current/previous clocks at 0.2.
This does not accept the missing entity factory, actor frame tick, sound
handler, complete provider initialization or menu integration.

Model-tree traversal/name lookup and copy dispatch follow `0x00156280`,
`0x001562C0`, `0x001568E0`, `0x00156960`, `0x00156970`, `0x00156120`,
`0x001561E0` and `0x001588E0`. The 17 copy-slot bindings from `0x00155A10`
now call all eight distinct translated routines, including shader parameters
and blendshapes. Node copy offsets at +0xC remain separate from auxiliary
value offsets at +0. The actual 48 names at `0x00339A08` bind auxiliary
targets. Blendshape checks preserve type-9/type-10 attribute offsets, the
single-target weight gate, allocator arguments and the source-record overwrite
before D3D registration. This does not translate the complete `0x00155A10`
renderer initialization.

Shader setup follows `0x0015B0C0`, `0x0015B0E0`, `0x0015B1F0`, `0x0015B230`,
`0x0015B260`, `0x0015B040`, `0x00165300`, `0x00165360`, `0x00165570`,
`0x001657A0` and `0x00165830`. Recovering the latter callee's EBX count pointer
and ESI tree argument in the read-only project restores Ghidra's previously
eliminated allocation branch. Signed rate conversion, 0.0001 scaling,
case-sensitive names, scratch tail retention and parameter flags are checked.
`0x001658B0` / `0x00165A00` preserve accumulated translation, rotation, phase,
the 0.04 sine multiplier, center-rotation order and final transpose through
explicit original D3DX boundary interfaces. Game has not bound those interfaces.

Model construction `0x000F9840` covers shader-copy and pose-creation gates,
all type-specific virtual copies, identity pose initialization and shader-list
construction. Shader registration/release follows `0x000FDF60` /
`0x000FDFC0`; release preserves the 0x10 retention flag and first matching
registration. `0x00165A60`, `0x000F8BF0` and `0x00035BA0` preserve immediate
shader-list release, deferred model allocation ownership, fence values and
resource retention. Actor model replacement `0x00040C00` is checked through
translated bundle providers, including missing-resource failure after a
suppressed fatal error. Separate instances of one asset keep separate shader
pointer fields. Final instance release and deferred collection now follow
`0x00156160`, `0x00156210`, `0x001574B0`, `0x00031470`, `0x000F9C20`,
`0x000F8C50`, `0x000F9780` and `0x000F97F0`, including GPU busy polling,
physical-free arguments, unsigned fence wrap, the two-frame gate, swap-last
removal and forced-flush ownership. Resource reference release still leaves
provider unloading to the separate original resource collector.
Secondary model replacement `0x00041F90` borrows the main model's alternate pose
before its primary pose and does not free that borrowed allocation on cleanup.
The actual factory registration and frame/event paths remain required before
Game can use this model path.

Raw model relocation now follows `0x00052910`, with separate CPU/tree pointer
bases, unsigned 32-bit pointer arithmetic, GPU chunk resolution (`0x00051F20`),
all 17 registered node relocation slots and the physics callback table.
`0x00159430` preserves the vertex-shader cache's program/key/channel lookup;
declarations are copied on a miss but do not participate in lookup.
The tree's type binding and traversal follow `0x00158770`, `0x001588C0`,
`0x00156600` and `0x00156790`, including unchanged descriptors on a name miss.
Post-load/unload translate the node traversals, matrix ownership, push-buffer
physical allocations, pixel shader handles, texture headers and special-texture
callbacks (`0x00052B70` / `0x00052E50`). These paths require real heap and D3D
boundaries; Game has not yet bound them. The host asset-view adapter decodes
the actual node, shader and skeleton record offsets, including the extra scalar
indirection in named shader entries and the separate auxiliary value offsets.
The resource-hook integration test acquires through translated bundle providers,
constructs a model from those decoded views and checks its shader animation,
pose, auxiliary target and unload ownership.
The relocation tests cover pointer wrap, chunk boundaries, different pointer
bases, child-before-sibling traversal, first-shape/later-shape registration,
shader cache keys, physical allocation arguments, trailing byte copies and
special-texture clearing/retained fields.

The actor timer `0x00042360` also preserves scene freeze/scale, actor pause/rate,
positive-only countdown and the one-time expiration result. Angle wrapping
uses the retail remainder `0x00152BE0`, including float quotient truncation,
before `0x00105AA0` applies its -pi/pi comparisons.

Actor startup now translates `0x0003E940`, including attachment initialization
(`0x0003D1B0`), resource/table ownership (`0x0003BFD0` / `0x0003AA30`), initial
animation startup (`0x0003ACE0`), controller/callback cleanup (`0x000307C0`,
`0x0002F570`, `0x0002F2D0`, `0x0003AC20`) and final transform refresh
(`0x00040DE0`). Required typed views represent actual acquired resources;
missing model/table loaders are not replaced with successful values.
Parent, joint and bone bindings follow `0x0003B770`, `0x0003B8B0`,
`0x0003BAB0`, `0x00031530`, `0x000316B0`, `0x00051E40` and `0x00051CE0`.
The pose lookup preserves packed 4x4 versus CPU 3x4 storage and shared scratch.
World position/Euler helpers follow `0x000412A0` / `0x00041820`, including
dead-bind release. Bone lookup and attachment transforms follow `0x00052FC0`,
`0x0003AE60`, `0x0003AEB0`, `0x0003B050`, `0x0003B160`, `0x00040AA0`,
`0x000415F0` and `0x0003D510`. The scalar polynomial routines replace host trig
in these actor transforms (`0x00153230`, `0x00152E90`, `0x001530E0`), and column
normalization uses the retail reciprocal-square-root helper (`0x00103AF0`).
These helpers do not complete the attachment creation, actor frame or event
dispatch paths. Native checks cover constructor field/list setup, parent/joint/
bone matrix composition, mapped/missing bones and the retail cursor draw.

Actor sound requests now translate active-list membership (`0x0012A970`),
request removal (`0x0012A3D0`), pending front insertion (`0x0012A260`), queue
reset (`0x00129F70`), event probability (`0x0003A830`), direct requests/query/stop
(`0x00041D00`, `0x00041C80`, `0x00041E10`) and direction accumulation
(`0x00106220`). Pending removal does not stop a voice; queue reset leaves its
pending record's enabled byte unchanged. Cue pause/stop and voice retirement
follow `0x0015CDD0`, `0x0015CE30`, `0x00039690`, including the pause HRESULT
failure route (`0x0015B8E0`) and forced immediate stop for paused cues.
The user authorized native audio at retail XACT calls. The native pause boundary
returns XAudio2's actual Stop/Start result. The native fixture checks pause,
resume and immediate stop on a real source voice. Retail cue/bank creation and
the full actor event dispatcher still require translation before Game can use
these queues. Non-immediate native stop uses XAudio2 tails; retail XACT release
envelopes are not yet represented by that adapter.

Shader-bit and node-state helpers follow `0x000426B0`, `0x000426D0`,
`0x00040D90`, `0x00040D50` and `0x000F96F0`, preserving the unusual
boolean-before-shift query and unmasked byte value packing. Shared actor
creation options follow `0x0003A8B0`, retaining Euler values when their flag
is absent. The base factory size/constructor leaves follow `0x001074D0` and
`0x00107700`; factory registration still needs its complete callback chain.
Actor model matrix refresh follows `0x00040200`, including distinct simple
and complex multiplication order, dirty/forced update gates, alias-safe retail
matrix multiply (`0x00151E64`) and matrix/dirty-byte writes (`0x00156010`).
Native checks cover both orders and update gates; Game's actor matrix storage
and D3D boundaries remain unbound.
Render pose binding follows `0x000F9550`, including reverse packing, the
D3DX transpose split above three bones, alternate-pose selection and dirty
flag clearing. Model material lookup follows `0x000F94F0`, with first matching
type-7 block behavior and retained state on a missing/empty block. Render
state/frame accessors and setters follow `0x00156DD0`, `0x00156DE0`,
`0x00156300`, `0x001565C0`, `0x00156370` and `0x001564A0`. Tree draw dispatch
and model instance setup follow `0x00156240` / `0x000F9120`, including the
assembly-confirmed draw parameter and instance-storage arguments absent from
the C decompile, and the final rendering-byte clear. The real retail empty
function (`0x001628B0`) is translated as an empty body. Full lighting setup,
node draw callbacks and Game render-context bindings are still required.
Light list initialization/traversal/insertion follows `0x001599E0`,
`0x00159AC0`, `0x00159A30`, `0x00159A60` and `0x00158D80`, including stable
unsigned type ordering. Packed lighting flags and clearing follow `0x00158D20`
and `0x001593A0`; per-light parameters and enable/world-space state follow
`0x00159320`, `0x001592A0`, `0x001592E0`, `0x00159300`, `0x00159330`,
`0x00159360` and `0x001593C0`. Fog cache setup follows `0x00156EA0`,
`0x00156ED0` and `0x000FD5C0`. Base/ambient color follows `0x00156F30` /
`0x00156F70`, retaining NaN channels as the assembly's COMISS/JBE branches do.
Scene light direction follows `0x000FD9C0`, including override degrees,
camera-relative inverse and the D3DX normalization boundary. The shared affine
inverse now translates `0x00104400` with its actual alias/failure branches and
subtraction order; actor bind callers use that implementation.

The remaining rendering chain includes full vertex-light constant setup
(`0x00158F20`), scene ambient accumulation (`0x000FEE90` / `0x000FD850`),
lighting coordinate matrices (`0x000FCDF0`), lighting wrappers (`0x000FD2A0`,
`0x000FD3F0`) and final model/actor draw wrappers (`0x000F9080`, `0x0003C9E0`).
Those callers must not be connected with missing callee callbacks or invented
successful returns. Full actor event/frame/factory registration and the
remaining menu save/Play paths described above are still outstanding.

Decompiles were exported read-only from the user's project in
`Z:/GhouliesStuff/ghidra_projects` into the main checkout's ignored
`cache/ghidra_frontier/`. No Ghidra symbols or retail disc files were changed.
