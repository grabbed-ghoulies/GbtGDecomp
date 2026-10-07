#pragma once

#include "entity_types.h"

namespace gbtg
{
namespace entity_detail
{
void body_apply(Entity&);

extern WorldLists g_world;

// Xbox 0x000253B0
// Message 99. The input delta is zero. 0x00120760 keeps the body when its Y
// sits in the slab and its XZ is inside the combined radius, then 0x00120250
// scales the horizontal separation out to that radius. A hit adds the
// correction to body+0x18 and stores the midpoint at DAT_005111AC.
bool push_out(Entity& body, const Message& msg); // Xbox 0x000253B0

} // namespace entity_detail
// Xbox 0x000FCD70
void entity_copy_match_block(void* dest, const void* src); // Xbox 0x000FCD70

// 0x0032B270

// Xbox 0x0010AE90
i32 entity_match_blocks_equal(const EntityMatchBlock& left, const EntityMatchBlock& right); // Xbox 0x0010AE90

// namespace

// Xbox 0x00152BB0
float engine_absolute(float value); // Xbox 0x00152BB0

// Xbox 0x001534C0
float engine_reciprocal_sqrt(float value); // Xbox 0x001534C0

} // namespace gbtg
