#pragma once
#include "gbtg/object_factory.h"

namespace gbtg
{
// Body action children (statetable row actions): the weapon base (type 0xA, vtable 0x004DD714) and its
// move classes. Instance +8 holds the owning body's address.
void move_action_object_sizes(ObjectSizes& output); // Xbox 0x0007FCC0
bool initialize_runtime_weapon_action(FactoryObject& object,
    std::span<const u8> parameters); // Xbox 0x0002B600
// Installs the lifecycles of type 0xA (getter 0x0002AC20) and the move classes whose constructors are
// translated: 0x5B entityMoveXYZnoG (getter 0x00080490, vtable 0x00505518).
void install_runtime_move_action_classes(std::span<ObjectFactoryType> types);
}
