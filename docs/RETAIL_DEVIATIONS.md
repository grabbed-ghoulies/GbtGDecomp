# Deliberate deviations from retail

The port translates retail functions branch for branch (`AGENTS.md`). This file lists the few places where translated
code deliberately does something else, why, and how to undo each one. Add an entry for any new deviation and mark it
in the code with a comment that points here.

## The mirror pass restores the view's area, not its fitted rectangle

- **Where:** `draw_runtime_scene_mirror` in `src/engine/scene/scene_mirror.cpp` (Xbox `0x0011F690`), the saved viewport.
- **Retail:** the mirror pass saves the display record's fitted rectangle (`DAT_005077A8` +0x58, +0x5C, +0x60,
  +0x64), draws the reflection, and passes those four values to `0x000F5CC0`, which stores them as the record's area
  (+0x48..+0x54) and refits the view inside it (`0x000F57F0`).
- **Port:** saves the record's area (+0x48..+0x54) and restores that.
- **Why:** in widescreen (`DAT_005077F8` 1, the 16:9 row), a camera in the 4:3 aspect (mode 0) fits to three quarters
  of the area. Each mirror pass under such a camera therefore narrows the area again: 640, 480, 360 and on to 0, after
  which nothing draws until a restart. It is reproducible on chapter1_scene11_3viewcam's exit into
  chapter1_scene12_1playcam with widescreen on: the transition book's camera (`0x00048260`, flags 0xC, its cutscene a
  4:3 curve) is slot 0's active camera, so the scene's view present runs the mirror in mode 0. Every function on that
  path matches its dump, and nothing else in the XBE writes the area after boot, yet the original game does not go
  black in widescreen (confirmed on retail by the project owner, 2026-10-31). The runtime difference has not been
  found; an xemu comparison of the display record (`0x00507018` +0x3C, +0x48..+0x64) and slot 0's active camera
  (`0x004E16A0`) through this transition would settle it.
- **Effect in 4:3:** none. Mode 3 and the 4:3 row fit to the whole area, so the area and the fitted rectangle are equal.
- **Undo:** save `view.fitted_x`, `fitted_y`, `fitted_width` and `fitted_height` again in that function.
