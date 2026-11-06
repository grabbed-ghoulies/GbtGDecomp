#pragma once
#include "strategy_fields.h"
#include "strategy_target_queue.h"
#include "strategy_target_selection.h"
#include "player_state.h"

namespace gbtg
{
// entityStrategyImpFire (type 0x46, vtable 0x00505EA4) over the base strategy (type 8).
struct ImpFireStrategyState
{
    StrategyBase base;
    float attack_delay = 0.f; // +0x38
    u32 held_body = 0; // +0x3C, the held body's address
    u32 effect_state = 0; // +0x40
    u32 effect = 0; // +0x44, the effect object's address
    u32 bite_request = 0; // +0x48
    u32 parameters = 0; // +0x4C, the parameter block's address
};
static_assert(sizeof(ImpFireStrategyState) == 0x50);
struct RuntimeImpFireStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    RuntimeStrategyTargetChecks& checks;
    RuntimeStrategyTargetQueue& queue;
    std::span<const u8> parameters; // DAT_004287BC, the block strategy +0x4C names.
    ActorAnimationState& animation; // Actor +0x48; actor +0x50 is its controller.
    AnimationResourceRegistry& registry;
    std::function<u32(const char*, u32)> create_effect; // 0x0003DF40 under scene ownership; object address or 0.
    std::function<void(u32, std::span<const u32>)> object_message; // Actual object vtable +0xC.
    std::function<RuntimeStrategyBodyView(u32)> body_at; // Body named by strategy +0x3C.
    std::function<RuntimeStrategyBodyView(const Actor&)> body_of; // Actor +0x95C.
    std::function<const RuntimeBodyModelNodes*(const Actor&)> model_of; // Actor +0x204.
    const i32& head_marker_index; // DAT_00428684.
    const char* effect_name; // "aid_objparams_ghoulies_fx_mutatehalo", 0x00303058.
    const char* goal_attack; // "attack", 0x003211E4.
    const char* goal_mutate; // "mutateup", 0x0032164C.
    const char* goal_bite; // "jumpbite", 0x00321658.
    const char* goal_bite_prepare; // "jumpbiteprepare", 0x0031D15C.
};
void imp_fire_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000B0B90
void set_runtime_imp_fire_effect(u32 state, RuntimeImpFireStrategy& context); // Xbox 0x000B43B0, ESI strategy +0x40
void end_runtime_imp_fire_effect(RuntimeImpFireStrategy& context); // Xbox 0x000B4380, EAX strategy +0x40
bool runtime_imp_fire_effect_playing(RuntimeImpFireStrategy& context); // Xbox 0x000B4330, EAX actor
bool initialize_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context); // Xbox 0x000B4570
void destroy_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context); // Xbox 0x000B45B0
void release_runtime_imp_fire_hold(RuntimeImpFireStrategy& context); // Xbox 0x000C2D90, ECX strategy
float runtime_imp_fire_range_fraction(std::span<const u8> target,
    RuntimeImpFireStrategy& context); // Xbox 0x000B4010, EBX target, ESI actor
float blend_runtime_imp_fire_heading(float fraction, float angle); // Xbox 0x000B40E0
float select_runtime_imp_fire_heading(const RuntimeMindTarget& target,
    RuntimeImpFireStrategy& context); // Xbox 0x000B4160, EAX target, ECX actor
bool select_runtime_imp_fire_position(const RuntimeMindTarget& target, const Vec3& origin, float heading,
    Vec3& output, RuntimeImpFireStrategy& context); // Xbox 0x000B4260, EAX parameters, ECX actor
void approach_runtime_imp_fire_target(const RuntimeMindTarget& target,
    RuntimeImpFireStrategy& context); // Xbox 0x000B42D0, EAX actor, EBX parameters
bool roll_runtime_imp_fire_attachment(RuntimeImpFireStrategy& context); // Xbox 0x000B4EF0, EBX actor
void grab_runtime_imp_fire_target(const Actor* target, RuntimeImpFireStrategy& context); // Xbox 0x000B4F50
u32 engage_runtime_imp_fire_target(const RuntimeMindTarget& target,
    RuntimeImpFireStrategy& context); // Xbox 0x000B44D0, EAX target, ECX strategy
void update_runtime_imp_fire_seek(RuntimeImpFireStrategy& context); // Xbox 0x000B3F80, state 0x40C
void update_runtime_imp_fire_attack(RuntimeImpFireStrategy& context); // Xbox 0x000B4AF0, state 0x3EE
void update_runtime_imp_fire_retreat(RuntimeImpFireStrategy& context); // Xbox 0x000B4C20, state 0x401
void update_runtime_imp_fire_mutate(RuntimeImpFireStrategy& context); // Xbox 0x000B4CC0, state 0x40B
void update_runtime_imp_fire_wait(RuntimeImpFireStrategy& context); // Xbox 0x000B4D50, state 0x416
u16 runtime_imp_fire_idle_state(RuntimeImpFireStrategy& context); // Xbox 0x000B4DA0, EAX strategy
void update_runtime_imp_fire_bite(RuntimeImpFireStrategy& context); // Xbox 0x000B4DF0, state 0x3EF
void dispatch_runtime_imp_fire_message(std::span<const u32> message,
    RuntimeImpFireStrategy& context); // Xbox 0x000B46B0, vtable +0xC
void update_runtime_imp_fire_strategy(RuntimeImpFireStrategy& context); // Xbox 0x000B45D0, vtable +0x10
void enter_runtime_imp_fire_state(u16 state, u16 previous,
    RuntimeImpFireStrategy& context); // Xbox 0x000B4940, vtable +0x14
void leave_runtime_imp_fire_state(u16 state, u16 next, RuntimeImpFireStrategy& context); // Xbox 0x000B4A60, vtable +0x18
}
