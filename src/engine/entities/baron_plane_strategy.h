#pragma once
#include "body_contacts.h"
#include "body_attachment_requests.h"
#include "player_state.h"
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "strategy_fields.h"
#include "gbtg/sound_runtime.h"
#include <array>

namespace gbtg
{
struct SceneCutsceneRuntime;
// entityStrategyBaronPlane (factory type 0x38, getter 0x000A8C10, vtable 0x00505D1C; its name string follows the
// "entityStrategyBaronPlane" that row 0x37 names) over the base strategy (type 8): the Baron in his plane. He waits
// (0x40C) a time set by his energy and whether other ghoulies are out, then dives (0x3EE) a number of times, flying
// with his contact shapes (0x13) and the attack row 4 live and his engine sound looping. Knocked down (0x409) with
// spawns left (+0x4C, four), he lands at the nearest kind-0x1B marker (0x404), the scene paused, the script hears
// {0xB9, his tag} and the next wave of ghoulies comes; at 60 energy or less with none left he spins (0x414). His
// effect (the parameters' +0x18 object, holding all six body latches) is on while he waits and lands, and by body
// bit 0x35 when his energy is low. Message 0x3B starts his cutscene (the parameters' +0x118), which follows him.
namespace baron_plane_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> effect{};
constexpr RetailField<std::array<char, 0x80>, 0x98> sound{}; // the engine loop (sound slot 4)
constexpr RetailField<std::array<char, 0x80>, 0x118> cutscene{};
constexpr u32 kSize = 0x198;
static_assert(retail_fits(kSize, cutscene));
} // namespace baron_plane_parameter
struct BaronPlaneStrategyState
{
    StrategyBase base{}; // +0
    u32 hit_target = 0; // +0x38, the mind target was hit by him (message 0x6E); 0x3EE celebrates
    std::array<u8, 4> unk_3C{}; // +0x3C, never read or written by translated code
    float hit_pause = 0.f; // +0x40, 1 s from a hit while the actor is paused; no flying while it runs
    float wait = 0.f; // +0x44, 0x40C's wait, between the energy row's +0 and +4
    u32 dives = 0; // +0x48, dives left (the row's +8)
    u32 spawns = 0; // +0x4C, landings left, 4 from the constructor
    u32 knocked = 0; // +0x50, message 0x6E's +0xC: the knock-down lands him
    u32 landed = 0; // +0x54, 0x404 told the script ({0xB9, tag}) once
    u32 cutscene = 0; // +0x58, his cutscene (message 0x3B) plays
    u32 crowd_wait = 0; // +0x5C, with other ghoulies out: 2 s (+0x60) before he moves on
    float crowd_time = 0.f; // +0x60
    u32 spinning = 0; // +0x64, 0x414 entered: the last spin
    u32 effect_mode = 0; // +0x68, 0 off, 1 on, 2 by body bit 0x35
    float linger = 0.f; // +0x6C, 0.25 s the effect outlives a mode ending
    u32 effect = 0; // +0x70, the effect object (0x0003DF40), or 0
    std::array<char, 0x80> effect_name{}; // +0x74
    u32 flying = 0; // +0xF4, contact shapes 0x13 and the attack row are live (0x000A9BE0)
    u32 fly = 0; // +0xF8, the state flies (entered with it set)
    u32 sounding = 0; // +0xFC, the engine loop plays (0x000A9CF0)
    u32 sound = 0; // +0x100, the state plays the engine loop
    std::array<char, 0x80> sound_name{}; // +0x104
    std::array<char, 0x80> cutscene_name{}; // +0x184
};
static_assert(sizeof(BaronPlaneStrategyState) == 0x204);
struct RuntimeBaronPlaneStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyActorViews& views;
    RuntimeBodyCollisionState*& collision; // Body +0x2C8.
    std::span<const u8>& attack_resource; // Body +0x5C.
    const std::unordered_map<const u8*, const RuntimeAttachmentRequestTables*>& attack_tables;
    RuntimeActorSoundQueues& sounds;
    RuntimeActorPauseGroups& pause; // 0x00117400.
    entity_detail::LatchNode*& latches; // DAT_00511AA4.
    std::function<u32(const void*)> address_of; // Retail address of a native object or resource.
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40 under scene ownership; object address or 0.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    std::function<SceneCutsceneRuntime&()> cutscenes; // DAT_004E1B40..
    std::function<const u8*()> markers; // DAT_0051108C.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_wait_for_stand; // "waitforstand", 0x003217E0.
    const char* goal_far; // "far", 0x00321824.
    const char* goal_land; // 0x003217F0, loaded by 0x000A9D40.
    const char* goal_celebrate; // "celebrate", 0x0031B548.
    const char* goal_spin; // "spin", 0x0032102C.
};
void baron_plane_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000A8BF0
bool initialize_runtime_baron_plane_strategy(std::span<u8> strategy, std::span<const u8> parameters); // Xbox 0x000A8C60
void tick_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A88C0, EAX strategy
void set_runtime_baron_plane_effect_mode(u32 mode, RuntimeBaronPlaneStrategy& context); // Xbox 0x000A8960, ECX, EDX mode
void end_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A89C0, EAX body, EDI strategy
void start_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A8AD0, EAX body, ECX strategy
// The energy row: {wait minimum, wait maximum, dives, message 0x58's answer}, from 0x00302AA0 with other ghoulies
// out or 0x00302AF0 alone, by body +0x94 (above 120, 100, 80; 61 and up; less, and 0x00302AA0's last row alone).
const std::array<float, 4>& runtime_baron_plane_row(std::span<const u8> body,
    RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9AA0, stack body
bool runtime_baron_plane_crowded(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9B20: two DAT_00511B48 actors live
bool runtime_baron_plane_can_fly(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9B50, EAX strategy, ECX body
bool runtime_baron_plane_can_sound(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9BA0, EAX body
void start_runtime_baron_plane_flight(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9BE0, EAX strategy, ESI body
void end_runtime_baron_plane_flight(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9C90, EBX strategy, ESI body
void start_runtime_baron_plane_sound(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9CF0, EAX strategy, ECX actor
// The mind walks to the kind-0x1B marker nearest the actor (its +0xC position) with goal 0x003217F0.
void land_runtime_baron_plane(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9D40, EAX actor, EBX mind
void follow_runtime_baron_plane_cutscene(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9DB0, EAX actor
bool wait_runtime_baron_plane_crowd(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9EA0, EAX strategy, EDX actor
bool runtime_baron_plane_spin_ready(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9F90, EAX strategy, ECX body
void update_runtime_baron_plane_strategy(RuntimeBaronPlaneStrategy& context); // Xbox 0x000A8CD0, vtable +0x10
void dispatch_runtime_baron_plane_message(std::span<u32> message,
    RuntimeBaronPlaneStrategy& context); // Xbox 0x000A91A0, vtable +0xC
void enter_runtime_baron_plane_state(u16 state, u16 previous,
    RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9430, vtable +0x14
void leave_runtime_baron_plane_state(u16 state, RuntimeBaronPlaneStrategy& context); // Xbox 0x000A9980, vtable +0x18
// The marker with `id` gives its +0xC position; none leaves `position` as it is.
void runtime_marker_position(u16 id, Vec3& position, const u8* records); // Xbox 0x0011F260, DI id, ESI position
}
