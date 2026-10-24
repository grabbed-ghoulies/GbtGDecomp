#pragma once
#include "actor_clock.h"
#include "../audio/event_voices.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"
#include <functional>
#include <span>

namespace gbtg
{
struct Actor;

// entityAvatarMiscGhoulyKey (factory type 0x18, getter 0x0006E950, vtable 0x00504D24; the factory table's row 0x17
// carries the name) over the actor (type 2): the key a key-carrying ghouly drops ("misc_ghoulykey"). It rises and
// spins, and its find-key ditty starts 0.4 s after it appears. Its message slot is the actor's (0x0006C9B0 jumps
// to 0x0003EF70).
namespace ghouly_key_field
{
constexpr RetailField<float, 0x934> ditty_delay{}; // Seconds to the find-key ditty
constexpr u32 kSize = 0x938;
static_assert(retail_fits(kSize, ditty_delay));
} // namespace ghouly_key_field
struct RuntimeGhoulyKey
{
    std::span<u8> bytes;
    Actor& actor;
    const RuntimeActorClock& clock;
    AudioEventVoices& voices; // DAT_004E0CE8.
    std::function<void(i32 id)> callouts; // 0x0003BE20 over the actor's +0x8C records, callback 0, no options.
};
void ghouly_key_object_sizes(ObjectSizes& output); // Xbox 0x0006E930
bool initialize_runtime_ghouly_key(RuntimeGhoulyKey& context); // Xbox 0x0006E990
void update_runtime_ghouly_key(RuntimeGhoulyKey& context); // Xbox 0x0006EA00 after 0x000420D0
}
