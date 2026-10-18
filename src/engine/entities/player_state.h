#pragma once
#include "player_parent_lifecycle.h"
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimePauseActor
{
    RuntimePauseActor* next; // Actor +0x938.
    std::span<const u8> actor;
    std::function<void(u32)> message; // Actual actor vtable +0xC.
};
struct RuntimeActorPauseGroups
{
    std::span<u32, 2> references; // DAT_0050E764, DAT_0050E768.
    RuntimePauseActor*& first; // DAT_00511B38.
};
void change_runtime_actor_pause(u8 mask, i32 release, RuntimeActorPauseGroups& groups); // Xbox 0x00117400
// 0x00117400 over the scene's DAT_00511B38 list; empty outside a native scene, where that list is empty.
extern std::function<void(u8 mask, i32 release)> g_actor_pause;
void leave_runtime_player_strategy(std::span<u8> strategy, u16 old_state, u16 next_state,
    std::span<u8> body, u32& active_count, RuntimeActorPauseGroups& groups); // Xbox 0x000BF410
void initialize_runtime_body_motion(std::span<u8> body); // Xbox 0x00025800
u8 count_runtime_player_body_joints(const ActorModel* model,
    std::span<const u8> parameters); // Xbox 0x00073920
struct RuntimeBodyModelNode
{
    u32 kind;
    const u8* data;
};
struct RuntimeBodyModelNodes
{
    std::span<const RuntimeBodyModelNode> nodes;
};
struct RuntimeBodySpecialShape
{
    std::array<u8, 0x54> bytes;
    const u8* shape; // +0x54, native pointer expansion.
};
struct RuntimeBodyShapeStorage
{
    u32 selected_count; // Body +0x898.
    const u8** selected; // Body +0x8A0.
    u8* selected_state; // Body +0x89C.
    RuntimeBodySpecialShape* special; // Body +0x710.
    u8 special_count; // Body +0x714.
    u8 special_active; // Body +0x715.
};
void collect_runtime_player_body_shapes(const RuntimeBodyModelNodes* model,
    RuntimeBodyShapeStorage& storage); // Xbox 0x00075270
void collect_runtime_body_special_shapes(const RuntimeBodyModelNodes* model,
    RuntimeBodyShapeStorage& storage); // Xbox 0x00026010
struct RuntimeBodyAction
{
    FactoryObject& object; // Body +0x760.
    RuntimePlayerChildContext& child;
    std::function<void(FactoryObject&, u32)> message; // Actual action vtable +0xC.
};
void activate_runtime_body_action(std::span<u8> body, u32 value, u32 mode,
    RuntimeBodyAction& action); // Xbox 0x00027B60
void initialize_runtime_body_action(std::span<u8> body, std::span<const u8> parameters,
    RuntimeBodyAction& action); // Xbox 0x000278F0
}
