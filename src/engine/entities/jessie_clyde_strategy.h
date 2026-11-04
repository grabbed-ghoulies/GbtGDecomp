#pragma once
#include <array>
#include "strategy_fields.h"
#include "spider_strategy.h"
#include "actor_attachment_events.h"
#include "body_attachment_cleanup.h"

namespace gbtg
{
struct RuntimeActorResourceReference;

// entityStrategyJessieClyde (factory type 0x4A, getter 0x000B9440, vtable 0x00505F14; the factory table's row 0x49
// carries the name) over the base strategy (type 8): Jessie Clyde. She starts calm (+0x40): she walks about
// ("randomwalk") and, struck or with her target in view and near, transforms (+0xF0 step 1, "transform", her effect
// from parameters +0x18 and every body latch up). The transformation's end (step 2) leaves her angry: the effect and
// latches go, her flower with them ("noflower" at body +0x288), and the body's bit block byte 1 bit 7 makes her body
// slots (0x00075A70 / 0x00075AA0) pick actions 2 and 3. Angry, she counts down a cue (+0x44), then shouts (0x3ED,
// whose animation event scares the room through 0x0013B270), knocks down (0x3EE) or prepares her special (0x3EF).
struct JessieClydeStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_003031B8, the block her tunables come from
    u32 open = 0; // +0x3C, each tick: provoked (actor +0x978 / +0x969), or her target is no player in view
    u32 calm = 0; // +0x40, set by the constructor; the transformation's end (0x000B83A0 step 2) clears it
    float cue = 0.f; // +0x44, seconds before an angry attack; state 0x401 starts it at the block's +0
    u32 struck = 0; // +0x48, message 0x6E while calm, or state 0x403
    std::array<char, 0x80> effect_name{}; // +0x4C, parameters +0x18
    float special_time = 0.f; // +0xCC, the special's seconds, the block's +0x14..+0x18
    u32 unk_D0 = 0; // +0xD0, set entering the special; 0x000B91F0 and 0x000B9260 clear it
    u32 unk_D4 = 0; // +0xD4, message 0x6E from her target during the special ends it in 0x3F2
    u32 unk_D8 = 0; // +0xD8, set entering the special; 0x000B91F0 clears it
    u32 special_effect = 0; // +0xDC, the special's effect (+0x13C) and latches are up
    u32 special_step = 0; // +0xE0, 1 preparing ("specialprepare"), 2 the special itself
    u32 shout_step = 0; // +0xE4, 1 shouting ("shout"), 2 the shout's scare made
    u32 follow_up = 0; // +0xE8, message 0x57: the knockdown ends in 0x3F2, not 0x401
    u32 knockdown_effect = 0; // +0xEC, the knockdown's effect (+0x13C) and latches are up
    u32 transform_step = 0; // +0xF0, 0x000B83A0: 0 calm, 1 transforming, 2 transformed
    std::array<u8, 0x24> events{}; // +0xF4, callout 0x11's tokens (0x0013B8B0)
    std::array<u8, 0x24> other_events{}; // +0x118, released by the destructor
    u32 effect = 0; // +0x13C, the current effect object; message 0x14 forgets it
};
static_assert(sizeof(JessieClydeStrategyState) == 0x140);
struct RuntimeJessieClydeStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    // DAT_003031B8: +0 the cue, +4 the attack reach, +0xC message 0x77's reach, +0x10 the reach she sees in, +0x14 /
    // +0x18 the special's seconds, +0x1C / +0x20 the shout's scare intensity and scale, +0x24 / +0x28 its forward and
    // up offsets.
    std::span<const u8> parameters;
    u32 parameters_address; // 0x003031B8, kept at +0x38.
    std::function<u32(const char*, u32)> create_effect; // 0x00107720 / 0x00107520 with the actor at +8.
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    const RuntimeActorResourceReference& resource; // Actor +0x8C, the callouts' record.
    ActorAttachmentEventContext& callouts;
    RuntimeBodyAttachmentLists& lists;
    std::function<void(u32, u8)> model_nodes; // 0x000F96F0 on the +0x204 and +0x310 models (stack slot, AL value).
    // 0x0013B270: a scare at a point (EAX), by the actor, with an intensity, scale, broadcast flag and value.
    std::function<void(const Vec3&, u32, float, float, i32, u32)> scare_at;
    const char* goal_random_walk; // "randomwalk", 0x003216C4.
    const char* goal_angry_random_walk; // "angryrandomwalk", 0x0032148C.
    const char* goal_transform; // "transform", 0x003217CC.
    const char* goal_shout; // "shout", 0x0031CC24.
    const char* goal_knockdown; // "knockdown", 0x003217C0.
    const char* goal_special_prepare; // "specialprepare", 0x00321474.
    const char* goal_special; // "special", 0x00321484.
};
void jessie_clyde_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B9420
bool initialize_runtime_jessie_clyde_strategy(std::span<const u8> parameters,
    RuntimeJessieClydeStrategy& context); // Xbox 0x000B9490
void destroy_runtime_jessie_clyde_strategy(RuntimeJessieClydeStrategy& context); // Xbox 0x000B95A0
void update_runtime_jessie_clyde_strategy(RuntimeJessieClydeStrategy& context); // Xbox 0x000B9630, vtable +0x10
void dispatch_runtime_jessie_clyde_message(std::span<const u32> message,
    RuntimeJessieClydeStrategy& context); // Xbox 0x000B9750, vtable +0xC
void enter_runtime_jessie_clyde_state(u16 state, u16 previous,
    RuntimeJessieClydeStrategy& context); // Xbox 0x000B9A20, vtable +0x14
void leave_runtime_jessie_clyde_state(u16 state, RuntimeJessieClydeStrategy& context); // Xbox 0x000B9AC0, vtable +0x18
}
