#pragma once

#include "gbtg/types.hpp"

namespace gbtg
{

// Rare "base" AID filename hash used by Grabbed by the Ghoulies.
// Skips the "aid_" prefix; remaining bytes are folded with uppercase ASCII.
constexpr u32 aid_hash(std::string_view name)
{
    u32 hash = 0;
    size_t i = 0;
    if (name.size() >= 4 && name[0] == 'a' && name[1] == 'i' && name[2] == 'd' && name[3] == '_')
    {
        i = 4;
    }
    for (; i < name.size(); ++i)
    {
        hash = (hash << 4) + (static_cast<u8>(name[i]) & 0xDF);
        const u32 hi = hash & 0xF0000000u;
        if (hi)
        {
            hash ^= hi | (hi >> 24);
        }
    }
    return hash;
}

inline std::string aid_hash_hex(u32 hash)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string s(8, '0');
    for (int i = 7; i >= 0; --i)
    {
        s[i] = kHex[hash & 0xF];
        hash >>= 4;
    }
    return s;
}

inline bool parse_hex8(std::string_view s, u32& out)
{
    if (s.size() != 8)
    {
        return false;
    }
    u32 v = 0;
    for (char c : s)
    {
        v <<= 4;
        if (c >= '0' && c <= '9')
        {
            v |= u32(c - '0');
        }
        else if (c >= 'a' && c <= 'f')
        {
            v |= u32(c - 'a' + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            v |= u32(c - 'A' + 10);
        }
        else
        {
            return false;
        }
    }
    out = v;
    return true;
}

static_assert(aid_hash("aid_texture_ghoulies_scoreboard_eventicons_dontgethit") == 0x00559fa4u);
static_assert(aid_hash("aid_texture_ghoulies_scoreboard_energy_container_syringes") == 0x02fd7fb3u);

} // namespace gbtg
