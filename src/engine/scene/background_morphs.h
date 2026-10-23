#pragma once
#include "gbtg/types.h"

#include <vector>

namespace gbtg
{
// One pair of background parts that morph between each other (a kind 5 part, 0x00113620 adds it), 0x2C bytes in the
// DAT_0050E388 pool. Only the play flag is read by translated code so far.
struct RuntimeBackgroundMorph
{
    u8 playing = 0; // +0x28, 0 when added.
};
// DAT_0050E388: the scene's morph pairs. The scene load 0x00114180 makes the pool and 0x001153A0 fills it; neither is
// translated, so no scene has one yet.
struct RuntimeBackgroundMorphs
{
    std::vector<RuntimeBackgroundMorph> records;
};
// Every pair's +0x28 takes `playing` (BL); nothing happens without the pool.
void set_runtime_background_morphs_playing(RuntimeBackgroundMorphs* morphs, u8 playing); // Xbox 0x00114050
}
