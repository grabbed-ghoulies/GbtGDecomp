#pragma once
#include "gbtg/anim.h"
#include "../../gbtg/runtime/object_lifecycle.h"
#include "ghouly_box_decoder.h"
#include "ghouly_requests.h"
#include "../frontend/input_activation.h"
#include <functional>

namespace gbtg
{
void player_body_object_sizes(ObjectSizes& output); // Xbox 0x00075AD0
void player_mind_object_sizes(ObjectSizes& output); // Xbox 0x0007E0E0
void player_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AF030
void destroy_runtime_player_strategy(std::span<u8> strategy,
    u32& active_count); // Xbox 0x000BED90
struct RuntimePlayerMindRequest
{
    RuntimePlayerMindRequest* previous; // +0.
    RuntimePlayerMindRequest* next; // +4.
    u16 kind; // +8.
    u8 active; // +0xA.
    std::span<u8> bytes; // Retail request storage, including payload at +0x10.
};
struct RuntimePlayerMindRequests
{
    RuntimePlayerMindRequest*& first; // Mind +0x58, aliases the actual list head.
    std::function<void(RuntimePlayerMindRequest&)> release_active; // Mind vtable +0x1C.
};
void release_runtime_player_mind_request(RuntimePlayerMindRequest& request,
    RuntimePlayerMindRequests& mind); // Xbox 0x00029E00
void clear_runtime_player_mind_requests(RuntimePlayerMindRequests& mind); // Xbox 0x00029E80
void destroy_runtime_player_mind(std::span<const u8> actor, u32 runtime_flags,
    RuntimeInputActivationContext& input); // Xbox 0x0007F0D0
struct RuntimePlayerChildContext
{
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    u32 actor_address; // Actual parent actor token mapped by the scene owner.
};
void set_runtime_player_body(FactoryObject& body, const char* parameters,
    RuntimePlayerChildContext& context); // Xbox 0x00028DF0, actor +0x95C
void set_runtime_player_mind(FactoryObject& mind, const char* parameters,
    RuntimePlayerChildContext& context); // Xbox 0x00028E50, actor +0x958
struct RuntimePlayerStrategySetup
{
    RuntimePlayerChildContext& child;
    RuntimePlayerMindRequests*& mind; // actor +0x958's native request view.
    std::function<void(FactoryObject&, u16, u16)> leaving; // strategy vtable +0x18.
    std::function<void(FactoryObject&, u16, u16)> entering; // strategy vtable +0x14.
};
void set_runtime_player_strategy(FactoryObject& strategy, const char* parameters,
    RuntimePlayerStrategySetup& context); // Xbox 0x00028EB0, actor +0x954
struct RuntimePlayerSpawnController
{
    GhoulySpawnRecord* record; // actor +0xB98.
    u8 active; // actor +0xBA0.
    GhoulyController& controller; // Actual actor vtable+0xC mapping.
};
void set_runtime_player_spawn_controller(RuntimePlayerSpawnController& state,
    const u8* parameters, AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random); // Xbox 0x00028640
}
