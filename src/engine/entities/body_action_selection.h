#pragma once
#include "body_resources.h"
#include "../scripts/script_table_lookup.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeBodyAnimationNotice
{
    u32 id;
    u32 actor;
    const AnimationRecord* record;
    u32 zero;
    float position;
};
struct RuntimeBodyAnimationReceiver
{
    std::function<void(const RuntimeBodyAnimationNotice&)> message; // Actual vtable +0xC.
};
struct RuntimeBodyAttachedActor
{
    std::span<u8> bytes;
    u32 address;
    std::function<void(std::span<const u32>)> message; // Actual actor vtable +0xC.
    RuntimeBodyAttachedActor* host = nullptr; // Actor +0xD8C native owner.
    Actor* actor = nullptr; // Actual native owner of this attached actor's retail bytes.
};
struct RuntimeBodyActionSelection
{
    ActorAnimationState& animation;
    RuntimeBodyAttachedActor*& attached; // Body +0x48, aliases the same owner as attachment cleanup.
    RuntimeBodyAnimationReceiver*& first_receiver; // Body +0x10.
    RuntimeBodyAnimationReceiver*& second_receiver; // Body +0x14.
    FactoryObject& first; // Body +0x314.
    FactoryObject& second; // Body +0x744.
    const u8*& row; // Body +0x740.
    const RuntimeScriptLookupTable*& table; // Body +0x73C native view.
    const std::unordered_map<u32, const char*>& names; // Relocated row +0x104 strings, including null token.
    RuntimePlayerChildContext& child;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random;
    u32 body_address;
};
struct RuntimeBodyActionTemplates
{
    std::span<const u8> second; // Native view of optional structure +0.
    std::span<const u8> first; // Native view of optional structure +4.
};
void notify_runtime_body_animation(RuntimeBodyActionSelection& context); // Xbox 0x0001B020
const AnimationRecord* select_runtime_body_animation(const char* name,
    RuntimeBodyActionSelection& context); // Xbox 0x0001AF10
void create_runtime_body_first_action(const char* name, const RuntimeBodyActionTemplates* parameters,
    RuntimeBodyActionSelection& context); // Xbox 0x00027830
void create_runtime_body_second_action(const char* name, const RuntimeBodyActionTemplates* parameters,
    RuntimeBodyActionSelection& context); // Xbox 0x00027890
bool select_runtime_body_action_row(std::span<u8> body, const u8* row,
    const RuntimeBodyActionTemplates* parameters, RuntimeBodyActionSelection& context); // Xbox 0x000276A0
bool select_runtime_body_action(std::span<u8> body, u16 key,
    RuntimeBodyActionSelection& context); // Xbox 0x00027650
}
