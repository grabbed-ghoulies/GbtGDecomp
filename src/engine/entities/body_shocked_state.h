#pragma once
#include <array>

#include "actor_clock.h"
#include "body_hit_state.h"
#include "player_attachment.h"
#include "body_state_fields.h"
#include "gbtg/anim.h"

#include <functional>

namespace gbtg
{
// Factory type 0x80, entityStateShocked (getter 0x0009F420, vtable 0x00505AE0) over the body state (type 9): the
// body drops what it holds and freezes with every latch held while the parameters' effect (+0xC, made with the actor
// at +8) crackles on it, then recovers (0x00133F50) once its one-shot animation has played. The message and the
// +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace shocked_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> effect{};
constexpr u32 kSize = 0x8C;
static_assert(retail_fits(kSize, effect));
} // namespace shocked_state_parameter
struct ShockedState
{
    BodyStateBase base{}; // +0
    u32 effect = 0; // +0x18, the effect object, or 0 when its template was not found
};
static_assert(sizeof(ShockedState) == 0x1C);
struct RuntimeShockedStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    u32 actor_address;
    EntityBitBlock& bits; // Body +0x15C.
    RuntimePlayerAttachment& attachment; // 0x0001B810's views.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const RuntimeActorClock& clock;
    const ActorAnimationState& animation; // Actor +0x50.
    std::span<ExecutableResourcePool> resources;
    std::function<u32(std::span<const u8> parameters)> create_object; // 0x00107520 into the scene's objects.
    std::function<bool(Actor& actor, u32 object)> release_frame_object; // 0x0003DE30, ECX actor, ESI object.
    RuntimeBodyRecoveryAction& recovery; // 0x00133F50's views.
};
void shocked_state_object_sizes(ObjectSizes& output); // Xbox 0x0009F400
bool initialize_runtime_shocked_state(ShockedState& state, std::span<const u8> parameters,
    RuntimeShockedStateContext& context); // Xbox 0x0009F480
void destroy_runtime_shocked_state(ShockedState& state, RuntimeShockedStateContext& context); // Xbox 0x0009F550
void tick_runtime_shocked_state(std::span<u8> state, RuntimeShockedStateContext& context); // Xbox 0x0009F5F0
// Type 0x80 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_shocked_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeShockedStateContext&(u32 body_address)> context_of);
}
