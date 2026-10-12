#pragma once
#include "cinema_mind_lifecycle.h"
#include "cinema_mind_messages.h"
#include "cinema_mind_tick.h"
#include "cinema_mind_request_update.h"

namespace gbtg
{
// Native contexts for shared cinema type 0x35; the base type 7 owns requests, events, storage and
// script resources in NativePlayerMindSlots. The lifecycle is required before construction; the
// method contexts (vtable +0xC through +0x14) are attached once the actor's method graph exists.
struct NativeCinemaMindSlots
{
    RuntimeCinemaMindLifecycle& lifecycle;
    RuntimeCinemaMindMessages* messages = nullptr;
    RuntimeCinemaMindTick* tick = nullptr;
    RuntimeCinemaMindRequestUpdate* request_update = nullptr;
    std::function<std::span<const u8>()> target_bytes;
};
}
