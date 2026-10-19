#include "body_move_actions.h"
#include "player_children.h"
#include <cstring>

namespace gbtg
{
namespace
{
std::function<std::span<u8>(u32)> action_body_at; // A body's bytes by its retail address (action +8).

std::span<u8> body_of_action(const FactoryObject& object)
{
    return action_body_at(read_u32({object.bytes.get(), object.size}, 8));
}

bool construct_speed_limit_action(FactoryObject& object, std::span<const u8>)
{
    raise_runtime_body_speed_limit(body_of_action(object)); // 0x0007FD30
    return true;
}

void destroy_speed_limit_action(FactoryObject& object)
{
    restore_runtime_body_speed_limit(body_of_action(object)); // 0x0009F150
}

const ObjectFactoryVtable* speed_limit_lifecycle()
{
    static const ObjectFactoryVtable table{0x005054B8, move_action_object_sizes, construct_speed_limit_action,
        destroy_speed_limit_action};
    return &table;
}

// Xbox 0x001628B0
void destroy_empty_action(FactoryObject&)
{
}

const ObjectFactoryVtable* weapon_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD714, move_action_object_sizes, initialize_runtime_weapon_action,
        destroy_empty_action};
    return &table;
}

const ObjectFactoryVtable* move_type_5c_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505500, move_action_object_sizes, accept_runtime_object_construction,
        destroy_empty_action};
    return &table;
}

const ObjectFactoryVtable* move_xz_gravity_lifecycle()
{
    static const ObjectFactoryVtable table{0x005054E8, move_action_object_sizes, accept_runtime_object_construction,
        destroy_empty_action};
    return &table;
}

const ObjectFactoryVtable* stand_action_lifecycle()
{
    static const ObjectFactoryVtable table{0x005054D0, stand_action_object_sizes, initialize_runtime_stand_action,
        destroy_empty_action};
    return &table;
}

const ObjectFactoryVtable* move_xyz_no_gravity_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505518, move_action_object_sizes, accept_runtime_object_construction,
        destroy_empty_action};
    return &table;
}
}

// Xbox 0x0007FCC0
void move_action_object_sizes(ObjectSizes& output)
{
    output.instance = 0x10;
    output.parameters = 0xC;
}

// Xbox 0x0002B600
bool initialize_runtime_weapon_action(FactoryObject& object, std::span<const u8> parameters)
{
    std::memcpy(object.bytes.get() + 8, parameters.data() + 8, 4);
    return true;
}

// Xbox 0x0007FD30
void raise_runtime_body_speed_limit(std::span<u8> body)
{
    std::memcpy(body.data() + 0x330, body.data() + 0x334, 4);
    const float limit = 70.f; // DAT_0032B2C4.
    std::memcpy(body.data() + 0x334, &limit, 4);
    const u32 holders = read_u32(body, 0x338) + 1;
    std::memcpy(body.data() + 0x338, &holders, 4);
}

// Xbox 0x0009F150
void restore_runtime_body_speed_limit(std::span<u8> body)
{
    std::memcpy(body.data() + 0x334, body.data() + 0x330, 4);
    const u32 holders = read_u32(body, 0x338) - 1;
    std::memcpy(body.data() + 0x338, &holders, 4);
}

void install_runtime_move_action_classes(std::span<ObjectFactoryType> types, std::function<std::span<u8>(u32)> body_at)
{
    action_body_at = std::move(body_at);
    types[0x59] = ObjectFactoryType{0xA, speed_limit_lifecycle};
    types[0xA] = ObjectFactoryType{1, weapon_lifecycle};
    types[0x5A] = ObjectFactoryType{0xA, move_xz_gravity_lifecycle};
    types[0x5B] = ObjectFactoryType{0xA, move_xyz_no_gravity_lifecycle};
    types[0x5C] = ObjectFactoryType{0xA, move_type_5c_lifecycle};
    types[0x5D] = ObjectFactoryType{0xA, stand_action_lifecycle};
}

// Xbox 0x0007FDF0
void stand_action_object_sizes(ObjectSizes& output)
{
    output.instance = 0x3C;
    output.parameters = 0xC;
}

// Xbox 0x0007FE60
bool initialize_runtime_stand_action(FactoryObject& object, std::span<const u8>)
{
    const u32 one = 1;
    std::memcpy(object.bytes.get() + 0x10, &one, 4);
    return true;
}

// Xbox 0x0002B620
u16 runtime_body_action_next_key(const FactoryObject& object)
{
    return read_u16({object.bytes.get(), object.size}, 0xC);
}
}
