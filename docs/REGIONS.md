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
Running it fails at startup:

```text
error: executable resource table does not match this retail XBE:
```

`SceneExecutableResources` (`src/engine/scene/executable_resources.cpp`) reads
the AID resource pools from fixed PAL addresses such as `0x0033AC88`. At those
addresses NTSC-J holds other data. That is only the first check to fail; every
hardcoded retail address in the source assumes the PAL layout.

### How the executable differs

- **Code is re-linked.** Functions are in a different order. Matching byte
  windows places PAL functions anywhere from about −0x12000 to +0x7000 bytes
  away in NTSC-J, with no single offset. `.text` is 0x200 bytes larger.
- **Library sections moved.** `D3D` and `XGRPH` grew slightly (a different XDK
  library build), which shifts every section after `.text`.
- **Data moved.** `.rdata` and `.data` start at `0x00203120` and `0x0032D580`
  (PAL: `0x00202EA0` and `0x0032C800`). The `aid_animtable` and
  `aid_attackdata` pools sit 0xD90 bytes later than in PAL. The rest of the
  data has not been mapped.
- **Behavior may differ.** A build two months newer may have changed functions,
  not only moved them. That has not been checked.

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

1. A PAL→NTSC-J address map for functions, globals and tables, so the runtime
   can choose an address set from the XBE certificate instead of assuming PAL.
2. NTSC-J Ghidra exports for any function whose body changed, with those
   differences translated.
3. A check that the changed BNLs, fonts and Japanese, Korean and Chinese text
   load and draw correctly. That includes whatever the font and text code needs
   for CJK glyphs.
4. NTSC-J entries in the retail manifest.
