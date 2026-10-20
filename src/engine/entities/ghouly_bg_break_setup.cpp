#include "ghouly_bg_break_setup.h"
#include "actor_lookup.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "area_actor.h"
#include "ghouly_box_decoder.h"
#include "ghouly_spawn_trigger.h"
#include "player_mind_events.h"
#include "player_mind_messages.h"
#include "spawn_queue_gates.h"
#include "weapon_breakage.h"
#include "../navigation/navigation.h"
#include "../navigation/spawn_candidates.h"
#include "../scene/actor_loading.h"
#include "../scene/scene_markers.h"
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = ghouly_bg_break_field;
namespace parameter = ghouly_bg_break_parameter;

const char* name_at(std::span<const u32> message, size_t index)
{
    return reinterpret_cast<const char*>(message.data() + index);
}

float random_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// The DAT_0051108C row of this setup's marker (actor +0x1FC); retail writes through address zero without one.
u8* own_record(RuntimeGhoulyBgBreakSetup& context)
{
    u8* row = find_runtime_marker(context.records, i16(read_u16(context.bytes, 0x1FC)));
    if (row == nullptr)
    {
        throw Error("ghouly bg-break setup has no DAT_0051108C row for its marker");
    }
    return row;
}

void put_record_word(u8* row, size_t offset, u32 value)
{
    std::memcpy(row + offset, &value, 4);
}

u32 spawned(std::span<const u8> bytes, size_t slot)
{
    return read_u32(record(bytes, field::spawned, slot), 0);
}

void set_spawned(std::span<u8> bytes, size_t slot, u32 value)
{
    std::memcpy(record(bytes, field::spawned, slot).data(), &value, 4);
}

// Xbox 0x0006A350, ESI parameters, EDI setup
void keep_runtime_bg_break_box_by_chance(RuntimeGhoulyBgBreakSetup& context, std::span<const u8> parameters)
{
    const auto bytes = context.bytes;
    set(bytes, field::grouped, 0);
    if (random_fraction(context.random) <= get(parameters, parameter::box_chance))
    {
        std::strcpy(field_data(bytes, field::box), field_data(parameters, parameter::box));
        std::strcpy(field_data(bytes, field::attributes), field_data(parameters, parameter::attributes));
        set(bytes, field::radius, get(parameters, parameter::radius));
        set(bytes, field::chance, get(parameters, parameter::chance));
    }
}

// Xbox 0x0006A120, EDI parameters, ESI setup
void keep_runtime_bg_break_group_box(RuntimeGhoulyBgBreakSetup& context, std::span<const u8> parameters)
{
    const auto bytes = context.bytes;
    std::strcpy(field_data(bytes, field::box), field_data(parameters, parameter::box));
    std::strcpy(field_data(bytes, field::attributes), field_data(parameters, parameter::attributes));
    set(bytes, field::grouped, 1);
    set(bytes, field::group, get(parameters, parameter::group));
}

// Xbox 0x0006AF20, EAX setup
void clear_runtime_bg_break_record(RuntimeGhoulyBgBreakSetup& context)
{
    u8* row = own_record(context);
    put_record_word(row, 0x144, 0);
    put_record_word(row, 0x148, 0);
}

// Xbox 0x0006B080, EAX actor: message 0x13 to itself.
void send_runtime_bg_break_self_message(RuntimeGhoulyBgBreakSetup& context)
{
    std::array<u32, 1> message{0x13};
    context.self_message(message);
}

// Xbox 0x0006B0A0, ECX setup, EAX name, EDI attributes
void load_runtime_bg_break_box(RuntimeGhoulyBgBreakSetup& context, const char* name, const char* attributes,
    RuntimeMarkerObjects& objects)
{
    context.load = load_ghoulybox(name, objects.registry, objects.error, context.random); // 0x000F39A0
    if (context.load == nullptr)
    {
        throw Error("0x0006B0A0 writes the ghoulybox load through address zero");
    }
    set(context.bytes, field::load, context.address_of(context.load));
    GhoulySpawnRecord& load = *context.load;
    load.controller_24 = &context.controller;
    load.flags_2c = 2;
    if (attributes != nullptr)
    {
        std::strcpy(load.attributes_30, attributes);
    }
    load.timer_20 = 0.15f; // DAT_0032B168.
    link_actor_load_record(&g_side_list, load); // 0x000F3C30, DAT_00510568
}

// Xbox 0x0006A2C0, ESI setup
void mark_runtime_bg_break_broken(RuntimeGhoulyBgBreakSetup& context)
{
    put_record_word(own_record(context), 0x148, 1);
    set(context.bytes, field::broken, 1);
}

// Xbox 0x0006A310, EAX setup
void mark_runtime_bg_break_loaded(RuntimeGhoulyBgBreakSetup& context)
{
    put_record_word(own_record(context), 0x144, 1);
}

// Xbox 0x0006A170: one of the group's setups loads the box, more likely the more of the group have broken.
void share_runtime_bg_break_box(RuntimeGhoulyBgBreakSetup& context, RuntimeMarkerObjects& objects)
{
    const i32 count = count_runtime_markers(0xD, context.records); // 0x0011EF50, ESI 0xD
    float unbroken = 0.f;
    float members = 0.f;
    if (count <= 0)
    {
        return;
    }
    const u32 group = get(context.bytes, field::group);
    for (i32 index = 0; index < count; ++index)
    {
        u16 id = next_runtime_marker_id(0, 0xD, 1, context.records); // 0x0011F1C0, AX 0
        for (i32 step = index; step > 0; --step)
        {
            id = next_runtime_marker_id(id, 0xD, 1, context.records);
        }
        const u8* row = find_runtime_marker(context.records, i16(id));
        if (row == nullptr)
        {
            throw Error("0x0006A170 reads a kind 13 marker's row through address zero");
        }
        if (read_u32({row, 0x14C}, 0x140) == group)
        {
            if (read_u32({row, 0x14C}, 0x144) != 0)
            {
                mark_runtime_bg_break_broken(context); // 0x0006A2C0
                return;
            }
            if (read_u32({row, 0x14C}, 0x148) == 0)
            {
                unbroken = unbroken + 1.f;
            }
            members = members + 1.f;
        }
    }
    if (unbroken == 0.f)
    {
        return;
    }
    if (random_fraction(context.random) <= ((members - unbroken) + 1.f) / members
        && read_u32(context.bytes, field::box.offset) == 0x5F646961)
    {
        load_runtime_bg_break_box(context, field_data(context.bytes, field::box),
            field_data(context.bytes, field::attributes), objects); // 0x0006B0A0
        mark_runtime_bg_break_loaded(context); // 0x0006A310
    }
    mark_runtime_bg_break_broken(context); // 0x0006A2C0
}

// Xbox 0x0006A470, ESI setup: loads the box once the part is gone; true when there is nothing to wait for.
bool load_runtime_bg_break_box_after_break(RuntimeGhoulyBgBreakSetup& context, RuntimeMarkerObjects& objects)
{
    if (read_u32(context.bytes, field::box.offset) == 0x5F646961)
    {
        load_runtime_bg_break_box(context, field_data(context.bytes, field::box),
            field_data(context.bytes, field::attributes), objects); // 0x0006B0A0
        if (context.load != nullptr)
        {
            set(context.bytes, field::broken, 1);
            return context.load->value_b0 != 0;
        }
    }
    set(context.bytes, field::broken, 1);
    return true;
}

// Xbox 0x0006A760, EDX setup, ESI slot
bool find_runtime_bg_break_spawn_slot(std::span<u8> bytes, i32& slot)
{
    const i32 count = std::bit_cast<i32>(get(bytes, field::spawn_slots));
    if (count == 0)
    {
        set_spawned(bytes, 0, 0);
        slot = 0;
        return true;
    }
    for (i32 index = 0; index < std::bit_cast<i32>(get(bytes, field::spawn_slots)); ++index)
    {
        if (spawned(bytes, size_t(index)) == 0)
        {
            slot = index;
            return true;
        }
    }
    return false;
}

// Xbox 0x0006AE10, EAX position, ESI actor
i32 query_runtime_bg_break_pose(RuntimeGhoulyBgBreakSetup& context, i32 select, float radius, Vec3& position,
    Vec3& euler)
{
    const Vec3 origin = actor_world_position(context.actor); // 0x000412A0
    i32 found;
    if (select == 0)
    {
        runtime_random_annulus_position(position, origin, radius, 0.f, context.random); // 0x00106670
        found = 1;
    }
    else
    {
        const i32 floor = entity_detail::navigation_floor_index(origin, -1, 0); // 0x00110BF0
        found = entity_detail::select_navigation_spawn_position(floor, nullptr, origin, 0.f, radius, nullptr,
            nullptr, 0, 0, position, context.random, context.occupancy_initial) ? 1 : 0; // 0x00111420
        if (found == 0)
        {
            return 0;
        }
    }
    if (radius != 0.f) // DAT_0032B100.
    {
        const Vec3 direction{origin.x - position.x, origin.y - position.y, origin.z - position.z};
        float angle;
        if (runtime_direction_angle(direction, angle)) // 0x00105F90
        {
            euler = {0.f, angle, 0.f};
            return found;
        }
    }
    euler = actor_world_euler(context.actor); // 0x00041820
    return found;
}

// Xbox 0x0006A630, EAX setup, EBX/ESI the sources
bool query_runtime_bg_break_spawn_pose(RuntimeGhoulyBgBreakSetup& context, Vec3& source_position,
    Vec3& source_euler, Vec3& position, Vec3& euler)
{
    if (query_runtime_bg_break_pose(context, 0, 0.f, source_position, source_euler) == 0) // 0x0006AE10
    {
        return false;
    }
    const u32 anchor = get(context.bytes, field::anchor);
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
    context.message(anchor, message);
    if (message[14] == 0)
    {
        return false;
    }
    std::memcpy(&position, message.data() + 8, 12);
    std::memcpy(&euler, message.data() + 11, 12);
    return true;
}

// The pose message 0x96 0x0006A7B0 sends a 0xA9 / 0xAA spawn.
void place_runtime_bg_break_spawn(RuntimeGhoulyBgBreakSetup& context, FactoryObject& object, const Vec3& position,
    const Vec3& euler)
{
    std::array<u32, 7> message{};
    message[0] = 0x96;
    std::memcpy(message.data() + 1, &position, 12);
    std::memcpy(message.data() + 4, &euler, 12);
    context.message(context.address_of(object.bytes.get()), message);
}
}

// Xbox 0x0006A4D0
void ghouly_bg_break_setup_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x0006A530
bool initialize_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context, std::span<const u8> parameters)
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
    if (get(parameters, parameter::grouped) == 0)
    {
        keep_runtime_bg_break_box_by_chance(context, parameters); // 0x0006A350
    }
    else
    {
        keep_runtime_bg_break_group_box(context, parameters); // 0x0006A120
    }
    set(bytes, field::spawn_tag, get(parameters, parameter::spawn_tag));
    set(bytes, field::anchor, 0);
    set(bytes, field::broken, 0);
    set(bytes, field::background, 0);
    set(bytes, field::part, -1);
    set(bytes, field::hold_part, get(parameters, parameter::hold_part));
    set(bytes, field::unk_A60, get(parameters, parameter::unk_454));
    set(bytes, field::unk_A64, get(parameters, parameter::unk_458));
    clear_runtime_bg_break_record(context); // 0x0006AF20
    set(bytes, field::spawn_slots, get(parameters, parameter::spawn_slots));
    return true;
}

// Xbox 0x0006A600
void destroy_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context)
{
    remove_ghouly_controller(&context.controller); // 0x0012ACA0
    if (context.load != nullptr)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
        set(context.bytes, field::load, 0);
    }
}

// Xbox 0x0006A7B0
void dispatch_runtime_ghouly_bg_break_setup_message(RuntimeGhoulyBgBreakSetupMessages& context,
    std::span<u32> message)
{
    auto& setup = context.setup;
    const auto bytes = setup.bytes;
    switch (message[0])
    {
    case 0x13:
        if (get(bytes, field::background) != 0)
        {
            const Vec3 position = actor_world_position(setup.actor); // 0x000412A0
            setup.break_part(get(bytes, field::background), get(bytes, field::part), position,
                setup.address_of(bytes.data())); // 0x00045D40, EDI background, ESI part
        }
        break;
    case 0x20:
        message[1] = get(bytes, field::broken);
        break;
    case 0x7C:
        if (message[1] == get(bytes, field::anchor))
        {
            set(bytes, field::anchor, 0);
        }
        for (size_t slot = 0; slot < field::spawned.count; ++slot)
        {
            if (message[1] == spawned(bytes, slot))
            {
                set_spawned(bytes, slot, 0);
            }
        }
        break;
    case 0x7D:
    {
        const Vec3 position = actor_world_position(setup.actor); // 0x000412A0
        if (read_u16(bytes, 0x1FE) != 0)
        {
            set(bytes, field::anchor, find_runtime_actor_by_id(read_u16(bytes, 0x1FE), context.latch_owner,
                context.bytes_at)); // 0x00129ED0
        }
        u32 background = get(bytes, field::background);
        i32 part = get(bytes, field::part);
        const auto backgrounds = setup.backgrounds();
        if (!find_background_part_near(position, backgrounds, background, part)) // 0x0013B980
        {
            set(bytes, field::broken, 1);
            break;
        }
        set(bytes, field::background, background);
        set(bytes, field::part, part);
        if (get(bytes, field::hold_part) != 0)
        {
            setup.hold_part(background, part); // 0x00046440, EAX part, EDI background
        }
        break;
    }
    case 0xA7:
    {
        i32 slot;
        if (!find_runtime_bg_break_spawn_slot(bytes, slot)) // 0x0006A760
        {
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_bg_break_spawn_pose(setup, source_position, source_euler, position, euler)) // 0x0006A630
        {
            break;
        }
        const auto aid = [&](size_t index) -> const char*
        {
            return message[index] == 0x5F646961 ? name_at(message, index) : nullptr;
        };
        FactoryObject* object = create_runtime_marker_actor(name_at(message, 1), source_position, source_euler, 1.f,
            aid(0x21), aid(0x61), aid(0x41), get(bytes, field::spawn_tag), 0, position, euler, nullptr, 0, 0,
            context.objects); // 0x001349E0, EDX +0x184, ECX +0x104
        if (object == nullptr)
        {
            break;
        }
        std::array<u32, 9> goal{};
        clear_runtime_goal_message(goal); // 0x00049DB0
        goal[1] = 6;
        setup.message(setup.address_of(object->bytes.get()), goal);
        message[0x81] = 1;
        set_spawned(bytes, size_t(slot), setup.address_of(object->bytes.get()));
        break;
    }
    case 0xA8:
        if (setup.load == nullptr && (get(bytes, field::unk_A60) == 0 || get(bytes, field::broken) == 0))
        {
            if (get(bytes, field::broken) == 0)
            {
                field_data(bytes, field::box)[0] = 0;
                send_runtime_bg_break_self_message(setup); // 0x0006B080
            }
            load_runtime_bg_break_box(setup, name_at(message, 1), name_at(message, 0x21),
                context.objects); // 0x0006B0A0, EAX +4, EDI +0x84
            message[0x41] = 1;
        }
        break;
    case 0xA9:
    {
        i32 slot;
        if (!find_runtime_bg_break_spawn_slot(bytes, slot)) // 0x0006A760
        {
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_bg_break_spawn_pose(setup, source_position, source_euler, position, euler)) // 0x0006A630
        {
            break;
        }
        FactoryObject* object = context.keep(create_runtime_actor_pose(name_at(message, 1), source_position,
            source_euler, context.objects.resources, context.objects.factory)); // 0x00141840
        if (object == nullptr)
        {
            break;
        }
        place_runtime_bg_break_spawn(setup, *object, position, euler);
        set_spawned(bytes, size_t(slot), setup.address_of(object->bytes.get()));
        message[0x21] = 1;
        break;
    }
    case 0xAA:
    {
        i32 slot;
        if (!find_runtime_bg_break_spawn_slot(bytes, slot)) // 0x0006A760
        {
            break;
        }
        Vec3 source_position, source_euler, position, euler;
        if (!query_runtime_bg_break_spawn_pose(setup, source_position, source_euler, position, euler)) // 0x0006A630
        {
            break;
        }
        FactoryObject* object = context.keep(create_runtime_actor_at(name_at(message, 1), &source_position,
            &source_euler, 1.f, context.objects.resources, context.objects.factory, source_position,
            source_euler)); // 0x00112C50
        if (object != nullptr)
        {
            place_runtime_bg_break_spawn(setup, *object, position, euler);
            set_spawned(bytes, size_t(slot), setup.address_of(object->bytes.get()));
        }
        message[0x21] = 1;
        break;
    }
    case 0xAB:
        if (message[1] == get(bytes, field::background) && std::bit_cast<i32>(message[2]) == get(bytes, field::part)
            && get(bytes, field::broken) == 0)
        {
            if (get(bytes, field::grouped) != 0)
            {
                share_runtime_bg_break_box(setup, context.objects); // 0x0006A170
            }
            else if (!load_runtime_bg_break_box_after_break(setup, context.objects)) // 0x0006A470
            {
                break;
            }
            scare_near_runtime_actor(setup.actor, get(bytes, field::radius), setup.marks(),
                setup.message); // 0x0013B660, ESI actor
        }
        break;
    default:
        break;
    }
    context.base_message(context.base, message); // 0x0003EF70
}

// Xbox 0x0006ADC0 after 0x000420D0
void update_runtime_ghouly_bg_break_setup(RuntimeGhoulyBgBreakSetup& context)
{
    const auto bytes = context.bytes;
    if (get(bytes, field::grouped) == 0)
    {
        // 0x0006A3F0, EAX setup.
        if (get(bytes, field::broken) == 0 && get(bytes, field::radius) != 0.f && get(bytes, field::chance) != 0.f)
        {
            const Vec3 position = actor_world_position(context.actor); // 0x000412A0
            if (spawn_queue_below_maximum(context.spawns()) // 0x0013E790
                && check_ghouly_spawn_trigger(position, get(bytes, field::radius), get(bytes, field::chance),
                    context.blocked_players, context.random) == 1) // 0x0013E980
            {
                send_runtime_bg_break_self_message(context); // 0x0006B080
            }
        }
    }
    if (context.load != nullptr && context.load->remaining_c == 0)
    {
        release_ghouly_spawn_record(*context.load); // 0x000F3A00
        context.load = nullptr;
        set(bytes, field::load, 0);
    }
}
}
