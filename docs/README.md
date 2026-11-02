# Documentation

Start with [Architecture](ARCHITECTURE.md) and the [Glossary](GLOSSARY.md).
The project rules are in [`AGENTS.md`](../AGENTS.md).

## Getting started

- [Architecture](ARCHITECTURE.md): how a frame runs, the source layout, and native vs. host code
- [Glossary](GLOSSARY.md): AID, BNL, body, mind, unbound, `RetailField`, and other terms
- [Contributing](CONTRIBUTING.md): where code goes, logging, and game data hygiene
- [Disc layout](DISC_LAYOUT.md): what the extracted retail data looks like
- [Regional releases](REGIONS.md): PAL, NTSC (USA) and NTSC-J support
- [Linux](LINUX.md) and [Vulkan](VULKAN.md): non-Windows builds and the Vulkan presenter

## Reconstruction workflow

- [Ghidra workflow](GHIDRA.md): exporting decompiles from your own `default.xbe`
- [Executable map](XBE.md): sections and known addresses in `default.xbe`
- [Retail layouts](RETAIL_LAYOUTS.md): `RetailField` descriptors and how retail objects become structs
- [Legacy host code](LEGACY_HOST_CODE.md): host stand-ins still waiting to be replaced by the native path
- [Retail deviations](RETAIL_DEVIATIONS.md): the few deliberate departures from retail behavior
- [xemu](XEMU.md): running the retail game as a runtime oracle
- [Code oracle](../tools/oracle/README.md): comparing single functions against retail machine code
- [Original file changes](ORIGINAL_FILES.md): local patches to the retail dump and how to restore them

## Data formats

- [AID resource identifiers](AID.md): asset names, hashes and type numbers
- [File formats](FORMATS.md): bundles, loose resources and scripts
- [Generated tables](generated/README.md): name and hash maps produced by `gbtg-catalog`

## Subsystems

Each of these documents describes a native subsystem and the work that remains on it.

- [Player scene](PLAYER_SCENE.md): the first playable scene, and most of the actor, body and mind work
- [Scene presentation](NATIVE_SCENE_PRESENT.md): what each view draws, and how
- [Front end](NATIVE_FRONTEND.md): game select and menus
- [Dialogue presentation](NATIVE_DIALOGUE_PRESENT.md): tutorial and scene dialogue
- [Transitions](NATIVE_TRANSITIONS.md): the chapter book, map route and slime transitions
- [Cutscenes](NATIVE_CUTSCENES.md): the translated cutscene runtime
- [Audio](NATIVE_AUDIO.md): native audio and the XAudio2 boundary
- [Rendering performance](RENDERING_PERFORMANCE_PLAN.md): the `GBTG_OPTIMIZE` presenter work

## Tools and extensions

- [Scene inspector](INSPECTOR.md): the separate `gbtg-inspector` application
- [Plugins](PLUGINS.md): non-retail features, including Rare Replay; see also the [example plugins](../examples/plugins/README.md)

## Legal

- [Legal notes](LEGAL.md)

## Historical

These documents describe superseded work and are kept for reference.

- [Game select rendering](GAMESELECT_RENDERING.md): the deleted host front end
- [Native gameplay frame](NATIVE_GAMEPLAY_FRAME.md): folded into [Player scene](PLAYER_SCENE.md)
