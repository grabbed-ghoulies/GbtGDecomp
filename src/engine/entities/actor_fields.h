#pragma once
#include "gbtg/retail_field.h"
#include <array>
#include <span>
#include <type_traits>

namespace gbtg
{
// The actor base (type 2, 0x934 bytes) at the start of a derived record. The native Actor keeps most of what's in
// them; the fields named here are the ones it doesn't. See docs/RETAIL_LAYOUTS.md.
struct ActorBase
{
    std::array<u8, 0x194> unk_0{}; // +0, kept by the native Actor
    u16 bone = 0; // +0x194, 0: the animation moves the root bone (0x0002C2B0)
    std::array<u8, 0x182> unk_196{}; // +0x196, kept by the native Actor
    Vec3 eased{}; // +0x318, eased a tenth of the way toward `ease_target` each tick when easing
    Vec3 ease_target{}; // +0x324
    std::array<u8, 0x5F4> unk_330{}; // +0x330, kept by the native Actor
    float age = 0.f; // +0x924, seconds at the actor's rate
    u32 ticks = 0; // +0x928
    std::array<u8, 8> unk_92C{}; // +0x92C, kept by the native Actor
};
static_assert(sizeof(ActorBase) == 0x934);

// An actor-derived record's bytes viewed as its struct.
template <typename Record>
Record& actor_record_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<Record>);
    detail::check_retail_range(bytes.size(), 0, sizeof(Record));
    return *reinterpret_cast<Record*>(bytes.data());
}

template <typename Record>
const Record& actor_record_view(std::span<const u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<Record>);
    detail::check_retail_range(bytes.size(), 0, sizeof(Record));
    return *reinterpret_cast<const Record*>(bytes.data());
}
} // namespace gbtg
