#pragma once

#include "gbtg/types.h"
#include <functional>
#include <vector>

namespace gbtg
{
struct ActorLoadRecord
{
    ActorLoadRecord* prev = nullptr; // Xbox +0
    ActorLoadRecord* next = nullptr; // Xbox +4
    u8 flags_2c = 0;                 // Xbox +0x2C, bit 0 marks completion
};

using ActorLoadList = ActorLoadRecord;

// DAT_00510568/0051056C, also cleared by 0x000F4F30.
extern ActorLoadList g_side_list;

// One DAT_00511B38 actor as 0x001412F0 and 0x00132C70 read it through its +0x934 node: the front kind of its +0x980 goal ring
// (node +0x4C), and its +0x984 (node +0x50) and +0x98C (node +0x58) words.
struct RuntimeLoadingActor
{
    u16 tag = 0; // Object +6 (node -0x92E), which 0x00132C70 matches.
    u16 goal = 0;
    u32 word_984 = 0;
    u32 word_98c = 0;
    bool body = false; // Node +0x28 (actor +0x95C) set.
    u16 energy = 0; // Its +0x94.
};
// The native scene's DAT_00511B38 walk, in list order. Unset outside a native scene.
extern std::function<std::vector<RuntimeLoadingActor>()> g_loading_actors;
// DAT_00511B0C nonzero: actors wait on the scene's live list (DAT_00511B08) to be activated. Unset outside a
// native scene.
extern std::function<bool()> g_live_actors_pending;
std::vector<RuntimeLoadingActor> runtime_loading_actors(); // The native walk; raises with no scene bound.
bool actor_loading_complete(); // Xbox 0x001412F0
}
