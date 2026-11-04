#include "ghouly_swap_setup.h"
#include "actor_lookup.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "actor_transform.h"
#include "entity.h"
#include "ghouly_box_decoder.h"
#include "ghouly_spawn_trigger.h"
#include "../scene/actor_loading.h"
#include "gbtg/actor_spawn_factory.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
GhoulySwapState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<GhoulySwapState>(bytes);
}

const GhoulySwapState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<GhoulySwapState>(bytes);
}

namespace parameter = ghouly_swap_parameter;

const char* name_at(std::span<const u32> message, size_t index)
{
    return reinterpret_cast<const char*>(message.data() + index);
}

// A message name word block that begins with "aid_", else null.
const char* aid_at(std::span<const u32> message, size_t index)
{
    return message[index] == 0x5F646961u ? name_at(message, index) : nullptr;
}

Actor& source_of(RuntimeGhoulySwapSetup& context)
{
    Actor* source = context.actor_at(fields_of(context.bytes).source);
    if (source == nullptr)
    {
        throw Error("ghouly swap setup reads the pose of a missing +0x93C actor");
    }
    return *source;
}
}

// Xbox 0x0006BB10
void ghouly_swap_setup_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(GhoulySwapState);
    sizes.parameters = parameter::kSize;
}

// Xbox 0x00129F00, EAX actor
u32 find_runtime_actor_by_link_key(u16 key, const std::function<u32(const entity_detail::LatchNode&)>& owner,
    const std::function<std::span<u8>(u32)>& bytes_at)
{
    for (const auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next) // DAT_00511AA4.
    {
        const u32 address = owner(*node); // The node sits at the object's +0x900.
        if (read_u16(bytes_at(address), 0x1FE) == key)
        {
            return address;
        }
    }
    return 0;
}

// Xbox 0x0006BB70
bool initialize_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context, std::span<const u8> parameters)
{
    const auto bytes = context.bytes;
    const u32 key = get(parameters, parameter::controller_key);
    if (key != 0)
    {
        if (g_ghouly_controllers.size() == g_ghouly_controllers.capacity())
        {
            g_ghouly_controllers.reserve(g_ghouly_controllers.size() + 10); // 0x000662A0
        }
        g_ghouly_controllers.push_back({key, &context.controller});
    }
    fields_of(bytes).anchor = 0u;
    fields_of(bytes).source = 0u;
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    if (roll <= get(parameters, parameter::box_chance) && read_u32(parameters, parameter::box.offset) == 0x5F646961u)
    {
        std::strcpy(fields_of(bytes).box.data(), field_data(parameters, parameter::box));
        std::strcpy(fields_of(bytes).attributes.data(), field_data(parameters, parameter::attributes));
        fields_of(bytes).radius = get(parameters, parameter::radius);
        fields_of(bytes).chance = get(parameters, parameter::chance);
    }
    fields_of(bytes).unk_A50 = get(parameters, parameter::unk_444);
    return true;
}

// Xbox 0x0006BC70
void destroy_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context)
{
    remove_ghouly_controller(&context.controller); // 0x0012ACA0
    if (fields_of(context.bytes).load != 0)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
        fields_of(context.bytes).load = 0u;
    }
}

// Xbox 0x0006BCA0, EBX source euler
bool query_runtime_ghouly_swap_pose(RuntimeGhoulySwapSetup& context, Vec3& source_position, Vec3& source_euler,
    Vec3& position, Vec3& euler)
{
    Actor& source = source_of(context);
    source_position = actor_world_position(source); // 0x000412A0, ESI +0x93C
    source_euler = actor_world_euler(source); // 0x00041820
    const u32 anchor = fields_of(context.bytes).anchor;
    if (anchor == 0)
    {
        position = source_position;
        euler = source_euler;
        return true;
    }
    std::array<u32, 15> message{};
    message[0] = 0xA6;
    std::memcpy(message.data() + 1, &source_position, 12);
    std::memcpy(message.data() + 4, &source_euler, 12);
    message[7] = 1;
    context.message(anchor, message); // Anchor vtable +0xC.
    if (message[14] == 0)
    {
        return false;
    }
    std::memcpy(&position, message.data() + 8, 12);
    std::memcpy(&euler, message.data() + 11, 12);
    return true;
}

// Xbox 0x0006BDF0
void dispatch_runtime_ghouly_swap_setup_message(RuntimeGhoulySwapSetupMessages& context, std::span<u32> message)
{
    auto& setup = context.setup;
    const auto bytes = setup.bytes;
    switch (message[0])
    {
    case 0x7C:
        if (message[1] == fields_of(bytes).source)
        {
            fields_of(bytes).source = 0u;
        }
        if (message[1] == fields_of(bytes).anchor)
        {
            fields_of(bytes).anchor = 0u;
        }
        break;
    case 0x7D:
    {
        if (read_u16(bytes, 0x1FE) != 0)
        {
            fields_of(bytes).anchor = find_runtime_actor_by_id(read_u16(bytes, 0x1FE), context.latch_owner,
                context.bytes_at); // 0x00129ED0, DX +0x1FE
        }
        const Vec3 position = actor_world_position(setup.actor); // 0x000412A0
        u32 source = find_runtime_actor_by_link_key(read_u16(bytes, 0x1FC), context.latch_owner,
            context.bytes_at); // 0x00129F00, EAX the setup
        if (source == 0)
        {
            // The distance lands in the argument slot that held the message pointer (already kept in EBP).
            float distance;
            const Actor* nearest = find_nearest_spawn_actor(position, nullptr, &distance, 4); // 0x00129D70
            source = nearest == nullptr ? 0u : context.actor_address(*nearest);
        }
        fields_of(bytes).source = source;
        break;
    }
    case 0xA6:
    {
        Actor& source = source_of(setup);
        const Vec3 position = actor_world_position(source); // 0x000412A0, EDI message +0x20
        const Vec3 euler = actor_world_euler(source); // 0x00041820, EDI message +0x2C
        std::memcpy(message.data() + 8, &position, 12);
        std::memcpy(message.data() + 11, &euler, 12);
        message[14] = 1;
        break;
    }
    case 0xA7:
    {
        Actor& source = source_of(setup);
        if (!actor_can_supply_ghoulybox(source)) // 0x00146150, ECX +0x93C
        {
            message[0x81] = 0xFFFFFFFEu;
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_ghouly_swap_pose(setup, source_position, source_euler, position, euler)) // 0x0006BCA0
        {
            break;
        }
        // 0x001348F0: EDX +0x84, EAX +0x104, ECX +0x184, each when it holds an AID.
        FactoryObject* object = context.keep(create_spawn_actor_unit_scale(context.objects.factory,
            context.objects.resources, context.objects.records, name_at(message, 1), position, euler,
            aid_at(message, 0x21), aid_at(message, 0x41), aid_at(message, 0x61), nullptr, nullptr));
        const bool made = object != nullptr && object->bytes != nullptr;
        message[0x81] = made ? 1u : 0u;
        if (made)
        {
            context.runtime_flags |= 2;
            context.retire(fields_of(bytes).source); // 0x00112CC0, ESI +0x93C
            fields_of(bytes).swapped = 1u;
        }
        break;
    }
    case 0xA8:
        if (fields_of(bytes).source != 0 && actor_can_supply_ghoulybox(source_of(setup)) // 0x00146150
            && fields_of(bytes).load == 0)
        {
            setup.load = load_ghoulybox(name_at(message, 1), context.objects.registry, context.objects.error,
                setup.random); // 0x000F39A0, EAX message +4
            fields_of(bytes).load = setup.load == nullptr ? 0u : setup.address_of(setup.load);
            if (setup.load != nullptr)
            {
                GhoulySpawnRecord& load = *setup.load;
                load.controller_24 = &setup.controller;
                load.flags_2c = 0;
                std::strcpy(load.attributes_30, name_at(message, 0x21));
                link_actor_load_record(&g_side_list, load); // 0x000F3C30, DAT_00510568
                message[0x41] = 1;
            }
        }
        break;
    default:
        break;
    }
    context.base_message(context.base, message); // 0x0003EF70
}

// Xbox 0x0006C0A0
void update_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context)
{
    const auto bytes = context.bytes;
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    if (fields_of(bytes).swapped == 0
        && check_ghouly_spawn_trigger(position, fields_of(bytes).radius, fields_of(bytes).chance,
            context.random) == 1) // 0x0013E980
    {
        // {0xA8, box, attributes} through the setup's own vtable +0xC.
        std::array<u32, 0x42> request{};
        request[0] = 0xA8;
        std::strcpy(reinterpret_cast<char*>(request.data() + 1), fields_of(bytes).box.data());
        std::strcpy(reinterpret_cast<char*>(request.data() + 0x21), fields_of(bytes).attributes.data());
        context.self_message(request);
    }
    if (fields_of(bytes).load != 0 && context.load->remaining_c == 0)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
        fields_of(bytes).load = 0u;
    }
}
}
