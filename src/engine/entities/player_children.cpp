#include "player_children.h"
#include "strategy_fields.h"
#include "entity_heap.h"
#include "player_fields.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x0001DA30
void base_body_object_sizes(ObjectSizes& output)
{
    output.instance = 0x894;
    output.parameters = 0x7C8;
}

// Xbox 0x000CFFD0
bool accept_runtime_object_construction(FactoryObject&, std::span<const u8>)
{
    return true;
}

// Xbox 0x00076180
void parent_player_body_object_sizes(ObjectSizes& output)
{
    output.instance = 0x924;
    output.parameters = 0x820;
}

// Xbox 0x00075AD0
void player_body_object_sizes(ObjectSizes& output)
{
    output.instance = 0x950;
    output.parameters = 0x820;
}

// Xbox 0x0007E0E0
void player_mind_object_sizes(ObjectSizes& output)
{
    output.instance = 0x198;
    output.parameters = 0xA4;
}

// Xbox 0x000AF030
void player_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x54;
    output.parameters = 0x18;
}

// Xbox 0x000BED90
void destroy_runtime_player_strategy(std::span<u8> strategy, u32& active_count)
{
    PlayerStrategyState& state = strategy_view<PlayerStrategyState>(strategy);
    if (state.pause_held != 0)
    {
        --active_count;
        state.pause_held = 0;
    }
}

// Xbox 0x00029E00
void release_runtime_player_mind_request(RuntimePlayerMindRequest& request,
    RuntimePlayerMindRequests& mind)
{
    if (request.active != 0)
    {
        mind.release_active(request);
    }
    if (request.previous != nullptr)
    {
        request.previous->next = request.next;
    }
    if (request.next != nullptr)
    {
        request.next->previous = request.previous;
    }
    request.previous = nullptr;
    request.next = nullptr;
    engine_heap_free(&request);
}

// Xbox 0x00029E80
void clear_runtime_player_mind_requests(RuntimePlayerMindRequests& mind)
{
    RuntimePlayerMindRequest* request = mind.first;
    while (request != nullptr)
    {
        RuntimePlayerMindRequest* next = request->next;
        if (request->previous != nullptr)
        {
            request->previous->next = next;
        }
        if (request->next != nullptr)
        {
            request->next->previous = request->previous;
        }
        request->previous = nullptr;
        request->next = nullptr;
        release_runtime_player_mind_request(*request, mind);
        request = next;
    }
}

// Xbox 0x0007F0D0
void destroy_runtime_player_mind(std::span<const u8> actor, u32 runtime_flags,
    RuntimeInputActivationContext& input)
{
    if ((runtime_flags & 0x800) == 0 && player_record(actor).input_device != 0)
    {
        pop_runtime_input_entry(input);
    }
}

// Xbox 0x00028640
void set_runtime_player_spawn_controller(RuntimePlayerSpawnController& state,
    const u8* parameters, AnimationResourceRegistry& registry, ResourceErrorState& error,
    u32& random)
{
    if (std::memcmp(parameters + 0x53A, "aid_", 4) == 0)
    {
        if (state.record != nullptr)
        {
            release_ghouly_spawn_record(*state.record);
            state.record = nullptr;
        }
        state.active = 0;
        const char* name = reinterpret_cast<const char*>(parameters + 0x53A);
        if (name != nullptr)
        {
            state.record = load_ghoulybox(name, registry, error, random);
            if (state.record != nullptr)
            {
                state.record->controller_24 = &state.controller;
                state.record->flags_2c = 1;
                link_actor_load_record(&g_side_list, *state.record);
                return;
            }
        }
    }
}

// Xbox 0x00028DF0
void set_runtime_player_body(FactoryObject& body, const char* parameters,
    RuntimePlayerChildContext& context)
{
    if (body.bytes != nullptr)
    {
        release_factory_object(context.factory, body);
    }
    if (parameters != nullptr && std::memcmp(parameters, "aid_", 4) == 0)
    {
        auto clone = clone_executable_resource(context.resources, parameters);
        std::memcpy(clone.bytes.get() + 0x1B0, &context.actor_address, 4);
        body = create_factory_object(context.factory, {clone.bytes.get(), clone.size});
        clone.bytes.reset(); // 0x000663E0, paired clone heap release.
        return;
    }
}

// Xbox 0x00028E50
void set_runtime_player_mind(FactoryObject& mind, const char* parameters,
    RuntimePlayerChildContext& context)
{
    if (mind.bytes != nullptr)
    {
        release_factory_object(context.factory, mind);
    }
    if (parameters != nullptr && std::memcmp(parameters, "aid_", 4) == 0)
    {
        auto clone = clone_executable_resource(context.resources, parameters);
        std::memcpy(clone.bytes.get() + 0x9C, &context.actor_address, 4);
        mind = create_factory_object(context.factory, {clone.bytes.get(), clone.size});
        clone.bytes.reset(); // 0x000663E0, paired clone heap release.
        return;
    }
}

// Xbox 0x00028EB0
void set_runtime_player_strategy(FactoryObject& strategy, const char* parameters,
    RuntimePlayerStrategySetup& context)
{
    if (strategy.bytes != nullptr)
    {
        release_factory_object(context.child.factory, strategy);
    }
    if (context.mind != nullptr)
    {
        clear_runtime_player_mind_requests(*context.mind);
    }
    if (parameters != nullptr && std::memcmp(parameters, "aid_", 4) == 0)
    {
        auto clone = clone_executable_resource(context.child.resources, parameters);
        std::memcpy(clone.bytes.get() + 8, &context.child.actor_address, 4);
        strategy = create_factory_object(context.child.factory, {clone.bytes.get(), clone.size});
        clone.bytes.reset();
        if (strategy.bytes != nullptr)
        {
            const u16 next = read_u16({strategy.bytes.get(), strategy.size}, 0xC);
            context.leaving(strategy, read_u16({strategy.bytes.get(), strategy.size}, 0xE), next);
            const u16 previous = read_u16({strategy.bytes.get(), strategy.size}, 0xE);
            std::memcpy(strategy.bytes.get() + 0xE, &next, 2);
            clear_runtime_player_mind_requests(*context.mind);
            context.entering(strategy, next, previous);
        }
    }
}
}
