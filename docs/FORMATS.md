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

Named nodes (16-byte tags) sit after a 36-byte scene-graph header. `ndPushBuffer` / `ndBGPushBuffer` then store `u32 count`, `u32 this_ptr` (header address), `u32 prim` (5 = triangle list, 6 = strip, 7 = fan), then `count` `uint16` indices. Vertex sections with info-type `0x09` are `float3` positions (12 bytes); type `0x0D` is `float2` UVs.

GPU vertex/index bytes live in BNL chunk 2 / the loose resource chunk.

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
