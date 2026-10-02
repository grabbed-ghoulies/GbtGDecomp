# Optional notes for contributors

- Game data stays in `--game-dir` (default `OriginalFiles/` locally). Never stage it. If you patch it, copy `file` → `file.original` first and list the edit in `docs/ORIGINAL_FILES.md`.
- Format parsers belong in `src/gbtg/`. Reconstructed engine code belongs in `src/engine/`. Tools only print or dump.
- When you rename a Ghidra function, add a line to `docs/XBE.md` rather than committing the database.
- Unattended reconstruction claims one file from `queue/open/` and edits only the paths listed in that task. See `queue/README.md`.
