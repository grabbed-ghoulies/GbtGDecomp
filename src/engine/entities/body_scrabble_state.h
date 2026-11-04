#pragma once
#include <array>

#include "actor_clock.h"
#include "body_damage_tables.h"
#include "body_state_fields.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x7F, entityStateScrabble (getter 0x0009F060, vtable 0x00505AC0) over the body state (type 9): the
// body runs at the mind's goal point (mind +0x70) for two seconds, its animation sped from 0.25 to 0.7 of its length
// as the time runs out, then hands over its next action. The constructor swaps body +0x334 for 0.2 (kept at +0x330
// and restored by the destructor) and raises body +0x338. The sizes are 0x0008E740's (0x28 / 0xC). The message and
// the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
struct ScrabbleState
{
    BodyStateBase base{}; // +0
    std::array<u8, 4> unk_18{}; // +0x18, never read or written by translated code
    float time_left = 0.f; // +0x1C
    float duration = 0.f; // +0x20, 2 s (a random fraction of the 0 at DAT_0032B100, plus 2)
    std::array<u8, 4> unk_24{}; // +0x24, never read or written by translated code
};
static_assert(sizeof(ScrabbleState) == 0x28);
struct RuntimeScrabbleStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<const u8> actor_bytes;
    std::span<const u8> mind; // Actor +0x958; +0x70 the goal point.
    ActorAnimationState& animation; // Actor +0x50.
    const RuntimeActorClock& clock;
    const RuntimeBodyDamageTables& damage_tables; // 0x000259C0's tables.
    std::span<const float> mode_scales; // DAT_00217EC0.
    u32& random; // DAT_0054DD7C.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void scrabble_state_object_sizes(ObjectSizes& output); // Xbox 0x0008E740
bool initialize_runtime_scrabble_state(ScrabbleState& state, RuntimeScrabbleStateContext& context); // Xbox 0x0009F0C0
void destroy_runtime_scrabble_state(RuntimeScrabbleStateContext& context); // Xbox 0x0009F150
void tick_runtime_scrabble_state(ScrabbleState& state, RuntimeScrabbleStateContext& context); // Xbox 0x0009F170
// Type 0x7F (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_scrabble_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeScrabbleStateContext&(u32 body_address)> context_of);
}
