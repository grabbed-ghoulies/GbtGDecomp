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
| Actor matrix record (`ActorMatrixRecord`, actor +0x288, one per ndMtxArray matrix; 0x00040200, 0x00040640): binding, translation, euler, scale, center, second euler and its switch, active. Viewed over the actor's heap block by `actor_matrix_records`. | 0x48 | `include/gbtg/actor_runtime.h` |
| Debris record (`EffectDebrisRecord`, 50 in DAT_005068EC): position, euler, size, growth and its rate, colour channels (`EffectColourChannels`), phase and its time, next free, type, frame | 0x74 | `src/engine/graphics/effect_debris.h` |
| Damage record (`BodyDamage`, built on the stack by the hit messages and source contacts, handed to 0x00020DC0): kind, keys, joint, latch mask, switches, position, normal, contacts, the three actors, push, direction, timer, source class. Message 0x1A carries its bytes, so `offsetof` checks pin every member. | 0x68 | `src/engine/entities/body_damage_fields.h` |
| Actor effect particle (`EffectParticle`, 0x150-byte slots from an effect's +0x390, 0x000EFE30; run by 0x000F18E0): position, euler, velocity, spin, collision radius, bounce factors, size (and its rate, target and scale), gravity, drag, stopped and hit bytes, motion and channel flags, next free slot, collisions, colour channels (`EffectColourChannels`), life, colour durations, oscillation (`EffectOscillation`: rate, phase, center, amplitude, value), frames and three bounded channels (`BoundedChannel`), owner effect, command. Viewed inside `ActorEffectNode::bytes` by `actor_effect_particle`. The debris and trail pools' records are different types. | 0x150 | `src/engine/graphics/effect_particles.h` |
| Trail record (`EffectTrailRecord`, 50 in DAT_005068D8): position and rotation (the billboard's, from 0x000EBCB0), colour channels (`EffectColourChannels`), scale, size and its target and rate, frames (`BoundedChannel`), phase, type, phase time, life, next free | 0x84 | `src/engine/graphics/effect_trails.h` |
| Body-state base (`BodyStateBase`, the factory header and type 9's instance): vtable address, type, unk_6 (the parameters' +2), body (the parameters' +8), next key, unk_E, elapsed, unk_14. `body_state_view<T>` views a factory state's bytes as its struct. | 0x18 | `src/engine/entities/body_state_fields.h` |
| Hold state (`HoldState`, factory type 0x8A, 0x000A34A0): the base, timer, settle turn, body bit | 0x24 | `src/engine/entities/body_hold_state.h` |
| Grabbed state (`GrabbedState`, factory type 0x70, 0x00094350): the base, release mark, move hold, latch hold, body bit | 0x28 | `src/engine/entities/body_grabbed_state.h` |
| Pick-up state (`PickupState`, factory type 0x7C, 0x0009DE00): the base, taken, start position, release point | 0x2C | `src/engine/entities/body_pickup_state.h` |
| Lift state (`LiftState`, factory type 0x7D, 0x0009E4B0): the base, ask, hands and release points, hold height, weapon, stage | 0x30 | `src/engine/entities/body_pickup_state.h` |
| Die state (`DieState`, factory type 0x62, 0x0008C240): the base, animation name, body bit, moving-key, face-view and settle switches, target | 0x6C | `src/engine/entities/body_die_state.h` |
| Anim state (`AnimState`, factory type 0x5E, 0x00088580): next key, body bit, time left and timed switch, sound started, face-target switch and heading offset, stick point, carry and settle switches | 0x40 | `src/engine/entities/body_anim_state.h` |
| Death state (`DeathState`): waiting, delay, saved shape mode; +0x21 unk | 0x28 | `src/engine/entities/body_death_state.h` |
| Furnace hit state (`FurnaceHitState`): start position and euler, furnace position, facing, remaining | 0x4C | `src/engine/entities/body_furnace_hit_state.h` |
| Hit state (`HitState`, factory type 0x73, 0x000957A0): lunge velocity, target hit, open windows, hit callout, start euler, attached actor, trail rate and timers, face and carry switches, next key, event mark, body bit | 0x70 | `src/engine/entities/body_hit_state.h` |
| Hit-furnace state (`HitFurnaceState`, factory type 0x74, 0x00096710): next key, bit 0x38 switch, sweep and its point | 0x30 | `src/engine/entities/body_hit_state.h` |
| Scoop-whack, shocked, stunned and scrabble states (`ScoopWhackState`, `ShockedState`, `StunnedState`, `ScrabbleState`) | 0x20 / 0x1C / 0x20 / 0x28 | `body_scoop_whack_state.h`, `body_shocked_state.h`, `body_stunned_state.h`, `body_scrabble_state.h` |
| Throw state (`ThrowState`, factory type 0x8B, 0x000A37D0): entry euler, aim and launch, switches, near range, aim angle and mode, spread, end key, ended | 0x68 | `src/engine/entities/body_throw_state.h` |
| Struggle state (`StruggleState`, factory type 0x88, 0x000A2B90): input, idle and moving keys, settle switch, animations found, band timer and mode, started, turn limit, heading offset, turn rates | 0x4C | `src/engine/entities/body_struggle_state.h` |
| Player death state (`PlayerDeathState`, factory type 0x69, 0x0008EF30): phase, wait timer, saved shape mode, death word, contacts raised, fail stream, saved body +0x717, animations found | 0x3C | `src/engine/entities/body_player_death_state.h` |
| Hover, flip, explode, spawn-ghouly, slide and look-for-weapon states (`HoverState`, `FlipState`, `ExplodeState`, `SpawnGhoulyState`, `SlideState`, `LookForWeaponState`) | 0x24 / 0x30 / 0x28 / 0x20 / 0x3C / 0x34 | `body_hover_state.h`, `body_flip_state.h`, `body_explode_state.h`, `body_spawn_ghouly_state.h`, `body_slide_state.h`, `body_look_for_weapon_state.h` |
| Endure, flamethrower, shoot, transform, drone-appear and drone-dance states (`EndureState`, `FlamethrowerState`, `ShootState`, `TransformState`, `DroneAppearState`, `DroneDanceState`) | 0x1BC / 0x144 / 0xD0 / 0x3BC / 0x28 / 0xD0 | `body_endure_state.h`, `body_flamethrower_state.h`, `body_shoot_state.h`, `body_transform_state.h`, `body_drone_appear_state.h`, `body_drone_dance_state.h` |
| Stand alert (`StandAlertState`, factory type 0x87, 0x000A2590): input, alert and moving keys, counter, pause and hold switches, model node slot and keep switch, secondary detach and mode switches with the saved mode, sound started, body bit, saved shape mode | 0x50 | `src/engine/entities/body_state_actions.h` |
| Slam smash, yoyo spin, grab throw and arc attack states (`SlamSmashState`, `YoyoSpinState`, `GrabThrowState`, `ArcAttackState`) | 0xD8 / 0x1DC / 0x5C / 0x268 | `body_slam_smash_state.h`, `body_yoyo_spin_state.h`, `body_grab_throw_state.h`, `body_arc_attack_state.h` |
| Walk state (`WalkState`): bit, next, turn, time, length, orbit and size, the two names, three carried objects (`WalkObjectSlot`: orbit angle, bob angle, object) | 0x160 | `src/engine/entities/body_walk_state.h` |
| Hit stream state (`HitStreamState`): sounds, push speed, stream time, drained energy, hit and sounding switches, effect, callout 0x36's tokens (a byte block the attachment functions take) | 0x160 | `src/engine/entities/body_hit_stream_state.h` |
| Arc state (`ArcState`): end key, animations found, time and duration, start and target eulers, phase, count and latch switches, start and target, the 0x51 callout's tokens | 0x90 | `src/engine/entities/body_arc_state.h` |
| Swing state (`SwingState`, factory type 0x78, 0x00097F90): phase, the weapon's pose when taken, timers, poses, four trail effects (`SwingEffectPair`), seven animation words, shape mode, hit sound | 0x1AC | `src/engine/entities/body_swing_state.h` |
| Bandaged and blood suck states (`BandagedState`, `BloodSuckState`): their names, sounds, phases and switches, with the shared `InputTrackingRecord` | 0x3DC / 0x180 | `src/engine/entities/body_bandaged_state.h`, `body_blood_suck_state.h` |
| Input tracking as a struct (`InputTrackingRecord`): axes, activity, accumulation, hold timer, state. 0x00109810 takes its `retail_bytes`. | 0x18 | `src/engine/entities/input_tracking_record.h` |
| Dizzy state (`DizzyState`, factory type 0x64, 0x0008C9A0): two `InputTrackingRecord`s, player input, animation wait, settle switch, star scale, time left, star names, three stars (`DizzyStar`: orbit, bob, actor), body bit | 0x184 | `src/engine/entities/body_dizzy_state.h` |
| Spin dizzy state (`SpinDizzyState`, factory type 0x86 over 0x64, 0x000A21D0): the whole `DizzyState`, then turn and reach rates, speed, three body words, the circle's centre, sounding | 0x1AC | `src/engine/entities/body_spin_dizzy_state.h` |
| Knockdown state (`KnockdownState`, factory type 0x7A, 0x0009C5D0): phase and its clock, end timer, the seven animations' found flags (`KnockdownAnimation` slots), the end keys, the sweep point, callout tokens (`KnockdownCalloutTokens`), the pose and flight values, contact pool, landing point, switches | 0xF4 | `src/engine/entities/body_knockdown_state.h` |
| Walk target and fly alert states (`WalkTargetState`, `FlyAlertState`): the rows block (`WalkTargetRows`: four `WalkTargetRow`s and the band limits), arrival delay and arrived, the keys, direction and band | 0x7C | `src/engine/entities/body_walk_target_state.h` |
| Walk alert and fly states (`WalkAlertState`, `FlyState`): keys, switches, stick limit, row count and band, four `WalkAlertRow`s, sound | 0x88 / 0x78 | `src/engine/entities/body_state_actions.h` |
| Jump on state (`JumpOnState`, factory type 0x79, 0x0009A4C0): the leap (`LeapRecord`, 0x50: target, start and current euler, start, end and current point, length, time left), phase, switches, the ride (`JumpOnRide`, 0xC8: phase, strikes, animations found, wobble timer and wobble, hold and ride times, `InputTrackingRecord`, hold sound, hat intro), animations found, claim and target | 0x158 | `src/engine/entities/body_jump_on_state.h` |
| Generate ghouly state (`GenerateGhoulyState`): phase, grow, animations found, the sound, model and effect names, two bolt rows (`GhoulyBoltRow`: pool, shape, angle), texture switches, the ghouly's name, variant and kind, attached actor, scales, beam, spawn point, light | 0x4A0 | `src/engine/entities/generate_ghouly_state.h` |
| Grabbed hold state (factory type 0x71): the base and nothing else, so its functions take `BodyStateBase` | 0x18 | `src/engine/entities/body_grabbed_hold_state.h` |
| Base strategy (`StrategyBase`, factory type 8): vtable address, type, unk_6, actor, the goals' next states and state, attacking, unk_1B, pause count, the target queue link (kept by `RuntimeStrategyLink`), wait timer. `strategy_base` views any strategy's bytes as it. | 0x38 | `src/engine/entities/strategy_fields.h` |
| Scene-control base (`SceneControlBase`, type 3's instance): vtable address, type, unk_6, the list links (kept by `SceneControlInstance`), elapsed. `scene_control_view<T>` views a control's bytes as its struct. | 0x24 | `src/engine/scene/scene_control_fields.h` |
| Walkabout music control (`WalkaboutControlState`, 0x00080570): the playlist word (kept by `SceneControlInstance`), position and count, track, start countdown, timer switch | 0x3C | `src/engine/scene/scene_control_fields.h` |
| Chapter 2 scene 4 control (`Chapter2Scene4ControlState`, factory type 0x95): phase, wait, morph time | 0x38 | `src/engine/scene/chapter2_scene4_control.h` |
| Scene fx control (`SceneFxControlState`): scale, radius, count, effect, on and off times, rate, prerun, eight copies (`SceneFxSlot`) | 0x1EC | `src/engine/scene/scene_fx_control.h` |
| Final score control (`FinalScoreControlState`, factory type 0x99): the base and the page | 0x28 | `src/engine/frontend/final_score.h` |
| Haunted chairs control (`HauntedChairsControlState`, factory type 0x93; retail 0x74): chairs, their cutscene rows, count, phase | host | `src/engine/scene/haunted_chairs_control.h` |
| Relics store control (`RelicsStoreControlState`, factory type 0x94; retail 0x5C): walk cutscene, phase, pulses, marker, mummy, player, candle, flame block and its pointer words, pause held | host | `src/engine/scene/relics_store_control.h` |
| Trophy heads control (`TrophyHeadsControlState`, factory type 0x92; retail 0x188): cutscene, player, attacks, health, seek time, phase, chosen heads, per-head attacked and roaring flags, attacking head, pause hold, head events, challenge won | host | `src/engine/scene/trophy_heads_control.h` |
| Attract control (`AttractControlState`, factory type 0x9B; retail 0x324): clip, banner text, pool and id, camera, banner time, started, clips and cutscene, clip count, next script | host | `src/engine/scene/attract_control.h` |
| Super Scary challenge (`SuperScaryControlState`, factory type 0x98; retail 0x2C8): actor addresses, phase, input, camera, timers, clip names and placements, shock camera, presses and button row, bound position, head binding, button textures, settle, render swap | host | `src/engine/scene/super_scary_control.h` |
| Mind events (`MindEventRecord`, 0x0002A190; DAT_003398C8 sizes each kind): kind, timers and age. `MindSubjectEvent` adds the subject (kinds 3, 4); `MindContactEvent` adds other, count and payload (kinds 1, 2) | 0x20 / 0x24 / 0x30 | `src/engine/entities/player_mind_events.h` |
| Bounded channel (`BoundedChannel`, 0x00101980, 0x00101A40, 0x00101B00; three in a row for 0x001062E0, 0x001063D0, 0x00106590): lower, upper, rate, value, mode. Its functions take the struct. | 0x14 | `src/engine/entities/bounded_channels.h` |
| Actor base (`ActorBase`, type 2, the start of every actor-derived record): the fields the native `Actor` doesn't keep: bone, eased and its target, age, ticks | 0x934 | `src/engine/entities/actor_fields.h` |
| Colour channels (`EffectColourChannels`; an effect particle's +0x60, a debris record's +0x24, a trail record's +0x10; run by 0x000F1F60, 0x000F22F0, 0x000F23E0, 0x000F2430): five values, targets and rates as arrays, with `red()`, `alpha_rate()` and so on for single channels | 0x3C | `src/engine/graphics/effect_colour_channels.h` |
| Scene light record (`LightRecord`, 0x00051BC0; DAT_00509B7C): position, direction, local point and joint, kind, flags and swing flags, the two ranges, colour, the colour and range limits and periods. `light_record(u8*)` views a record. | 0x98 | `include/gbtg/model_runtime.h` |
| Light cones (`RuntimeLightCone`, 16 in DAT_0050EF20, 0x0011AF90) and their rim points (`LightConeRim`). The host adds the actor's native view. | 0x160, rim 0x14 | `src/engine/graphics/light_cones.h` |
| Event step (`EventStep`, a DAT_005064E0 pool row, built by script nodes 0x19..0x88, run by 0x000D0B40): type, delay block (`EventStepDelay`: left, started, waits for cutscene), icon, icon shown, and a union of one struct per type: tag, kill count, timed, done, spare tag, next different, unhit since, one type, number, key carrier, attack limit, tag kill count | 0x88 | `src/engine/scripts/script_event_step_fields.h` |
| Background cloth header (`BackgroundClothHeader`, the start of a cloth's block, 0x0004B5F0, viewed with `cloth_header`): the point graph's descriptor, grid, light, bounds, hooks, shadow state, sound, draw switches. The points, edges, colours and normals follow it. | 0x1F0 | `src/engine/graphics/background_cloth.h` |
| Actor effect header (`ActorEffectHeader`, an emitter, 0x000ED1C0, `ActorEffectNode::bytes`, viewed with `effect_header`): particle counts and free head, emitting, emit timer, life, ticks, flags, position and its previous and tick copies, emit point, motion, euler, spin, jitter, offset, velocity, particle size, volume euler and spin, scale, binding, the emission volume and its angles, velocity mode, speed and angle ranges, script, command, draw and depth modes, texture and its frames, model, the particle and collision sounds and the collision effect. The particles follow it. | 0x390, plus 0x150 per particle | `src/engine/graphics/actor_effect_nodes.h` |
| Point graph (`src/engine/graphics/point_graph.h`, 0x00102500 and on): the solver's `PointGraphHeader` (counts, sorted and squared switches, damping; also the start of a cloth's header), a background animation's whole `PointGraphRecord` (the address words, bind, rest and placed matrices and two inverses, centre, reference edges, warm-up frames, age, iterations, active, contacted), `PointGraphPoint` (position, previous, force, inverse mass, pinned) and `PointGraphEdge` (ends by index and address, rest length, inverse squared and squared lengths) | 0x20 / 0x1E0 / 0x30 / 0x1C | `src/engine/graphics/point_graph.h` |
| Candle flame block (`CandleFlameBlock`, 0x000C6C80..0x000C7540; the candle's +0xE5C and the relics store's +0x40): lit, light slot, point light; its pointer words are `RuntimeCandleFlame`'s references | 0x18 | `src/engine/entities/candle_weapon.h` |


The actor matrix, debris and damage records were consolidated after their naming passes. Each one's fields were all
named and nothing read it by offset, apart from a few writers the census had missed: the sampled matrix's scale and
height (0x00040640), the weapon pieces, the bone debris and the background effect sync all wrote matrix records
directly, and `body_source_contacts.cpp` built its own damage records by offset. They now use the structs too. The
trail and debris types stay on descriptors, as they are resource data.

The effect particle followed once its oscillation block was named (0x000F1AF0 and 0x00106A80 set and tick its rate,
phase, center, amplitude and value) and its last raw readers were converted: the collision response and commands,
the channel setup, the three draws (which read the oscillation's value as a size scale and the bounded channels'
values as euler offsets), the bone debris and the background part destruction. Byte +0x54 is `stopped`: the move
returns at once while it is set, and the bone debris reads it as its piece resting. The channel flags are a word, as
the draws read them. The frames and the three bounded channels are `BoundedChannel` structs; the shared bounded and
colour channel functions still take byte spans (trail frames, the scoreboard and others use them too), so struct
members are handed over through `retail_bytes`. The particle lives inside the effect's byte buffer, viewed in place by
`actor_effect_particle`.

The trail record followed. Its +0..+0xF had looked untouched because its spawner isn't translated, but the retail
trail draw (0x000EBCB0, still unbound here) settles it: it hands +0 to the billboard queue (0x0005C1A0) as the
position and +0xC as the rotation, with the size from +0x50 and the frame from the frames' value. +0x72 and +0x80 are
not read or written by any translated code or that draw, so they are `unk_72` and `unk_80`.

The body states start with the hold state. The base was settled from the factory: 0x00107520 writes the vtable
address at +0 and the type at +4, the base object's constructor (0x00107700) copies the parameters' +2 to +6, and
type 9's (0x0002B600) copies the parameters' +8, the body, to +8. Type 9's instance is 0x18 bytes (0x00095530), and
every +0x14 the states name is a parameter field, so the base ends with an unnamed `unk_14`. The hold state's own
three words match its dumps (0x000A3520, 0x000A35E0, 0x000A3670, exported as ranges because the project has no
functions there). Its parameters are never copied in, so `settle_turn` and `body_bit` stay 0 unless something writes
them later; that is the dump's behaviour, kept as it is. The shared goal-action selector (0x00134060) still takes
the state's bytes.

The grabbed, pick-up, lift and die states followed on the same base. Their own fields were already all named, so
only their functions, the factory views and the body-message calls changed. Their parameter blocks are script data
and stay on descriptors.

Ten more followed: the anim, death, furnace hit, hit, hit-furnace, scoop-whack, shocked, stunned, throw and scrabble
states. Their own sources never read them by offset, and their only unnamed bytes are padding or words no translated
code touches (`unk_`). The furnace hit's own `elapsed` (+0x10) was the base's, so it is `base.elapsed` now. The shared
next-, recovery- and goal-action selectors still take the states' bytes through `retail_bytes`. A first run of
this pass froze Cooper in the hit-furnace state: two callers whose state was still a byte span had been given
`retail_bytes(state)`, which views the span object's own pointer and length, so the next key never reached the
state. `retail_bytes` now refuses a span at compile time.

Fourteen more followed: the hover, flip, struggle, explode, spawn-ghouly, player death, slide, look-for-weapon, endure,
flamethrower, shoot, transform, drone-appear and drone-dance states. Most keep host side tables of the animation
records and contact overrides retail holds as pointers; those stay keyed by the state's address, now through
`body_state_key`. The explode and drone-dance namespaces' own +0x10 floats were the base's `elapsed`. These passes
compare captures only, and whether the tests reach every one of these states wasn't checked.

The stand alert, slam smash, yoyo spin, grab throw and arc attack states followed. Their last offset uses were found
words picked by byte offset: the arc attack's animations now go by `ArcAttackAnimation`, and the slam smash's start
takes the found word. `arc_attack_state`'s +0x28 is the one word here no translated code touches (`unk_28`).

The walk and hit stream states' trailing +0x13C blocks were named from the code that uses them: three carried-object
slots, and the attachment tokens 0x0013B8B0 records. The tokens stay bytes because the attachment functions and the
projectile records share that form.

The arc, swing, bandaged and blood suck states' unnamed stretches were named the same way: the arc's +0x6C is the
same token block, the swing's +0x64 is four trail-effect pairs, and the bandaged +0x2A4 and blood suck +0x54 are
`InputTrackingRecord`s. Their old `struggle` and `tracking_state` fields were that record's accumulation and state.

The spin dizzy state is type 0x86 over the dizzy state, so `SpinDizzyState` embeds `DizzyState`. Its old fields at
+0xC, +0x58, +0x164, +0x170 and +0x17C were the dizzy state's next key, time left and three star actors, and are now
reached through `state.dizzy`.

The knockdown state followed. Its callout tokens had been named both as a whole and as a count and ids, so they are a
small struct handed to the attachment functions through `retail_bytes`. +0x24 and +0xDC are `unk_`.

The four row-table states followed. Each pair shares code retail runs on both layouts, so static checks pin the
offsets those functions depend on. 0x00091F70 runs on walk target and fly alert objects alike, so it views both as
`WalkTargetState`. 0x000A5FD0 serves the walk alert and fly states and now takes just the row count and rows. The
record lookups take the state's address (`body_state_key`). The parameter rows read from the XBE stay on descriptors.

The jump on state's unnamed stretches were its two records. +0x18 is the leap 0x00103650 starts and 0x001036D0 steps,
which the state had reached by raw offset (+0x30, +0x3C, +0x54..). +0x80 is the ride; its `shaken` and `shake_state`
were its `InputTrackingRecord`'s accumulation and state.

The family was closed out with the leftovers of the first passes (the shocked tick, a few destroy functions), the
grabbed hold and generate ghouly states, and the shared selectors 0x00133F50, 0x00134060 and 0x00134180, which only
ever wrote the next key and now take `BodyStateBase&`. That ended `retail_bytes(state)` at their call sites and
`body_state_field`, which is gone. `retail_bytes` remains for the blocks handed to shared byte functions: bounded
channels, input tracking, callout tokens and the wobble timer.

The base strategy was read by literal offset almost everywhere (+8, +0xE, +0x1A, +0x1B and +0x1C, some 275 places in all),
so it went straight to a struct rather than through descriptors. The `tests/*_checks.cpp` files keep their literal
offsets, since checking the retail layout is their job.

Every derived strategy is now a struct on `StrategyBase`, viewed with `strategy_view<T>`, each beside its own code; the
player's is in `strategy_fields.h`, as the state and children files read it too. Each `.cpp` has a small `fields_of`
for its own struct. The strings typed as one word (imp flying and imp ninja's effect names) are `std::array<char, 0x80>`,
and the haunted TV's six numbered textures and the baron's timelines are arrays. The actor-derived records with one
header and one source file went the same way, on `ActorBase` (the actor's 0x934 bytes) and `actor_record_view<T>`.

Only some of these run in the regular modes. The strategies for baron, baron plane, chicken, generic, ghoulie girl, giant
hand, haunted coat, door and picture, hunchback, imp flamethrower, imp ninja, Jessie and Clyde, cursed mummy, pirate, ugly
girl, vampire, vampire chicken, warlock and worm, and the anim delete, ghouly key, Super Scary door and shock actors, were
built and their offsets checked, but none of the regular modes runs them. The all-mode run below reaches all of them but
the baron and baron plane.

The shared actor records and the weapons followed: the avatars, bg break key, ghouly bg break and ghouly swap, then
`WeaponBase` (replacing `weapon_field`, viewed with `weapon_base`) and each weapon class on it, with the extinguisher,
single shot and water pistol on `GunWeaponState`. Words the host keeps natively are named `_word`, so a member marked
"never read" really is unaccounted for: +0xF40 in the water pistol, for one. The chair (`ChairWeaponState`) had no
descriptors, so its names come from `weapon_actor.cpp` alone. The hop and drop blocks are still read by inner offset,
and `read_u32(holder_bytes, 0xCE0)` and its like are the player's bytes (`PlayerRecord`). In the regular modes the
chair and large projectile run in 9 modes, the bob in 3, and the candle, container, water pistol and thrown projectile
in 1 each. The homing projectile, extinguisher and single shot run only in the all-mode run below, and the gun in none.

`CarriedWeaponStrategy` names the +0x38 word that 0x000BEB80 reads for both the pirate's chest and the baron's cane. The
custom effects are on `FxCustomBase` (the base object's header, the node words, elapsed and actor), viewed with
`fx_custom_view<T>`. `FxFadeState`'s +0x24 is a union: the fade-to-grey's texture or the fade-out's switch. Plain fxCustom
(type 0xC) and the fade-out (type 0x33) don't run in the regular modes; the all-mode run below creates the fade-out too.

`BodyRecord` replaces `body_field`. The record's size was listed as unsettled, but 0x0001DA30's sizes function gives 0x894,
0x00076180 gives the parent player body's 0x924, 0x00075AD0 the player body's 0x950 and 0x00073170 the camera body's 0x914.
About 1,500 sites moved. A site became a member access only where the accessor's type matched the member; otherwise it
keeps its accessor with `offsetof`, so nothing changes bit for bit. Those 100 or so are worth reading: they show retail using
a field as two types (`ease_rate` copied as words, a float zero written into `contact_count`), or a computed offset
(`offsetof(BodyRecord, unk_309) + index`), or a block copy.

The names are only as good as the evidence. Most fields new to this pass stay `unk_<offset>`, and counters raised and
dropped by many states (+0x69, +0x6A, +0x6B, +0x6D, +0x309..+0x30C, +0x7E5) are prime targets for a real name once someone
reads them against the dumps. +0x15C is one word, a pointer to the bit block, so "byte 7 of +0x15C" means a byte of that
block, not of the record. The two 0x54-byte blocks at +0x178 and +0x1CC are contact match blocks (0x000FCD70), the same size
as the weapon's `contact_match`; only their parts the code names are split out.

The layout checks in `tests/*_checks.cpp` keep their literal offsets. They run at the start of every player mode, so
`--player-death`, `--player-fight` and `--player-opening` exercise them; some of their fixtures grew to 0x950 bytes, as the
record views check the whole size.

`PlayerRecord` replaces `player_field`, on `ActorBase`. Its +0x998 is a 0x200-byte block of goal names and +0xC20 an
attributes string beside the box's AID. Weapons share these offsets, so a site's variable decides which struct it is:
`attached`, `held` and `secondary` read `WeaponBase`, which is what the 46 reads of +0x93D were. The area actor, which also
has words at +0x934..+0x958, is `AreaActorState`. `record_at(holder, 0xCF8)` and `bytes_at(actor, 0xCE8)` pass sizes.
`read_u32(target->bytes, 0xD00)` in `cinema_mind_movement.cpp` reads past the player's 0xCF8. Its target has spawn points,
so this is probably `WeaponBase::floor`, but it stays literal until the dump confirms every target is a weapon.

The minds are structs too: `MindRecord`, `MindTargetRecord`, `CinemaMindRecord`, `CameraMindRecord`, `PlayerMindRecord`,
`StickTrackingRecord` and `InputTrackingRecord`, each with a view (`mind_record` and so on). A player mind embeds a whole
target record at +0xB8 (`own_target`), which explains its literal writes: +0xC8 is that record's `actor`, +0xD4 its `shape`
and +0x114 its `direction`. Offsets above 0x7C mean different things in each class, so the file decided which struct applied.
`record.bytes[0x16]` in `body_resources.cpp` and `target_event->bytes + 0x20` aren't target records and stay as they are.
Most of the player mind's fields from +0x120 up are `unk_<offset>`.

The structs were then checked against the build before them (d8b0055e) across all 144 test modes, each in a fresh working
directory, which also ran most of the code that had only been compiled. The modes create 148 of the 156 registered factory
types, 54 of them new to the regular modes. 121 modes were byte-identical at once. 21 others differ, but also differ between
runs of one build (long fight scenes above all). `scene3c5_300.bmp` looked like a real 5-pixel change after three matching
runs per build, until four more gave the other build's hash, so agreement among a few runs doesn't make a difference real.
The other two, `--scene3a2` and `--scene3b4`, were a bug: the homing projectile's flight runs the thrown projectile's trail
0x000CC910, and viewing its 0xF60 bytes as the 0xF70-byte `ThrownProjectileState` raised. `ProjectileTrailFields` now views
just what the classes share, and both modes match. Still not created by any mode: baron, baron plane, movie volume, gallery,
gun, and the base rows 0xA..0xC. The flamethrower state appears only in `--scene2c5`, which has no matching pair of runs to
compare. About 60 modes fail the same way on both builds, so what lies past their failures didn't run. Creating a type also
doesn't show every branch of it ran.

So a whole-record view is only safe where every object reaching that code is that large. Check shared functions and
variable-size kinds before sizing a view, and read the retail size table where there is one. The mind events show the
pattern: DAT_003398C8 sizes kind 0 at 0x20 bytes, kinds 1 and 2 at 0x30 and kinds 3 and 4 at 0x24, so they are three
records rather than one. Literal `+0x20` reads of kind 3 events in other files were this record's `subject`. One read of
`age` stays on `read_f32` with `offsetof`, as retail reads through a null event there and the test's address-zero block is
2 bytes.

The light record's raw users moved onto its struct too (the candle, the generate ghouly state, the shadow casters and the
registration), which settled two names: 0x00120FE0 reads +0x58 as a local point and +0x68 as a signed joint while flag 8 is
set, so `unk_68` is `joint`. The colour channels' arrays stay arrays, as their functions loop over them, with accessors for the
single channels most other code wants.

Every event step row is 0x88 bytes and every builder fills a full-size buffer (0x000D05D0 then copies only the type's size,
which DAT_0042890C gives), so one struct with a union of the per-type bodies is safe here, unlike the mind events. In a run
of 41 modes 13 step types ran: 0, 1, 2, 3, 4, 5, 7, 8, 9, 0xA, 0xC, 0xE and 0x10. Types 6, 0xB, 0xD, 0xF (the key carrier),
0x11 and 0x12 did not. The cloth's old `unk_40` and `unk_44` were `light.y` and `light.z`, which its creator zeroes after
setting the whole Vec3.

The actor effect's stream copied words between integer and float fields through `store_effect_field`, which is gone. A store
of a read `u32` into a float field is now `std::bit_cast<float>`, so its bits don't change, and each `Vec3` or `speed` element is
named at the store. The raw +0x15C, +0x1DC and +0x25C uses named the two sound names and the effect name fired from 0x000EE560.
`--player-death` and `--scene2b5` can stall under load soon after their scene starts, with no more log or captures. The
pre-struct build (d8b0055e) does the same, so the structs didn't cause it, and the cause is unknown. Separately, one
`--scene3c3` run of the actor effect build crashed with an access violation at 30 captures. 28 more runs of that build and 20 of
the one before it didn't crash, so whether this change is involved is unknown.

`RuntimePointGraph::record()` views 0x1E0 bytes, so only an animation's graph may use it; a cloth's descriptor is its own
0x1F0-byte type with different words from +0x20, so everything the solver shares with cloths goes through `header()`. Static
asserts in `background_cloth.h` tie the two.

Mind requests keep their descriptors. DAT_00217F18 gives each kind's size (stride 8) and every named field fits, so structs
would be safe, but `get(request, escape_request::speed)` already reads well. The gain was in naming the raw writes beside them,
about 90 sites, and in 0x0007C580: each moving kind draws `speed` from a low and a high, which replaces several `unk_` names, and
the escapes draw `safe_distance` the same way. Body messages stay too: a message's length is its sender's, and a whole-struct
view could demand more bytes than a sender provides. Kind 8's name and chance are `chance_flag_request`. The camera's request
+0x10 in 0x00079E80 stays literal until its kind is confirmed.

`ActorBytes` became `ActorBase` once it had named fields. Parameter rows, presets and other read-only script and XBE data stay
on descriptors, as elsewhere: `walk_alert_row_field`, `strategy_move_settings_field`, `input_tracking_preset_field`,
`cutscene_event_field` and `animation_event_field`. So do messages and mind requests, whose layout depends on an id or kind.

The scene controls come in two kinds. Those with no host pointers (walkabout, chapter 2 scene 4, chapter 5 countdown,
scene fx, final score) are overlays like the body states, on `SceneControlBase`. Those whose retail words hold pointers can't lay
them out in the factory bytes on a 64-bit host, so their fields join the host struct that already holds the pointers,
one struct per control, as `GameSelectControlState` did.

The haunted chairs, relics store, trophy heads and attract controls moved this way. The factory bytes stay allocated at
the retail size and keep their address, which the relics store and game select use as the control's identity, but only
`SceneControlBase` is read from them. The trophy heads' `player` had been written to both places and read only from
the host struct, so the byte copy went. At the retail offsets its `attacked[0]` overlaps `chosen` and `attacked[21]`
overlaps `roaring[0]`; head ids are never 0, so separate members behave the same. Game select only moved its elapsed.

Super Scary moved the same way. Its player, head, bat and head binding words were written beside their host copies and
never read, so they went, and with them the service that made the binding's retail address. `bound_position_y` at
+0x280 was `bound_position.y`. The texture array became `button_textures`, as `buttons` is the press row.

The tests can read these bytes too. Literal offsets in a test are not caught by the compiler, and `--scene6` read the
trophy heads' phase that way until its probe saw zeros and failed. Its captures were still identical, so check a
mode's failure message against the probe as well as the game. `--attract`'s clip count compares native pointers, so a
clip allocated at the last one's address isn't counted, and that number can move by one when host allocations change.

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
| Super Scary parameters (factory type 0x98; the state is in the host struct, below) | parameters 0x1D8 | `super_scary_parameter`: the construction block | `src/engine/scene/super_scary_control.h` |
| Trophy heads parameters (factory type 0x92; the state is in the host struct, above) | parameters 0x108 | `trophy_heads_parameter` | `src/engine/scene/trophy_heads_control.h` |
| Factory parameter header | — | `object_parameter_field`: size, unk_2, type | `include/gbtg/object_factory.h` |
| Walk-target and fly-alert parameters (factory types 0x8F and 0x6E; the states are structs, above) | parameters 0x60 | `walk_target_parameter`, `fly_alert_parameter` | `src/engine/entities/body_walk_target_state.h` |
| Pick-up and lift state parameters (factory types 0x7C and 0x7D; the states are structs, above) | parameters 0x10 / 0x1C | `pickup_state_parameter`, `lift_state_parameter` | `src/engine/entities/body_pickup_state.h` |
| Throw state parameters (factory type 0x8B; the state is a struct, above) | parameters 0x2C | `throw_state_parameter` | `src/engine/entities/body_throw_state.h` |
| Skeleton strategy (factory type 0x51) | — | `SkeletonStrategyState`: no spawn in reach, fetch delay; and the base strategy's `StrategyBase::state` (+0xE, `strategy_fields.h`) | `src/engine/entities/skeleton_strategy.h` |
| Bob weapon (factory type 0xA0, 0x000C5B90), the bob's own part | 0xFE8, parameters 0x7FC | `BobWeaponState`: holders, hits, the break animation and its events, state 7 timer, cutscene name, unk_FE4; `bob_weapon_parameter` | `src/engine/entities/bob_weapon.h` |
| Swing state parameters (factory type 0x78; the state is a struct, above) | parameters 0x128 | `swing_state_parameter` | `src/engine/entities/body_swing_state.h` |
| Large projectile (0x000CD620), its own part | 0xF08, parameters 0x604 | `LargeProjectileState`: throw target, fall, heading, spin, shatter, bounces, hits, rest pose, catch, hold offset and rise, command attachments, trail; `large_projectile_parameter` | `src/engine/entities/projectile_weapon.h` |
| Ghouly cutscene setup (factory type 0x25, 0x0006B0F0), its own part | 0x9DC, parameters 0x3C4 | `GhoulyCutsceneState`, `ghouly_cutscene_parameter` | `src/engine/entities/ghouly_cutscene_setup.h` |
| Bone debris (factory type 0x12, 0x00067E40), its own part | 0xAAC, parameters 0x55C | `BoneDebrisState`, `bone_debris_parameter` | `src/engine/entities/bone_debris_actor.h` |
| Ghouly bg-break setup (factory type 0x24, 0x0006A4F0), its own part | 0xA7C, parameters 0x460 | `GhoulyBgBreakState`, `ghouly_bg_break_parameter` | `src/engine/entities/ghouly_bg_break_setup.h` |
| Bg-break key (factory type 0x1E, 0x0006CC70), its own part | 0xA70, parameters 0x3B4 | `BgBreakKeyState`, `bg_break_key_parameter` | `src/engine/entities/bg_break_key.h` |
| Save record, one field | — | `save_record_field::unk_184` (0x0010DC20 / 0x0010E2C0) | `src/engine/saves/save_scan.h` |
| Base mind (0x00029D10), shared by every mind type | 0x7C, parameters 0xA0 | `MindRecord`, viewed with `mind_record`: actor, target, target record, action record, action actor, path search, target mode, gaze anchor and point, alert mode and timer, last request result, target flags, categories and range, goal point; unk_2C, unk_34, unk_38 | `src/engine/entities/mind_fields.h` |
| Body record (`BodyRecord`, actor +0x95C, built by 0x00024C50): every offset the code reads, viewed with `body_record` | 0x894 | `src/engine/entities/body_fields.h` |
| Parent player body (`ParentPlayerBodyRecord`, factory type 0x27, 0x00076180): collision radius and height, the selection words, joint limit, six tracking joints (`PlayerBodyJoint`) | 0x924 | `src/engine/entities/body_fields.h` |
| Player body (`PlayerBodyRecord`, factory type 0x2A, 0x00075AD0): control tokens, saved energy | 0x950 | `src/engine/entities/body_fields.h` |
| Camera body (`CameraBodyRecord`, factory type 0x28, 0x00073170): view position and euler, separation | 0x914 | `src/engine/entities/body_fields.h` |
| Knockdown state parameters (factory type 0x7A; the state is a struct, above) | parameters 0x50 | `knockdown_state_parameter` | `src/engine/entities/body_knockdown_state.h` |
| Hit and furnace-hit state parameters (factory types 0x73 and 0x74; the states are structs, above) | parameters 0x28 / 0x14 | `hit_state_parameter`, `hit_furnace_state_parameter` | `src/engine/entities/body_hit_state.h` |
| Struggle state parameters (factory type 0x88; the state is a struct, above) | parameters 0x34 | `struggle_state_parameter` | `src/engine/entities/body_struggle_state.h` |
| Dizzy state parameters (factory type 0x64; the state is a struct, above) | parameters 0x11C | `dizzy_state_parameter` | `src/engine/entities/body_dizzy_state.h` |
| Anim state parameters (factory type 0x5E; the state is a struct, above) | parameters 0xB0 | `anim_state_parameter` (with the inline sound name) | `src/engine/entities/body_anim_state.h` |
| Die state parameters (factory type 0x62; the state is a struct, above) | parameters 0x20 | `die_state_parameter` | `src/engine/entities/body_die_state.h` |
| Player death state parameters (factory type 0x69; the state is a struct, above) | parameters 0x18 | `player_death_state_parameter` | `src/engine/entities/body_player_death_state.h` |
| Grabbed state parameters (factory type 0x70; the state is a struct, above) | parameters 0x18 | `grabbed_state_parameter` | `src/engine/entities/body_grabbed_state.h` |
| Stand alert parameters (factory type 0x87; the state is a struct, above) | parameters 0xBC | `stand_alert_parameter` | `src/engine/entities/body_state_actions.h` |
| Walk alert and fly parameters, and their parameter rows (factory types 0x8E and 0x6D; the states are structs, above) | parameters 0xB8 / 0xA8, rows 0x10 | `walk_alert_parameter`, `fly_state_parameter`, `walk_alert_row_field` | `src/engine/entities/body_state_actions.h` |
| fxCustom and its effects (factory types 0xC, 0x2E, 0x30, 0x31, 0x32, 0x33) | 0x18 base; 0xA8, 0x28, 0xD0, 0x54, 0x28 | `fx_custom_field` (seconds alive, actor); `fx_electric_field` (sound, texture loaded, spark count and rows, light slot); `fx_fade_field` (the fade-to-grey's and fade-out's level, target, speed and their +0x24 word); `fx_outline_field` (phase, hold end, time left, the three times, sound, texture loaded, scale, colour, level, shape group, rows, light slot); `fx_solid_fade_in_field` (phase, time left, level, the four times, colour, strength, hold end); and their parameters | `src/engine/entities/fx_custom_electric.h` |
| Weapon base (factory type 0xB over the actor, 0x00145C80), its own part | 0xE54 (0x934 onward), parameters 0x5F8 | `weapon_parameter` (the block 0x00145C80 copies from); `WeaponBase`: mask and its neighbours, firing, accepted classes, contact test, spawn-model block, names, break category, grab prompt (pulse, phase, alpha, fade), shape and point counts, pick-up count, variant name and attributes, spawn trigger, group owner, state and its next and initial values and time, scene start, thrown and follow motion, floor, voice, spawn owner, broken pieces, hit kind/strength/style, animation, target record | `src/engine/entities/weapon_fields.h` |
| Player (factory type 5 over the actor, 0x00028830), its own part | 0xCF8 (0x934 onward) | `PlayerRecord`, viewed with `player_record`: case link, mind, body, input device, provoked and the two countdowns, unk_984, goal volume, latch and delay, ghouly spawn record, kind, count and box, saved, start position and euler, pause count, mode, owner class, live, cells, floor, damage class, tag | `src/engine/entities/player_fields.h` |
| Actor parameters (type 2's block, read by 0x0003E940, filled by 0x000411A0) | 0x334 | `actor_parameter_field`: position, euler, scale, model scale, the model, animation, animation table, resource and attachment names, attachment bone, marker id and next, secondary mode and model; with `object_parameter_field`'s size, unk_2 and type before them | `include/gbtg/object_factory.h` |
| Ghouly trigger (factory type 0x23 over the actor, 0x00069660) | 0xC6C, parameters 0x65C | `TriggerState`: spawn radii, goal and goal names, anchor, box record, facing, unk_B4C, box name and attributes, spawn trigger radius and chance, slot count and slots; `trigger_parameter` | `src/engine/entities/trigger_actor.h` |
| Mind requests (a mind's queued steps, run by the cinema mind's 0x0007C0E0) | varies by kind | `mind_request_field` (kind, elapsed) and one namespace per kind the cinema movement runs: target action (2), escape (0xD), vertical escape (0xE), vertical position (0xF), vertical point (0x10), vertical target (0x11), random position (0x14), heading position (0x15), timed motion (0x24), facing (0x25..0x27), position (0x2D), target position (0x2F); most fields named by the navigation calls they feed (0x00136B00, 0x00136D60) | `src/engine/entities/mind_request_fields.h` |
| Mind target record (a live attachment record, 0x0012AFE0, in the DAT_00545988 pool) | 0x68 | `MindTargetRecord`, viewed with `mind_target_record`: actor, kind, hold, flags, unk_18, floor, the two 0x20-byte shapes (and the shape position, height and radius and the secondary radius by name), direction | `src/engine/entities/player_mind_targets.h` |
| Motion-blur avatar (entityAvatarFxMotionBlur, type 15, 0x00066E20): a dying actor's fading copy | 0xA50, parameters 0x4B8 | `AvatarMotionBlurState`: height scale, timer, fade, light, phase, pixel shader, resource, effect, ignite effect; `avatar_motion_blur_parameter`: source, resource, effect, ignite effect | `src/engine/entities/avatar_fields.h` |
| After-image avatar (type 16, 0x000690F0): the swing trail's fading copy | 0x968, parameters 0x354 | `AvatarAfterimageState`: time left, life, alpha, start alpha, start and end scale, texture parameter, texture, glow; `avatar_afterimage_parameter`: source, life, alpha, end scale, glow, texture record (built by 0x0013B030) | `src/engine/entities/avatar_fields.h` |
| Anim emitter (entityAvatarFxAnimEmitter, type 0xD, 0x00109FC0): a powerup pickup that flies out, waits and fades once collected | 0x9E4, parameters 0x3C4 | `AnimEmitterState`: state, kind, amount, extra, id, callout tokens (count, ids), origin, landing, flight timer, bounces, sprite size, two pulse phases, sprite alpha, fade timer; `anim_emitter_parameter`: kind, amount, extra, sprite, id | `src/engine/entities/fx_anim_emitter.h` |
| Debris type (DAT_005068E8, ten types from aid_misc_ghoulies_fx_cheap_splat; resource data) | 0xAC | `effect_debris_type_field`: texture and its address, colour, size, growth, and the grow, hold and fade time ranges; `effect_debris_types` | `src/engine/graphics/effect_debris.h` |
| Trail type (DAT_005068E0, four types from aid_misc_ghoulies_fx_cheap_dust; resource data) | 0xA0 | `effect_trail_type_field`: texture address and name, two colours, start and end size, frame rate; `effect_trail_types` | `src/engine/graphics/effect_trails.h` |
| Model image records (read by address through `RuntimeRelocationImage::get` / `set`) | varies | The directory and its entries, the tree block and tree, the node header (`model_node_field`: type name and index, mask, instance offset, descriptor, child, next) and each node type (`vertex_buffer_node`, `blendshape_node`, `vertex_shader_node`, `push_buffer_node`, `skeleton_node`, `transform_node`, `shader_node`, `matrix_array_node`, `rigid_skin_node`), the node type registry entry, vertex buffer, joint, pixel shader, material (with its stages, constants and entries), texture and its table, and the geometry block and stream. The physics block and its posed and grid meshes, cells and triangles are named too (`physical_mesh_queries.cpp` reads them). Directory blocks 0xE, 0x11, 0x16 and 0x18 and the name and pointer lists are only relocated and stay by offset | `include/gbtg/model_nodes.h` |
| Input tracking (0x00109810; the struggle states' and the boost's) and its preset, and the player mind's stick tracking (+0x94; 0x0007D460, 0x0007D5A0) | 0x18 / 0x10 / 0x20 | `input_tracking_field`: axes, activity, accumulation, hold timer, state; `input_tracking_preset_field`: threshold, rise, decay, cap; `stick_tracking_field`: pressed, state, press timer, unk_0C, direction, magnitude, gesture time, gesture | `src/engine/entities/player_mind_control.h` |
| Animation event (0x0003A380 runs it by kind) | varies by kind | `animation_event_field` (begin, end, kind) and per-kind namespaces: shader bits (4), effect (0xC, 0x11, 0x24, 0x26, 0x27, 0x29, 0x2A), camera effect (0x16), stop sound (0x1F), trail (0x25), voice (0x30), node state (0x31) | `src/engine/entities/actor_event_messages.h` |
| Cinema mind (its own part after the base mind; 0x0007C7D0) | 0xD0, parameters 0x128 | `CinemaMindRecord`: roam and target radii, sound resource, count and timer, notices, avoidance time, unk_9C..unk_CC; `cinema_mind_parameter`: the radii and the sound resource name | `src/engine/entities/mind_fields.h` |
| Effect script command (0x000F0390) | varies by opcode | `effect_command_field` (opcode) and argument layouts by shape: value, range, timed (colour and duration), colour, colour range, face, channels (oscillation and bounded channels), spin range, bounce | `src/engine/graphics/effect_commands.h` |
| Player mind's own part (after the base mind) | | `PlayerMindRecord` (0x198 bytes): target, target timer and count, the stick tracking record (+0x94), attachment noticed, control lock | `src/engine/entities/player_mind_control.h` |
| Parent mind requests (0x000295E0) and the navigation request (0x2E, 0x00136620) | varies by kind | `named_request`, `value_request`, `reach_request`, `body_range_request`, `slot_action_request`, `flag_action_request`, `wait_request`, `flag_test_request`, `navigation_request`; the header's `started` byte | `src/engine/entities/mind_request_fields.h` |
| Body messages (0x00074080, 0x00075C60, 0x00075DD0) | varies by id | `body_message_field` (id), `control_message`, `point_message`, `collision_hit_message`, `impact_message`, `movement_message`; and the body's actor, action key, alert level, control tokens and saved energy | `src/engine/entities/player_body_messages.h` |
| Camera mind (factory type 0x36, 0x0007AFC0), its own part after the 0x7C-byte base mind | 0xDC, parameters 0xA0 | `CameraMindRecord`: focus actor and its blend, follow point, held target point, heading goal (active, restart, heading, hold, speed, tag), heading, pushed time, heading held, distance, route reaches eye, above floor | `src/engine/camera/camera_mind_rig.h` |

The camera mind was checked with `--player-opening` and headless captures of `chapter1_scene1_2playcam` at 300 and
900 frames: the 14 test captures (the intro and the left-stick walk the camera follows) and both scene captures are
byte-identical to the build before the change. That is weaker evidence than it looks: `--player-opening` is not
deterministic (two back-to-back runs of one build gave different walk captures), so it matched by luck as much as
by equivalence. Which camera paths those runs reach (the focus blend, the held target point under body bit 0x30, the
push timer's move) wasn't checked. Its base part is `MindRecord`.

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

The damage record was named by script in the hit messages and the damage dispatch, processors, tables, motion and
notices: byte reads and writes of the first four bytes, scalar reads, `memcpy` writes of a whole field, and Vec3
reads (by component where the code reads one). The reflected hit's six component negations became two Vec3
negations that compute the same `0.f - x`. Naming it turned up a bug: the direct hit (0x000205B0, body message 0x2A)
wrote its 1.0 at +0x2C, the normal's z, where the dump writes `local_40`, +0x28, the normal's y; it now sets the
normal to (0, 1, 0). The ten regular modes are byte-identical to the previous build apart from `scene6_title.bmp`'s
known 3-byte variant, so none of them shows the fix; whether they reach message 0x2A wasn't checked.

The two avatars were named by hand in `avatar_motion_blur.cpp` and the swing trail's parameter builder
(`actor_trail_requests.cpp`). The after-image's start and end scales are Vec3 fields; its tick reads both before the
same per-component blend, and the builder's three end-scale stores became one Vec3 store into the stack block. The
death effect's parameters (type 0x42, 0x55C bytes, `body_death_effects.cpp`) share the source and name offsets but
are their own type and stay by offset. The ten regular modes are byte-identical to the previous build apart from
`scene6_title.bmp`'s known variant; which of them create either avatar wasn't checked.

The event steps were named in their runners (`script_event_sequence.cpp`, by function, then the icon setup and
"next one different" by hand) and their builders (`script_actions.cpp`). The time steps' builders copy the node's
word as the seconds' bits, now through `std::bit_cast<float>`; signed fields replace `std::bit_cast<i32>` reads.
"Next one different" reads its tag and total rows with `.at()`, so a count past the rows throws where retail would
read on. The ten regular modes are byte-identical to the previous build apart from `scene6_title.bmp`'s known
variant. `--scene2a2-reaper` plays the "Kill All/Time Limit" event (steps 0 and 4, their icons and delay blocks)
five times; the other step types weren't run.

The anim emitter was named by hand in `fx_anim_emitter.cpp`. Its burst builder (0x00069590) builds after-image
parameters and now writes them through `avatar_afterimage_parameter`. The landing point arrives as three words and is
stored with `std::bit_cast<float>`; the sprite's "aid_" test compares the name's first four bytes as before. The
callout ids are read with `.at()`, so a count past eight throws where retail would read on. The ten regular modes
are byte-identical to the previous build apart from `scene6_title.bmp`'s known variant; which of them drop a pickup
wasn't checked.

The colour channels were named once and used in their four functions (whose loops now index the value, target and
rate arrays through `field_data`), the effect particle's four callers (`colour_channels` in place of
`subspan(0x60)`) and the debris. The debris file was rewritten through its names, with `sample_range` taking a type's
range field; the pool and type loops count records instead of offsets, and the debris tick loop and draw read the
same names. All 602 captures of the ten regular modes are byte-identical to the previous build. Effects with colour
commands run in every mode; whether any spawns debris (collision command 0x20 type) wasn't checked.

The bounded channel was named in its six functions and the trail file was rewritten through its names, like the
debris; the scene's pool views and the trail billboard stub (which counted from +0x18) use the pool records too.
Nothing spawns a trail on the native path yet, so the trail code never ran in the tests; it was checked by reading
it against the old code. Whether any effect particle in the tests uses its bounded channels wasn't checked. The ten
regular modes are
byte-identical to the previous build apart from `scene6_title.bmp`'s known variant.

The model image records were named through new typed `get` / `set` on `RuntimeRelocationImage`, by function and
variable, in the relocation, the views, the texture post-load and the node draws; several node types reuse offsets
with different meanings, so each function's variables map to one node type. Reads of a 32-bit count's low half
(`image.half` of a target's buffer count, the vertex shader's registers) are written as `u16(get(...))`. All 602
captures of the ten regular modes are byte-identical to the previous build; every mode loads and draws models.

The actor effect was named in its placement, tick, binding, particle, draw, texture, node and stream code. The
stream and setup copy words between integer and float fields, so their writes go through `store_effect_field`, which
stores the value's bits unchanged into a named field (or one element of it). The particle draws' oscillation and
bounded-channel reads (particle +0xE8.., +0x118..) stayed by offset until the particle became a struct.
All ten regular modes are byte-identical to the previous build apart from `scene6_title.bmp`'s known variant.

The physics meshes and the cinema mind's own part were named in one more pass. The grid mesh's three binding lambdas
computed the same cell count, so they share `resolve_grid_mesh`; the triangle loops read a row's three vertex indices
once. All 602 captures of the ten regular modes are byte-identical to the previous build. In `book_map.cpp` and
`scene_pause.cpp` the census's largest remaining clusters are reads of executable constants by address
(`real(s, 0x0032B0F4)`), not object fields; they belong with the global names below.

`player_update.cpp` was named through `player_field`, `body_field` and the new `actor_field`; its runs of component
writes into one Vec3 field (position, velocity, euler, the saved position) became single Vec3 writes, which store the
same bytes since nothing reads the field between them. The hit and walk-target states and the hit messages were
done the same way, adding the body's hit timer (+0x754, +0x760) and a few `unk_` fields. All 602 captures of the ten
regular modes are byte-identical to the previous build.

The batch after the light record named the effect commands, the player mind's own part, the parent and navigation
requests, the mind events, the body messages, the walkabout control and the base strategy's goal states across 37
files. One name was first declared too large: a 0x40-byte name at +0x10 in a 0x44-byte request threw the range
check, where the old code had only taken a pointer. The names are now 0x30 bytes, and the range error now prints
the offset, size and span length. All ten regular modes are byte-identical to the previous build.

## Next

- Merge the per-file `word` / `put_word` / `real` copies into `retail_field.h` as their files are converted.
- The 0x30-byte screen texture (0x000533A0) is the engine's texture resource (width +8, height +0xA, header offset
  +4, data +0x14, size +0x18), which the widgets' `texture_width` helpers and the presenter also read. It belongs
  with the texture resource's own layout.
- The entities. The actor's own 0x934 bytes are mostly read through the native `Actor`; the
  raw reads left on them (+0x18, +0xB8, +0x1FC) are few.
- `DAT_` globals, with the same evidence rule.
