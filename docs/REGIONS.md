# Regional releases

The decomp supports the **PAL** and **NTSC (USA)** releases. It does not
support the **NTSC-J (Japan)** release.

| Release | Supported | `default.xbe` SHA-256 | Build path in the XBE debug header |
|---------|-----------|-----------------------|------------------------------------|
| PAL | Yes (reference dump) | `e149f50bbc48203a0d2f5efe69f3a40280d3d045a3b63166762efc9dde64c4c5` | `c:\source_24.09.03.0028\...` |
| NTSC | Yes | `a2a5bbe51ecc11392612bad30ee4238b796d4d0ed7262e7d23dc4120388294b1` | `c:\source_24.09.03.0028\...` |
| NTSC-J | No | `ca63ba526a1a41823bbabbf6bc4c3cced1f3b67ad45c70b27805632bba052d17` | `c:\Source_18.11.03.3012\...` |

Every `// Xbox 0x........` address in the source, every `cache/ghidra_frontier`
dump and the [retail manifest](../manifests/retail_files.json) come from the
PAL executable.

## PAL and NTSC

PAL and NTSC are the same build. They share the link timestamp (`0x3F71DA41`),
entry point (`0x0014FAE9`) and section layout, so every function and table
address matches. The game data files (`bundles/`, `demand/`, `font/`,
`loctext/`, `movie/`, `xwavebank/`) are byte-identical, so an NTSC dump works
anywhere a PAL dump does.

The two `default.xbe` files differ in only 533 bytes:

- The header signature, and the certificate's game region (PAL `0x4`, NTSC
  `0x1`) and version (PAL `0x2`, NTSC `0x1`).
- One code byte at `0x0015106F` in `0x00151000`. PAL has `jge` (`7D`) and acts
  on the result of the media check `0x00150E37`. The NTSC dump has `jmp` (`EB`)
  and ignores that result. This is most likely a media patch added by the
  ripping tool rather than a difference on the retail disc. The PAL bytes are
  the reference for translation.

## NTSC-J

NTSC-J is a later build, from about two months after PAL/NTSC: link timestamp
`0x3FB9F5C5`, entry `0x0014FADC`, certificate region `0x2`, version `0x3`.

The port reads retail tables and constants from `default.xbe` at PAL addresses
(`SceneExecutableResources`). It never runs the executable's code. Those
addresses are wrong for NTSC-J, so NTSC-J is read through an address map (see
below). With the map, NTSC-J boots through all executable data and starts the
attract scene, then crashes drawing text on about the 6th frame: the font
resources differ (see [Missing for NTSC-J support](#missing-for-ntsc-j-support)).
It does not reach a visible frame yet.

### The address map

`tools/regions/xbe_address_map.py` builds the map from your own PAL and NTSC-J
`default.xbe`:

```powershell
python tools/regions/xbe_address_map.py --pal <PAL>/default.xbe --ntscj <NTSC-J>/default.xbe
```

It writes `src/engine/scene/ntsc_j_address_map.inl`, which holds addresses only
and no game bytes, and a report with the evidence for every address at
`cache/address_maps/ntsc_j.json`. The `.inl` is committed so NTSC-J owners don't
need a PAL dump. Rerun the tool when new code reads the XBE at an address it
hasn't seen.

The tool works in four steps:

1. **Data blocks.** It aligns `.rdata` and `.data` dword by dword, with
   address-like dwords masked. First come ordered anchors, then an LCS inside
   each gap for data that shifted by a few dwords, unique-value matches for
   fields that moved, and the remaining equal-size spans as content changed in
   place. Last, it relocates tables the linker moved elsewhere. This covers
   99.89% of PAL's file-backed data.
2. **Code references.** For each address the source names, and each address PAL
   code names inside an unaligned gap, the tool finds the instruction in NTSC-J
   `.text` by its surrounding bytes. Image addresses and `call`/`jmp`/`jcc` rel32
   are wildcarded, and the tool reads the operand. When code and data disagree,
   the side whose item (a string, or a 4-byte value) matches PAL wins. Otherwise
   the side with clearly more matching data around it wins. Otherwise the
   address is left unresolved.
3. **Value.** As a last resort, an item whose bytes occur exactly once in each
   build's data maps there. Three constants use this.
4. **Pointers.** Every aligned dword that holds a pointer is recorded. A pointer
   into data is resolved through the blocks at run time. A pointer into code
   takes the PAL value from the same slot, so the port's dispatch on PAL
   function addresses keeps working.

At startup, `SceneExecutableResources` recognizes NTSC-J by its timestamp and
builds a PAL-layout view (`src/engine/scene/ntsc_j_layout.cpp`). Each NTSC-J
block is copied to its PAL address and its pointers are rewritten. NTSC-J data
with no PAL place stays readable at `0x01000000` plus its NTSC-J address.
Callers keep using PAL addresses and need no changes. If a read starts in an
unmapped PAL range or at an address the map lists as missing, the view throws
`NTSC-J has no data for PAL 0x...` instead of returning invented bytes. PAL and
NTSC never take this path.

Rebuilt in Python from the generated `.inl`, the view matches PAL for 99.97% of
the dwords it covers. The rest is data that differs in NTSC-J.

Still missing (the code reading them differs between builds):

| PAL address | Read by |
|-------------|---------|
| `0x0020A64C`, `0x0020A65C`, `0x0020A668`, `0x00434118`, `0x00434120` | `frontend/virtual_keyboard.cpp` (NTSC-J likely has a kana keyboard) |
| `0x0032A530` (`"Scene"`) | `scene/scene_pause.cpp`. NTSC-J formats the pause title in a different word order (`%d %s, %d %s: %s`) |

Mapped by value only: `0x0032B49C` (the dialogue panel alpha, 224.0), and
`0x0032B570` and `0x0032B574` in `virtual_keyboard.cpp`.

### How the executable differs

- **Code is re-linked.** Functions are in a different order. Matching byte
  windows places PAL functions anywhere from about −0x12000 to +0x7000 bytes
  away in NTSC-J, with no single offset. `.text` is 0x200 bytes larger.
- **Library sections moved.** `D3D` and `XGRPH` grew slightly (a different XDK
  library build), which shifts every section after `.text`.
- **Data moved.** `.rdata` and `.data` start at `0x00203120` and `0x0032D580`
  (PAL: `0x00202EA0` and `0x0032C800`). Data moves in blocks, mostly by about
  +0xD80 in `.data`. Some tables moved between sections; the `bigarrowup` and
  `bigarrowdown` texture names, for example, moved into `.data`. Float constant
  pools were laid out again.
- **Some functions changed.** The pause title, the virtual keyboard and the
  function that reads the dialogue panel alpha differ from PAL, not only in
  their addresses. Some NTSC-J tables also hold different values: about 0.03%
  of the dwords the map aligns.

### How the game data differs

- `loctext/` holds `english`, `japanese`, `korean` and `chinese` instead of
  PAL's `english`, `french`, `german`, `italian` and `spanish`.
- The files in `font/` differ, as do some `bundles/aid_script` BNLs
  (`attractsequence_final1/2`, `gameselect_gameselect`,
  `chapter2a_scene13_2playcam`, `chapter3a_scene10_2playcam`,
  `chapter3b_scene2_1playcam`).
- `demand/` has 221 files to PAL's 210.
- The kiosk content (`cdxmedia/`, `Videos/`, `Other PNGs/`) has Japanese
  entries. The port doesn't use it.

### Missing for NTSC-J support

1. **Fonts.** This is the current blocker. `SceneDialogueResourceOwner::load_fonts`
   loads the two English fonts by name, and NTSC-J's English font files differ
   from PAL's. `publish_font` turns each glyph's texture offset into a pointer
   without a bounds check. With NTSC-J fonts, the first text draw in the attract
   scene (`draw_runtime_dialogue_glyph` → `draw_dialogue_texture`) reads through
   a bad glyph texture pointer and crashes. NTSC-J's font format, and how its
   code reads it, needs checking against an NTSC-J decompile.
2. NTSC-J Ghidra exports for the functions that changed (pause title, virtual
   keyboard, dialogue panel alpha), with those differences translated.
3. A check that the changed BNLs and the Japanese, Korean and Chinese text load
   and draw correctly.
4. NTSC-J entries in the retail manifest.
