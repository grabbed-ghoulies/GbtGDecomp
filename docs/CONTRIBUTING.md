# Optional notes for contributors

- Game data stays in `--game-dir` (default `OriginalFiles/` locally). Never stage it. If you patch it, copy `file` → `file.original` first and list the edit in `docs/ORIGINAL_FILES.md`.
- Format parsers belong in `src/gbtg/`. Reconstructed engine code belongs in `src/engine/`. Tools only print or dump.
- Game behavior comes from the native, translated path. Do not add host re-implementations, and do not extend those listed in `docs/LEGACY_HOST_CODE.md`; move their callers to native code instead. Host code covers only boundaries and the D3D11 presentation of recorded native draws.
- Accept visible or audible work only after checking it with `--screenshot` captures, not just a headless run that exits 0.
- When you rename a Ghidra function, add a line to `docs/XBE.md` rather than committing the database.
