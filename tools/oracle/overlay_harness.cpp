// Host side of overlay_oracle.py: player body overlay 0x0001DF70 and digits 0x0001E3E0.
#include "entities/player_overlay.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
using namespace gbtg;

namespace gbtg
{
// Heap boundary used by 0x000412A0 for a stale bind; the oracle never supplies one.
void engine_heap_free(void* block)
{
    std::free(block);
}
}

namespace
{
float as_float(u32 bits)
{
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
u32 bits_of(float value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}
void put(std::vector<u8>& bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        std::array<std::array<u8, 0x20>, 14> sprites{};
        std::array<const u8*, 10> digits{};
        for (size_t index = 0; index < 4; ++index)
        {
            u32 frames, divisor;
            std::cin >> frames >> divisor;
            sprites[index][0x10] = u8(frames);
            sprites[index][0x11] = u8(divisor);
        }
        for (size_t index = 0; index < 10; ++index)
        {
            u32 width;
            std::cin >> width;
            sprites[4 + index][8] = u8(width);
            sprites[4 + index][9] = u8(width >> 8);
            digits[index] = sprites[4 + index].data();
        }
        DisplayView view;
        u32 fx, fy, fz;
        std::cin >> fx >> fy >> fz;
        view.forward = Vec3{as_float(fx), as_float(fy), as_float(fz)};
        static BillboardQueue queue;
        queue.count = 0;
        if (kind == "digits")
        {
            i32 value;
            u32 px, py, pz, color;
            std::cin >> value >> px >> py >> pz >> color;
            queue_runtime_overlay_digits(value, Vec3{as_float(px), as_float(py), as_float(pz)}, color, digits.data(), view, queue);
        }
        else
        {
            i32 draw_kind;
            std::cin >> draw_kind;
            std::vector<u8> body(0x100);
            u32 e0, dc, a4, a5, a8, a9, health, maximum, bc;
            std::cin >> e0 >> dc >> a4 >> a5 >> a8 >> a9 >> health >> maximum >> bc;
            put(body, 0xE0, e0);
            put(body, 0xDC, dc);
            body[0xA4] = u8(a4);
            body[0xA5] = u8(a5);
            body[0xA8] = u8(a8);
            body[0xA9] = u8(a9);
            body[0x94] = u8(health);
            body[0x95] = u8(health >> 8);
            body[0x96] = u8(maximum);
            body[0x97] = u8(maximum >> 8);
            put(body, 0xBC, bc);
            Actor owner;
            u32 values[8];
            for (u32& value : values)
            {
                std::cin >> value;
            }
            owner.position = Vec3{as_float(values[0]), as_float(values[1]), as_float(values[2])};
            owner.radius_y = as_float(values[3]);
            owner.radius_xz = as_float(values[4]);
            owner.model_size_x = as_float(values[5]);
            owner.model_size_y = as_float(values[6]);
            owner.model_size_z = as_float(values[7]);
            std::vector<u8> owner_bytes(0xA00);
            u32 tag, time, flag, timer, blocked, has_attributes, attribute_word;
            std::cin >> tag >> time >> flag >> timer >> blocked >> has_attributes >> attribute_word;
            owner_bytes[6] = u8(tag);
            owner_bytes[7] = u8(tag >> 8);
            put(owner_bytes, 0x924, time);
            owner_bytes[0x969] = u8(flag);
            put(owner_bytes, 0x978, timer);
            put(owner_bytes, 0x98C, blocked);
            std::vector<u8> attributes(0xC0);
            put(attributes, 0xB8, attribute_word);
            u32 has_attached, state, count;
            std::cin >> has_attached >> state >> count;
            std::vector<u8> attached_bytes(0xA00);
            attached_bytes[0x93D] = u8(state);
            attached_bytes[0x944] = u8(count);
            for (u32 row = 0; row < count; ++row)
            {
                u32 key, value;
                std::cin >> key >> value;
                attached_bytes[0x948 + row * 0x10] = u8(key);
                attached_bytes[0x949 + row * 0x10] = u8(key >> 8);
                attached_bytes[0x94C + row * 0x10] = u8(value);
                attached_bytes[0x94D + row * 0x10] = u8(value >> 8);
            }
            RuntimeBodyAttachedActor attached{attached_bytes, 0, {}};
            RuntimeBodyOverlay overlay{body, owner, owner_bytes,
                has_attributes != 0 ? std::span<const u8>(attributes) : std::span<const u8>(), has_attached != 0 ? &attached : nullptr,
                sprites[0].data(), sprites[1].data(), sprites[2].data(), sprites[3].data(), digits.data(), view, queue};
            queue_runtime_body_overlay(draw_kind, overlay);
        }
        std::printf("%d", queue.count);
        for (i32 index = 0; index < queue.count; ++index)
        {
            const auto& entry = queue.entries[size_t(index)];
            const i32 sprite = i32((entry.sprite - sprites[0].data()) / 0x20);
            std::printf(" %d:%08X,%08X,%08X:%08X,%08X:%08X:%08X:%u", sprite, bits_of(entry.position.x), bits_of(entry.position.y),
                bits_of(entry.position.z), bits_of(entry.size[0]), bits_of(entry.size[1]), bits_of(entry.rotation), entry.color, entry.frame);
        }
        std::printf("\n");
    }
}
