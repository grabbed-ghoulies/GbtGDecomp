#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"
#include "gbtg/model_runtime.h"
#include "gbtg/retail_field.h"
#include <array>

namespace gbtg
{
// An actor effect (0x000ED1C0): an emitter of 0x150-byte particles (effect_particle_field) placed from +0x390.
// See docs/RETAIL_LAYOUTS.md.
namespace actor_effect_field
{
constexpr RetailField<u16, 0x000> used_count{}; // lowered as each particle is freed
constexpr RetailField<u16, 0x002> capacity{};
constexpr RetailField<u16, 0x004> free_count{};
constexpr RetailField<u16, 0x006> free_head{}; // signed
constexpr RetailField<float, 0x008> emitting{}; // -3 when created, -1 once its life runs out
constexpr RetailField<float, 0x00C> emit_timer{};
constexpr RetailField<float, 0x010> life{}; // -1 for no end
constexpr RetailField<u32, 0x014> ticks{}; // 0 before its first tick
// 2: the volume turns with the euler; 4: the volume samples its surface; 8: retained, removed once empty;
// 0x10: positions are jittered; 0x80: faces the camera.
constexpr RetailField<u8, 0x018> flags{};
constexpr RetailField<Vec3, 0x01C> position{};
constexpr RetailField<Vec3, 0x028> previous_position{};
constexpr RetailField<Vec3, 0x034> emit_point{}; // where particles start, after the binding
constexpr RetailField<Vec3, 0x040> motion{}; // the emitter's own velocity, added to each particle's
constexpr RetailField<float, 0x04C> motion_scale{};
constexpr RetailField<Vec3, 0x050> euler{};
constexpr RetailField<Vec3, 0x05C> spin{}; // turns the offset
constexpr RetailField<Vec3, 0x068> spin_rate{};
constexpr RetailField<Vec3, 0x074> jitter{}; // a position's random extent on each axis
constexpr RetailField<Vec3, 0x080> offset{}; // the emit point's offset from the position
constexpr RetailField<Vec3, 0x08C> velocity{}; // added to each particle's
constexpr RetailField<float, 0x098> particle_size{};
constexpr RetailField<float, 0x09C> size_variance{};
constexpr RetailField<Vec3, 0x0A0> volume_euler{};
constexpr RetailField<Vec3, 0x0AC> volume_spin_rate{};
constexpr RetailField<float, 0x0B8> scale{};
constexpr RetailField<float, 0x0BC> unk_BC{}; // -1 when created
constexpr RetailField<u32, 0x0C4> binding_mode{};
constexpr RetailField<i32, 0x0C8> binding_index{}; // -1 for none
constexpr RetailField<u32, 0x0CC> binding_shape{}; // by address
constexpr RetailField<u16, 0x0D0> binding_locator{}; // signed
constexpr RetailField<u32, 0x0D4> binding_locator_address{};
// The emission volume: 0 a point, 1 a line, 2 an ellipse, 3 a rectangle, 4 a box, 5 an ellipsoid, 6 a cylinder.
constexpr RetailField<u32, 0x0DC> volume{};
constexpr RetailField<Vec3, 0x0E0> volume_size{}; // x alone is a sphere binding's radius
constexpr RetailField<Vec3, 0x0EC> volume_lower{}; // angles
constexpr RetailField<Vec3, 0x0F8> volume_upper{};
// How a particle's velocity is aimed: 0 none, 1 away from the position, 2 away from the emit point, 3..5 at
// random angles (5 also redraws the speed).
constexpr RetailField<u32, 0x110> velocity_mode{};
constexpr RetailField<std::array<float, 2>, 0x114> speed{}; // lower and upper
constexpr RetailField<Vec3, 0x11C> angle_lower{};
constexpr RetailField<Vec3, 0x128> angle_upper{};
constexpr RetailField<u32, 0x134> script{}; // the command script, by address
constexpr RetailField<u32, 0x138> command{}; // the command it runs next
constexpr RetailField<u32, 0x13C> draw_mode{};
constexpr RetailField<u32, 0x140> depth_mode{};
constexpr RetailField<u32, 0x144> texture{};
constexpr RetailField<u32, 0x148> frame_count{};
constexpr RetailField<float, 0x14C> frame_rate_low{};
constexpr RetailField<float, 0x150> frame_rate_high{};
constexpr RetailField<u8, 0x154> frame_flags{}; // 1, 4, 8, 0x10: how the frames advance
constexpr RetailField<u32, 0x158> model{};
constexpr RetailField<u16, 0x2E4> unk_2E4{}; // 0xFFFF when created
constexpr RetailField<u8, 0x36E> unk_36E{}; // 1 when created
constexpr RetailField<Vec3, 0x370> tick_position{}; // the position at the start of the tick
constexpr u32 particles_offset = 0x390;
} // namespace actor_effect_field

// Element `element` of a field, written from a value of the element's size. The stream copies words between integer
// and float fields, so the value's bits are stored as they are.
template <typename V, typename T, u32 Offset>
void store_effect_field(std::span<u8> bytes, RetailField<T, Offset>, const V& value, u32 element = 0)
{
    static_assert(sizeof(T) % sizeof(V) == 0);
    detail::check_retail_range(sizeof(T), element * sizeof(V), sizeof(V));
    detail::check_retail_range(bytes.size(), Offset + element * sizeof(V), sizeof(V));
    std::memcpy(bytes.data() + Offset + element * sizeof(V), &value, sizeof(V));
}

struct ActorEffectNode
{
    std::span<u8> bytes;
    std::span<const u8> script; // native resource pointer at +0x134
    std::span<const u8> texture; // +0x144
    void* binding_actor; // +0xC0
    i32 binding_joint; // +0xD8
    const u8* binding_shape = nullptr; // Native view of +0xCC.
    const u8* binding_locator = nullptr; // Native view of +0xD4.
    RuntimeActorSound sound; // native sound slot at +0x2DC
    std::unique_ptr<RuntimeModelInstance> model; // +0x158
};
struct ActorEffectModelContext
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    const RuntimeModelResourceViews& views;
    RuntimeModelDeferredPool& deferred;
    u32& fence;
    std::span<const RuntimeModelNodeType> types;
    RuntimeShaderRegistrationState& shaders;
    RuntimeShaderAnimationScratch& scratch;
    const std::function<void(u8*)>& heap_free;
};
extern std::array<ActorEffectNode*, 150> g_actor_effect_nodes; // DAT_00506930
extern i32 g_last_actor_effect; // DAT_0042A434

void initialize_actor_effect_free_chain(ActorEffectNode& effect); // Xbox 0x000ED170
ActorEffectNode* create_actor_effect_node(const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000ED1C0
i32 allocate_actor_effect_slot(i32 category, const char* name, u16 count,
    const Vec3& position, const Vec3& euler, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000ED550
void reset_actor_effect_to_debris(ActorEffectNode& effect,
    ActorEffectModelContext& context); // Xbox 0x000ED6E0
void recycle_actor_effect_particles(ActorEffectNode& effect); // Xbox 0x000EF790
void set_actor_effect_movement(ActorEffectNode& effect, u32 mode, const Vec3& velocity,
    const Vec3& minimum, const Vec3& maximum, i32 relative); // Xbox 0x000EF7F0
void set_actor_effect_script(ActorEffectNode& effect, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x000ED120
bool retain_last_actor_effect(float duration); // Xbox 0x0013E750
float actor_effect_event_duration(i32 enabled, std::span<const u8> event,
    const ActorAnimationState& animation); // Xbox 0x0003A9A0
void set_actor_effect_model(ActorEffectNode& effect, const char* name,
    ActorEffectModelContext& context); // Xbox 0x000ED8C0
u32 set_actor_effect_texture(ActorEffectNode& effect, const char* name, float rate,
    float spread, u32 setting, i32 mode, i32 enabled, i32 blend,
    ActorEffectModelContext& context); // Xbox 0x000ED730
}
