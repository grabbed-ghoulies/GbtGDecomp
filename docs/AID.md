# AID (asset id) names and hashes

Rare’s Xbox pipeline names every cooked resource `aid_<type>_ghoulies_<...>`. On disc, loose files are stored under the **8-digit hex hash** of that string (no extension).

## Hash (`gbtg::aid_hash`)

Skip the `aid_` prefix. For each remaining byte, uppercase ASCII letters (`c & 0xDF`), fold into a 32-bit nibble hash, and XOR out the high nibble when it is set:

```text
hash = 0
for c in name[4:]:
    hash = (hash << 4) + (c & 0xDF)
    hi = hash & 0xF0000000
    if hi:
        hash ^= hi | (hi >> 24)
```

Known checks (public TCRF / static asserts from independent RE):

| AID name | Hash |
|----------|------|
| `aid_texture_ghoulies_scoreboard_eventicons_dontgethit` | `00559fa4` |
| `aid_texture_ghoulies_scoreboard_energy_container_syringes` | `02fd7fb3` |

`gbtg-catalog` pulls `aid_*` strings out of `default.xbe` and matches them to hashed files under `demand/`, `font/`, `movie/`, `loctext/`, and `xwavebank/`. `--out docs/generated` writes name/hash maps only (`aid_names.txt`, `aid_resolved.txt`).

## Resource type field

Loose containers and BNL entries start with a `uint32` type (see [FORMATS.md](FORMATS.md)):

| Id | AID prefix / role |
|----|-------------------|
| 1 | `aid_texture_` |
| 2 | `aid_anim_` |
| 4 | `aid_model_` |
| 5 | `aid_animevents_` |
| 7 | `aid_cutscene_` |
| 8 | cutscene events |
| 10 | misc |
| 11 | `aid_actorgoals_` |
| 12 | marker |
| 13 | callout / fx |
| 14 | `aid_aidlist_` |
| 16 | `aid_loctext_` |
| 18 | soundbank |
| 19 | DSP |
| 20 | cue list |
| 21 | `aid_font_` (also `font/`) |
| 22 | ghouly box |
| 23 | ghouly spawn |
| 24 | `aid_script_` |
| 25 | `aid_actorattribs_` |
| 26 | `aid_fxemitter_` / emitter |
| 27 | particles |
| 28 | rumble |
| 29 | shake cam |

## Where names live

- **Bundles:** uncompressed BNL file table stores the full `aid_...` string (128-byte field).
- **Loose files:** only the hash is on disk; recover the name from XBE string tables or by hashing candidate AID names.
- **XBE:** thousands of `aid_*` C strings (the registry the runtime looks up).
