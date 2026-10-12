#include "body_move_actions.h"
#include "player_children.h"
#include <cstring>

namespace gbtg
{
namespace
{
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

void install_runtime_move_action_classes(std::span<ObjectFactoryType> types)
{
    types[0xA] = ObjectFactoryType{1, weapon_lifecycle};
    types[0x5B] = ObjectFactoryType{0xA, move_xyz_no_gravity_lifecycle};
    types[0x5C] = ObjectFactoryType{0xA, move_type_5c_lifecycle};
}
}
