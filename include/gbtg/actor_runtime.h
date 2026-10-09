#pragma once

#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"

namespace gbtg
{

struct RuntimeActorCreationOptions
{
    u32 flags = 0; // +0
    Vec3 position{}; // +4
    Vec3 euler{}; // +0x10
    std::array<u8, 0x10> retained{}; // +0x1C..+0x2B, not written by 0x0003A8B0
    float scale = 0; // +0x2C
    void* user = nullptr; // +0x30
};
extern RuntimeActorCreationOptions g_actor_creation_options; // DAT_004E1C5C
struct RuntimeActorMatrixState
{
    std::span<const u8> records; // actor +0x288, stride 0x48
    i32 count = 0; // +0x28C
    std::span<const u8> node; // +0x290
    std::span<u8> instance; // +0x294
};
void refresh_actor_model_matrices(const Actor& actor, RuntimeActorMatrixState& state,
    i32 force, const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x00040200
RuntimeActorCreationOptions& actor_creation_options(const Actor& actor, void* user,
    i32 scaled, i32 include_euler); // Xbox 0x0003A8B0
void clear_actor_shader_bits(Actor& actor, i16 slot, u32 mask); // Xbox 0x000426B0
void set_actor_shader_bits(Actor& actor, i16 slot, u32 mask); // Xbox 0x000426D0
i32 actor_shader_state(const Actor& actor, u8 slot); // Xbox 0x00040D90

struct RuntimeActorLink
{
    RuntimeActorLink* previous = nullptr;
    RuntimeActorLink* next = nullptr;
};
struct RuntimeActorStartupState
{
    Actor& actor;
    ActorAnimationState& animation;
    RuntimeActorModelState models;
    ActorModel model_view;
    RuntimeActorResourceReference resource;
    ActorAttachmentList attachments; // +0x1AC
    RuntimeActorLink live; // +0x900
    u32 word_288 = 0;
    u32 word_28c = 0;
    float value_29c = 0.f;
    u32 word_2a0 = 0;
    i32 secondary_mode = 0; // +0x30C
    std::array<u8, 2> secondary_flags{}; // +0x330
    u32 color = 0; // +0x198
    std::array<u8, 4> flags{}; // +0x19C
    u16 bone = 0; // +0x194
    u16 setting_1fc = 0;
    u16 setting_1fe = 0;
};
struct RuntimeActorStartupContext
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    const RuntimeModelResourceViews& models;
    const AnimationTableResourceViews& tables;
    const std::unordered_map<const u8*, ActorModel>& actor_model_views;
    RuntimeModelDeferredPool& deferred;
    u32& fence;
    std::span<const RuntimeModelNodeType> types;
    RuntimeShaderRegistrationState& shaders;
    RuntimeShaderAnimationScratch& scratch;
    const std::function<void(u8*)>& heap_free;
    RuntimeActorLink& live_actors; // DAT_00511B08 sentinel
    u32& random_state;
};
bool initialize_runtime_actor(RuntimeActorStartupState& state, std::span<const u8> parameters,
    RuntimeActorStartupContext& context); // Xbox 0x0003E940
void set_actor_node_state(RuntimeActorStartupState& state, u8 slot, u8 value,
    std::span<u32> shared_states); // Xbox 0x00040D50

} // namespace gbtg
