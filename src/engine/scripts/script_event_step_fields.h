#pragma once
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// An event step (0x88 bytes, a DAT_005064E0 pool row): what the event asks of the player. Script nodes 0x19..0x88
// build them and 0x000D05D0 queues them; 0x000D0B40 runs each by its type. The fields after +0x18 depend on the type.
// See docs/RETAIL_LAYOUTS.md.
namespace event_step_field
{
constexpr RetailField<u32, 0x00> type{};
constexpr RetailField<float, 0x04> delay{}; // the delay block's time left (event_step_delay_field)
constexpr RetailRecord<0x04, 0x0C> delay_block{}; // 0x000D0700's ECX
constexpr RetailField<u32, 0x10> icon{}; // the scoreboard message (0x0012FB80), 0 for none
constexpr RetailField<u32, 0x14> icon_shown{};
constexpr u32 kSize = 0x88;
} // namespace event_step_field

// The step's delay block (+4): waited out once the step is otherwise done (0x000D0700).
namespace event_step_delay_field
{
constexpr RetailField<float, 0x0> left{};
constexpr RetailField<u32, 0x4> started{};
constexpr RetailField<u32, 0x8> waits_for_cutscene{}; // a cutscene ran when it started: wait for it instead
} // namespace event_step_delay_field

// Type 2 (0x000CFDA0): hold while an actor of the tag lives.
namespace tag_event_step
{
constexpr RetailField<u16, 0x18> tag{};
constexpr RetailField<u32, 0x1C> seen{}; // one has lived (or none needs to)
} // namespace tag_event_step

// Type 3 (0x000CFE20): kill `required` of the tag.
namespace kill_count_event_step
{
constexpr RetailField<i32, 0x18> required{};
constexpr RetailField<u16, 0x1C> tag{};
} // namespace kill_count_event_step

// Types 4 (0x000CFE50, a time limit) and 5 (0x000CFEE0, survive): the seconds left.
namespace timed_event_step
{
constexpr RetailField<float, 0x18> seconds{};
} // namespace timed_event_step

// Type 6 (0x000CFFD0): done at once; its builder keeps a word from the script node.
namespace done_event_step
{
constexpr RetailField<u32, 0x18> unk_18{};
} // namespace done_event_step

// Type 7 (0x000CFF60): fail once the player's kills of the tag grow past the total kept on the first run.
namespace spare_tag_event_step
{
constexpr RetailField<u16, 0x18> tag{};
constexpr RetailField<i32, 0x1C> kept_total{}; // -1 until the first run
} // namespace spare_tag_event_step

// Type 8 (0x000CFFE0): "next one different": a kill of the current tag fails it.
namespace next_different_event_step
{
constexpr RetailField<u16, 0x18> current{}; // 0 until a tag is killed
constexpr RetailField<std::array<u16, 9>, 0x1A> tags{};
constexpr RetailField<std::array<i32, 8>, 0x2C> seen{}; // each tag's last kill total
constexpr RetailField<u8, 0x4C> count{};
} // namespace next_different_event_step

// Type 10 (0x000D00C0): fail once the player takes a kind-3 hit past the total kept on the first run.
namespace unhit_since_event_step
{
constexpr RetailField<i32, 0x18> kept_total{}; // -1 until the first run
} // namespace unhit_since_event_step

// Type 0xD (script node 0x50): one of the named tags, drawn at random.
namespace one_type_event_step
{
constexpr RetailField<u16, 0x18> tag{};
} // namespace one_type_event_step

// Type 0xE (0x000D0160): the number still to go against `target`.
namespace number_event_step
{
constexpr RetailField<i32, 0x18> target{};
} // namespace number_event_step

// Type 0xF (0x000CFAE0): the key carrier, then the key and its camera.
namespace key_carrier_event_step
{
constexpr RetailField<u32, 0x18> carrier_found{};
constexpr RetailField<u32, 0x1C> key_shown{};
constexpr RetailField<u32, 0x20> key{}; // the key object, by address
constexpr RetailField<u32, 0x84> camera_linked{}; // the +0x24 camera view is linked
} // namespace key_carrier_event_step

// Type 0x10 (0x000D01D0): fail once the attacks pass `limit`.
namespace attack_limit_event_step
{
constexpr RetailField<i32, 0x18> limit{};
} // namespace attack_limit_event_step

// Type 0x12 (0x000CFDF0, script node 0x88): kill `required` of the tag drawn at random.
namespace tag_kill_count_event_step
{
constexpr RetailField<u16, 0x18> tag{};
constexpr RetailField<i32, 0x1C> required{};
} // namespace tag_kill_count_event_step
} // namespace gbtg
