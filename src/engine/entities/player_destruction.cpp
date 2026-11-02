#include "player_destruction.h"
#include "player_fields.h"
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x00028A30
void destroy_runtime_player(std::span<u8> actor, RuntimePlayerDestruction& context)
{
    auto& owner = context.owner;
    if (get(actor, player_field::mode) == 1)
    {
        --owner.player_count;
    }
    actor[0x98B] = 0;
    while (context.goals.ring != nullptr)
    {
        remove_runtime_player_goal(context.goals);
    }
    const bool had_strategy = owner.strategy.bytes != nullptr;
    set_runtime_player_strategy(owner.strategy, nullptr, owner.strategy_setup);
    if (had_strategy)
    {
        put(actor, 0x954, 0); // Write performed by the translated setter's release branch.
    }
    auto& factory = owner.strategy_setup.child.factory;
    if (owner.mind.bytes != nullptr)
    {
        release_factory_object(factory, owner.mind);
        put(actor, 0x958, 0);
    }
    if (owner.input.data() != nullptr)
    {
        switch (read_u32(owner.input, 0))
        {
        case 1:
        case 2:
        case 3:
            release_runtime_simple_input(owner.input);
            break;
        case 4:
            release_runtime_composite_input(owner.input, owner.input_release);
            break;
        case 5:
            release_runtime_actor_input(owner.input, owner.input_release);
            break;
        }
    }
    // Retail deliberately leaves +0x960 unchanged after freeing its allocation.
    if (owner.body.bytes != nullptr)
    {
        release_factory_object(factory, owner.body);
        put(actor, 0x95C, 0);
    }
    if (owner.attributes.resource.data() != nullptr)
    {
        release_animation_resource(owner.attributes.registry, owner.attributes.resource);
        owner.attributes.resource = {};
        put(actor, 0x964, 0);
    }
    if (owner.spawn.record != nullptr)
    {
        release_ghouly_spawn_record(*owner.spawn.record);
        owner.spawn.record = nullptr;
        put(actor, 0xB98, 0);
    }
    owner.spawn.active = 0;
    actor[0xBA0] = 0;
    for (size_t index = 0; index < 4; ++index)
    {
        auto& link = context.links[index];
        if (link.prev != nullptr)
        {
            static_cast<entity_detail::ListHead*>(link.prev)->next = link.next;
        }
        if (link.next != nullptr)
        {
            static_cast<entity_detail::ListHead*>(link.next)->prev = link.prev;
        }
        link.prev = nullptr;
        link.next = nullptr;
        put(actor, 0x934 + index * 8, 0);
        put(actor, 0x938 + index * 8, 0);
    }
    owner.actor.mask_flag = 0; // Native alias of the cleared actor +0x93C word.
    entity_detail::release_navigation_cell_registration(context.navigation, get(actor, player_field::floor));
    put(actor, 0xCE8, 0);
    if (get(actor, player_field::damage_class) != 0)
    {
        const u16 zero = 0;
        std::memcpy(actor.data() + 0xCF4, &zero, 2);
        owner.actor.damage_class = 0; // Native alias of the cleared actor +0xCF4 word.
    }
}
}
