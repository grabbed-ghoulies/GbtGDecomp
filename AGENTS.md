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

### Assigned queue tasks

When working on an assigned task file under `queue/`:

- Edit only files named in the task. `src/engine/game.cpp` is the thin
  `frame_tick` caller; edit it only if the task lists it.
- Put `// Xbox 0x........` on each translated function, matching the task address.
- Translate the dump rather than pasting it as the implementation. Do not add
  branches absent from the dump.
- An untranslated callee blocks the task. Do not stub it.
- Use Allman braces, as in the rest of `src/engine/`.
- Do not rename Ghidra symbols, append to `docs/XBE.md`, or commit. The supervisor
  handles those after the build passes.
- Do not stage `OriginalFiles/`, `*.xbe`, `*.bnl`, `*.iso`, or `ghidra_projects/`.
- Read the decompile from `cache/ghidra_frontier/<VA>.txt` on the main checkout;
  `cache/` is not available in a worktree.
- Touch `queue/claimed/<VA>.heartbeat` on the main checkout while working, and
  create `queue/claimed/<VA>.ready` when stopping.

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
