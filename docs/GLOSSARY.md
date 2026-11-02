# Glossary

Terms used throughout the code, comments and docs. For how the pieces fit
together, see [Architecture](ARCHITECTURE.md).

## Reconstruction

**Translated function**: C++ written from the Ghidra decompile of one retail
function, with the same branches, comparisons and offsets. It is marked with
`// Xbox 0x........`, the function's address in the retail executable.

**Dump / frontier dump**: a function's Ghidra decompile, exported to
`cache/ghidra_frontier/<VA>.txt`. It is the source a translation must match.
See [Ghidra workflow](GHIDRA.md).

**VA**: virtual address in `default.xbe`. Comments write addresses as
`0x0011E4C0` for code and `DAT_0050EA9C` (Ghidra's name) for data.

**Native path**: the translated retail code, run in retail order from
`frame_tick`. "Native" means "translated from retail", not "host-native".

**Host code / host boundary**: PC code. It is allowed only where retail calls
out to the platform (file IO, D3D, XInput, heap, thread-local storage, XACT
audio) and for presenting recorded output. Host code that still reimplements
game behavior is listed in [Legacy host code](LEGACY_HOST_CODE.md).

**Unbound / untranslated callee**: a retail function the native path reaches
that has no translation yet. It throws when reached (`unbound(...)`) rather
than being stubbed, so a run stops at the next piece of work.

**Oracle**: something that answers "what does retail actually do here?".
[xemu](XEMU.md) runs the whole game. The [code oracle](../tools/oracle/README.md)
runs single retail functions in Unicorn and compares their outputs with the
translation.

**Retail deviation**: one of the few deliberate departures from retail
behavior, each listed in [Retail deviations](RETAIL_DEVIATIONS.md).

## Data and layout

**`RetailField` / `RetailRecord`**: a named descriptor (type, offset, name) for
a field of a retail object kept at its Xbox offsets. Code reads and writes
fields through these instead of `base + 0x24`. See
[Retail layouts](RETAIL_LAYOUTS.md) and `include/gbtg/retail_field.h`.

**`unk_<offset>`**: a field whose meaning the dumps do not settle yet.

**`+0x...`**: an offset within a retail object, as in "body +0x84".

## Game data

**AID**: Rare's asset identifier, a string such as
`aid_script_ghoulies_chapter1_scene1_2playcam`. On disc, loose files are
stored under the hash of that string. See [AID](AID.md).

**BNL / bundle**: a packed archive of resources, such as `common.bnl` or the
per-scene bundles under `bundles/aid_script/`. See [File formats](FORMATS.md)
and [Disc layout](DISC_LAYOUT.md).

**`aid_script` / scene**: a scene is driven by its script resource. The scene
order list in `common.bnl` gives the sequence `--scene` indexes into.

**Marker**: a placed record in a scene (`aid_marker_*`) that creates or
positions actors and triggers.

## Game objects

**Actor**: a game object in a scene. Most have a **body** and a **mind**.

**Body**: the physical side of an actor, covering animation, collision,
movement and damage.

**Mind**: the deciding side of an actor. A player's mind reads input. An
enemy's mind runs a strategy and navigation.

**Ghouly**: one of the game's enemies.

**Factory / factory owner**: the retail object factory that creates actors by
type, and the per-type owner classes it dispatches to (for example
`player_factory_owner.cpp`).

**Actor services**: the boot-owned graph that wires scenes, actors, lists and
message dispatch together (`scene/scene_actor_services.cpp`).

**Scene control**: a script-created object that drives scene-specific logic,
such as a challenge's rules or game select.

## Rendering

**`SceneD3DDevice`**: the recording D3D device. Translated code draws into it.
It keeps the state retail reads back and records every draw.

**Presenter**: host code that replays the recorded draws through D3D11, Vulkan
or a null backend.

## Build flags

**`GBTG_RETAIL_GAME_ONLY`** (default ON): builds only retail behavior. Plugins
and other non-retail code live inside `#if !defined(GBTG_RETAIL_GAME_ONLY)`.

**`GBTG_OPTIMIZE`**: enables the host presenter optimizations. Output matches
the default path.

**`GBTG_INSPECTOR_APP`**: defined when the engine is compiled into the
separate `gbtg-inspector`.

**Rare Replay**: the plugin that ticks the game at 60 Hz, as the Rare Replay
release does, instead of retail's 30 Hz. See [Plugins](PLUGINS.md).
