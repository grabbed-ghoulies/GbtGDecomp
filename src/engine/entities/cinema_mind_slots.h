#pragma once
#include "cinema_mind_lifecycle.h"
#include "cinema_mind_messages.h"
#include "cinema_mind_tick.h"
#include "cinema_mind_request_update.h"

namespace gbtg
{
// Required native contexts for shared cinema type 0x35; the base type 7 owns
// requests, events, storage and script resources in NativePlayerMindSlots.
struct NativeCinemaMindSlots
{
    RuntimeCinemaMindLifecycle& lifecycle;
    RuntimeCinemaMindMessages& messages;
    RuntimeCinemaMindTick& tick;
    RuntimeCinemaMindRequestUpdate& request_update;
    std::function<std::span<const u8>()> target_bytes;
};
}
