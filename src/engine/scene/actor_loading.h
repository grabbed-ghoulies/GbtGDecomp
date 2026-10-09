#pragma once

#include "gbtg/types.h"

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
bool actor_loading_complete(); // Xbox 0x001412F0
}
