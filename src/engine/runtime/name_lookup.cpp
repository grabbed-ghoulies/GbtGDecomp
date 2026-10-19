#include "name_lookup.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x000FBFB0. Binary search. High starts at count-1. A miss returns -1.
int binary_search_names(i32 count, const char* const* table, const char* key)
{
    i32 high = count - 1;
    i32 low = 0;
    if (high < 0)
    {
        return -1;
    }
    const u8* want = reinterpret_cast<const u8*>(key);
    while (low <= high)
    {
        const i32 mid = (low + high) >> 1;
        const u8* row = reinterpret_cast<const u8*>(table[mid]);
        u8 tc = row[0];
        u8 kc = want[0];
        if (tc == kc)
        {
            i32 i = 0;
            for (;;)
            {
                const u8 prev = row[i];
                i += 1;
                if (prev == 0)
                {
                    return mid;
                }
                tc = row[i];
                kc = want[i];
                if (tc != kc)
                {
                    break;
                }
            }
        }
        const i32 diff = i32(tc) - i32(kc);
        if (diff < 0)
        {
            low = mid + 1;
        }
        else if (diff > 0)
        {
            high = mid - 1;
        }
        else
        {
            return mid;
        }
    }
    return -1;
}

// Xbox 0x000357D0
bool transition_aid_equal(std::string_view a, std::string_view b)
{
    if (!a.starts_with("aid_"))
    {
        return !b.starts_with("aid_");
    }
    if (!b.starts_with("aid_"))
    {
        return false;
    }
    for (size_t i = 4;; ++i)
    {
        const u8 ac = i < a.size() ? u8(a[i]) & 0xdf : 0;
        const u8 bc = i < b.size() ? u8(b[i]) & 0xdf : 0;
        if (ac != bc)
        {
            return false;
        }
        if (ac == 0)
        {
            return true;
        }
    }
}

// Xbox 0x00035960, EAX the held scene order, EBX the current name: the name after it, wrapping when asked.
const char* next_scene_order_name(std::span<const u8> order, const char* current, i32 wrap)
{
    if (order.data() == nullptr)
    {
        return nullptr;
    }
    const auto entry = [&order](size_t offset) -> const char*
    {
        return reinterpret_cast<const char*>(order.data() + offset);
    };
    const auto is_aid = [&order, &entry](size_t offset)
    {
        return offset + 4 <= order.size() && std::memcmp(entry(offset), "aid_", 4) == 0;
    };
    if (current == nullptr)
    {
        return entry(0);
    }
    size_t at = 0;
    while (!transition_aid_equal(entry(at), current)) // 0x000357D0
    {
        if (!is_aid(at + 0x80))
        {
            return nullptr;
        }
        at += 0x80;
    }
    if (!is_aid(at + 0x80))
    {
        return wrap == 0 ? nullptr : entry(0);
    }
    return entry(at + 0x80);
}
}
