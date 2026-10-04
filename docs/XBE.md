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
| `0x0014FB5B` | `fn_14fb5b` | scene |
| `0x0014FCF4` | `fn_14fcf4` | scene |
| `0x0015028C` | `fn_15028c` | scene |
| `0x00150833` | `fn_150833` | scene |
| `0x0015118E` | `fn_15118e` | scene |
| `0x0014B122` | `fn_14b122` | scene |
| `0x0014B22F` | `fn_14b22f` | scene |
| `0x0014C2D5` | `fn_14c2d5` | scene |
| `0x0014C532` | `fn_14c532` | scene |
| `0x0014C633` | `fn_14c633` | scene |
| `0x0014C6B3` | `fn_14c6b3` | scene |
| `0x0014CD37` | `fn_14cd37` | scene |
| `0x0014CDB7` | `fn_14cdb7` | scene |
| `0x0014CE19` | `fn_14ce19` | scene |
| `0x0014CE4F` | `fn_14ce4f` | scene |
| `0x0014CE69` | `fn_14ce69` | scene |
| `0x0015162F` | `fn_15162f` | scene |
| `0x0014AABA` | `fn_14aaba` | scene |
| `0x0014AF34` | `fn_14af34` | scene |
| `0x0014E44E` | `fn_14e44e` | scene |
| `0x0014E872` | `fn_14e872` | scene |
| `0x0014EFED` | `fn_14efed` | scene |
| `0x0015040D` | `fn_15040d` | scene |
| `0x0015059A` | `fn_15059a` | scene |
| `0x001508AB` | `fn_1508ab` | scene |
| `0x00150CC6` | `fn_150cc6` | scene |
| `0x00150E37` | `fn_150e37` | scene |
| `0x00151000` | `fn_151000` | scene |
| `0x00151AB5` | `fn_151ab5` | scene |
| `0x0014D08C` | `fn_14d08c` | scene |
| `0x00154BA0` | `fn_154ba0` | entity |
| `0x00154C90` | `fn_154c90` | entity |
| `0x001660E9` | `fn_1660e9` | scene |
| `0x00154D70` | `fn_154d70` | entity |
| `0x00166ECB` | `fn_166ecb` | scene |
| `0x00154E50` | `fn_154e50` | entity |
| `0x001673C1` | `fn_1673c1` | scene |
| `0x00154F80` | `fn_154f80` | entity |
| `0x0016AFDD` | `fn_16afdd` | scene |
| `0x00155000` | `fn_155000` | entity |
| `0x0016D805` | `fn_16d805` | scene |
| `0x00019140` | `fn_19140` | entity |
| `0x00165F0E` | `fn_165f0e` | scene |
| `0x00154F00` | `fn_154f00` | entity |
| `0x0016619A` | `fn_16619a` | scene |
| `0x001550A0` | `fn_1550a0` | entity |
| `0x00167F2E` | `fn_167f2e` | scene |
| `0x00155110` | `fn_155110` | entity |
| `0x001681C1` | `fn_1681c1` | scene |
| `0x00155260` | `fn_155260` | entity |
| `0x00168883` | `fn_168883` | scene |
| `0x001557B0` | `fn_1557b0` | entity |
| `0x00168FED` | `fn_168fed` | scene |
| `0x001670F6` | `fn_1670f6` | scene |
| `0x00167BDE` | `fn_167bde` | scene |
| `0x00167C9C` | `fn_167c9c` | scene |
| `0x00167EC9` | `fn_167ec9` | scene |
| `0x0016805F` | `fn_16805f` | scene |
| `0x001681B8` | `fn_1681b8` | scene |
| `0x00168AF5` | `fn_168af5` | scene |
| `0x0016993D` | `fn_16993d` | scene |
| `0x0016A17C` | `fn_16a17c` | scene |
| `0x0016AE1F` | `fn_16ae1f` | scene |
| `0x0016AE40` | `fn_16ae40` | scene |
| `0x0016B050` | `fn_16b050` | scene |
| `0x00166BF9` | `fn_166bf9` | scene |
| `0x0016810F` | `fn_16810f` | clock |
| `0x00168AFE` | `fn_168afe` | scene |
| `0x00168B96` | `fn_168b96` | scene |
| `0x00168C8E` | `fn_168c8e` | scene |
| `0x00168E08` | `fn_168e08` | scene |
| `0x00168E34` | `fn_168e34` | scene |
| `0x0016B308` | `fn_16b308` | scene |
| `0x0016D9F8` | `fn_16d9f8` | scene |
| `0x0016DEA6` | `fn_16dea6` | scene |
| `0x0016DFB5` | `fn_16dfb5` | scene |
| `0x0016E770` | `fn_16e770` | scene |
| `0x0016B40D` | `fn_16b40d` | scene |
| `0x0016BA2F` | `fn_16ba2f` | clock |
| `0x0016B45F` | `fn_16b45f` | scene |
| `0x0016BEF6` | `fn_16bef6` | clock |
| `0x0016BAAE` | `fn_16baae` | scene |
| `0x0016C177` | `fn_16c177` | scene |
| `0x0016D466` | `fn_16d466` | scene |
| `0x0016D55F` | `fn_16d55f` | scene |
| `0x0016E79D` | `fn_16e79d` | scene |
| `0x0016F095` | `fn_16f095` | scene |
| `0x0016F172` | `fn_16f172` | scene |
| `0x0016F18D` | `fn_16f18d` | scene |
| `0x0016D583` | `fn_16d583` | scene |
| `0x0016D7E1` | `fn_16d7e1` | scene |
| `0x0016DC45` | `fn_16dc45` | scene |
| `0x0016DC70` | `fn_16dc70` | scene |
| `0x0016DE82` | `fn_16de82` | scene |
| `0x0016E07A` | `fn_16e07a` | scene |
| `0x0016F1A8` | `fn_16f1a8` | scene |
| `0x0016F266` | `fn_16f266` | scene |
| `0x00170523` | `fn_170523` | scene |
| `0x0017052A` | `fn_17052a` | scene |
| `0x00173900` | `fn_173900` | scene |
| `0x00177AF0` | `fn_177af0` | scene |
| `0x00171C20` | `fn_171c20` | scene |
| `0x00171E10` | `fn_171e10` | scene |
| `0x00172720` | `fn_172720` | scene |
| `0x00172DA0` | `fn_172da0` | scene |
| `0x00172EE0` | `fn_172ee0` | scene |
| `0x00172F90` | `fn_172f90` | scene |
| `0x00173000` | `fn_173000` | scene |
| `0x001731A0` | `fn_1731a0` | scene |
| `0x0005E0C0` | `fn_5e0c0` | gfx |
| `0x001041B0` | `fn_1041b0` | anim |
| `0x00176F10` | `fn_176f10` | scene |
| `0x0005E110` | `fn_5e110` | gfx |
| `0x0005E2A0` | `fn_5e2a0` | gfx |
| `0x00151E64` | `fn_151e64` | gfx |
| `0x00151F62` | `fn_151f62` | gfx |
| `0x00171BB0` | `fn_171bb0` | gfx |
| `0x00176EE0` | `fn_176ee0` | gfx |
| `0x00176F80` | `fn_176f80` | gfx |
| `0x001771A0` | `fn_1771a0` | gfx |
| `0x001770A0` | `fn_1770a0` | gfx |
| `0x00177220` | `fn_177220` | gfx |
| `0x001782C0` | `fn_1782c0` | gfx |
| `0x001799A0` | `fn_1799a0` | gfx |
| `0x0015AD20` | `fn_15ad20` | present |
| `0x00072B10` | `fn_72b10` | entity |
| `0x00072CB0` | `fn_72cb0` | entity |
| `0x00124CC0` | `fn_124cc0` | scene |
| `0x00124E50` | `fn_124e50` | scene |
| `0x00124F30` | `fn_124f30` | scene |
| `0x00124FC0` | `fn_124fc0` | scene |
| `0x001251E0` | `fn_1251e0` | scene |
| `0x001259B0` | `fn_1259b0` | scene |
| `0x00126140` | `fn_126140` | scene |
| `0x001269D0` | `fn_1269d0` | scene |
| `0x00126A10` | `fn_126a10` | scene |
| `0x00126E10` | `fn_126e10` | scene |
| `0x00127340` | `fn_127340` | scene |
| `0x001274B0` | `fn_1274b0` | scene |
| `0x00127BC0` | `fn_127bc0` | scene |
| `0x00128050` | `fn_128050` | scene |
| `0x00128770` | `fn_128770` | scene |
| `0x00128BC0` | `fn_128bc0` | scene |
| `0x00132680` | `fn_132680` | scene |
| `0x001326D0` | `fn_1326d0` | scene |
| `0x00132720` | `fn_132720` | scene |
| `0x001327A0` | `fn_1327a0` | scene |
| `0x00132800` | `fn_132800` | scene |
| `0x00132840` | `fn_132840` | scene |
| `0x001328D0` | `fn_1328d0` | scene |
| `0x00132940` | `fn_132940` | scene |
| `0x001329A0` | `fn_1329a0` | scene |
| `0x00132A00` | `fn_132a00` | scene |
| `0x00132A30` | `fn_132a30` | scene |
| `0x00132A60` | `fn_132a60` | scene |
| `0x00132AB0` | `fn_132ab0` | scene |
| `0x00132B50` | `fn_132b50` | scene |
| `0x00132BC0` | `fn_132bc0` | scene |
| `0x00132BF0` | `fn_132bf0` | scene |
