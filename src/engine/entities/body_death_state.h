#pragma once
#include <array>

#include "body_knockdown_state.h"
#include "body_state_fields.h"

namespace gbtg
{
struct LiveScript;
// Factory type 0x63, entityStateDie (getter 0x0008C5A0, vtable 0x00505740; the factory table's row 0x62 carries the
// name) over the body state (type 9): a ghouly's death. The body takes every latch and its dead counters, drops the
// mind's target and the mind's type 3 event, and switches shape (the parameters' +0x10 mode); once its death
// animation has played and a second more has passed, 0x00134AE0 finishes it (drops, death effect, notices). The
// message and the +0x14 / +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body. (The
// port's type 0x62, `body_die_state`, is row 0x61's entityStateCheer.)
namespace death_state_parameter
{
constexpr RetailField<u32, 0x0C> release_attachments{}; // the actor's live attachments (+0x8F8) are released
constexpr RetailField<u8, 0x10> shape_mode{}; // 0x0001FEB0's mode while dying
constexpr u32 kSize = 0x14;
static_assert(retail_fits(kSize, shape_mode));
} // namespace death_state_parameter
struct DeathState
{
    BodyStateBase base{}; // +0
    u32 waiting = 0; // +0x18, 0 while the death animation plays, 1 for the second after it
    float delay = 0.f; // +0x1C, seconds left before 0x00134AE0
    u8 shape_mode = 0; // +0x20, the shape mode 0x0001FEB0 replaced, restored on release
    std::array<u8, 7> unk_21{}; // +0x21, never read or written by translated code
};
static_assert(sizeof(DeathState) == 0x28);
struct RuntimeDeathStateContext
{
    RuntimeKnockdownContext& knockdown; // Body, actor, bits, animation, shape, live attachments and death effects.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::span<u8> mind; // Actor +0x958; empty when the actor has none.
    LiveScript* mind_events; // Mind +0x24's anchor; its next (mind +0x28) is the first event.
};
void death_state_object_sizes(ObjectSizes& output); // Xbox 0x00076BC0
bool initialize_runtime_death_state(DeathState& state, std::span<const u8> parameters,
    RuntimeDeathStateContext& context); // Xbox 0x0008C600
void destroy_runtime_death_state(DeathState& state, RuntimeDeathStateContext& context); // Xbox 0x0008C750
void tick_runtime_death_state(DeathState& state, RuntimeDeathStateContext& context); // Xbox 0x0008C7E0
// Type 0x63 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_death_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDeathStateContext&(u32 body_address)> context_of);
}
