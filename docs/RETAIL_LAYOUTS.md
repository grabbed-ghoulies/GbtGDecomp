# Retail field layouts

Retail objects were first reconstructed as byte buffers laid out exactly as on the Xbox, so translated code
matched the Ghidra dumps offset for offset. Reading those buffers through literal offsets (`word(state, 0x2C0)`)
is faithful but unreadable. Fields get names in two steps, neither of which changes behavior:

1. **Name** every field with a descriptor (`RetailField`) over the existing bytes.
2. **Consolidate** a fully named type into a plain typed struct, with each member's retail offset in a comment.

This document describes both steps and tracks which types are at each one.

## Step 1: descriptors

`include/gbtg/retail_field.h` declares a field once, beside the object that owns it:

```cpp
namespace super_scary_field
{
constexpr RetailField<u32, 0x2C0> player_hidden{};
constexpr RetailField<std::array<u32, 20>, 0x214> buttons{};
}
```

and code reads and writes it by name:

| Before | After |
|---|---|
| `word(state, 0x2C0)` | `get(state, field::player_hidden)` |
| `put_word(state, 0x2C0, 0)` | `set(state, field::player_hidden, 0)` |
| `reinterpret_cast<const char*>(state.data() + 0xB8)` | `field_data(state, field::first_clip)` |
| `word(state, 0x214 + i * 4)` | `field_data(state, field::buttons)[i]` |

- `RetailField<T, Offset>`: a scalar, array or `Vec3` at a byte offset. `get` / `set` copy it in and out
  (bounds-checked, like `read_u32`). `field_data` gives its address for the calls that take a field by pointer.
  `field_byte` gives one of its bytes.
- `RetailRecord<Offset, Stride, Count>`: a sub-object or an array of them. `record(bytes, r, i)` is record `i`'s
  bytes, and the record's own fields are `RetailField`s relative to its start. Records nest.
- `retail_fits(size, f)` is for `static_assert`: the field or record lies inside the object.
- Words that hold pointers stay as host members of the owning struct (retail pointers are 4 bytes, host pointers
  8), with a `// +0x...` note. The descriptor namespace notes them by offset so the layout reads end to end.

Descriptors change names only. Every replaced access keeps its offset, width and signedness. An unsigned
comparison stays unsigned (cast at the use site, with a comment). A word copied between two float fields is
copied as a float. `memcpy` keeps the bits, so the result is the same.

## Step 2: typed structs

Once every field of a type is named, and the type is host-allocated with nothing reading it by offset, replace
its byte array and descriptors with members:

```cpp
struct GameDeletionWidget : GameSelectWidget
{
    i32 state = 0; // +0, 0x000DBDC0 sets it; 0x000DB9E0 switches on it
    GameSelectTextLine title; // +4, the game's name, from +0x48C
    GameSelectTextLine prompt; // +0x210, deleteprompt, then deleting
    RuntimeDialogueTextPool* pool = nullptr; // +0x41C
    ...
};
```

- Members stay in retail order, each with its retail offset and the descriptor's evidence comment. Host pointers
  and spans sit where the retail pointer words were.
- Repeated sub-layouts become nested or shared structs, and their textures move in with them (a sprite's texture
  word is at its +0).
- Initialize every member (`= 0`, `{}`). The byte arrays were zero-initialized, so the structs must be too.
- A field read or written one byte at a time in retail (a color's alpha) gets a small helper (`color_alpha`,
  `set_color_alpha`) rather than a byte view.
- Watch for record locals that were spans: `const auto x = record(...)` becomes `auto& x = ...`. A copy would
  lose the writes, and a reference can't be rebound.
- Byte-for-byte equality with the retail layout is no longer checked by the compiler. The offset comments are
  documentation; a type that other code reads by offset stays on descriptors.

The game-select widgets were converted with a script that rewrites each descriptor call into member access
(`get(E, ns::f)` to `E.m`, `set(E, ns::f, v)` to `E.m = v`, and so on), then fixed by hand where a helper took
a span.

## Shared layouts

Some sub-layouts repeat across types. They are declared once next to the types that share them:

- `GameSelectTextLine` (`src/engine/frontend/gameselect_rows.h`, 0x20C bytes in retail): wide text, x, y, and the
  0x0005F5C0 record id. Used for the slot-select prompt and plate labels, the deletion title and prompt, and the
  contents chapter names, title, progress and completed lines.
- `GameSelectSprite` (same header, 0x18 bytes in retail): texture, position, color, scale. Used for the
  slot-select plates, discs and backgrounds, and the contents panels, backgrounds, splat and challenge icon.

The retail code never passes either one as a struct, so the evidence for treating them as shared types is only the
identical layout in every place they appear. The header comments say so.

## Naming rules

1. **Name from evidence.** Read every dump that touches the field. Run the census (below) to list them. Write the
   strongest evidence on the declaration: the function that sets it, the constant it starts from, what it is
   compared with.
2. **Say when a name is inferred.** If the evidence fits but doesn't prove the name, say so in the comment
   (see `cursor_row`). If there's no good guess, use `unk_<offset>`, for example `unk_B4`. A wrong name is worse
   than an offset.
3. **One object type per change.** Name the whole type in one pass, so that every function using a field agrees
   on what it is called.
4. **Ghidra symbols stay as they are.** `DAT_` / `FUN_` names in comments and the Ghidra project are not renamed.
5. **Keep the address notes.** `// Xbox 0x........` notes and existing offset comments that cite a dump stay.

## Census

`tools/layout/offset_census.py` scans `src/` for accessor calls and `bytes[...]` indexes with a hex offset. For
each one it records the enclosing `// Xbox` function, the buffer expression, the width and whether it reads or
writes.

```
python tools/layout/offset_census.py                         # whole tree
python tools/layout/offset_census.py --file trophy_heads_control.cpp
```

It writes `cache/layout/census.json` and one table per file (`cache/layout/<file>.md`). Each table lists every
offset per buffer with its widths, R/W kinds, functions and lines, plus the dump path of each function. Offsets
added to a record base (`p + 0x434`) are grouped under that base. Literal offsets into the second and third
copies of a repeated record (0x734, 0xA38, …) show up as their own rows, which is a sign there's a record to
declare. It misses writes through `memcpy(x.data() + 0x..)` and file-local wrappers with other names, so grep
for the type's offsets too.

## Global names

Retail globals are named for what they hold, with the address beside the declaration
(`i32 g_pause_count = 0; // DAT_00510CEC`). `tools/layout/address_names.py` fails on a `g_<hex address>` name. The
few that keep their address are listed in the script with the reason. Most of them are a second host variable for
a retail address that another named global already holds, and need merging before they can be renamed.

## Per-type session brief

For one object type:

1. Run the census for the type's files and read every dump it lists.
2. Declare the fields beside the owning struct, with evidence comments, `static_assert(retail_fits(...))`, and
   records for repeated sub-objects.
3. Replace the literal-offset accesses. Delete the file-local `word` / `put_word` / `real` helpers once nothing
   uses them.
4. Build `gbtg` and the transition tests (`--target gbtg gbtg-transition-tests`). Run the test modes that reach
   the type, and compare their captures with captures from before the change.
5. If the type is host-allocated and nothing reads it by offset, consolidate it into a struct (step 2) and
   compare the captures again.
6. Add the type to the tables below.

## Consolidated into structs

| Type | Retail size | Where |
|---|---|---|
| Slot-select widget (`SlotSelectWidget`, 0x000DD730): 3 plates (status, label, 4 sprites, movie name and UV, box), 2 background sprites, cursor, timers, slot names | 0xEAC | `src/engine/frontend/gameselect_slots.h` |
| Deletion widget (`GameDeletionWidget`, 0x000DB720): title and prompt lines, saved plate flags, save name | 0x594 | `src/engine/frontend/gameselect_delete.h` |
| Contents widget (`GameContentsWidget`, 0x000D8AB0): 3 chapter panels (name line, sprite, movie state, box), title/progress/completed lines, 4 sprites, timers, splat | 0xF34 | `src/engine/frontend/gameselect_contents.h` |
| Button sequence (`ButtonSequence`, contents +0xF30, 0x00142E50): state, step, presses, held buttons, step flags | 0x70 | `src/engine/frontend/gameselect_contents.h` |
| Naming widget (`GameNamingWidget`, 0x000DCEB0), typed from the start | 0x178 | `src/engine/frontend/gameselect_naming.h` |
| Features widget (`GameFeaturesWidget`, 0x000D4EA0), typed from the start: title and banner lines, page, camera, saved plate flags, name, prompts | 0x498 | `src/engine/frontend/gameselect_features.h` |
| Scene-replay widget (`GameSceneReplayWidget`, 0x000E1E00), typed from the start: title and banner lines, page, camera, saved plate flags, name, chapter number, owner, prompts | 0x49C | `src/engine/frontend/gameselect_scene_replay.h` |
| Button-bat menu (`ButtonBatMenu`, `DAT_005461A4`, 0x00143550), typed from the start, with its items (0x23C) and slide-in sprites (0x2C) | 100 | `src/engine/frontend/button_bat_menu.h` |
| Game-select control (`GameSelectControlState`, factory type 0x9A, 0x000835F0): rows, start script, pending start, start mode, book turn, start ready. +0x20 stays on the factory bytes as the base's `elapsed`. | 300 | `src/engine/scene/gameselect_control.h` |

Game select checked with `--gameselect`, `--gameselect-native`, `--gameselect-new-game`, `--gameselect-delete` and
`--gameselect-stats`. All pass, and all 25 captures are byte-identical to the captures taken before the first
naming change, both after naming and after consolidation. Those runs don't reach the contents completed splat
(`0x000DAD50` / `0x000DAED0` / `0x000DAF10`, a finished game only) or the challenge icon (shown only with unplayed
challenges). Those paths compile but haven't run there. `--gameselect-retail-save` (a finished retail save,
added later) does show the completed splat, so that path has now run. The control is exercised by every row change and the new-game
start. The button sequence is built and ticked but never entered; its unlock (0x000DB380) is still untranslated.

## Named with descriptors

| Type | Size | Layout | Where |
|---|---|---|---|
| Super Scary challenge (factory type 0x98, 0x000842D0) | 0x2C8, parameters 0x1D8 | `super_scary_field`: actor addresses, phase, timers, clip names and placements, shock camera, presses and button row, settle, render swap; `super_scary_parameter`: the construction block | `src/engine/scene/super_scary_control.h` |
| Scene-control base (factory type 3) | — | `scene_control_field::elapsed` (+0x20) only; the list links at +8, +0x10 and +0x18 are SceneControlInstance members | `src/engine/scene/scene_control_fields.h` |
| Trophy heads (factory type 0x92, 0x000816A0) | 0x188, parameters 0x108 | `trophy_heads_field`: player, attacks, health, seek time, phase, chosen heads, per-head attacked and roaring flags, attacking head, pause hold, head events name, challenge won; `trophy_heads_parameter`: the clip and events names; `cutscene_event_field`: the two event-record words 0x00082440 reads | `src/engine/scene/trophy_heads_control.h` |
| Factory parameter header | — | `object_parameter_field`: size, unk_2, type | `include/gbtg/object_factory.h` |
| Body-state base | — | `body_state_field`: next key (+0xC), unk_E, elapsed (+0x10); +8 is the state's native body view | `src/engine/entities/body_state_fields.h` |
| Hold state (factory type 0x8A, 0x000A34A0) | 0x24 | `hold_state_field`: timer, settle turn, body bit | `src/engine/entities/body_hold_state.h` |
| Walk-target state (factory type 0x8F, 0x00091A90) | 0x7C, parameters 0x60 | `walk_target_field`, `walk_target_parameter`; the rows block (`walk_target_rows_field`, 0x4C bytes: four `walk_target_row_field` rows and the band limits) | `src/engine/entities/body_walk_target_state.h` |
| Pick-up and lift states (factory types 0x7C and 0x7D) | 0x2C / 0x30, parameters 0x10 / 0x1C | `pickup_state_field`, `pickup_state_parameter`, `lift_state_field`, `lift_state_parameter` | `src/engine/entities/body_pickup_state.h` |
| Throw state (factory type 0x8B, 0x000A37D0) | 0x68, parameters 0x2C | `throw_state_field`, `throw_state_parameter` | `src/engine/entities/body_throw_state.h` |
| Skeleton strategy (factory type 0x51) | — | `skeleton_strategy_field`: no spawn in reach, fetch delay; and the base strategy's `strategy_field::state` (+0xE, `strategy_fields.h`) | `src/engine/entities/skeleton_strategy.h` |
| Bob weapon (factory type 0xA0, 0x000C5B90), the bob's own part | 0xFE8, parameters 0x7FC | `bob_weapon_field`: holders, hits, the break animation and its events, state 7 timer, cutscene name, unk_FE4; `bob_weapon_parameter` | `src/engine/entities/bob_weapon.h` |
| Swing state (factory type 0x78, 0x00097F90) | 0x1AC, parameters 0x128 | `swing_state_field`: phase, the weapon's pose when taken, trail and spin timers, direction, spin and grab poses, four effect slots, the effect and hit-sound names, seven animation records, shape mode, count, cutscene; `swing_state_parameter` | `src/engine/entities/body_swing_state.h` |
| Large projectile (0x000CD620), its own part | 0xF08, parameters 0x604 | `large_projectile_field`: throw target, fall, heading, spin, shatter, bounces, hits, rest pose, catch, hold offset and rise, command attachments, trail; `large_projectile_parameter` | `src/engine/entities/projectile_weapon.h` |
| Ghouly cutscene setup (factory type 0x25, 0x0006B0F0), its own part | 0x9DC, parameters 0x3C4 | `ghouly_cutscene_field`, `ghouly_cutscene_parameter` | `src/engine/entities/ghouly_cutscene_setup.h` |
| Bone debris (factory type 0x12, 0x00067E40), its own part | 0xAAC, parameters 0x55C | `bone_debris_field`, `bone_debris_parameter` | `src/engine/entities/bone_debris_actor.h` |
| Ghouly bg-break setup (factory type 0x24, 0x0006A4F0), its own part | 0xA7C, parameters 0x460 | `ghouly_bg_break_field`, `ghouly_bg_break_parameter` | `src/engine/entities/ghouly_bg_break_setup.h` |
| Bg-break key (factory type 0x1E, 0x0006CC70), its own part | 0xA70, parameters 0x3B4 | `bg_break_key_field`, `bg_break_key_parameter` | `src/engine/entities/bg_break_key.h` |
| Save record, one field | — | `save_record_field::unk_184` (0x0010DC20 / 0x0010E2C0) | `src/engine/saves/save_scan.h` |
| Base mind (0x00029D10), shared by every mind type | 0x7C, parameters 0xA0 | `mind_field`: actor, target, target record, action record, action actor, path search, target mode, gaze anchor and point, alert mode and timer, last request result, target flags, categories and range, goal point; unk_2C, unk_34, unk_38 | `src/engine/entities/mind_fields.h` |
| Body record (actor +0x95C, built by 0x00024C50), two passes | not settled | `body_field`: position, move holds and move hold time, focus point, attached actor, unk_70, reach, hit record, energy and its maximum, stagger, ground point and has-ground, motion, holds, attacker, unk_274, contact count, velocity, gravity, step speed, heading, ease rate (and its saved value and swap count), turn rates, euler (and `level_body_euler`), surface (and its override and frames), scaled surfaces, height and radius scales | `src/engine/entities/body_fields.h` |
| Knockdown state (factory type 0x7A, 0x0009C5D0) | 0xF4, parameters 0x50 | `knockdown_state_field`: phase and its clock, end timer, the seven animations' found flags (`KnockdownAnimation` slots), the end keys, the sweep point, callout tokens, start and landing points, saved gravity and shape mode, yaw, pitch, flight time, pose marks, rise height, bounce scale, attacker, contact pool, latch and body-bit words; `knockdown_state_parameter` | `src/engine/entities/body_knockdown_state.h` |
| Hit and furnace-hit states (factory types 0x73, 0x000957A0, and 0x74, 0x00096710) | 0x70 / 0x30, parameters 0x28 / 0x14 | `hit_state_field`: lunge velocity, target hit, open windows, hit callout, start euler, attached actor, trail rate and timers, face and carry switches, next key, event mark, body bit; `hit_furnace_state_field`: next key, bit 0x38 switch, sweep point; and their parameters | `src/engine/entities/body_hit_state.h` |
| Struggle state (factory type 0x88, 0x000A2B90) | 0x4C, parameters 0x34 | `struggle_state_field`: input, idle and moving keys, settle switch, animations found, band timer and mode, started, turn limit, heading offset, turn rates; `struggle_state_parameter` | `src/engine/entities/body_struggle_state.h` |
| Dizzy state (factory type 0x64, 0x0008C9A0) | 0x184, parameters 0x11C | `dizzy_state_field`: the two input trackers and their states, player input, animation wait, settle switch, star scale, time left, star names, the three stars (`dizzy_star_field`: orbit, bob, actor), body bit; `dizzy_state_parameter` | `src/engine/entities/body_dizzy_state.h` |
| Anim state (factory type 0x5E, 0x00088580) | 0x40, parameters 0xB0 | `anim_state_field`: next key, body bit, time left and timed switch, sound started, face-target switch and heading offset, stick point, carry and settle switches; `anim_state_parameter` (with the inline sound name) | `src/engine/entities/body_anim_state.h` |
| Die state (factory type 0x62, 0x0008C240) | 0x6C, parameters 0x20 | `die_state_field`: animation name, body bit, moving-key, face-view and settle switches, target; `die_state_parameter` | `src/engine/entities/body_die_state.h` |
| Player death state (factory type 0x69, 0x0008EF30) | 0x3C, parameters 0x18 | `player_death_state_field`: phase, wait timer, saved shape mode, death word, contacts raised, fail stream, saved body +0x717, animations found; `player_death_state_parameter` | `src/engine/entities/body_player_death_state.h` |
| Grabbed state (factory type 0x70, 0x00094350) | 0x28, parameters 0x18 | `grabbed_state_field`: release mark, move hold, latch hold, body bit; `grabbed_state_parameter` | `src/engine/entities/body_grabbed_state.h` |
| Stand alert (factory type 0x87, 0x000A2590) | 0x50, parameters 0xBC | `stand_alert_field`: input, alert and moving keys, counter, pause and hold switches, model node slot and keep switch, secondary detach and mode switches with the saved mode, settle switch, sound started, body bit, saved shape mode; `stand_alert_parameter` | `src/engine/entities/body_state_actions.h` |
| Walk alert and fly states (factory types 0x8E, 0x000A5650, and 0x6D, 0x00090D90) | 0x88 / 0x78, parameters 0xB8 / 0xA8 | `walk_alert_field` / `fly_state_field`: stop and alert keys, stick limit, heading hold, row count, band, the four speed-band rows (`walk_alert_row_field`: animation, lower and upper durations, top speed), sound started; the walk alert's settle, stop-at-once, alert-when-calm, fixed-ratio and body-bit words; the fly state's loop; and their parameters | `src/engine/entities/body_state_actions.h` |
| fxCustom and its effects (factory types 0xC, 0x2E, 0x30, 0x31, 0x32, 0x33) | 0x18 base; 0xA8, 0x28, 0xD0, 0x54, 0x28 | `fx_custom_field` (seconds alive, actor); `fx_electric_field` (sound, texture loaded, spark count and rows, light slot); `fx_fade_field` (the fade-to-grey's and fade-out's level, target, speed and their +0x24 word); `fx_outline_field` (phase, hold end, time left, the three times, sound, texture loaded, scale, colour, level, shape group, rows, light slot); `fx_solid_fade_in_field` (phase, time left, level, the four times, colour, strength, hold end); and their parameters | `src/engine/entities/fx_custom_electric.h` |
| Weapon base (factory type 0xB over the actor, 0x00145C80), its own part | 0xE54 (0x934 onward), parameters 0x5F8 | `weapon_parameter` (the block 0x00145C80 copies from); `weapon_field`: mask and its neighbours, firing, accepted classes, contact test, spawn-model block, names, break category, grab prompt (pulse, phase, alpha, fade), shape and point counts, pick-up count, variant name and attributes, spawn trigger, group owner, state and its next and initial values and time, scene start, thrown and follow motion, floor, voice, spawn owner, broken pieces, hit kind/strength/style, animation, target record | `src/engine/entities/weapon_fields.h` |
| Player (factory type 5 over the actor, 0x00028830), its own part | 0xCF8 (0x934 onward) | `player_field`: case link, mind, body, input device, provoked and the two countdowns, unk_984, goal volume, latch and delay, ghouly spawn record, kind, count and box, saved, start position and euler, pause count, mode, owner class, live, cells, floor, damage class, tag | `src/engine/entities/player_fields.h` |
| Actor parameters (type 2's block, read by 0x0003E940, filled by 0x000411A0) | 0x334 | `actor_parameter_field`: position, euler, scale, model scale, the model, animation, animation table, resource and attachment names, attachment bone, marker id and next, secondary mode and model; with `object_parameter_field`'s size, unk_2 and type before them | `include/gbtg/object_factory.h` |
| Ghouly trigger (factory type 0x23 over the actor, 0x00069660) | 0xC6C, parameters 0x65C | `trigger_field`: spawn radii, goal and goal names, anchor, box record, facing, unk_B4C, box name and attributes, spawn trigger radius and chance, slot count and slots; `trigger_parameter` | `src/engine/entities/trigger_actor.h` |
| Actor effect particle (0x150-byte slots from an effect's +0x390, 0x000EFE30; run by 0x000F18E0) | 0x150 | `effect_particle_field`: position, euler, velocity, spin, collision radius, bounce factors, size (and its rate, target and scale), gravity, drag, hit bytes, motion and channel flags, next free slot, collisions, colour channels, life, colour durations, oscillation, frames, bounded channels, owner, command. The debris and trail pools' records are different types and stay by offset | `src/engine/graphics/effect_particles.h` |
| Mind target record (a live attachment record, 0x0012AFE0, in the DAT_00545988 pool) | 0x68 | `mind_target_field`: actor, kind, hold, flags, unk_18, floor, the two 0x20-byte shapes (and the shape position, height and radius and the secondary radius by name), direction | `src/engine/entities/player_mind_targets.h` |
| Camera mind (factory type 0x36, 0x0007AFC0), its own part after the 0x7C-byte base mind | 0xDC, parameters 0xA0 | `camera_mind_field`: focus actor and its blend, follow point, held target point, heading goal (active, restart, heading, hold, speed, tag), heading, pushed time, heading held, distance, route reaches eye, above floor | `src/engine/camera/camera_mind_rig.h` |

The camera mind was checked with `--player-opening` and headless captures of `chapter1_scene1_2playcam` at 300 and
900 frames: the 14 test captures (the intro and the left-stick walk the camera follows) and both scene captures are
byte-identical to the build before the change. That is weaker evidence than it looks: `--player-opening` is not
deterministic (two back-to-back runs of one build gave different walk captures), so it matched by luck as much as
by equivalence. Which camera paths those runs reach (the focus blend, the held target point under body bit 0x30, the
push timer's move) wasn't checked. Its base part is `mind_field`.

The base mind was named in one pass over 81 files: accessors whose type matched the field were rewritten by script,
the rest (float fields written as words, the +0x20 byte, the Vec3 blocks, `memcpy` writes) by hand. Every replaced
access keeps its offset, width and bits. `--scene3` (54 captures) and `--scene6` (64 captures) are byte-identical to
the build before the change; one `--scene6` run differed in 3 bytes of `scene6_title.bmp`, and two reruns matched, so
that mode has rare noise too. That noise was the game polling real-time audio; with the tests' virtual audio clock
(`pcm_use_virtual_audio_clock`), `--player-opening` (14 captures) and `--player-death` (95 captures and its per-frame
imp log) are byte-identical with and without the change.
Offsets into derived minds (the player mind's +0xB8 record, the cinema mind's +0x7C radius and +0x98) stay literal.

The mind target record was named by script from the census's exact file:line pairs (204 reads in 42 files; the
census lists no writes into it). Same-buffer reads in the blend-shape draw, the event sequence and the player mind's
setup and events were left alone, since their buffers there aren't target records. All captures are byte-identical
to the build before the change in `--player-opening`, `--player-death`, `--scene3`, `--scene2a2-reaper`,
`--scene2b8`, `--scene2c1`, `--scene2c7` and `--scene3a4`. `--scene6` matched except for `scene6_title.bmp`, which
differed from one baseline run by 3 bytes and matched a second baseline run exactly. So the title noise survives the
virtual audio clock. `--scene3c1` can't serve as evidence: one baseline exe gives different runs, starting from the
frame-0 region yaw (0.965 in one run, 3.800 in the other), so some source other than audio timing still varies there.

The body record's first pass (`body_field`, 25 fields) rewrote about 405 lines in 115 files by script: census accessors
whose type matches the field, the files' own Vec3 writers at a Vec3 field's base, and byte reads of the two surface
bytes. `memcpy` accesses and single-component writes into the Vec3 fields stay literal. All captures are
byte-identical to the build before the change in `--player-opening` (14), `--player-death` (95), `--player-fight`
(52), `--scene3` (54), `--scene5` (63), `--scene6` (64), `--scene2a2-reaper` (123), `--scene2b3` (84) and `--scene3a4`
(34). In `--scene2c7` two captures differed from the HEAD run but matched two earlier runs exactly; that mode varies
the same way as `--scene3c1`. In both, the frame-0 log prints a kind-0 region's yaw that changes from run to run
(1.1e-44, 0.95, 5.4), which suggests a read of uninitialized memory.

The knockdown, hit and furnace-hit states were named by hand, one file each. Every state and parameter offset now
has a name, and the knockdown's animation slots are `KnockdownAnimation` constants. All captures are byte-identical to
the build before the change in `--player-death` (95), `--player-fight` (52, whose imps take Cooper's knockdown
finisher), `--scene3` (54), `--scene5` (63, where Cooper runs the hit and furnace-hit states), `--scene6` (64),
`--scene2a2-reaper` (123), `--scene2b3` (84), `--scene3a4` (34) and `--player-opening` (14). The logs differ only in
the exe's name and a code address in a stack trace.

The struggle, dizzy, anim, die, player death and grabbed states were named by hand, along with the body's turn rates
(+0x71C..+0x724, which the hover state now writes through `body_field::turn_rates`). The comparison reused the previous
build's stored captures as its baseline instead of re-running it. All captures match in `--player-death`,
`--player-fight`, `--scene5`, `--scene3`, `--scene2a2-reaper`, `--scene2b3`, `--scene3a4` and `--player-opening`.
`--scene6` matched except for `scene6_title.bmp`, which has two versions 3 bytes apart that every build has produced.
The imp fight in `--player-fight` and `--player-death` runs the dizzy, die and anim states, and `--player-death` runs
the player death state. No mode in the set is known to reach the struggle, grabbed or hover states.

The stand alert, walk alert and fly states were named by hand across their constructors (`body_state_actions.cpp`)
and ticks (`body_walk_alert_tick.cpp`, `body_stand_alert_tick.cpp`); the band code both alert states share reads the
walk alert's names, and a static_assert holds the fly state's rows at the same offsets. Body velocity and euler
writes set the whole Vec3. Against the previous build's stored captures, all match in the same nine modes, apart from
`scene6_title.bmp`'s other known version. `--scene5` puts Cooper in the stand and walk alerts; no mode in the set is
known to reach the fly state.

fxCustom and its five effects were named by hand in `fx_custom_electric.cpp`; each function reads its effect through
a span of the effect's own size. Against the previous build, all captures match in `--scene2a2-reaper` (the fade to
grey), `--scene2a10` (the haunted TV's electric effect; its baseline was run fresh), `--player-death`,
`--player-fight`, `--scene5`, `--scene3`, `--scene2b3` and `--player-opening`. `scene6_title.bmp` showed its other
known version again. `scene3a4_2400.bmp` differed by 1 byte from all eight earlier runs, and a second run of the same
build matched them exactly. So `--scene3a4` also varies, though rarely.

The weapon base is its own part of type 0xB (+0x934..+0xE54 over the 0x934-byte actor). Players (type 5, 0xCF8
bytes) also derive from the actor, so a player's +0xCDC..+0xCF4 are different fields from the weapon's at the same
offsets. The weapon modules were rewritten by script: typed reads and stores whose width matches the field, the u8
fields' byte accesses, the strings' address forms, and float stores through `std::bit_cast`. The last float zeroes
and the aim's writes were edited by hand. Against the previous build, all captures match in `--scene3` (the pick-up,
lift and throw of weapons), `--scene2a10`, `--scene2a2-reaper`, `--player-death`, `--player-fight`, `--scene5`,
`--scene2b3` and `--player-opening`. `scene6_title.bmp` and `scene3a4_2400.bmp` matched other earlier runs exactly
(their known variation).

The player's part (type 5 after the actor) was named by script from the census lines on actor-like buffers (the
actor, owner, source, holder and player views; the weapon modules skipped), 199 lines in 75 files. The class word at
+0xCDE is `owner_class` (0x0001D180's parameter); +0xCF4, which some callers had called the actor class, is
`damage_class`, as the native `Actor` names it. Against the previous build, all captures match in `--player-death`,
`--player-fight`, `--scene5`, `--scene3`, `--scene2a10`, `--scene2a2-reaper`, `--scene6`, `--scene2b3` and
`--player-opening`. `scene3a4_2400.bmp` again showed its 1-byte variant; this time the log's only difference was the
frame-0 region yaws (31.45 against 4.03, and so on). So the scene3a4 noise follows the same uninitialized region yaw
as `--scene2c7` and `--scene3c1`.

The actor's parameter block (`actor_parameter_field`, 0x334 bytes) is named in its reader (0x0003E940), its defaults
(0x000411A0) and the marker handlers' builders, which also write the object header through `object_parameter_field`.
The builders' type-specific fields past +0x334 stay by offset until each consuming type names its parameters. Against
the previous build all captures match in the nine regular modes (`scene6_title.bmp` its other known version).

The kind-0 region yaw the scene logs printed was an occupancy sphere's +0x34, which 0x001246A0 never writes (only
boxes have a yaw) on a block from `engine_heap_allocate`, whose retail HeapAlloc doesn't zero. The host heap now
zero-fills new and grown blocks, so it reads 0 every run. That did not make `--scene2c7` or `--scene3c1` repeat:
two runs of one exe still diverge in scene3c1's ghoul fights (near frame 28,600 in one pair, 30,000 in another, with
the exe's own ASLR turned off), and scene2c7 still varies in one capture. Nothing reads a sphere's yaw, so it was a
symptom of uninitialized memory, not the cause; the source is still to be found. The nine regular modes match the
previous build with the zero-filled heap.

The body's second pass named its next most-read fields by the same script, and replaced the states' 35 pairs of pitch and roll zeroings with `level_body_euler`, which writes the same bytes. Component writes of the position and velocity stay as they are, since folding them would reorder reads and writes where one expression reads another component.

The marker handlers' parameter builders now write through their consuming types' names: the ghouly trigger (`trigger_parameter`, new), the background-break setup, the cutscene and swap setups, the background-break key and the scene fx control. Builders that were named for their marker rather than the object they build took the object's name: `..._effect` is now `..._bg_break_setup`, `..._switch` `..._cutscene_setup`, `..._named_trigger` `..._swap_setup`, and `..._named` `..._bg_break_key`.
The Super Scary shock's builder (`..._multi_name`, now `..._super_scary_shock`) and the weapon template's patch (0x001007F0, 0x00146C80) write through `super_scary_shock_parameter` and the new `weapon_parameter`, and the builders' remaining actor-header writes go through `actor_parameter_field`. The chair's own parameters (+0x5F8 onward) stay by offset.

Super Scary checked with `--scene6`: it passes, the challenge's phase sequence and the trophy-heads states match the
log from before the change line for line, and all 45 captures are byte-identical. That run covers phases 1–5 and
8–11, through both failure and success. It doesn't reach phases 6 and 7 or the DAT_005122F0 auto-press path, and it
doesn't check which scare modes occur, so the bound-mode (2/3) branches may not have run. The attract control still
reads +0x20 directly.

The trophy heads and the scene 3 types above (the code the scene 6 and scene 3 work added) were named in one pass.
`--scene6` and `--scene3` pass, and all 90 of their captures are byte-identical to the captures from before the
change. Those runs cover the trophy heads, the walk-target, hold, pick-up, lift and throw states, the skeleton, the
bob, the large projectile (the trophy) and the bone debris. They don't reach the bound scare modes, the bob's state 7
or the bone debris' optional sound. Only each type's own fields are named: offsets into the objects these types reach
but don't own (the player body, mind and actor, the weapon base's +0xC0C..+0xDD0, the mind's target records, the
matrix rows and effect particles the bone debris drives, and the 0x000426F0 bound record) stay literal until those
types are named. `strategy_target_selection.cpp` touches only such shared objects and is unchanged.

## Next

- Super Scary can be consolidated the same way: its state is reached only through its own functions and the
  `--scene6` test.
- Merge the per-file `word` / `put_word` / `real` copies into `retail_field.h` as their files are converted.
- The 0x30-byte screen texture (0x000533A0) is the engine's texture resource (width +8, height +0xA, header offset
  +4, data +0x14, size +0x18), which the widgets' `texture_width` helpers and the presenter also read. It belongs
  with the texture resource's own layout.
- The attract control, starting with `scene_control_field::elapsed`, then the entities. The body (`body_field`),
  base mind (`mind_field`), mind target records (`mind_target_field`), weapon base (`weapon_field`) and the player's
  part (`player_field`) are named now. The actor's own 0x934 bytes are mostly read through the native `Actor`; the
  raw reads left on them (+0x18, +0xB8, +0x1FC) are few. The body's first pass leaves most offsets literal; player
  body +0x728 (written by Super Scary through `set_body_word_728`) is one of them. The weapon base's accepted-class
  rows (+0x948) and +0xAC0 kept flags, and the player part's writes through local `put` / `memcpy` helpers, are still
  by offset.
- `DAT_` globals, with the same evidence rule.
