#pragma once
#include "gbtg/types.h"
#include <span>

namespace gbtg
{
i32 find_runtime_aid_list_entry(std::span<const u8> list,
    const char* name); // Xbox 0x00035A40
}
