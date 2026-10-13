#pragma once
#include "gbtg/types.h"

namespace gbtg
{
void* engine_heap_allocate_aligned(i32 size, u32 alignment); // Xbox 0x000F7330
}
