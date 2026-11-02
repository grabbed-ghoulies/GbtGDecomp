# Project instructions

These instructions apply throughout this repository. They are adapted from the
rules in `.cursor/rules/`; the scoped sections below apply only to the work they
describe.

## Reconstruction

- A reconstructed function must translate `cache/ghidra_frontier/<VA>.txt` with
  the same branches, comparisons, and offsets. Host types are fine; branches
  absent from the dump are not.
- A host boundary is allowed only where the dump calls it: file IO, D3D, XInput,
  heap, or thread TLS. It must return what the dump tests.
- Anything else is missing. Do not assign an invented return value to a missing
  callee. `gbtg --headless` is a crash check, not acceptance of a function.

### Native-first workflow

The game runs on translated retail code driven by retail state, in the retail
call order from `frame_tick`. That native path is the only source of game
behavior.

- **Put new behavior on the native path.** Do not write host re-implementations
  of game logic, such as format parsing for gameplay, curve sampling, timing,
  menu or dialogue behavior, or camera choice, that bypass the translated
  functions. If the native path lacks something, translate it.
- **Host code is boundaries and presentation only.** The boundaries are those
  listed above, plus XACT audio. Native drawing goes to the recording D3D
  boundary (`SceneD3DDevice`), and the D3D11 presenter turns that recorded
  output into host draws. Extend the presenter rather than calling `gfx_*` from
  game code. Host rendering helpers (texture decode, mesh upload) may be reused
  for presentation.
- **Do not extend host stand-ins.** Older code that re-implements retail
  behavior on the host is listed in `docs/LEGACY_HOST_CODE.md`. When native code
  covers one of those features, switch its callers to the native path and
  update that document.
- **A clean run does not mean the feature works.** A headless run that exits 0
  shows only that it doesn't crash. For anything visible or audible, take
  `--screenshot` captures and look at them, and say plainly what is still not
  shown or heard.

### Working on a function or feature

- Read the decompile from `cache/ghidra_frontier/<VA>.txt`. If it is missing,
  export it with the read-only Ghidra command (the project is never written):
  `cmd /c "tools\ghidra\support\analyzeHeadless.bat Z:\GhouliesStuff\ghidra_projects GbtG -process default.xbe.original -readOnly -noanalysis -scriptPath tools\ghidra_scripts -postScript ExportPlayerScene.java <VA...>"`
- Work from a live entry point inward: run the scene, find the first
  untranslated or unbound callee it reaches, and translate that. Do not work
  through isolated addresses that nothing native calls yet.
- Put `// Xbox 0x........` on each translated function, matching its address.
- Translate the dump rather than pasting it as the implementation. Do not add
  branches absent from the dump.
- An untranslated callee raises when reached. Do not stub it.
- `src/engine/game.cpp` is the thin `frame_tick` caller. Keep logic out of it.
- Do not rename Ghidra symbols, and do not commit unless asked.
- Do not stage `OriginalFiles/`, `*.xbe`, `*.bnl`, `*.iso`, or `ghidra_projects/`.

## Test volume

Every test that can open the audio device sets the master volume to 10%.
`gbtg-transition-tests` calls `pcm_set_master_volume(0.1f)` at the top of
`main`, and `gbtg --headless` runs use 10% instead of the config volume. A new
test executable or test mode that plays audio must set 10% before the XAudio2
master voice is created.

Tests also call `pcm_use_virtual_audio_clock()`. The game polls whether cues are
still playing, and headless test frames don't run in real time, so answers from
the device made two runs of one build differ. Under the virtual clock those
answers follow game time, and a mode's captures are byte-identical run to run.

## OriginalFiles

`OriginalFiles/` is the user's retail disc dump. Never stage, commit, or distribute
it or `*.iso`, `*.xbe`, `*.bnl`, or `*.original` payloads.

Before the first change to any file under `OriginalFiles/`:

1. Copy the untouched file beside itself with a `.original` suffix, for example
   `default.xbe` to `default.xbe.original`. Preserve an existing original backup.
2. Document the edit in `docs/ORIGINAL_FILES.md`: path, reason, changes, and how to
   restore it.

Keep that documentation outside `OriginalFiles/`. ISO packing must skip
`*.original`; use `tools/xemu/build_iso.ps1`, which stages into `cache/xemu_root/`.

xemu is a runtime oracle for reconstructed functions, including which BNL opens
and which AID is requested. See `docs/XEMU.md`.

## C/C++ brace style

For all `*.c`, `*.h`, `*.cpp`, and `*.inl` files:

- Use Allman braces: put the opening `{` on the next line at the same indentation
  as the statement. Do not use K&R braces such as `if (x) {`.
- Apply this to functions, methods, lambdas, control flow (`if`, `else`, `for`,
  `while`, `switch`, `do`), classes, structs, enums, and namespaces.
- Always brace control-flow bodies; do not write `if (x) stmt;`.
- Keep braced initialization such as `Foo{1, 2}` and `std::vector<int>{}` inline.

```cpp
void tick(int n)
{
    if (n > 0)
    {
        run();
    }
    else
    {
        stop();
    }
}
```

## Source organization and names

- Project-owned headers must use `.h`; do not create `.hpp` files.
- Put source files in folders named for their subsystem (camera, entities, scene, scripts, frontend, transitions, runtime, graphics, audio, and so on). Group functions in separately compiled `.cpp` modules with narrow internal `.h` interfaces beside their owning modules.
- Give reconstructed functions descriptive names derived from their implementation.
- Always put `// Xbox 0x........` immediately above each reconstructed definition and beside its declaration, using the original function address.
- Preserve reconstruction behavior and existing address notes when moving or renaming functions.
- Read and write retail object fields through named `RetailField` / `RetailRecord` descriptors (`include/gbtg/retail_field.h`) declared beside the owning type, not through literal offsets. Name a field from every dump that touches it, and use `unk_<offset>` when the evidence doesn't settle the name. Once a host-allocated type is fully named and nothing reads it by offset, consolidate it into a plain typed struct: members in retail order, each commented with its `+0x...` offset. See `docs/RETAIL_LAYOUTS.md`; `tools/layout/offset_census.py` lists the evidence.
