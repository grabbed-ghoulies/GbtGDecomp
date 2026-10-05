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

`ndShaderParam2` binds samplers via a texture-reference table plus named slots. `colour0`, `colour1`, `colour2`, and `colour3` stay on the slots that bind wrote. `RARE_celmap0` is a 256×2 cel ramp (body tone, highlight band, then black), sampled with the Lambert term of the type `0x0A` normal. It is not a diffuse map. `RARE_lightmap0` / `RARE_lightmap-1` are lightmaps. Indices are into the model's embedded texture list — the same path for actors and environments.

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

CPU blob is a packed list of nodes. The binder adds the first dword to the node address, which lands on the next node.

```text
u32 size                 // includes this header; next node is at this + size
u32 type
u8  payload[size - 8]    // C strings sit in the payload (model aids, titles, tags)
```

A type of 0, size 8, ends the blob. Chapter `*_setup` scripts in `common.bnl` are one type-6 node (chapter index in the payload) plus that terminator.

Attract `aid_script_ghoulies_attractsequence_final1` (304 bytes) is five nodes: type 5, type `0x4C`, type 1 `aid_model_editors_introbackground`, type 2 `aid_objparams_ghoulies_scenecontrol_attract1`, then the terminator. The matching type-14 aidlist in the same BNL lists the attract cutscenes.

Playcam scripts are the same packing with more types: `0x0A` region name, `0x0E` event title, `0x13` cutscene, `0x18`/`0x29` body, `0x2A` ghouly box, `0x3D` timeout, `0x3C` end of an action block, `0x9F` actor tag.

`gbtg::parse_scene_script` keeps those nodes plus the ASCII / `aid_*` strings. Loading a script walks the nodes (`0x00128A00`). Type `0x3C` stops the walk. Types `0x3D`, `0x3E`, `0x40`, `0x47`, `0x48`, `0x94`, `0x95`, and `0x99` copy a condition into the runtime list (`0x00124C80`). Opcode 0 is `scriptConditionId_TimeOut`. The other 26 names live in the table at `0x00314258`. The frame tick (`0x00117100`) updates that list and runs whatever fired.

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

`movie/` files are Bink (`BIKi`). `gbtg.exe` opens them from `--game-dir` and decodes them in-process. The build links LGPL FFmpeg for that decoder. There is no conversion step, and the RAD Bink SDK is not part of this tree.
