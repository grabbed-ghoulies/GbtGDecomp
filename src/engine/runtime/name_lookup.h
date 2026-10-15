#pragma once

#include "gbtg/types.h"
#include <string_view>

namespace gbtg
{
// Two resource names compare equal past their "aid_" prefixes, ignoring case.
bool transition_aid_equal(std::string_view a, std::string_view b); // Xbox 0x000357D0
int binary_search_names(i32 count, const char* const* table, const char* key); // Xbox 0x000FBFB0
}
