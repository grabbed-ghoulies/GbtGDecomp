#pragma once
#include "gbtg/object_factory.h"

namespace gbtg
{
// Body action children (statetable row actions): the weapon base (type 0xA, vtable 0x004DD714) and its
// move classes. Instance +8 holds the owning body's address.
void move_action_object_sizes(ObjectSizes& output); // Xbox 0x0007FCC0
bool initialize_runtime_weapon_action(FactoryObject& object,
    std::span<const u8> parameters); // Xbox 0x0002B600
u16 runtime_body_action_next_key(const FactoryObject& object); // Xbox 0x0002B620
void stand_action_object_sizes(ObjectSizes& output); // Xbox 0x0007FDF0
bool initialize_runtime_stand_action(FactoryObject& object,
    std::span<const u8> parameters); // Xbox 0x0007FE60, entityStateAnim2Stand
// Installs the lifecycles of type 0xA (getter 0x0002AC20) and the move classes whose constructors are
// translated: 0x5A (getter 0x000801B0, vtable 0x005054E8), 0x5B (getter 0x00080490, vtable 0x00505518), 0x5C (getter 0x000802F0, vtable 0x00505500) and
// 0x5D entityStateAnim2Stand (getter 0x0007FE10, vtable 0x005054D0).
void install_runtime_move_action_classes(std::span<ObjectFactoryType> types);
}
