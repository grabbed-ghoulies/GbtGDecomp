#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"
#include "gbtg/model_runtime.h"
#include "gbtg/retail_field.h"
#include <array>

namespace gbtg
{
// An actor effect (0x000ED1C0): an emitter of 0x150-byte particles (EffectParticle) placed from +0x390.
// See docs/RETAIL_LAYOUTS.md.
struct ActorEffectHeader
{
    u16 used_count = 0; // +0, lowered as each particle is freed
    u16 capacity = 0; // +2
    u16 free_count = 0; // +4
    u16 free_head = 0; // +6, signed
    float emitting = 0.f; // +8, -3 when created, -1 once its life runs out
    float emit_timer = 0.f; // +0xC
    float life = 0.f; // +0x10, -1 for no end
    u32 ticks = 0; // +0x14, 0 before its first tick
    // +0x18, 2: the volume turns with the euler; 4: the volume samples its surface; 8: retained, removed once empty;
    // 0x10: positions are jittered; 0x80: faces the camera.
    u8 flags = 0;
    std::array<u8, 3> unk_19{}; // +0x19
    Vec3 position{}; // +0x1C
    Vec3 previous_position{}; // +0x28
    Vec3 emit_point{}; // +0x34, where particles start, after the binding
    Vec3 motion{}; // +0x40, the emitter's own velocity, added to each particle's
    float motion_scale = 0.f; // +0x4C
    Vec3 euler{}; // +0x50
    Vec3 spin{}; // +0x5C, turns the offset
    Vec3 spin_rate{}; // +0x68
    Vec3 jitter{}; // +0x74, a position's random extent on each axis
    Vec3 offset{}; // +0x80, the emit point's offset from the position
    Vec3 velocity{}; // +0x8C, added to each particle's
    float particle_size = 0.f; // +0x98
    float size_variance = 0.f; // +0x9C
    Vec3 volume_euler{}; // +0xA0
    Vec3 volume_spin_rate{}; // +0xAC
    float scale = 0.f; // +0xB8
    float unk_BC = 0.f; // +0xBC, -1 when created
    std::array<u8, 4> binding_actor_word{}; // +0xC0, the host keeps ActorEffectNode::binding_actor
    u32 binding_mode = 0; // +0xC4
    i32 binding_index = 0; // +0xC8, -1 for none
    u32 binding_shape = 0; // +0xCC, by address
    u16 binding_locator = 0; // +0xD0, signed
    std::array<u8, 2> unk_D2{}; // +0xD2
    u32 binding_locator_address = 0; // +0xD4
    std::array<u8, 4> binding_joint_word{}; // +0xD8, the host keeps ActorEffectNode::binding_joint
    // +0xDC, the emission volume: 0 a point, 1 a line, 2 an ellipse, 3 a rectangle, 4 a box, 5 an ellipsoid, 6 a cylinder.
    u32 volume = 0;
    Vec3 volume_size{}; // +0xE0, x alone is a sphere binding's radius
    Vec3 volume_lower{}; // +0xEC, angles
    Vec3 volume_upper{}; // +0xF8
    std::array<u8, 0xC> unk_104{}; // +0x104
    // +0x110, how a particle's velocity is aimed: 0 none, 1 away from the position, 2 away from the emit point, 3..5 at
    // random angles (5 also redraws the speed).
    u32 velocity_mode = 0;
    std::array<float, 2> speed{}; // +0x114, lower and upper
    Vec3 angle_lower{}; // +0x11C
    Vec3 angle_upper{}; // +0x128
    u32 script = 0; // +0x134, the command script, by address
    u32 command = 0; // +0x138, the command it runs next
    u32 draw_mode = 0; // +0x13C
    u32 depth_mode = 0; // +0x140
    u32 texture = 0; // +0x144
    u32 frame_count = 0; // +0x148
    float frame_rate_low = 0.f; // +0x14C
    float frame_rate_high = 0.f; // +0x150
    u8 frame_flags = 0; // +0x154, 1, 4, 8, 0x10: how the frames advance
    std::array<u8, 3> unk_155{}; // +0x155
    u32 model = 0; // +0x158
    std::array<char, 0x80> particle_sound{}; // +0x15C, played as a particle is notified without a collision (0x000EE560)
    std::array<char, 0x80> collision_sound{}; // +0x1DC, played for a collided particle
    std::array<char, 0x80> collision_effect{}; // +0x25C, an "aid_" name emitted where a particle is notified
    std::array<u8, 8> sound_word{}; // +0x2DC, the host keeps ActorEffectNode::sound
    u16 unk_2E4 = 0; // +0x2E4, 0xFFFF when created
    std::array<u8, 0x88> unk_2E6{}; // +0x2E6
    u8 unk_36E = 0; // +0x36E, 1 when created
    u8 unk_36F = 0; // +0x36F
    Vec3 tick_position{}; // +0x370, the position at the start of the tick
    std::array<u8, 0x14> unk_37C{}; // +0x37C
};
static_assert(sizeof(ActorEffectHeader) == 0x390);
static_assert(offsetof(ActorEffectHeader, volume) == 0xDC);
static_assert(offsetof(ActorEffectHeader, velocity_mode) == 0x110);
static_assert(offsetof(ActorEffectHeader, particle_sound) == 0x15C);
static_assert(offsetof(ActorEffectHeader, unk_36E) == 0x36E);

// The block's header; the particles follow it (effect_particles.cpp).
inline ActorEffectHeader& effect_header(std::span<u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(ActorEffectHeader));
    return *reinterpret_cast<ActorEffectHeader*>(bytes.data());
}
inline const ActorEffectHeader& effect_header(std::span<const u8> bytes)
{
    detail::check_retail_range(bytes.size(), 0, sizeof(ActorEffectHeader));
    return *reinterpret_cast<const ActorEffectHeader*>(bytes.data());
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
