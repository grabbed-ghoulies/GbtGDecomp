// Host side of mind_request_update_oracle.py. player_mind_request_update.cpp is linked alone; every
// callee is a logging stub returning the case's scripted value.
#include "entities/player_mind_request_update.h"
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
using namespace gbtg;

namespace
{
std::vector<std::string> g_log;
u32 g_flag = 0;
u32 g_navigate = 0;
u32 g_parent = 0;
const u8* g_body = nullptr;

template <typename T>
T& junk()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}

void add(const std::string& text)
{
    g_log.push_back(text);
}
}

namespace gbtg
{
u32 runtime_body_flag_name_index(const char* name, std::span<const char* const, 0x41>)
{
    add(std::string("flag ") + name);
    return g_flag;
}

bool select_runtime_body_action(std::span<u8> body, u16 key, RuntimeBodyActionSelection&)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "select %x %d", key, body.data() == g_body ? 1 : 0);
    add(buffer);
    return true;
}

void update_runtime_player_control(RuntimePlayerControlUpdate&)
{
    add("control 1");
}

u32 handle_runtime_player_navigation_request(RuntimePlayerNavigationControl&, std::span<const u8> message)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "navigate %d 1", read_u16(message, 8) == 0x2E ? 1 : 0);
    add(buffer);
    return g_navigate;
}

i32 update_runtime_parent_mind_request(RuntimeParentMindRequests&, std::span<const u8>)
{
    add("parent 1 1");
    return i32(g_parent);
}
}

int main()
{
    int count;
    std::cin >> count;
    for (int test = 0; test < count; ++test)
    {
        u32 kind, effect, contact;
        std::array<u32, 8> bits{};
        std::cin >> kind >> g_flag;
        for (u32& value : bits)
        {
            std::cin >> value;
        }
        std::cin >> g_navigate >> g_parent >> effect >> contact;
        g_log.clear();
        std::array<u8, 8> flag_bytes{};
        for (size_t index = 0; index < 8; ++index)
        {
            flag_bytes[index] = u8(bits[index]);
        }
        EntityBitBlock flags{flag_bytes.data()};
        std::array<u8, 0x100> body{};
        g_body = body.data();
        std::array<u8, 0x40> request_bytes{};
        const u16 kind16 = u16(kind);
        std::memcpy(request_bytes.data() + 8, &kind16, 2);
        std::memcpy(request_bytes.data() + 0x10, "flagname", 9);
        RuntimePlayerMindRequest request{nullptr, nullptr, kind16, 0, request_bytes};
        static const char* const names[0x41]{};
        RuntimePlayerMindRequestUpdate context{junk<RuntimeParentMindRequests>(), junk<RuntimePlayerControlUpdate>(),
            junk<RuntimePlayerNavigationControl>(), body, flags, junk<RuntimeBodyActionSelection>(), names,
            [effect]()
            {
                add("effectkey 1");
                return u16(effect);
            },
            [contact]()
            {
                add("contactkey 1");
                return u16(contact);
            }};
        const i32 result = update_runtime_player_mind_request(context, request);
        std::string out;
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%x |", u32(result));
        out = buffer;
        for (size_t index = 0; index < g_log.size(); ++index)
        {
            out += (index == 0 ? " " : " ; ") + g_log[index];
        }
        std::printf("%s\n", out.c_str());
    }
    return 0;
}
