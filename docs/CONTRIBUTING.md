# Optional notes for contributors

- Game data stays in `--game-dir` (default `OriginalFiles/` locally). Never stage it.
- Format parsers belong in `src/gbtg/`. Tools only print or dump.
- When you rename a Ghidra function, add a line to `docs/XBE.md` rather than committing the database.
