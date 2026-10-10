#pragma once

#include "entity_types.h"

namespace gbtg
{
// Xbox 0x000F72D0
void* engine_heap_allocate(i32 size); // Xbox 0x000F72D0
void* engine_heap_reallocate(void* block, i32 size); // Xbox 0x000F73A0

// Xbox 0x000663E0
void engine_heap_free(void* block); // Xbox 0x000663E0

} // namespace gbtg
