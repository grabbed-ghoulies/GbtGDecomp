# Generated maps

`gbtg-catalog --out docs/generated` writes:

- `aid_names.txt` — hash and AID string from `default.xbe` (no file bytes)
- `aid_resolved.txt` — hash, relative path, and name for hashed disc files that matched

These files contain names and checksums only. Regenerate them against your own `--game-dir`.

`ghidra_functions.txt` is the recovered XBE symbol map (addresses and names only). Refresh it with `ExportEngine.java` after renaming functions in Ghidra.
