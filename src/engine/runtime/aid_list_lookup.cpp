#include "aid_list_lookup.h"
#include "gbtg/anim.h"

namespace gbtg
{
// Xbox 0x00035A40
i32 find_runtime_aid_list_entry(std::span<const u8> list, const char* name)
{
    if (list.data() == nullptr)
    {
        return -1;
    }
    u32 offset = 0; // The span is *EAX (the list), not the caller's pointer wrapper.
    i32 index = 0;
    do
    {
        if (resource_names_match(name, reinterpret_cast<const char*>(list.data() + offset)))
        {
            return index;
        }
        offset += 0x80;
        ++index;
        if (read_u32(list, offset) != 0x5F646961u)
        {
            return -1;
        }
    }
    while (true);
}
}
