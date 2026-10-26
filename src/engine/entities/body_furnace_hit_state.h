#pragma once

#include "body_knockdown_state.h"
#include "body_state_actions.h"

namespace gbtg
{
// Factory type 0x75 entityStateHitFurnace (getter 0x00096A50, vtable 0x00505980; the factory table's row 0x74 carries
// the name) over the body state (type 9): a body a furnace caught (body +0x26C) is drawn into it. Its message, +0x14
// and +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620; the parameters (0xC bytes) are not read. Over
// one second (+0x48) the actor slides from where it stood to the furnace and turns to face away from it, then dies
// (0x00134AE0). State +8 is the body.
namespace furnace_hit_state_field
{
constexpr RetailField<float, 0x10> elapsed{}; // Seconds at the actor's rate; nothing reads it back.
constexpr RetailField<Vec3, 0x18> start{}; // The actor's world position on entry.
constexpr RetailField<Vec3, 0x24> start_euler{}; // Its world euler on entry.
constexpr RetailField<Vec3, 0x30> furnace{}; // The furnace's world position (body +0x26C).
constexpr RetailField<Vec3, 0x3C> facing{}; // The euler it turns to: yaw away from the furnace.
constexpr RetailField<float, 0x48> remaining{}; // 1.0 down to 0.
constexpr u32 kSize = 0x4C;
static_assert(retail_fits(kSize, remaining));
} // namespace furnace_hit_state_field
struct RuntimeFurnaceHitStateContext
{
    RuntimeKnockdownContext& knockdown; // Body, actor, bits, clock, attachment, death and callout views.
    RuntimeBodyStateContext& state; // Actor +0x310 / +0x30C, body +0x4C and +0x14, and the body's vtable +0xC.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
};
void furnace_hit_state_object_sizes(ObjectSizes& output); // Xbox 0x00096A30
bool initialize_runtime_furnace_hit_state(std::span<u8> state,
    RuntimeFurnaceHitStateContext& context); // Xbox 0x00096AB0
void destroy_runtime_furnace_hit_state(std::span<u8> state,
    RuntimeFurnaceHitStateContext& context); // Xbox 0x00096E60
void tick_runtime_furnace_hit_state(std::span<u8> state,
    RuntimeFurnaceHitStateContext& context); // Xbox 0x00096FC0
// Type 0x75 (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_furnace_hit_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFurnaceHitStateContext&(u32 body_address)> context_of);
}
