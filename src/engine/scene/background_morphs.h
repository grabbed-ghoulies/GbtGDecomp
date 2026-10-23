#pragma once
#include "gbtg/types.h"

#include <functional>
#include <vector>

namespace gbtg
{
struct RuntimeBackgroundCloth;
// One pair of curtain halves (a kind 5 part, 0x00113620 adds it), 0x2C bytes in the DAT_0050E388 pool. While +0x28
// is 1 the pair draws open over 100 morph steps, while 2 it closes again; the morph then clears it.
struct RuntimeBackgroundMorph
{
    RuntimeBackgroundCloth* first = nullptr; // +0
    RuntimeBackgroundCloth* second = nullptr; // +4
    Vec3 center{}; // +8, the halves' centres averaged.
    std::vector<u32> first_handles; // +0x14: per row, the point two thirds along it (pinned).
    std::vector<u32> second_handles; // +0x18
    std::vector<u32> first_rail; // +0x1C: per row, its first point.
    std::vector<u32> second_rail; // +0x20
    i32 steps = 0; // +0x24, 0 to 100.
    u8 playing = 0; // +0x28, 0 when added.
};
// DAT_0050E388: the scene's curtain pairs. The scene load 0x00114180 makes the pool and 0x001153A0 fills it.
struct RuntimeBackgroundMorphs
{
    std::vector<RuntimeBackgroundMorph> records;
};
// Every pair's +0x28 takes `playing` (BL); nothing happens without the pool.
void set_runtime_background_morphs_playing(RuntimeBackgroundMorphs* morphs, u8 playing); // Xbox 0x00114050
// Adds the pair (EBX second): their handle and rail rows, the handles pinned, both settled and re-sorted.
void add_runtime_background_morph(RuntimeBackgroundMorphs* morphs, RuntimeBackgroundCloth& first,
    RuntimeBackgroundCloth& second, const std::function<void(RuntimeBackgroundCloth&)>& settle); // Xbox 0x00113620
// One morph step of every playing pair.
void step_runtime_background_morphs(RuntimeBackgroundMorphs* morphs); // Xbox 0x001137E0
}
