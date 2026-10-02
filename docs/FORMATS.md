# Asset formats

Independent parsers live under `src/gbtg/`. All multi-byte fields are little-endian.

## Shared resource header (loose files)

`demand/`, `font/`, and `loctext/` files:

```text
u32 type
u32 unknown          // often a float-looking word
u32 magic            // 0x48E58836
u32 chunk_count      // 2
{ u32 offset, u32 size } file_chunk     // CPU descriptor
{ u32 offset, u32 size } resource_chunk // GPU / payload (offset usually 0x20)
```

`resource_chunk` at offset 32 contains `ResourceChunkHeader` (`u32 size`, `u32 item_count`) plus `item_count` `{offset, size}` pairs into the GPU payload. Textures store a 28-byte `TextureHeader` in the file chunk.

## BNL bundles

40-byte header, then a **zlib** blob whose decompressed size equals the sum of the four chunk sizes.

```text
u16 file_count
u8  flag             // 1
u8  pad[5]
{ u32 offset, u32 size } chunk[4]
```

Offsets in the header are relative to the start of the file (include the 40-byte header). After inflate, subtract 40.

| Chunk | Contents |
|-------|----------|
| 0 | File table: `file_count` records of 160 bytes (`char name[128]` + resource header) |
| 1 | Per-file `ResourceChunkHeader` + GPU sub-chunk list |
| 2 | GPU / vertex / texture payload |
| 3 | CPU descriptors (texture headers, model trees, scripts) |

## Textures

28-byte header:

```text
u32 xbox_d3d_format
u32 header_size      // 28
u16 width, height
u32 flags
u8  unknown, tile_count; u16 pad
u32 texture_offset   // into GPU payload
u32 data_size
```

Xbox formats used here include DXT1 (`0x0C`), DXT2/3 (`0x0E`), DXT4/5 (`0x0F`), and linear A8R8G8B8 (`0x12`). DXT is stored as linear 4×4 blocks. Uncompressed non-`LIN_` images use original-Xbox NV2A Morton swizzle (not 360 `XGAddress2DTiledOffset`). `gbtg-dump` writes BGRA BMP.

## Models

CPU blob: 32-byte header (`tail_offset`, `tail_count`) then a tail of `{u32 type, u32 offset}`. Type 0 is the main scene graph; type 7 lists embedded texture headers.

Named nodes sit in a `HeaderItem` tree (40-byte records, child/sibling links) walked from model base `+64+52`, matching [project-grabbed](https://github.com/x1nixmzeng/project-grabbed). Payload offsets are relative to model `+64`.

`ndPushBuffer` / `ndBGPushBuffer` (`un_other6 == 1`) store `u32 count`, `u32 this_ptr` (header address), `u32 prim` (5 = triangle list, 6 = strip, 7 = fan), then `count` `uint16` indices. Vertex sections with info-type `0x09` are `float3` positions; type `0x0D`/`0x0E`/`0x0F` are `float2` UV sets.

`ndShaderParam2` binds samplers via a texture-reference table plus named slots. **`colour0`** is the diffuse albedo. On environments **`colour1`** is often a baked lighting atlas (use as lightmap when `RARE_lightmap*` is absent) — do not treat it as a second diffuse that overwrites `colour0`. The PC present multiplies that atlas with `MODULATE2X` (0.5 in the texture is neutral). `colour2`/`colour3` are detail layers; when `colour0` is empty or a near-black base (RGB sum under about 80 while `colour2` is much brighter), promote `colour2` to the visible albedo. `RARE_celmap0` is a 256×2 cel ramp (body tone, highlight band, then black), sampled with the Lambert term of the type `0x0A` normal. It is not a diffuse map. `RARE_lightmap0` / `RARE_lightmap-1` are lightmaps (skip soft radial cookies). Indices are into the model's embedded texture list — the same path for actors and environments.

GPU vertex/index bytes live in BNL chunk 2 / the loose resource chunk.

## Cutscenes (type 7)

CPU blob header (little-endian):

```text
u16 keyframes        // typically duration ≈ keyframes / 30
u8  flag             // 1 in attract clips
u8  tracks           // camera / actor channels (3–12 on attract)
f32 duration         // seconds; authoritative clip length
u32 table offsets…   // relocatable; +8 points at the camera record
```

Camera list entries are 8 bytes: `u32 name_offset`, `u32 channel_offset` (`0x4E0F6`). Each channel is

```text
u16 keyframes
u16 pad
f32 duration
u32 data_offset      // relative to the channel table; 0x4F700 adds that table address
```

`0x514C0` samples `floor(time / duration * count)` and lerps to the next key. Channels 0..2 are position (`0x4F770`). Channels 3..6 are a quaternion xyzw (`0x4F8C0` → `0x51590`). Channel 7 is the scalar written to the camera at +0x24 (35 on attract clips).

Embedded NUL-terminated AIDs: `aid_camera_*`, `aid_model_*`, `aid_anim_*`. Type 8 is cutscene events. Attract plays the type-14 `scenecontrol` aidlist in order (`gbtg::CutsceneQueue`) and writes the sampled pose onto `entityBodyCamera`.

## Scripts (type 24)

CPU blob is a **relocatable** record, not a bytecode stream yet reconstructed as a VM.

```text
u32 root_offset          // often 0x0C; add blob base at load
…pointer-sized fields…   // u32 values that land inside the blob are offsets
char aid_* / labels[]    // NUL-terminated names, typically 128-byte slots
```

Attract `aid_script_ghoulies_attractsequence_final1` (304 bytes) points at `aid_model_editors_introbackground` and `aid_objparams_ghoulies_scenecontrol_attract1`. The matching type-14 aidlist in the same BNL lists the attract cutscenes. Chapter `*_setup` scripts in `common.bnl` are 20-byte headers (chapter index, no names). Playcam scripts are larger and also embed `objTag_*`, event titles, ghouly boxes, and cutscene AIDs.

`gbtg::parse_scene_script` collects header words plus ASCII / `aid_*` strings. Condition/action opcodes are still Xbox-side (`scriptConditionId_*` in the XBE).

## Aidlists (type 14)

CPU blob is a packed array of **128-byte** NUL-padded AID name strings. `aid_aidlist_ghoulies_sceneorder_game` is the boot scene list (attract → game select → books → playcams). `save_bundle` inflates, edits, and zlib-deflates a BNL.

## Loctext

After the resource header, the file chunk is:

1. A small name/index table
2. Magic `LSBL`
3. Wide-character strings (UTF-16LE, null-terminated)
4. ASCII keys
5. Optional comment / extra chunks

Pairs are joined by the index in the ASCII table.

## Wave banks

`xwavebank/` files are Xbox XACT `WBND` version 3 (4 segments, no filename table). `gbtg-dump` lists entries and extracts uncompressed PCM to WAV. ADPCM/WMA cues are identified but not decoded yet.

## Movies

`movie/` files are Bink (`BIKi`). This project only identifies them. Convert locally; do not add a Bink decoder.
