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

- `"There is no script for this scene!"` (`DAT_00512494`)
- `scriptConditionId_*` (PlayerEntersRegion, NumberGhouliesKilled, TimeOut, GhoulyTriggered, …)
- Scene scripts live in BNL type 24 (`aid_script_...`). CPU blob is a relocatable pointer table plus embedded AID names (background model, `aid_objparams_*`, aidlists of cutscenes).
- Attract `scenecontrol` aidlist walks `aid_cutscene_ghoulies_attractmode_*`. Type 7 header is `u16 keyframes`, `u8 tracks`, `f32 duration`. Dense camera curves are `float[keyframes]` (position, then euler) and drive `entityBodyCamera`. `objMsgId_Scene_CutsceneAlloc` / `CutsceneFree` / `Begin` / `End` are still Xbox-side.

**Audio**

- `audioLoadFile`, `audioSoundBankPlayCue`, `audioWaveBankExpertCreate`
- XACT cue names `XACT_SOUNDBANK_*`

**Renderer**

- D3D8 debug strings (`SetTexture`, `SetRenderTarget`, `SetVertexShaderInput`, pixel shader assembler)
- Xbox shader assembler comments still in the binary

## Recovered functions (Ghidra)

Addresses are Xbox virtual. Names are from Code Browser / headless export. Reconstruction lives under `src/engine/`. Re-export with `tools/ghidra/ghidra_scripts/ExportEngine.java`.

### Boot

| VA | Name | Notes |
|----|------|--------|
| `0x0014FAE9` | `entry` / `mainCRTStartup` | Creates a thread at `0x0014FA75`, then `CloseHandle`. |
| `0x0014FA75` | `xapi_thread` | `XapiInitProcess`, `_rtinit`, `_cinit` (entity type static ctors), then `WinMain(0,0,0)`. |
| `0x00032390` | `WinMain` | QPC timer, `gfx_init`, `fs_init`, `game_init`, then `while (true) frame_tick()`. Checks `d:\movie\0de65e94`. |

`WinMain` callees:

| VA | Name | Role |
|----|------|------|
| `0x00032470` | `gfx_init` | `XGetLaunchInfo`; copies `"D:\\GrabbedByTheGhoulies.xbe"`. |
| `0x00033980` | `fs_init` | File/AID setup; sets title string `"ghoulies"`. |
| `0x00032630` | `game_init` | `file_pump` (`0xE9E60`) then `game_init_wait` (`0x39D50`) until not `0xEA`. |
| `0x00033B50` | `frame_tick` | Pad/time/audio/systems/file pump/draw/present. Dt global `0x005096C0`. |
| `0x00033480` / `0x00032AB0` / `0x00033A90` | present | End of `frame_tick`. PC builds submit scene meshes through `gfx_*` (DirectX 11). Xbox remains D3D8. |
| `0x000DF9A0` | `frontendGameSlotSelectBasic` | Game-select state machine. Three slots, index at `menu+0xB2C`. Case 6 walks the plate table at `0x429040` stride `0x124`. |
| `0x000DEB20` | slot texture bind | Empty slot (no save name) uses that slot's `oneframe` / `twoframe` / `threeframe`. Occupied slots use the chapter table at `0x429290`. |
| `0x000DF140` | slot backdrop | Empty slots load `emptyslotbackground` and scale it to the viewport (`DAT_005096E8` / `DAT_005096EA`, 640×480). |
| `0x000DE540` | slot highlight | Lerps the selected plate from the 0.16 rest box (`+ DAT_00428FBC` = 12) to the 0.25 box (`+ DAT_00428FC0` = 20). |
| `0x000DE7B0` | slot center | At rest, center is the per-slot pair at `0x42902C`: (130, 200), (320, 200), (510, 200). |
| `0x000E0210` | slot move | D-pad: code 8 decrements the slot, code 4 increments it, wrapping 0..2, then state 6. |
| `0x000E02D0` | slot select | A (prompt id 7). Empty slot sets widget mode 4 (naming). Status 2, 3, or 4 sets mode 3 (slot contents). |
| `0x000E05D0` | back to attract | B. Copies `aid_script_ghoulies_gameselect_backtoattract` and requests that script. |
| `0x000E5A80` | frontend reset | Boot. `DAT_00429BC0 = 1` (next widget mode is slot select). Clears the name buffers. |
| `0x000E5AE0` | attract widget | Stream id 2 idle (`0x00039D50` returns 0): vtable is `mode[DAT_00429BC0]` (`0x000E5B27`). Still streaming: mode 18 for 5s (`DAT_0032B184`), then `0x000E0A60` requests that same mode. |
| `0x000DD210` | name commit | Done. Nonempty success path `0x000DD270` sets mode 1 and slot status 4 (`0x000DD6C0`). |
| `0x0030A000` | frontend modes | Stride `0x18`, six pointers. 1 slot select, 2 deletion, 3 slot contents, 4 naming, 5 info, 6 play row, 8 features, 18 attract loading plate. |
| `0x00011870` | keyboard build | Five rows per page. Origin x = 64, y = 135, row pitch 42 (`+0xEC`). First key of a row then gains 4px. |
| `0x00015090` | keyboard input | Codes 13/14 move the column (`0x00016D40` / `0x00016DA0`). Codes 11/12 move the row (`0x00016950` / `0x00016B30`). Code 1 activates the key (`0x000152F0`). |
| `0x00016950` / `0x00016B30` | keyboard row change | Remap `+0xC4` from the row widths, remembering the letter column at `+0xC8`. Then wrap the row. |
| `0x00017560` | keyboard skip | Symbols page skips Shift `0x10002` and Caps Lock `0x10003`. A skipped landing repeats the move. |
| `0x000152F0` | keyboard key | Inserts a character, backspace `0x10000`, caret arrows `U+22B2` / `U+22B3`. `0x10007` commits the name. |
| `0x00011150` | key label | Table `0x338D78`: `0x10004` Alphabet `+0x28`, `0x10005` Symbols `+0x2C`, `0x10006` Accents `+0x30`, `0x10007` Done `+0x34`. Space/backspace/shift/caps are `+0x18/+0x1C/+0x20/+0x24`. |
| `0x00016EF0` | key text | Scale 0.65 (`DAT_0032B1EC`). Pen x is 64 plus the running width; y starts at 135. |
| `0x00015A10` | name text | Scale 1.45 (`DAT_0032B538`) at (130, 54). |
| `0x000516A0` | font fixup | CPU blob is the font. Glyphs at `+0x18`, count `last-first+1`, stride `0x18`: texture offset, tag, x0, y0, x1, y1. |
| `0x00051760` | glyph advance | `x1 - x0`. Missing, and space via `0x000607F0`, use another width (`'l'` for space, cell width otherwise). |
| `0x00053550` | gloopy frame | If texture byte `+0x13` is `0xFF`, `frame = (int)(clock * byte+0x10) % byte+0x11`, then `+0x13` is set back to `0xFF`. Each frame is `data_size` of 32×32 DXT3. |
| `0x0005CA60` | sprite bind | A frame index other than -1, and below the tile count, is stored at texture `+0x13` before `0x00053550`. -1 leaves the clock running. |
| `0x00060070` | text frame | `0x00016EF0` (keys) and `0x00015A10` (name) submit with eax=1. After the opening `tile_count/rate` seconds that path returns `tile_count-1`. The key at `+0xC0`/`+0xC4` is the one left on the `0x00053550` clock. |
| `0x00033030` | `fs_more` | After file setup: 60 Hz (50 if PAL flags), `dt = 1/fps`, `300*dt` split. |
| `0x000FC030` | `pad_clock` | QPC; `dt` clamped to `5/fps`. |
| `0x00033C80` | `frame_time` | `DAT_004DEA60 += dt_raw`; increment frame counters; fire 20 delayed callbacks at `0x004DEA64`. |
| `0x000E9E60` | `file_pump` | Walk async job list `DAT_0050661C`; state 2 frees a finished job. |
| `0x00039D50` | `game_init_wait` | Sum remaining bytes for stream id across 12 slots; `0xEA` while any remain. |

### Asset paths

| VA | Name | Format |
|----|------|--------|
| `0x00033E00` / `0x00033FB0` / `0x00034290` | bundle open | `snprintf("%s%s%s", root, "bundles\\", name)` then `Z:\` alias |
| `0x00034FAA` | demand open | `sprintf("%s%s%08x", root, "demand\\", hash)` |
| `0x001287F0` | `script_set_default_name` | Allocates a 0x20 buffer; copies `"There is no script for this scene!"` into `DAT_00512494`. |
| `0x00128AB0` | `script_set_name` | `EAX` = C string; copies into `DAT_00512494`. |
| `0x001289A0` | `script_clear` | Restores the default name, clears script heap. |
| `0x001234B0` | `aidlist_load` | `EAX` = name; requires little-endian `"aid_"` (`0x5F646961`); `0x35E90` resolves the resource. |

### Entities

Type descriptors are C vtables. `entity_type_fill_defaults` (`0x0001DA60`) writes 24 default slots into `EAX`, then a type ctor overwrites. No MSVC RTTI.

| VA | Name | Notes |
|----|------|--------|
| `0x0001DA60` | `entity_type_fill_defaults` | Default Body vtable (includes `fn_25ae0` apply at slot 6). |
| `0x00073190` | `entityBodyCamera_ctor` | Desc `0x00504F80`. Size `0x914` / extra `0x7C8`. |
| `0x00073170` | `entityBodyCamera_sizeof` | Writes those two sizes. |
| `0x000731F0` | `entityBodyCamera_tick` | Copies actor world pos/euler to body+`0x89C` / +`0x8A8`. |
| `0x00073300` | `entityBodyCamera_shutdown` | Clears `DAT_00510F70`. |
| `0x00073320` | `entityBodyCamera_msg` | Msg id `99` translates body+`0x18`. |
| `0x000412A0` | `actor_world_position` | Unattached: actor+`0x08`. Attached: parent matrix. |
| `0x00041820` | `actor_world_euler` | Unattached: actor+`0x14`. |
| `0x00025AE0` | camera apply | Pose copy used by BanjoCam / xemu fly-cam patch. |
| `0x00072BA0` | BanjoCam look-at | |

Body types in `.rdata`: `entityBodyPlayer`, `Human`, `Camera`, `Zombie`, `Vampire`, `Worm`, `JessieClyde`. Minds: `Player`, `Human`, `Cinema`. Strategies: one per ghouly plus `Player`, `Director`, `Generic`, `FNPC`.

## Ghidra starting points

1. Entry `0x0014FAE9` (after XOR decode — ghidra-xbe does this).
2. Xrefs to the string `bundles\` / `demand\` / `aid_script`.
3. Xrefs to `audioLoadFile` and `There is no script for this scene!`.
4. Kernel import thunk at `0x00202EA0` for XAPI / Nt / D3D.

Named functions: [`docs/generated/ghidra_functions.txt`](generated/ghidra_functions.txt). Do not commit the Ghidra database. Headless scripts live in `tools/ghidra/ghidra_scripts/`.

## Supervisor log

| VA | Name | Notes |
|----|------|-------|
| `0x0001DD00` | `fn_1dd00` | scene |
| `0x00032AB0` | `present_32ab0` | present |
| `0x00038640` | `xact_38640` | audio |
| `0x0001EC20` | `fn_1ec20` | scene |
| `0x00033480` | `present_33480` | present |
| `0x00038C20` | `xact_38c20` | audio |
| `0x0001EEC0` | `fn_1eec0` | scene |
| `0x00033A90` | `present_33a90` | present |
| `0x000396F0` | `xact_396f0` | audio |
| `0x0001FB40` | `fn_1fb40` | scene |
| `0x0003A040` | `audio_listener` | audio |
| `0x000663E0` | `fn_663e0` | entity |
| `0x00021C50` | `fn_21c50` | scene |
| `0x000FA3C0` | `pad_process` | clock |
| `0x0015B780` | `audio_update` | audio |
| `0x00025130` | `fn_25130` | scene |
| `0x000FAD80` | `pad_buttons` | clock |
| `0x00027960` | `fn_27960` | scene |
| `0x000426F0` | `fn_426f0` | scene |
| `0x0011BE40` | `fn_11be40` | scene |
| `0x0001D850` | `fn_1d850` | entity |
| `0x0012B160` | `fn_12b160` | scene |
| `0x0001EA90` | `fn_1ea90` | entity |
| `0x00152B30` | `fn_152b30` | scene |
| `0x0001EDE0` | `fn_1ede0` | entity |
| `0x00041730` | `fn_41730` | entity |
| `0x00072BA0` | `fn_72ba0` | entity |
| `0x000F57F0` | `fn_f57f0` | entity |
| `0x000FCD70` | `fn_fcd70` | entity |
| `0x00104040` | `fn_104040` | entity |
| `0x00105E20` | `fn_105e20` | entity |
| `0x0010AE90` | `fn_10ae90` | entity |
| `0x0001DA60` | `fn_1da60` | entity |
| `0x0001FE00` | `fn_1fe00` | entity |
| `0x000253B0` | `fn_253b0` | entity |
| `0x00048C70` | `fn_48c70` | entity |
| `0x00055AA0` | `fn_55aa0` | entity |
| `0x00073760` | `fn_73760` | entity |
| `0x000F72D0` | `fn_f72d0` | entity |
| `0x001203E0` | `fn_1203e0` | entity |
| `0x00120480` | `fn_120480` | entity |
| `0x00120760` | `fn_120760` | entity |
| `0x001226C0` | `fn_1226c0` | entity |
| `0x0013D2C0` | `fn_13d2c0` | entity |
| `0x00040FB0` | `fn_40fb0` | entity |
| `0x00056720` | `fn_56720` | entity |
| `0x00057D40` | `fn_57d40` | entity |
| `0x000FD620` | `fn_fd620` | entity |
| `0x00118470` | `fn_118470` | entity |
| `0x00119160` | `fn_119160` | entity |
| `0x0011B630` | `fn_11b630` | entity |
| `0x0011EAD0` | `fn_11ead0` | entity |
| `0x001233C0` | `fn_1233c0` | entity |
| `0x001234B0` | `fn_1234b0` | entity |
| `0x00123B30` | `fn_123b30` | entity |
| `0x00129F30` | `fn_129f30` | entity |
| `0x00070006` | `fn_70006` | scene |
| `0x0010D3B0` | `fn_10d3b0` | entity |
| `0x0014AAED` | `fn_14aaed` | scene |
| `0x00152BB0` | `fn_152bb0` | entity |
| `0x0014D217` | `fn_14d217` | scene |
| `0x001534C0` | `fn_1534c0` | entity |
| `0x0014D283` | `fn_14d283` | scene |
