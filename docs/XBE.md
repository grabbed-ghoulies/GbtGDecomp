# `default.xbe` notes

Recovered from a retail dump with `gbtg-catalog` / `gbtg::parse_xbe`. Load **your** XBE in Ghidra (see [GHIDRA.md](GHIDRA.md)). Use **xemu** or Cxbx-Reloaded as a runtime oracle; they are not the PC port.

## Identity

| Field | Value |
|-------|--------|
| Title | Grabbed by the Ghoulies |
| Title ID | `0x4D530053` |
| Debug path | `c:\source_24.09.03.0028\Ghoulies\game\XBox_Release\GrabbedByTheGhoulies.exe` |
| Image base | `0x00010000` |
| Entry (retail XOR `0xA8FC57AB`) | `0x0014FAE9` |
| Kernel thunk (XOR `0x5B6D40B6`) | `0x00202EA0` |
| XDK / libs | **5659** (`XAPILIB`, `D3D8`, `D3DX8`, `XGRAPHC`, `XBOXKRNL`, `LIBCMT`, `LIBCPMT`, `XACTENG`, `DSOUND`) |

MSVC RTTI (`.?AV`) is almost absent — only `std::exception` family. Game types are **not** exposed as RTTI; recover them from string tables and vtables in Ghidra.

## Sections (file-load map)

| Name | Role |
|------|------|
| `.text` | Game + engine code (~1.4 MB) |
| `D3D` / `D3DX` / `XGRPH` | Xbox D3D8 / graphics helpers |
| `BINK*` / `BINKDATA` | RAD Bink (do not reimplement in-tree) |
| `XACTENG` / `DSOUND` / `WMADEC` / `DOLBY` | Audio |
| `XPP` | XAPI |
| `.rdata` / `.data` | AID strings, tables, globals |
| `$$XTIMAGE` / `$$XSIMAGE` | Title / save images |
| `.XTLID` | Library id table |

## Subsystems (from strings)

**Assets / streaming**

- Paths `bundles\` and `demand\`
- AID registry: ~5500 unique `aid_*` C strings (`anim`, `objparams`, `animevents`, `model`, `actorgoals`, `script`, `texture`, …)
- zlib `inflate 1.1.3` (BNL payload)
- Model nodes: `ndVertexBuffer`, `ndPushBuffer`, `ndShader2`, `ndVertexShader`, `ndPixelShader`

**Actors / AI**

- `entityBodyPlayer`, `entityMindPlayer`, `entityStrategyPlayer`, `entityBodyCamera`
- `actorMindGoalId_*` (WalkToTarget, AttackTarget, Celebrate, BeginScript, BanjoCamAttachedToFocalActor, …)
- `actorStrategyId_*` (Idle, Attack_*, Evade, Coward, Director_FollowPlayer, …)
- `objMsgId_Player_*`, `objMsgId_Ghouly_*`, `objMsgId_Camera_*`, `objMsgId_Scene_*`

**Script VM**

- `"There is no script for this scene!"`
- `scriptConditionId_*` (PlayerEntersRegion, NumberGhouliesKilled, TimeOut, GhoulyTriggered, …)
- Scene scripts live in BNL type 24 (`aid_script_...`)

**Audio**

- `audioLoadFile`, `audioSoundBankPlayCue`, `audioWaveBankExpertCreate`
- XACT cue names `XACT_SOUNDBANK_*`

**Renderer**

- D3D8 debug strings (`SetTexture`, `SetRenderTarget`, `SetVertexShaderInput`, pixel shader assembler)
- Xbox shader assembler comments still in the binary

## Ghidra starting points

1. Entry `0x0014FAE9` (after XOR decode — ghidra-xbe does this).
2. Xrefs to the string `bundles\` / `demand\` / `aid_script`.
3. Xrefs to `audioLoadFile` and `There is no script for this scene!`.
4. Kernel import thunk at `0x00202EA0` for XAPI / Nt / D3D.

Export renamed functions to `docs/generated/` later; do not commit the Ghidra database. `tools/ghidra/ExportGbtg.java` dumps AID strings and xrefs to loader / audio / script / `ndVertexBuffer` once the XBE is imported with **ghidra-xbe** (not as a flat binary).
