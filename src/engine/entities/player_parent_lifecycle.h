#pragma once
#include "player_children.h"
#include "../scripts/script_conditions.h"
#include "../navigation/floor_tracking.h"

namespace gbtg
{
void base_mind_object_sizes(ObjectSizes& output); // Xbox 0x00029D10
void base_strategy_object_sizes(ObjectSizes& output); // Xbox 0x0002B6A0
struct RuntimeMindClassSettings
{
    i32 count; // DAT_0050E148.
    std::span<const u8> rows; // DAT_0050E14C, stride 4.
};
void initialize_runtime_mind_settings(std::span<u8> mind, std::span<const u8> parameters,
    u16 actor_class, const RuntimeMindClassSettings& settings); // Xbox 0x00029A60
bool initialize_runtime_base_mind(std::span<u8> mind, std::span<const u8> parameters,
    u16 actor_class, const RuntimeMindClassSettings& settings, u32& random,
    std::span<ExecutableResourcePool> resources,
    std::span<const u8>& script_table); // Xbox 0x00029F20, table aliases mind +0x60.
bool initialize_runtime_mind_request(std::span<u8> request, u32& random); // Xbox 0x00029EC0
void clear_runtime_mind_events(LiveScript*& first); // Xbox 0x0002A240, mind +0x28.
using RuntimeMindStorage = entity_detail::NavigationPathSearch; // Scratch is retail +0x14.
void release_runtime_mind_storage(RuntimeMindStorage& storage); // Xbox 0x00036B60
void destroy_runtime_base_mind(RuntimePlayerMindRequests& requests, LiveScript*& events,
    RuntimeMindStorage*& storage, std::span<const u8>& script_table); // Xbox 0x00029FF0
struct RuntimeStrategyClassRanges
{
    i32 count; // DAT_0050E1C4.
    std::span<const u8> rows; // DAT_0050E1C8, stride 0xC.
};
bool sample_runtime_strategy_class_range(std::span<u8, 4> output, u16 actor_class,
    const RuntimeStrategyClassRanges& ranges, u32& random); // Xbox 0x0010F760
bool initialize_runtime_base_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    u16 actor_class, const RuntimeStrategyClassRanges& ranges, u32& random); // Xbox 0x0002B740
struct RuntimeStrategyLink
{
    RuntimeStrategyLink* previous; // +0.
    RuntimeStrategyLink* next; // +4.
    void* owner; // +8 on anchor.
    u32 retained; // +0xC.
    u32 active; // +0x10.
};
void detach_runtime_strategy_link(RuntimeStrategyLink& link); // Xbox 0x00112A20
void destroy_runtime_base_strategy(FactoryObject& strategy, RuntimeStrategyLink& link,
    RuntimePlayerStrategySetup& context); // Xbox 0x0002B7A0
void leave_runtime_base_strategy_state(std::span<u8> strategy, u16 state); // Xbox 0x000BFA50
}
