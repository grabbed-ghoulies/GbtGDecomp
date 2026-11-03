#pragma once
#include "actor_effect_nodes.h"

namespace gbtg
{
// An effect script command (0x000F0390): the opcode at +4, then its arguments. Opcodes share a few argument
// layouts, named below by shape. See docs/RETAIL_LAYOUTS.md.
namespace effect_command_field
{
constexpr RetailField<u32, 0x4> opcode{}; // signed; 2 ends the script, 7000..0x1F5C are effect commands
} // namespace effect_command_field

// 0x1B5B gravity, 0x1B65 spin, 0x1B67 size, 0x1B69 target size, 0x1B6B collision radius, 0x1B72 drag; 0x1B63 the
// life; 0x1B6D and 0x1B74..0x1B7D a mode word.
namespace value_command
{
constexpr RetailField<float, 0x8> value{};
constexpr RetailField<u32, 0x8> word{};
} // namespace value_command

// 0x1B5C, 0x1B64, 0x1B66, 0x1B68, 0x1B6E: a value drawn between low and high.
namespace range_command
{
constexpr RetailField<float, 0x8> low{};
constexpr RetailField<float, 0xC> high{};
} // namespace range_command

// 0x1B61, 0x1B62, 0x1B69: a colour, alpha or size reached over a duration drawn between the low and high.
namespace timed_command
{
constexpr RetailField<std::array<u8, 3>, 0x8> colour{}; // 0x1B61 reads the first byte as its alpha
constexpr RetailField<float, 0xC> duration_low{};
constexpr RetailField<float, 0x10> duration_high{};
} // namespace timed_command

// 0x1B59: the five channel values, one byte each.
namespace colour_command
{
constexpr RetailField<std::array<u8, 5>, 0x8> values{};
} // namespace colour_command

// 0x1B5A: the five channel values drawn between two sets of bytes.
namespace colour_range_command
{
constexpr RetailField<std::array<u8, 4>, 0x8> low{};
constexpr RetailField<std::array<u8, 4>, 0xC> high{};
constexpr RetailField<u8, 0x10> fifth_low{};
constexpr RetailField<u8, 0x11> fifth_high{};
} // namespace colour_range_command

// 0x1B6A: the particle faces its velocity (motion flags 2 and 8).
namespace face_command
{
constexpr RetailField<u32, 0x8> faces{};
constexpr RetailField<u32, 0xC> faces_fully{};
} // namespace face_command

// 0x1B6C (an oscillation) and 0x1B73 (bounded channels): lower and upper limits, a random start, a start and rates.
namespace channels_command
{
constexpr RetailField<Vec3, 0x08> lower{};
constexpr RetailField<Vec3, 0x14> upper{};
constexpr RetailField<u32, 0x20> random_start{};
constexpr RetailField<Vec3, 0x24> start{};
constexpr RetailField<Vec3, 0x30> rates{};
} // namespace channels_command

// 0x1B6F: the spin drawn per axis between lower and upper (degrees for y and z).
namespace spin_range_command
{
constexpr RetailField<Vec3, 0x08> lower{};
constexpr RetailField<Vec3, 0x14> upper{};
} // namespace spin_range_command

// 0x1B70: the bounce factors.
namespace bounce_command
{
constexpr RetailField<float, 0x8> bounce{};
constexpr RetailField<float, 0xC> unk_0C{}; // stored at the particle's +0x48 (its size)
} // namespace bounce_command

bool execute_actor_effect_commands(std::span<u8> particle, ActorEffectNode& owner, u32& cursor,
    const std::function<std::span<const u8>(u32)>& resolve, u32& random,
    const i32& pal, const i32& override_pal); // Xbox 0x000F0390
}
