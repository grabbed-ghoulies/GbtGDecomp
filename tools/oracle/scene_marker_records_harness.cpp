// Record-only differential harness. Marker handler bodies are not exercised here.
#include "scene/scene_markers.h"
#include <iostream>
#include <vector>

using namespace gbtg;

namespace
{
u32 checksum(std::span<const u8> bytes)
{
    u32 value = 2166136261u;
    for (u8 byte : bytes)
    {
        value = (value ^ byte) * 16777619u;
    }
    return value;
}
u32 input()
{
    u32 value;
    std::cin >> value;
    return value;
}
}

int main()
{
    const u32 cases = input();
    for (u32 index = 0; index < cases; ++index)
    {
        const u32 mode = input();
        u32 random = input();
        std::vector<u8> records(input());
        for (u8& byte : records)
        {
            byte = u8(input());
        }
        const i32 swap_count = i32(input());
        std::vector<u16> swaps(size_t(swap_count) * 2);
        for (u16& id : swaps)
        {
            id = u16(input());
        }
        const i32 group_count = i32(input());
        std::vector<u16> groups(size_t(group_count) * 10);
        for (u16& id : groups)
        {
            id = u16(input());
        }
        const u32 handler_mask = input();
        const u16 id = u16(input());
        const i16 kind = i16(input());
        const i32 wrap = i32(input());
        RuntimeSceneMarkerRandomizer randomizer{random, swap_count, swaps, group_count, groups};
        RuntimeSceneMarkerHandlers handlers{};
        std::vector<u32> calls;
        for (u32 handler = 0; handler < 0x1C; ++handler)
        {
            if ((handler_mask & (1u << handler)) != 0)
            {
                handlers.handlers[handler] = [&calls](u8* row)
                {
                    const std::span<const u8> header{row, 8};
                    calls.push_back(read_u16(header, 4));
                    calls.push_back(read_u16(header, 6));
                    calls.push_back(checksum({row, read_u32(header, 0)}));
                };
            }
        }
        u32 result = 0;
        switch (mode)
        {
        case 0:
            swap_runtime_marker_pairs(records.data(), randomizer);
            break;
        case 1:
            shuffle_runtime_marker_groups(records.data(), randomizer);
            break;
        case 2:
            dispatch_runtime_marker_records(records.data(), handlers);
            break;
        case 3:
            result = next_runtime_marker_id(id, kind, wrap, records.data());
            break;
        case 4:
            restart_runtime_marker_player(records.data(), handlers);
            break;
        case 5:
            start_runtime_scene_markers(records.data(), randomizer, handlers);
            break;
        }
        std::cout << result << ' ' << random << ' ' << checksum(records) << ' ' << calls.size();
        for (u32 value : calls)
        {
            std::cout << ' ' << value;
        }
        std::cout << '\n';
    }
}
