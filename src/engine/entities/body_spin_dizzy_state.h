#pragma once

#include "body_dizzy_state.h"
#include "body_state_fields.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x86 (getter 0x000A21F0, vtable 0x00505BA0) over the dizzy state (type 0x64, whose constructor and
// destructor run around this one's): the dizzy body is also pushed round a circle, its body +0x318 velocity the
// sine and cosine of an angle that turns with the dizzy time left (+0x58, here a random length from DAT_004286F0 ..
// DAT_004286F4), so the spin slows as the stars run out. Body +0x71C .. +0x724 take the parameters' values while it
// lasts and go back to DAT_0032B220 / DAT_0032B21C / DAT_0032B218. A sound (+0x11C) plays on actor slot 1. The +0x14 /
// +0x18 slots are the empty 0x001628B0 and +0x1C is 0x0002B620. State +8 is the body.
namespace spin_dizzy_state_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x11C> sound{}; // actor slot 1 for the state
constexpr RetailField<float, 0x19C> turn_rate{}; // the angle per second of dizzy time
constexpr RetailField<float, 0x1A0> reach_rate{}; // the radius per second of dizzy time
constexpr RetailField<float, 0x1A4> speed{};
constexpr RetailField<u32, 0x1A8> body_71c{};
constexpr RetailField<u32, 0x1AC> body_720{};
constexpr RetailField<u32, 0x1B0> body_724{};
constexpr u32 kSize = 0x1B4;
static_assert(retail_fits(kSize, body_724));
} // namespace spin_dizzy_state_parameter
namespace spin_dizzy_state_field
{
constexpr RetailField<u16, 0x0C> next_key{}; // message 0x27's 0x00026950
constexpr RetailField<float, 0x58> dizzy_left{}; // the dizzy state's time left
constexpr RetailField<u32, 0x164> first_star{}; // the dizzy state's three star actors
constexpr RetailField<u32, 0x170> second_star{};
constexpr RetailField<u32, 0x17C> third_star{};
constexpr RetailField<float, 0x184> turn_rate{};
constexpr RetailField<float, 0x188> reach_rate{};
constexpr RetailField<float, 0x18C> speed{};
constexpr RetailField<u32, 0x190> body_71c{};
constexpr RetailField<u32, 0x194> body_720{};
constexpr RetailField<u32, 0x198> body_724{};
// The circle's centre in the velocity; no dump writes it, so it keeps the factory's zero fill.
constexpr RetailField<Vec3, 0x19C> unk_19c{};
constexpr RetailField<u32, 0x1A8> sounding{};
constexpr u32 kSize = 0x1AC;
static_assert(retail_fits(kSize, sounding));
} // namespace spin_dizzy_state_field
struct RuntimeSpinDizzyStateContext
{
    RuntimeDizzyStateContext& dizzy;
    std::array<float, 2> length_range; // DAT_004286F0, DAT_004286F4
    std::array<u32, 3> rest_71c; // DAT_0032B220, DAT_0032B21C, DAT_0032B218: body +0x71C .. +0x724 afterwards
    std::function<u16()> primary_key; // 0x00026950 on the body.
};
void spin_dizzy_state_object_sizes(ObjectSizes& output); // Xbox 0x000A21D0
bool initialize_runtime_spin_dizzy_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSpinDizzyStateContext& context); // Xbox 0x000A2250
void destroy_runtime_spin_dizzy_state(std::span<u8> state, RuntimeSpinDizzyStateContext& context); // Xbox 0x000A2340
void tick_runtime_spin_dizzy_state(std::span<u8> state, RuntimeSpinDizzyStateContext& context); // Xbox 0x000A23A0
void message_runtime_spin_dizzy_state(std::span<u8> state, std::span<const u32> message,
    RuntimeSpinDizzyStateContext& context); // Xbox 0x000A24B0
// Type 0x86 (base 0x64) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_spin_dizzy_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSpinDizzyStateContext&(u32 body_address)> context_of);
}
