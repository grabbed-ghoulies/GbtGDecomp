#include "scene/scene_marker_handlers.h"
#include <cstring>
#include <iostream>
#include <string>

using namespace gbtg;

int main()
{
    u32 count;
    std::cin >> count;
    for (u32 index = 0; index < count; ++index)
    {
        u32 mode, value, id, next;
        std::string first, second;
        std::array<u8, 0x800> bytes;
        std::array<u32, 6> vectors;
        std::array<u32, 9> values;
        std::cin >> mode >> value >> id >> next >> first >> second;
        for (u32& word : vectors)
        {
            std::cin >> word;
        }
        for (u32& word : values)
        {
            std::cin >> word;
        }
        for (u8& byte : bytes)
        {
            u32 word;
            std::cin >> word;
            byte = u8(word);
        }
        Vec3 position, euler;
        std::memcpy(&position, vectors.data(), 12);
        std::memcpy(&euler, vectors.data() + 3, 12);
        if (mode == 0)
        {
            build_runtime_marker_named_parameters(std::span<u8, 0x3B4>{bytes.data(), 0x3B4},
                position, euler, u16(id), u16(next), first.c_str(), second.c_str());
        }
        else if (mode == 1)
        {
            set_runtime_marker_template_names(bytes, first.c_str(), second.c_str(), value);
        }
        else if (mode == 2)
        {
            set_runtime_marker_template_chain_name(bytes, first.c_str());
        }
        else if (mode == 3)
        {
            build_runtime_marker_effect_parameters(std::span<u8, 0x460>{bytes.data(), 0x460}, position,
                euler, u16(id), u16(next), value, first.c_str(), second.c_str(), u16(value), values, second.c_str());
        }
        else if (mode == 4)
        {
            build_runtime_marker_switch_parameters(std::span<u8, 0x3C4>{bytes.data(), 0x3C4}, position,
                euler, u16(id), u16(next), value, u16(value), first.c_str(), values[0], values[1], second.c_str());
        }
        else if (mode == 5)
        {
            build_runtime_marker_named_trigger_parameters(std::span<u8, 0x4C8>{bytes.data(), 0x4C8}, position,
                euler, u16(id), u16(next), value, first.c_str(), second.c_str(),
                std::span<const u32, 4>{values.data(), 4}, second.c_str());
        }
        else
        {
            const std::array<const char*, 8> names{first.c_str(), second.c_str(), first.c_str(), second.c_str(),
                first.c_str(), second.c_str(), first.c_str(), second.c_str()};
            build_runtime_marker_multi_name_parameters(std::span<u8, 0x754>{bytes.data(), 0x754}, position, euler,
                std::span<const u32, 5>{values.data(), 5}, names, second.c_str());
        }
        for (u8 byte : bytes)
        {
            std::cout << u32(byte) << ' ';
        }
        std::cout << '\n';
    }
}
