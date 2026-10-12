#pragma once
#include "gbtg/types.h"

namespace gbtg
{
using RuntimeSortCompare = int (*)(const void* left, const void* right);
// Selection pass over [low, high] by element width; EAX is high.
void runtime_short_sort(u8* low, u8* high, size_t width, RuntimeSortCompare compare); // Xbox 0x00166710
// The CRT quicksort: median of three, partitions of more than eight elements, explicit 30-entry stack.
void runtime_quick_sort(void* base, size_t count, size_t width, RuntimeSortCompare compare); // Xbox 0x00166780
}
