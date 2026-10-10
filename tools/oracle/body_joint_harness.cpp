// Host side of body_joint_oracle.py, using the actual reconstructed dependencies.
#include "entities/player_body_motion.h"
#include <bit>
#include <iostream>
#include <string>

using namespace gbtg;

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "angle")
        {
            std::array<u32, 6> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::cout << std::bit_cast<u32>(ease_runtime_body_angle(
                std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]),
                std::bit_cast<float>(words[2]), std::bit_cast<float>(words[3]),
                std::bit_cast<float>(words[4]), std::bit_cast<float>(words[5]))) << '\n';
            continue;
        }
        std::array<u8, 0x924> body;
        for (u8& byte : body)
        {
            u32 value;
            std::cin >> value;
            byte = u8(value);
        }
        std::array<u32, 11> words;
        for (u32& word : words)
        {
            std::cin >> word;
        }
        size_t slot;
        std::cin >> slot;
        Actor actor;
        actor.euler = {std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]), std::bit_cast<float>(words[2])};
        actor.animation_rate = std::bit_cast<float>(words[3]);
        actor.animation_pause = std::bit_cast<float>(words[4]);
        float freeze = std::bit_cast<float>(words[5]);
        float dt = std::bit_cast<float>(words[6]);
        float scale = std::bit_cast<float>(words[7]);
        u32 block = 0, index = 0;
        std::array<u8, 1> enabled{1};
        RuntimeActorClock clock{freeze, dt, scale, block, enabled, index};
        const Vec3 joint{std::bit_cast<float>(words[8]), std::bit_cast<float>(words[9]), std::bit_cast<float>(words[10])};
        std::array<u8*, 6> records{};
        RuntimeBodyBitStorage* flags = nullptr;
        RuntimePlayerJointTargets context{actor, flags, records, clock, 0x10001000};
        u32 tracking;
        const float weight = select_runtime_player_joint_target(body, slot, joint, tracking, context);
        std::cout << std::bit_cast<u32>(weight) << ' ' << tracking;
        for (u8 byte : body)
        {
            std::cout << ' ' << u32(byte);
        }
        std::cout << '\n';
    }
}
