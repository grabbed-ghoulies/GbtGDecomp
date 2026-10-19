#pragma once
#include "gbtg/object_factory.h"
#include <functional>

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
// Type 0x59 (the names' entityMoveAlert): while it lives the body's speed limit +0x334 is 70 (DAT_0032B2C4), the old
// one kept at +0x330, and +0x338 counts the holders.
void raise_runtime_body_speed_limit(std::span<u8> body); // Xbox 0x0007FD30, action +8 the body
void restore_runtime_body_speed_limit(std::span<u8> body); // Xbox 0x0009F150, action +8 the body
// Installs the lifecycles of type 0xA (getter 0x0002AC20) and the move classes whose constructors are
// translated: 0x5A (getter 0x000801B0, vtable 0x005054E8), 0x5B (getter 0x00080490, vtable 0x00505518), 0x5C (getter 0x000802F0, vtable 0x00505500) and
// 0x5D entityStateAnim2Stand (getter 0x0007FE10, vtable 0x005054D0), and 0x59 (getter 0x0007FCE0, vtable 0x005054B8),
// whose constructor and destructor reach the body at action +8 through `body_at`.
void install_runtime_move_action_classes(std::span<ObjectFactoryType> types,
    std::function<std::span<u8>(u32)> body_at);
}
