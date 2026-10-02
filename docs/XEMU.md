# xemu original-game oracle

Boots **your** retail dump in [xemu](https://xemu.app/). Use it to check reconstructed functions against the original: which BNL opens, which AID is requested, what the title actually does.

This never commits ISOs, XBEs, or `OriginalFiles/`. See [ORIGINAL_FILES.md](ORIGINAL_FILES.md) and [LEGAL.md](LEGAL.md).

The optional preview scripts still skip attract into chapter 1 hallway with a fly cam so you can look around. That is **not** the PC decomp. `gbtg.exe` follows retail `sceneorder_game`.

## One-time

1. Own the game. Keep an extracted disc tree in `OriginalFiles/` (or pass `--game-dir`).
2. xemu at `D:\Emulation\xemu\xemu.exe` with MCPX + BIOS + HDD already configured.
3. `tools/extract-xiso/extract-xiso.exe` in-tree.
4. Build: `cmake --build build --config Release` (produces `gbtg-xemu-patch.exe`).

## Agent / compare loop

```text
.\tools\xemu\launch_preview.cmd
.\tools\xemu\capture.ps1
```

From PowerShell at the repo root: `.\tools\xemu\launch_preview.ps1`. Close any existing xemu window first, or pass `-Force`. Do not paste the `.ps1` path into Command Prompt; use the `.cmd` wrapper there.

Capture writes `dump/xemu_preview.png`.

`launch_preview.ps1` restores each target from `*.original` if present, re-applies patches, stages `cache/xemu_root/` **without** `*.original`, rebuilds `cache/GbtG_preview.iso`, and starts xemu with `-dvd_path`, short boot animation, `-snapshot`, and a QEMU monitor on `127.0.0.1:4510`.

## Camera (xemu)

BanjoCam still runs until a stick passes deadzone. Then sim dt is frozen (`0x4312dc`) and an FPS fly cam takes over: left stick move, right stick look, analog triggers and bumpers up/down. The overlay writes the fly eye into the camera body pose (`+0x89c`) and skips the follow look-at packets so they cannot fight that pose. If that overlay takes the title down, restore `default.xbe.original` and rebuild the ISO.

## Scripts

| Script | Role |
|--------|------|
| `tools/xemu/apply_preview_patches.ps1` | `gbtg-xemu-patch --game-dir OriginalFiles` |
| `tools/xemu/build_iso.ps1` | extract-xiso `-c` from staged tree |
| `tools/xemu/launch_preview.cmd` | Same launcher from cmd.exe (bypasses execution policy) |
| `tools/xemu/launch_preview.ps1` | patch + ISO + xemu |
| `tools/xemu/capture.ps1` | monitor `screendump` |

Flags: `-SkipPatch`, `-SkipIso`, `-KeepHdd`, `-Force`, `-GameDir`, `-Xemu`, `-Iso`.

Do not load the 7 GB redump. Only the XISO built from `OriginalFiles`.
