#pragma once
#include <array>
#include "strategy_fields.h"
#include "spider_strategy.h"
#include "body_action_selection.h"
#include "body_contacts.h"
#include "body_attachment_requests.h"
#include "../graphics/light_cones.h"
#include "gbtg/sound_runtime.h"
#include <unordered_map>

namespace gbtg
{
// entityStrategyMedusa (factory type 0x4B, getter 0x000BB1D0, vtable 0x00505F30; the factory table's row 0x4A
// carries the name) over the base strategy (type 8): the Medusa. She walks at random (0x401) until her
// special-attack timer (+0x150, drawn from the attributes' first two floats) runs out, then gazes (0x3EF): two light
// cones leave her eyes (joints 7 and 6) and, while the body's bit 0x2D is set, a ray along the gaze that reaches her
// target starts the Super Scary challenge (0x000873A0) and hits it (message 0x15). Close to a target she attacks
// ("attack", walk mode 2); a dizzy spell (0x409) and the shock camera messages (0x8E) come from outside.
struct MedusaStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, DAT_004287F0
    std::array<char, 0x80> challenge_first{}; // +0x3C, parameters +0x18
    std::array<char, 0x80> challenge_second{}; // +0xBC, parameters +0x98
    u32 walk_mode = 0; // +0x13C, 1 walking at random, 2 attacking
    float attack_delay = 0.f; // +0x140, seconds before another attack (parameters +0x1C)
    u32 gaze_phase = 0; // +0x144, 1 starting, 2 facing, 3 walking, 4 spinning, 5 over
    float spin_delay = 0.f; // +0x148, parameters +0xC
    float face_delay = 0.f; // +0x14C, parameters +0x14
    float special_delay = 0.f; // +0x150, seconds to the next gaze
    float gaze_time = 0.f; // +0x154, the gaze's length, parameters +4..+8
    u32 gazed = 0; // +0x158, set by the gaze's start; 0x3F0 ends it
    u32 gaze_hit = 0; // +0x15C, the ray reached the target
    u32 rattle_sound = 0; // +0x160, actor +0x614's sound plays
    u32 unk_164 = 0; // +0x164, actor +0x83C's sound plays
    u32 always_special = 0; // +0x168, the attributes' +8 is 1
    u32 gaze_toggle = 0; // +0x16C, flips each ray test
    i32 first_cone = 0; // +0x170, the joint 7 cone, or -1
    i32 second_cone = 0; // +0x174, the joint 6 cone, or -1
    u32 shocked = 0; // +0x178, message 0x8E's shock camera holds bit 0x3E and latch mask 0x3F
    u32 unk_17C = 0; // +0x17C
    u32 unk_180 = 0; // +0x180
    u32 dizzy_step = 0; // +0x184, state 0x409: 1 waits for body bit 0x30, 2 recovering
    u32 attack_request = 0; // +0x188, body +0x84 holds the 0xF8 attack row (0x000BA010)
    u32 shock_won = 0; // +0x18C, message 0x8E's word 7 sets it, 8 clears it
};
static_assert(sizeof(MedusaStrategyState) == 0x190);
struct RuntimeMedusaStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    std::span<const u8> parameters; // DAT_004287F0, 0x58 bytes.
    RuntimeLightCones*& cones; // DAT_0050EF20, set once the scene has them.
    std::span<const u8>& attack_resource; // Body +0x5C.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    std::function<u32(const void*)> address_of;
    RuntimeActorSoundQueues& sounds;
    RuntimeBodyActionSelection& actions; // 0x0001AF10.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<float()> attachment_radius; // 0x0001D3D0 on the body.
    std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor;
    std::function<void(RuntimeContactReport&)> report; // Message 0x41 through the actor's vtable +0xC.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520.
    u32& runtime_flags; // DAT_0050EA9C.
    const char* goal_special_attack; // "specialattack", 0x003216B4.
    const char* goal_spin; // "spin", 0x0032102C.
    const char* goal_special_walk; // "specialattackwalk", 0x0032155C.
    const char* goal_special_succeed; // "specialattacksucceed", 0x00321570.
    const char* goal_back_to_normal; // "backtonormal", 0x003215A0.
    const char* goal_random_walk; // "randomwalk", 0x003216C4.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* shock_camera_first; // "shockcam1", 0x0031CF88.
    const char* shock_camera_second; // "shockcam2", 0x0031CF7C.
};
void medusa_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000BB1B0
bool initialize_runtime_medusa_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeMedusaStrategy& context); // Xbox 0x000BB220
void update_runtime_medusa_strategy(RuntimeMedusaStrategy& context); // Xbox 0x000BB340, vtable +0x10
void dispatch_runtime_medusa_message(std::span<const u32> message,
    RuntimeMedusaStrategy& context); // Xbox 0x000BB400, vtable +0xC
void enter_runtime_medusa_state(u16 state, u16 previous, RuntimeMedusaStrategy& context); // Xbox 0x000BB720
void leave_runtime_medusa_state(u16 state, RuntimeMedusaStrategy& context); // Xbox 0x000BB7D0, vtable +0x18
// Body +0x90 / +0x92 to 2, +0x91 cleared and +0x8C a fresh random fraction.
void reset_runtime_body_flinch(std::span<u8> body, u32& random); // Xbox 0x0001D9D0, ECX body
// The angle between two points' horizontal parts, from 0 to pi; 0 when either is too short.
float runtime_horizontal_angle_between(const Vec3& first, const Vec3& second); // Xbox 0x00106140, ESI first, EDI second
}
