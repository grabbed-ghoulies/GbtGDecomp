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
// Strategy bytes: +0x1A attacking, +0x1C pause count.
namespace baron_plane_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x18> effect{};
constexpr RetailField<std::array<char, 0x80>, 0x98> sound{}; // the engine loop (sound slot 4)
constexpr RetailField<std::array<char, 0x80>, 0x118> cutscene{};
constexpr u32 kSize = 0x198;
static_assert(retail_fits(kSize, cutscene));
} // namespace baron_plane_parameter
namespace baron_plane_strategy_field
{
constexpr RetailField<u32, 0x38> hit_target{}; // the mind target was hit by him (message 0x6E); 0x3EE celebrates
constexpr RetailField<float, 0x40> hit_pause{}; // 1 s from a hit while the actor is paused; no flying while it runs
constexpr RetailField<float, 0x44> wait{}; // 0x40C's wait, between the energy row's +0 and +4
constexpr RetailField<u32, 0x48> dives{}; // dives left (the row's +8)
constexpr RetailField<u32, 0x4C> spawns{}; // landings left, 4 from the constructor
constexpr RetailField<u32, 0x50> knocked{}; // message 0x6E's +0xC: the knock-down lands him
constexpr RetailField<u32, 0x54> landed{}; // 0x404 told the script ({0xB9, tag}) once
constexpr RetailField<u32, 0x58> cutscene{}; // his cutscene (message 0x3B) plays
constexpr RetailField<u32, 0x5C> crowd_wait{}; // with other ghoulies out: 2 s (+0x60) before he moves on
constexpr RetailField<float, 0x60> crowd_time{};
constexpr RetailField<u32, 0x64> spinning{}; // 0x414 entered: the last spin
constexpr RetailField<u32, 0x68> effect_mode{}; // 0 off, 1 on, 2 by body bit 0x35
constexpr RetailField<float, 0x6C> linger{}; // 0.25 s the effect outlives a mode ending
constexpr RetailField<u32, 0x70> effect{}; // the effect object (0x0003DF40), or 0
constexpr RetailField<std::array<char, 0x80>, 0x74> effect_name{};
constexpr RetailField<u32, 0xF4> flying{}; // contact shapes 0x13 and the attack row are live (0x000A9BE0)
constexpr RetailField<u32, 0xF8> fly{}; // the state flies (entered with it set)
constexpr RetailField<u32, 0xFC> sounding{}; // the engine loop plays (0x000A9CF0)
constexpr RetailField<u32, 0x100> sound{}; // the state plays the engine loop
constexpr RetailField<std::array<char, 0x80>, 0x104> sound_name{};
constexpr RetailField<std::array<char, 0x80>, 0x184> cutscene_name{};
constexpr u32 kSize = 0x204;
static_assert(retail_fits(kSize, cutscene_name));
} // namespace baron_plane_strategy_field
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
