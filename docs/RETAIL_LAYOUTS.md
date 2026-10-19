# Retail field layouts

Retail objects are kept as byte buffers laid out exactly as on the Xbox, so translated code matches the Ghidra
dumps offset for offset. Reading those buffers through literal offsets (`word(w, 0xB2C)`) is faithful but
unreadable. This document describes how fields get names without changing the layout or the behavior, and tracks
which object types are done.

## The pattern

`include/gbtg/retail_field.h` declares a field once, beside the object that owns it:

```cpp
namespace slot_select_field
{
constexpr RetailField<i32, 0xB2C> slot{}; // the selected plate, 0..2
constexpr RetailRecord<0x214, 0x304, 3> plates{};
}
```

and code reads and writes it by name:

| Before | After |
|---|---|
| `integer(w, 0xB2C)` | `get(w.bytes, field::slot)` |
| `put_real(w, p + 0x434, x)` | `set(plate_sprite, sprite::scale_x, x)` |
| `reals(w, 0xB44)` | `field_data(background, sprite::position)` |
| `w.bytes[0xB4F] = a` | `field_byte(background, sprite::color, 3) = a` |
| `w.bytes.data() + 0xB9C + i * 0x100` | `record(w.bytes, field::slot_names, i).data()` |

- `RetailField<T, Offset>`: a scalar, array or `Vec3` at a byte offset. `get` / `set` copy it in and out
  (bounds-checked, like `read_u32`). `field_data` gives its address for the calls that take a field by pointer.
  `field_byte` gives one of its bytes.
- `RetailRecord<Offset, Stride, Count>`: a sub-object or an array of them. `record(bytes, r, i)` is record `i`'s
  bytes, and the record's own fields are `RetailField`s relative to its start. Records nest. In slot select, a
  plate record holds four sprite records.
- `retail_fits(size, f)` is for `static_assert`: the field or record lies inside the object.
- Words that hold pointers stay as host members of the owning struct (retail pointers are 4 bytes, host pointers
  8), with a `// +0x...` note. The descriptor namespace notes them by offset so the layout reads end to end.

Descriptors change names only. Every replaced access keeps its offset, width and signedness. An unsigned
comparison stays unsigned (cast at the use site, with a comment). A word copied between two float fields is
copied as a float. `memcpy` keeps the bits, so the result is the same.

## Shared layouts

Some sub-layouts repeat across types. Declare them once, next to the types that share them, and refer to them as
records:

- `gameselect_text_line` (`src/engine/frontend/gameselect_rows.h`, 0x20C bytes): wide text, x, y, and the
  0x0005F5C0 record id. Used for the slot-select prompt and plate labels, the deletion title and prompt, and the
  contents chapter names, title, progress and completed lines.
- `gameselect_sprite` (same header, 0x18 bytes): texture word (a host member), position, color, scale. Used for the
  slot-select plates, discs and backgrounds, and the contents panels, backgrounds, splat and challenge icon.

The retail code never passes either one as a struct, so the evidence for treating them as shared types is only the
identical layout in every place they appear. The header comments say so.

## Naming rules

1. **Name from evidence.** Read every dump that touches the field. Run the census (below) to list them. Write the
   strongest evidence on the declaration: the function that sets it, the constant it starts from, what it is
   compared with.
2. **Say when a name is inferred.** If the evidence fits but doesn't prove the name, say so in the comment
   (see `cursor_row`). If there's no good guess, use `unk_<offset>`, for example `unk_B74`. A wrong name is worse
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
python tools/layout/offset_census.py --file gameselect_contents.cpp
```

It writes `cache/layout/census.json` and one table per file (`cache/layout/<file>.md`). Each table lists every
offset per buffer with its widths, R/W kinds, functions and lines, plus the dump path of each function. Offsets
added to a record base (`p + 0x434`) are grouped under that base. Literal offsets into the second and third
copies of a repeated record (0x734, 0xA38, …) show up as their own rows, which is a sign there's a record to
declare.

## Per-type session brief

For one object type:

1. Run the census for the type's files and read every dump it lists.
2. Declare the fields beside the owning struct, with evidence comments, `static_assert(retail_fits(...))`, and
   records for repeated sub-objects.
3. Replace the literal-offset accesses. Delete the file-local `word` / `put_word` / `real` helpers once nothing
   uses them.
4. Build `gbtg` and the transition tests (`--target gbtg gbtg-transition-tests`). Run the test modes that reach
   the type, and compare their captures with captures from before the change.
5. Add the type to the table below.

## Done

| Type | Size | Layout | Where |
|---|---|---|---|
| Slot-select widget (`SlotSelectWidget`, 0x000DD730) | 0xEAC | `slot_select_field`: 3 plate records (status, label line, 4 sprites, movie name and UV, box), 2 background sprites, cursor, timers, slot names | `src/engine/frontend/gameselect_slots.h` |
| Deletion widget (`GameDeletionWidget`, 0x000DB720) | 0x594 | `game_deletion_field`: title and prompt lines, saved plate flags, save name | `src/engine/frontend/gameselect_delete.h` |
| Contents widget (`GameContentsWidget`, 0x000D8AB0) | 0xF34 | `game_contents_field`: 3 chapter records (name line, sprite, movie state, box), title/progress/completed lines, 4 sprites, timers, splat | `src/engine/frontend/gameselect_contents.h` |
| Super Scary challenge (factory type 0x98, 0x000842D0) | 0x2C8, parameters 0x1D8 | `super_scary_field`: actor addresses, phase, timers, clip names and placements, shock camera, presses and button row, settle, render swap; `super_scary_parameter`: the construction block | `src/engine/scene/super_scary_control.h` |
| Game-select control (factory type 0x9A, 0x000835F0) | 300 | `gameselect_control_field`: current and requested row, start script, pending start, start mode, book turn, start ready | `src/engine/scene/gameselect_control.h` |
| Button sequence (contents +0xF30, 0x00142E50) | 0x70 | `button_sequence_field`: state, step, presses, held buttons, step flags | `src/engine/frontend/gameselect_contents.h` |
| Scene-control base (factory type 3) | — | `scene_control_field::elapsed` (+0x20) only; the list links at +8, +0x10 and +0x18 are SceneControlInstance members | `src/engine/scene/scene_control_fields.h` |

Super Scary checked with `--scene6`: it passes, the challenge's phase sequence and the trophy-heads states match the
log from before the change line for line, and all 45 captures are byte-identical. That run covers phases 1–5 and
8–11, through both failure and success. It doesn't reach phases 6 and 7 or the DAT_005122F0 auto-press path, and it
doesn't check which scare modes occur, so the bound-mode (2/3) branches may not have run. The attract and
trophy-heads controls still read +0x20 directly.

Game select checked together: `--gameselect`, `--gameselect-native`, `--gameselect-new-game`, `--gameselect-delete` and
`--gameselect-stats` pass, and all 25 captures are byte-identical to captures from the code before the change.
Those runs don't reach the contents completed splat (`0x000DAD50` / `0x000DAED0` / `0x000DAF10`, a finished game
only) or the challenge icon (shown only with unplayed challenges). Those converted paths compile but haven't been
run. The control fields are exercised by every row change and the new-game start. The button sequence is built
and ticked but never entered; its unlock (0x000DB380) is still untranslated.

## Next

- Merge the per-file `word` / `put_word` / `real` copies into `retail_field.h` as their files are converted.
- Game select is named except the 0x30-byte screen texture (0x000533A0). That is the engine's texture resource
  (width +8, height +0xA, header offset +4, data +0x14, size +0x18), which the widgets' `texture_width` helpers
  and the presenter also read, so it belongs with the texture resource's own layout.
- The other scene controls (`trophy_heads_control`, `attract_control`), starting with
  `scene_control_field::elapsed`, then the entities. Player body +0x728 (written by Super Scary through
  `set_body_word_728`) waits for the body's layout.
- `DAT_` globals, with the same evidence rule.
- Types whose fields are all named and that hold no pointers can become real packed structs with
  `static_assert(offsetof(...))`.
