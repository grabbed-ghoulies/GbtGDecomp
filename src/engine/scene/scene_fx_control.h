#pragma once
#include "scene_control_fields.h"
#include "../entities/actor_clock.h"
#include "../graphics/actor_effect_nodes.h"
#include "../graphics/actor_effect_stream.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"

#include <functional>

namespace gbtg
{
// entitySceneControlFx (factory type 0x97, getter 0x00082FA0, vtable 0x00505608) over the scene control (type 3): up
// to eight copies of one named effect (0x00053170) around the control's marker, each jittered within a radius. Each
// copy switches on for a random time in [+0xB0, +0xB4) and off for one in [+0xB8, +0xBC); with either range empty it
// stays on. A started copy runs at a random rate from [+0xC0, +0xC4) and, from the constructor, is pre-run for +0xC8
// seconds. The message is the scene control's 0x00115EE0; the destroy and +0x14 slots are the empty 0x001628B0.
namespace scene_fx_parameter
{
constexpr RetailField<Vec3, 0x08> position{};
constexpr RetailField<Vec3, 0x14> euler{};
constexpr RetailField<float, 0x20> scale{}; // the creation options' +0x2C
constexpr RetailField<float, 0x24> radius{}; // the copies' jitter
constexpr RetailField<i32, 0x28> count{};
constexpr RetailField<std::array<char, 0x80>, 0x2C> effect{};
constexpr RetailField<std::array<float, 2>, 0xAC> on_time{};
constexpr RetailField<std::array<float, 2>, 0xB4> off_time{};
constexpr RetailField<std::array<float, 2>, 0xBC> rate{};
constexpr RetailField<float, 0xC4> prerun{};
constexpr u32 kSize = 0xC8;
static_assert(retail_fits(kSize, prerun));
} // namespace scene_fx_parameter
// One placed copy of the effect.
struct SceneFxSlot
{
    Vec3 position{}; // +0
    Vec3 euler{}; // +0xC
    u32 on = 0; // +0x18
    float timer = 0.f; // +0x1C, the time left in the current on or off spell
    i32 effect = 0; // +0x20, its DAT_00506930 index, -1 for none
};
static_assert(sizeof(SceneFxSlot) == 0x24);
struct SceneFxControlState
{
    SceneControlBase base{}; // +0
    float scale = 0.f; // +0x24
    float radius = 0.f; // +0x28
    i32 count = 0; // +0x2C
    std::array<char, 0x80> effect{}; // +0x30
    std::array<float, 2> on_time{}; // +0xB0
    std::array<float, 2> off_time{}; // +0xB8
    std::array<float, 2> rate{}; // +0xC0
    float prerun = 0.f; // +0xC8
    std::array<SceneFxSlot, 8> slots{}; // +0xCC
};
static_assert(sizeof(SceneFxControlState) == 0x1EC);
struct SceneFxControlServices
{
    const RuntimeActorClock& clock;
    u32& random; // DAT_0054DD7C.
    ActorEffectStreamContext& effects; // 0x00053170's views.
    std::function<void(ActorEffectNode&, float)> tick_effect; // 0x000EF2D0 with the scene's particle simulation.
};
void scene_fx_control_object_sizes(ObjectSizes& output); // Xbox 0x00082F80
bool construct_runtime_scene_fx_control(SceneFxControlState& state, std::span<const u8> parameters,
    SceneFxControlServices& services); // Xbox 0x000832F0
// The copy's effect stops (its node fades: +0x18 bit 3, +8 -1) and it waits a random off time.
void stop_runtime_scene_fx(SceneFxControlState& state, size_t slot,
    SceneFxControlServices& services); // Xbox 0x00082FF0, ECX slot, EAX state
// The copy's effect starts at its pose for a random on time; `prerun` runs it for the control's +0xC8 seconds.
void start_runtime_scene_fx(SceneFxControlState& state, size_t slot, u32 prerun,
    SceneFxControlServices& services); // Xbox 0x00083080, EAX slot, EDI state
void step_runtime_scene_fx(SceneFxControlState& state, float step, u32 prerun,
    SceneFxControlServices& services); // Xbox 0x00083280, EAX state
void tick_runtime_scene_fx_control(SceneFxControlState& state, SceneFxControlServices& services); // Xbox 0x00083570
}
