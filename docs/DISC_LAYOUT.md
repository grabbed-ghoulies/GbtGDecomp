# Disc layout

User-supplied Xbox data (extracted, not a raw ISO) looks like this. Sizes are from one retail dump; verify with `manifests/retail_files.json`.

## Game (port this)

| Path | Role |
|------|------|
| `default.xbe` | Main game executable (~4.35 MB). Build path inside: `source_24.09.03.0028\Ghoulies\game\XBox_Release\GrabbedByTheGhoulies.exe` |
| `bundles/common.bnl` | Shared bundle (HUD, fonts, setup scripts, some models) |
| `bundles/aid_script/*.bnl` | Per-scene bundles (chapters, challenges, attract, credits) |
| `bundles/aid_objparams/*.bnl` | Player / cook actor packs |
| `demand/` | Loose hashed resources (textures and other on-demand assets) |
| `font/` | Loose hashed fonts (resource type 21) |
| `loctext/{english,french,german,italian,spanish}/` | Localized string tables (type 16); same 20 hash names per language |
| `movie/` | In-game Bink videos (`BIKi`), hashed names |
| `xwavebank/` | Xbox wave banks (`WBND`) |

Hashed filenames are 8 lowercase hex digits: Rare AID hash of names such as `aid_texture_ghoulies_...`. See [AID.md](AID.md).

`.bnl` files are zlib-compressed scene packs, not “script only”. A playcam bundle holds models, animations, actor goals, and the scene script.

## Kiosk (ignore)

| Path | Role |
|------|------|
| `cdx.xbe` | Microsoft CDX trailer player |
| `cdxmedia/` | CDX UI (`cdx.inx`, `cdx.xpr`, `cdx.xwb`, `cdx.xsb`, font) |
| `Videos/*.xmv` | Promo trailers (Kameo, PGR2, …) |
| `Other PNGs/` | CDX menu thumbnails |

## First scene used by the engine slice

1. `bundles/common.bnl`
2. `bundles/aid_script/ghoulies_chapter1_scene1_2playcam.bnl`
3. `bundles/aid_objparams/ghoulies_actor_player_boy.bnl`
