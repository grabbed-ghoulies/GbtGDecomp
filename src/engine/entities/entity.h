#pragma once

#include "entity_types.h"
#include <array>

namespace gbtg
{
namespace entity_detail
{
extern WorldLists g_world;
// What a read through a null retail pointer finds (the u16 at address 0 that an empty goal ring's front reads).
extern const std::array<u8, 0x1000> g_address_zero;

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
