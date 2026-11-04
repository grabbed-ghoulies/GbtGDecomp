#pragma once
#include "gbtg/retail_field.h"
#include <array>
#include <span>
#include <type_traits>

namespace gbtg
{
// The actor base's (type 2, 0x934 bytes) raw fields that the native Actor doesn't keep. See docs/RETAIL_LAYOUTS.md.
namespace actor_field
{
constexpr RetailField<u16, 0x194> bone{}; // 0: the animation moves the root bone (0x0002C2B0)
constexpr RetailField<Vec3, 0x318> eased{}; // eased a tenth of the way toward `ease_target` each tick when easing
constexpr RetailField<Vec3, 0x324> ease_target{};
constexpr RetailField<float, 0x924> age{}; // seconds at the actor's rate
constexpr RetailField<u32, 0x928> ticks{};
constexpr u32 kSize = 0x934;
} // namespace actor_field

// The actor base's bytes at the start of a derived record. The native Actor keeps most of what's in them.
struct ActorBytes
{
    std::array<u8, actor_field::kSize> bytes{};
};

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
