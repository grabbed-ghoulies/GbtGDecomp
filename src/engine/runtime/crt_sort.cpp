#include "crt_sort.h"

namespace gbtg
{
namespace
{
void swap_sort_elements(u8* left, u8* right, size_t width)
{
    if (left != right)
    {
        for (size_t index = 0; index < width; ++index)
        {
            const u8 value = left[index];
            left[index] = right[index];
            right[index] = value;
        }
    }
}
}

// Xbox 0x00166710
void runtime_short_sort(u8* low, u8* high, size_t width, RuntimeSortCompare compare)
{
    while (high > low)
    {
        u8* largest = low;
        for (u8* probe = low + width; probe <= high; probe += width)
        {
            if (compare(probe, largest) > 0)
            {
                largest = probe;
            }
        }
        if (largest != high && width != 0)
        {
            swap_sort_elements(largest, high, width);
        }
        high -= width;
    }
}

// Xbox 0x00166780
void runtime_quick_sort(void* base, size_t count, size_t width, RuntimeSortCompare compare)
{
    if (count < 2 || width == 0)
    {
        return;
    }
    u8* low_stack[30];
    u8* high_stack[30];
    i32 depth = 0;
    u8* low = static_cast<u8*>(base);
    u8* high = low + (count - 1) * width;
    while (true)
    {
        const size_t size = size_t(high - low) / width + 1;
        if (size <= 8)
        {
            runtime_short_sort(low, high, width, compare);
        }
        else
        {
            u8* middle = low + (size >> 1) * width;
            if (compare(low, middle) > 0)
            {
                swap_sort_elements(low, middle, width);
            }
            if (compare(low, high) > 0)
            {
                swap_sort_elements(low, high, width);
            }
            if (compare(middle, high) > 0)
            {
                swap_sort_elements(middle, high, width);
            }
            u8* low_guy = low;
            u8* high_guy = high;
            while (true)
            {
                if (middle > low_guy)
                {
                    do
                    {
                        low_guy += width;
                    } while (low_guy < middle && compare(low_guy, middle) <= 0);
                }
                if (middle <= low_guy)
                {
                    do
                    {
                        low_guy += width;
                    } while (low_guy <= high && compare(low_guy, middle) <= 0);
                }
                do
                {
                    high_guy -= width;
                } while (high_guy > middle && compare(high_guy, middle) > 0);
                if (high_guy < low_guy)
                {
                    break;
                }
                swap_sort_elements(low_guy, high_guy, width);
                if (middle == high_guy)
                {
                    middle = low_guy;
                }
            }
            high_guy += width;
            if (middle < high_guy)
            {
                do
                {
                    high_guy -= width;
                } while (high_guy > middle && compare(high_guy, middle) == 0);
            }
            if (middle >= high_guy)
            {
                do
                {
                    high_guy -= width;
                } while (high_guy > low && compare(high_guy, middle) == 0);
            }
            if (high_guy - low >= high - low_guy)
            {
                if (low < high_guy)
                {
                    low_stack[depth] = low;
                    high_stack[depth] = high_guy;
                    ++depth;
                }
                if (low_guy < high)
                {
                    low = low_guy;
                    continue;
                }
            }
            else
            {
                if (low_guy < high)
                {
                    low_stack[depth] = low_guy;
                    high_stack[depth] = high;
                    ++depth;
                }
                if (low < high_guy)
                {
                    high = high_guy;
                    continue;
                }
            }
        }
        --depth;
        if (depth < 0)
        {
            return;
        }
        low = low_stack[depth];
        high = high_stack[depth];
    }
}
}
