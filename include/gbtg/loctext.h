#pragma once

#include "gbtg/types.h"

#include <map>

namespace gbtg
{

struct LocTable
{
    std::string name;
    std::map<std::string, std::string> keys_to_utf8;
};

LocTable parse_loctext(std::span<const u8> file_bytes, std::string name = {});

} // namespace gbtg
