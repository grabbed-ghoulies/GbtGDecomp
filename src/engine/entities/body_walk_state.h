#pragma once

#include "body_dizzy_state.h"
#include "body_state_fields.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// Factory type 0x8D, entityStateWalk (getter 0x000A4CF0, vtable 0x00505C80) over the body state (type 9): the body
// turns on the spot along an arc of the parameters' angle (+0x14..+0x18 degrees, either way at random) for +0x1C
// seconds, optionally with three objects (+0x20: the parameters' +0x24 / +0xA4 names) orbiting its head. It ends early
// when its strategy (actor +0x954) is gone or in state 1000, setting the parameters' body bit (+0xC) and taking the
// parameters' next action (+0x10), else 0x00026950's. +0x14 / +0x18 are the empty 0x001628B0 and +0x1C is
// 0x0002B620. State +8 is the body.
namespace walk_state_parameter
{
constexpr RetailField<u32, 0x0C> bit{}; // a body bit set at the end (0 for none)
constexpr RetailField<u16, 0x10> next{}; // the next action, or 0 for 0x00026950's
constexpr RetailField<float, 0x14> least_turn{}; // degrees
constexpr RetailField<float, 0x18> most_turn{};
constexpr RetailField<float, 0x1C> length{}; // seconds
constexpr RetailField<u32, 0x20> orbit{}; // set: three objects orbit the head
constexpr RetailField<std::array<char, 0x80>, 0x24> first_name{};
constexpr RetailField<std::array<char, 0x80>, 0xA4> second_name{};
constexpr RetailField<float, 0x124> size{}; // the objects' size, times the actor's +0x38 and 0.2
constexpr u32 kSize = 0x128;
static_assert(retail_fits(kSize, size));
} // namespace walk_state_parameter
namespace walk_state_field
{
constexpr RetailField<u32, 0x18> bit{};
constexpr RetailField<u16, 0x1C> next{};
constexpr RetailField<float, 0x20> start_yaw{}; // actor +0x18 at the start
constexpr RetailField<float, 0x24> turn{}; // radians over the whole walk
constexpr RetailField<float, 0x28> time{};
constexpr RetailField<float, 0x2C> length{};
constexpr RetailField<u32, 0x30> clockwise{}; // set: the yaw falls
constexpr RetailField<u32, 0x34> orbit{};
constexpr RetailField<float, 0x38> size{};
constexpr RetailField<std::array<char, 0x80>, 0x3C> first_name{};
constexpr RetailField<std::array<char, 0x80>, 0xBC> second_name{};
constexpr u32 kSlots = 0x13C; // three 0xC-byte slots: orbit angle, bob angle, the object's address
constexpr u32 kSize = 0x160;
} // namespace walk_state_field
struct RuntimeWalkStateContext
{
    std::span<u8> body;
    Actor& actor; // Body +8.
    RuntimeBodyBitStorage& bits; // Body +0x15C.
    const RuntimeActorClock& clock;
    std::span<const u8> strategy; // Actor +0x954, or empty.
    RuntimeDizzyStars& objects; // The orbiting objects' views (0x00107520 and the created actors).
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    const char* empty_name; // DAT_00546208.
    u32& random; // DAT_0054DD7C.
    std::function<u16()> primary_key; // 0x00026950 on the body.
};
void walk_state_object_sizes(ObjectSizes& output); // Xbox 0x000A4CD0
bool initialize_runtime_walk_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeWalkStateContext& context); // Xbox 0x000A4D50
void destroy_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context); // Xbox 0x000A5060
void tick_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context); // Xbox 0x000A5100
void message_runtime_walk_state(std::span<u8> state, std::span<const u32> message,
    RuntimeWalkStateContext& context); // Xbox 0x000A5270
// The body bit set and the next action taken.
void end_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context); // Xbox 0x000A5340, ESI state, EDI actor
// Each live object moves along its orbit by `step` (XMM0) times 5 degrees and is placed.
void orbit_runtime_walk_objects(std::span<u8> state, float step,
    RuntimeWalkStateContext& context); // Xbox 0x000A5390
// The object's position (ESI) and euler (EBX) for slot `index` (EDX) around the body (EAX).
void place_runtime_walk_object(std::span<const u8> state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler); // Xbox 0x000A54D0
// Type 0x8D (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_walk_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeWalkStateContext&(u32 body_address)> context_of);
}
