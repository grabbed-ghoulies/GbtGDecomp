#pragma once

#include "actor_clock.h"
#include "actor_model_anchors.h"
#include "gbtg/model_runtime.h"
#include <functional>

namespace gbtg
{
// Native pointer views of the three indirections tested in animation row +0.
struct RuntimeBodyChannelBindings
{
    struct Asset
    {
        const u32* channels; // Asset +0x20.
    };
    struct Resource
    {
        const Asset* asset; // Resource +0.
    };
    struct Row
    {
        const Resource* resource; // Animation row +0, stride 0x28.
    };
    std::span<const Row> rows; // Animation +0x58, count +0x64.
};
bool runtime_body_channel_is_bound(const RuntimeBodyChannelBindings& animation,
    i32 channel); // Xbox 0x00030B40
struct RuntimeBodyChannelContext
{
    Actor& actor;
    const RuntimeActorClock& clock;
    RuntimeBodyChannelBindings*& animation; // Actor +0x50.
    RuntimeModelInstance*& model; // Actor +0x204.
    std::span<u8>& rows; // Body +0x150, stride 0xC.
    std::array<std::span<const u8>, 10> banks; // Body +0x100, stride 8.
    std::array<std::span<const u8>, 4> keys; // DAT_00338F64/F88/EE0/EA4.
    std::span<const u8> address_zero; // Invalid key mode's literal address-zero reads.
};
void ease_runtime_body_channel_targets(std::span<const u8> body,
    RuntimeBodyChannelContext& context); // Xbox 0x0001EA90
void tick_runtime_body_channel_keys(std::span<u8> body,
    RuntimeBodyChannelContext& context); // Xbox 0x0001D850
void tick_runtime_body_channel_blends(std::span<u8> body,
    RuntimeBodyChannelContext& context); // Xbox 0x0001EDE0
void apply_runtime_body_channels(std::span<const u8> body,
    RuntimeBodyChannelContext& context); // Xbox 0x0001EC20
void apply_runtime_body_default_blend(std::span<const u8> body,
    RuntimeBodyChannelContext& context); // Xbox 0x0001EEC0
struct RuntimeBodyChannelChild
{
    std::function<void()> step; // Child vtable +0x14.
};
void step_runtime_body_channels(std::span<u8> body, RuntimeBodyChannelChild*& attachment,
    RuntimeBodyChannelContext& context); // Xbox 0x00072AC0
void update_runtime_body_model_anchors(std::span<u8> body,
    entity_detail::ListHead& anchors); // Xbox 0x00025190
void apply_runtime_body_model_state(std::span<const u8> body,
    RuntimeModelInstance*& model, RuntimeModelInstance*& secondary,
    std::span<u32> shared_states); // Xbox 0x00025130
}
