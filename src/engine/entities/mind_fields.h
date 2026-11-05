#pragma once
#include "gbtg/retail_field.h"
#include <array>
#include <span>
#include "gbtg/types.h"

namespace gbtg
{
// The base mind's fields (0x7C bytes from 0x00029D10), shared by every mind type: the player, cinema and camera
// minds and the strategies' minds. Derived minds' own fields start at +0x7C (see docs/RETAIL_LAYOUTS.md).
// One of a mind target's two shapes (0x20 bytes).
struct MindTargetShape
{
    u32 kind = 0; // +0
    Vec3 position{}; // +4
    u32 unk_10 = 0; // +0x10
    float height = 0.f; // +0x14
    u32 unk_18 = 0; // +0x18
    float radius = 0.f; // +0x1C
};
static_assert(sizeof(MindTargetShape) == 0x20);

// A target record (0x68 bytes), in the target pool (DAT_00545988) or embedded in a player mind.
struct MindTargetRecord
{
    std::array<u32, 4> links{}; // +0, the pool's list words
    u32 actor = 0; // +0x10, the target actor
    // +0x14: 0 for an actor target; other kinds pick a bit of the mind's target categories (0x00029C80).
    u8 kind = 0;
    u8 hold = 0; // +0x15, held while nonzero (0x00042090 / 0x000420B0)
    // +0x16: The record's flags byte (the native record's u16 tag): 1 and 4 gate the target search (0x00129920), 8
    // makes the target require an action (0x001413C0), 0x10 picks the secondary shape's radius (0x00096260) and an
    // isolated approach (0x0007BC80).
    u8 flags = 0;
    u8 unk_17 = 0; // +0x17
    u8 unk_18 = 0; // +0x18, nonzero rejects the target (0x00029C80, 0x00129920)
    u8 floor = 0; // +0x19, the navigation floor the target stands on
    std::array<u8, 2> unk_1A{}; // +0x1A, padding
    // +0x1C: The record's two shapes (0x0012AFE0 copies them in).
    MindTargetShape shape{};
    MindTargetShape secondary_shape{}; // +0x3C
    // +0x5C: The native record's origin. Its readers use it as a direction: a throw leads its aim by it (0x000A4100),
    // and the Medusa (0x000BAE60) and the cinema mind (0x0007BC80) take it as the target's facing.
    Vec3 direction{};
};
static_assert(sizeof(MindTargetRecord) == 0x68);

struct MindRecord
{
    u32 vtable = 0; // +0
    u16 type = 0; // +4
    u16 unk_6 = 0; // +6
    // +0x8: The mind's own actor: the parameters' +0x9C (0x00029F20). Messages naming it (0x0007C8B0) and the camera's
    // yaw (0x000784B0) read it.
    u32 actor = 0;
    // +0xC: The actor the mind targets; the camera rig (0x000781F0) and the cinema tick (0x0007CE30) follow it, and
    // messages naming it clear it (0x0002A460).
    u32 target = 0;
    // +0x10: The target's record in the target pool (DAT_00545988) or a mind's own +0xB8 record; 0x00029C80 tests it.
    u32 target_record = 0;
    // +0x14: The record the current action is aimed at: 0x000295E0 copies it from +0x10, 0x00136830 sets it from the
    // request selection, and messages naming it clear it (0x0002A2E0).
    u32 action_record = 0;
    // +0x18: The actor the mind acts on: the weapon the look-for-weapon and pick-up states take (0x0009DB30,
    // 0x0009DF90), the nearest spawn 0x000295E0 picks, the cinema mind's movement goal (0x0007BF60). Messages naming it
    // clear it (0x0002A460).
    u32 action_actor = 0;
    u32 path_search = 0; // +0x1C, the path search block 0x00136700 allocates
    // +0x20: The target mode beside +0xC: 0x0007E6B0 sets 0 or 1 with the target, the cinema tick (0x0007CE30) 1 while
    // the target is within its radius.
    u8 target_mode = 0;
    std::array<u8, 0xB> unk_21{}; // +0x21, never read or written by translated code
    // +0x2C: Copied into each new or refreshed event's +0x14 (0x0002A190, 0x0002A280): a random value between the
    // parameters' +0x88 and +0x8C (0x00029F20).
    float unk_2C = 0.f;
    // +0x30: The model anchor the gaze follows. Message 0x85 naming it clears it with +0x34 and +0x38 (0x0002A2E0).
    u32 gaze_anchor = 0;
    // +0x34: Every mind tick adds the frame step to +0x34 (0x0002A9F0, 0x0007FB10, 0x0007CE30); a change of +0x20
    // raises it to +0x38 (0x0007E6B0).
    float unk_34 = 0.f;
    float unk_38 = 0.f; // +0x38
    // +0x3C: A point the mind is told to look at: set by the ghoul intro (0x000F4560) and the jump-on state
    // (0x00099AD0); 0x00137140 tests +0x3C.
    u32 gaze_point_set = 0;
    Vec3 gaze_point{}; // +0x40
    // +0x4C: The alert mode 0x0002A650 switches on (0x40 from 0x00029F20, up to 0x44); it sets body +0x158.
    u32 alert_mode = 0;
    float alert_timer = 0.f; // +0x50, counts down in 0x0002A650; messages add to it (0x0007C8B0)
    std::array<u8, 8> unk_54{}; // +0x54, never read or written by translated code
    // +0x5C: The last finished request's result (0x0002AA60); request kind 0xA (0x000295E0) compares it.
    u32 last_request_result = 0;
    std::array<u8, 4> unk_60{}; // +0x60, never read or written by translated code
    // +0x64: The target search's flags, categories and range: the parameters' +0x90, +0x94 and +0x98, the range
    // replaced by the actor class's row when it has one (0x00029A60); 0x00029AC0 and 0x00029C80 read them.
    u32 target_flags = 0;
    u32 target_categories = 0; // +0x68
    float target_range = 0.f; // +0x6C
    // +0x70: The point the mind moves toward: the strategy goals' candidate (0x0013A0F0), the camera's eye
    // (0x000781F0).
    Vec3 goal_point{};
};
static_assert(sizeof(MindRecord) == 0x7C);
// The cinema mind's own part (0x7C..0xD0, factory parameters 0x128) after the base mind.
struct CinemaMindRecord
{
    MindRecord base{}; // +0
    float roam_radius = 0.f; // +0x7C, the arrival radius with no target (target mode 0)
    float target_radius = 0.f; // +0x80
    u32 sound_resource = 0; // +0x84, by address, 0 for none
    u32 sound_count = 0; // +0x88, the resource's named sounds, up to three
    float sound_timer = 0.f; // +0x8C
    i32 notices = 0; // +0x90
    std::array<u8, 4> unk_94{}; // +0x94, never read or written by translated code
    float avoidance_time = 0.f; // +0x98, set: the mind is avoiding
    // +0x9C: Cleared when the mind starts: three pairs, the second of each 0, 1 and 1.
    u32 unk_9C = 0;
    u32 unk_A0 = 0; // +0xA0
    u32 unk_A4 = 0; // +0xA4
    u32 unk_A8 = 0; // +0xA8
    u32 unk_AC = 0; // +0xAC
    u32 unk_B0 = 0; // +0xB0
    u32 unk_B4 = 0; // +0xB4
    u32 unk_B8 = 0; // +0xB8
    u32 unk_BC = 0; // +0xBC
    u32 unk_C0 = 0; // +0xC0
    u32 unk_C4 = 0; // +0xC4
    u32 unk_C8 = 0; // +0xC8
    float unk_CC = 0.f; // +0xCC, counts down each tick
};
static_assert(sizeof(CinemaMindRecord) == 0xD0);

// A record's bytes viewed as its struct.
#define GBTG_MIND_VIEW(name, Record) \
    inline Record& name(std::span<u8> bytes) \
    { \
        detail::check_retail_range(bytes.size(), 0, sizeof(Record)); \
        return *reinterpret_cast<Record*>(bytes.data()); \
    } \
    inline const Record& name(std::span<const u8> bytes) \
    { \
        detail::check_retail_range(bytes.size(), 0, sizeof(Record)); \
        return *reinterpret_cast<const Record*>(bytes.data()); \
    }
GBTG_MIND_VIEW(mind_record, MindRecord)
GBTG_MIND_VIEW(cinema_mind_record, CinemaMindRecord)
GBTG_MIND_VIEW(mind_target_record, MindTargetRecord)
GBTG_MIND_VIEW(mind_target_shape, MindTargetShape)

// The cinema mind's factory parameters after the base mind's.
namespace cinema_mind_parameter
{
constexpr RetailField<float, 0xA0> roam_radius{};
constexpr RetailField<float, 0xA4> target_radius{};
constexpr RetailField<std::array<char, 0x80>, 0xA8> sound_resource{}; // an AID, or empty for none
constexpr u32 kSize = 0x128;
static_assert(retail_fits(kSize, sound_resource));
} // namespace cinema_mind_parameter
} // namespace gbtg
