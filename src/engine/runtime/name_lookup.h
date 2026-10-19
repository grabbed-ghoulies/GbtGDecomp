#pragma once

#include "gbtg/types.h"
#include <span>
#include <string_view>

namespace gbtg
{
// Two resource names compare equal past their "aid_" prefixes, ignoring case.
bool transition_aid_equal(std::string_view a, std::string_view b); // Xbox 0x000357D0
int binary_search_names(i32 count, const char* const* table, const char* key); // Xbox 0x000FBFB0
// A held scene order (DAT_00511B64) is 0x80-byte AIDs, ending at one not starting "aid_". The name after `current`,
// the first when `current` is null, the first after the last when `wrap` is set, or null when `current` is absent.
const char* next_scene_order_name(std::span<const u8> order, const char* current, i32 wrap); // Xbox 0x00035960
}
