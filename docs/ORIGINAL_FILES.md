# OriginalFiles edits

Local-only patches to a user-supplied retail dump. **Never commit** `OriginalFiles/`, `*.original` payloads, ISOs, or XBEs.

## Protocol

Before the first change to any file under `OriginalFiles/`:

1. Copy the untouched file to the same path with a `.original` suffix (`foo.bnl` → `foo.bnl.original`).
2. Add a row below: path, why, what changed, how to restore.

ISO builds must **not** pack `*.original`. Use `tools/xemu/build_iso.ps1`, which copies into `cache/xemu_root/` and skips those backups.

`gbtg-xemu-patch` restores each file from `.original` when present, then re-applies, so patches stay idempotent.

Restore a file with:

```text
Copy-Item -Force OriginalFiles\<path>.original OriginalFiles\<path>
```

## Patches

Applied by `gbtg-xemu-patch` / `tools/xemu/apply_preview_patches.ps1` for the [xemu preview](XEMU.md).

### `bundles/common.bnl`

- **Backup:** `bundles/common.bnl.original`
- **Why:** Skip attract, game select, and chapter-1 books so boot lands in the PC slice room.
- **Change:** Type-14 aidlist `aid_aidlist_ghoulies_sceneorder_game` (128-byte name slots). First five names (attract ×2, gameselect, `scene0_1book`, `scene1_1book`) are overwritten with `aid_script_ghoulies_chapter1_scene1_2playcam`. Inflate, patch, zlib-deflate; chunk sizes unchanged.
- **Restore:** copy `.original` back.

### `default.xbe`

- **Backup:** `default.xbe.original`
- **Why:** Retail has no debug camera. BanjoCam cannot fly the hallway.
- **Change:** Hook BanjoCam look-at (`0x72ba0`) for FPS fly math. After pose copy (`0x72b8a`) and during tick pose copy (`0x73284`), write the fly eye and eulers into camera `+0x89c` (the on-screen view). Tick still registers the camera. While flying, skip `0x25c50`’s follow look-at packets (`0x12b160`) so they cannot fight the body pose. Cooper’s actor is not moved. FPS fly: left stick move, right stick look (stick X/Y swapped to match this pad), analog triggers and white/black (bumpers) for Y. On engage, freeze sim dt (`0x4312dc = 1.0`).
- **Restore:** copy `.original` back.
