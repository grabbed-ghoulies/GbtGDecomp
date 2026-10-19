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
| Button-bat menu (`ButtonBatMenu`, `DAT_005461A4`, 0x00143550), typed from the start, with its items (0x23C) and slide-in sprites (0x2C) | 100 | `src/engine/frontend/button_bat_menu.h` |
| Game-select control (`GameSelectControlState`, factory type 0x9A, 0x000835F0): rows, start script, pending start, start mode, book turn, start ready. +0x20 stays on the factory bytes as the base's `elapsed`. | 300 | `src/engine/scene/gameselect_control.h` |

Game select checked with `--gameselect`, `--gameselect-native`, `--gameselect-new-game`, `--gameselect-delete` and
`--gameselect-stats`. All pass, and all 25 captures are byte-identical to the captures taken before the first
naming change, both after naming and after consolidation. Those runs don't reach the contents completed splat
(`0x000DAD50` / `0x000DAED0` / `0x000DAF10`, a finished game only) or the challenge icon (shown only with unplayed
challenges). Those paths compile but haven't run. The control is exercised by every row change and the new-game
start. The button sequence is built and ticked but never entered; its unlock (0x000DB380) is still untranslated.

## Named with descriptors

| Type | Size | Layout | Where |
|---|---|---|---|
| Super Scary challenge (factory type 0x98, 0x000842D0) | 0x2C8, parameters 0x1D8 | `super_scary_field`: actor addresses, phase, timers, clip names and placements, shock camera, presses and button row, settle, render swap; `super_scary_parameter`: the construction block | `src/engine/scene/super_scary_control.h` |
| Scene-control base (factory type 3) | — | `scene_control_field::elapsed` (+0x20) only; the list links at +8, +0x10 and +0x18 are SceneControlInstance members | `src/engine/scene/scene_control_fields.h` |

Super Scary checked with `--scene6`: it passes, the challenge's phase sequence and the trophy-heads states match the
log from before the change line for line, and all 45 captures are byte-identical. That run covers phases 1–5 and
8–11, through both failure and success. It doesn't reach phases 6 and 7 or the DAT_005122F0 auto-press path, and it
doesn't check which scare modes occur, so the bound-mode (2/3) branches may not have run. The attract and
trophy-heads controls still read +0x20 directly.

## Next

- Super Scary can be consolidated the same way: its state is reached only through its own functions and the
  `--scene6` test.
- Merge the per-file `word` / `put_word` / `real` copies into `retail_field.h` as their files are converted.
- The 0x30-byte screen texture (0x000533A0) is the engine's texture resource (width +8, height +0xA, header offset
  +4, data +0x14, size +0x18), which the widgets' `texture_width` helpers and the presenter also read. It belongs
  with the texture resource's own layout.
- The other scene controls (`trophy_heads_control`, `attract_control`), starting with
  `scene_control_field::elapsed`, then the entities. Player body +0x728 (written by Super Scary through
  `set_body_word_728`) waits for the body's layout.
- `DAT_` globals, with the same evidence rule.
