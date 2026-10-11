// Host side of body_joint_oracle.py, using the actual reconstructed dependencies.
#include "entities/player_body_motion.h"
#include "entities/body_channels.h"
#include "entities/body_timers.h"
#include "entities/body_wash.h"
#include "entities/body_shape_push.h"
#include "entities/body_action_ids.h"
#include "entities/body_animation_commands.h"
#include "entities/body_damage_tables.h"
#include <bit>
#include <cstring>
#include <iostream>
#include <string>

using namespace gbtg;

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "damagevalue")
        {
            std::array<u32, 12> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 256 * 12> values{};
            std::array<u8, 0x1000> classes;
            classes.fill(u8(words[11]));
            for (size_t offset = 0; offset < values.size(); offset += 12)
            {
                values[offset] = u8(words[3]);
                values[offset + 1] = u8(words[4]);
                std::memcpy(values.data() + offset + 4, &words[5], 4);
                std::memcpy(values.data() + offset + 8, &words[6], 4);
            }
            RuntimeBodyDamageTables tables{values, classes};
            Actor actor;
            actor.mask_mode = i16(words[7]);
            actor.mask_flag = std::bit_cast<i32>(words[8]);
            actor.damage_class = u16(words[9]);
            u32 result;
            switch (words[0])
            {
            case 0:
                result = std::bit_cast<u32>(interpolate_runtime_damage_value(u8(words[1]), u8(words[2]), tables));
                break;
            case 1:
                result = std::bit_cast<u32>(runtime_class_damage_value(u8(words[1]), u16(words[10]), tables));
                break;
            case 2:
                result = u32(runtime_actor_damage_value(u8(words[1]), actor.damage_class, tables));
                break;
            case 3:
                result = u32(runtime_body_damage_pulse_count(actor, u16(words[10]), tables));
                break;
            case 4:
                result = std::bit_cast<u32>(runtime_body_damage_pause(actor, u16(words[10]), tables));
                break;
            case 5:
                result = std::bit_cast<u32>(runtime_body_damage_source_pause(actor, u16(words[10]), tables));
                break;
            default:
                return 2;
            }
            std::cout << result << '\n';
            continue;
        }
        if (kind == "actionid")
        {
            std::array<u32, 10> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 0xD00> attached_bytes{}, host_bytes{};
            const u16 count = u16(words[2]);
            std::memcpy(attached_bytes.data() + 0x944, &count, 2);
            for (size_t index = 0; index < 4; ++index)
            {
                const size_t offset = index < 2 ? 0x948 + index * 4 : 0x958 + (index - 2) * 4;
                const u16 value = u16(words[4 + index]);
                std::memcpy(attached_bytes.data() + offset, &value, 2);
            }
            attached_bytes[0x93D] = u8(words[1]);
            attached_bytes[0x93F] = u8(words[8]);
            const u16 mode = u16(words[9] >> 1);
            std::memcpy(host_bytes.data() + 0xCDC, &mode, 2);
            RuntimeBodyAttachedActor host, attached;
            host.bytes = host_bytes;
            attached.bytes = attached_bytes;
            attached.host = (words[9] & 1) != 0 ? &host : nullptr;
            RuntimeBodyActionIds ids{&attached, u16(words[3]), 0x10001000};
            u32 result = 0;
            switch (words[0])
            {
            case 0:
                result = runtime_body_primary_action_id(ids);
                break;
            case 1:
                result = runtime_body_secondary_action_id(ids);
                break;
            case 2:
                result = runtime_body_contact_action_id(ids);
                break;
            case 3:
                result = runtime_body_effect_action_id(ids);
                break;
            case 4:
                result = runtime_body_damage_action_id(ids);
                break;
            case 5:
                result = runtime_body_attachment_action_id(ids);
                break;
            case 6:
                result = runtime_body_range_action_id(ids);
                break;
            case 7:
            case 8:
                {
                    u16 key = 0xABCD, fallback = 0x1234;
                    if (words[0] == 7)
                    {
                        runtime_body_primary_action_pair(ids, key, fallback);
                    }
                    else
                    {
                        runtime_body_secondary_action_pair(ids, key, fallback);
                    }
                    result = u32(key) | (u32(fallback) << 16);
                }
                break;
            default:
                return 2;
            }
            std::cout << result << '\n';
            continue;
        }
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
        if (kind != "joint")
        {
            entity_detail::LatchNode* first = nullptr;
            RuntimeBodyChannelBindings* animation = nullptr;
            RuntimeModelInstance* model = nullptr;
            std::span<u8> rows;
            RuntimeBodyChannelContext channels{actor, clock, animation, model, rows, {}, {}, {}};
            if (kind == "damagepulse")
            {
                actor.mask_mode = (words[9] & 1) != 0 ? 2 : 0;
                actor.mask_flag = i32((words[9] >> 1) & 1);
                reduce_runtime_body_damage_pulse(body, actor, std::bit_cast<i32>(words[8]));
            }
            else if (kind == "damagehealth")
            {
                actor.mask_mode = (words[10] & 1) != 0 ? 2 : 0;
                actor.mask_flag = i32((words[10] >> 1) & 1);
                EntityBitBlock flags{body.data() + 0x400};
                const std::array<u32, 2> blocked{(words[10] >> 2) & 1, (words[10] >> 2) & 1};
                const std::array<u8, 1> modes{u8(words[10] >> 8)};
                const u32 result = subtract_runtime_body_damage_health(body, actor, flags,
                    words[8], words[9], blocked, modes, 0);
                std::memcpy(body.data() + 0x110, &result, 4);
            }
            else if (kind == "commandtimer")
            {
                u32 random = words[10];
                command_runtime_body_random_timer(body, std::bit_cast<float>(words[8]),
                    std::bit_cast<float>(words[9]), random);
                std::memcpy(body.data() + 0x110, &random, 4);
            }
            else if (kind == "commandcounters")
            {
                command_runtime_body_side_counters(body, words[8], words[9]);
            }
            else if (kind == "commandpush")
            {
                std::memcpy(&actor.anchor, body.data() + 0x100, sizeof(actor.anchor));
                add_runtime_body_direction_push(actor, std::bit_cast<i32>(words[8]),
                    std::bit_cast<float>(words[9]));
                std::memcpy(body.data() + 0x100, &actor.anchor, sizeof(actor.anchor));
            }
            else if (kind == "movement")
            {
                add_runtime_player_body_movement(body, joint, actor, clock);
            }
            else if (kind == "blends")
            {
                tick_runtime_body_channel_blends(body, channels);
            }
            else if (kind == "pulse")
            {
                tick_runtime_body_pulse(body, actor, first, clock);
            }
            else if (kind == "timers")
            {
                RuntimeBodyTimerRecipient* first_recipient = nullptr;
                RuntimeBodyTimerRecipient* second_recipient = nullptr;
                tick_runtime_body_timers(body, actor, first_recipient, second_recipient, first, clock);
            }
            else if (kind == "cylinder")
            {
                EntityCylinderBound moving, fixed;
                std::memcpy(&moving, body.data(), sizeof(moving));
                std::memcpy(&fixed, body.data() + 0x40, sizeof(fixed));
                Vec3 push, contact;
                std::memcpy(&push, body.data() + 0x100, sizeof(push));
                std::memcpy(&contact, body.data() + 0x110, sizeof(contact));
                body[0x10C] = u8(push_runtime_collision_cylinder(moving, fixed, joint, push, contact));
                std::memcpy(body.data() + 0x100, &push, sizeof(push));
                std::memcpy(body.data() + 0x110, &contact, sizeof(contact));
            }
            else if (kind == "triangle" || kind == "sphere")
            {
                Vec3 point{read_f32(body, 0x100), read_f32(body, 0x104), read_f32(body, 0x108)};
                const bool result = kind == "triangle"
                    ? separate_runtime_collision_triangle(body, point, std::bit_cast<float>(words[8]))
                    : separate_runtime_collision_sphere(body, point, std::bit_cast<float>(words[8]));
                std::memcpy(body.data() + 0x100, &point, sizeof(point));
                body[0x10C] = u8(result);
            }
            else if (kind == "wash")
            {
                std::array<u8, 0xD00> actor_bytes{};
                std::memcpy(actor_bytes.data() + 0x978, &words[8], 4);
                std::memcpy(actor_bytes.data() + 0x98C, &words[9], 4);
                actor_bytes[0x969] = u8(slot);
                tick_runtime_body_wash(body, actor_bytes, nullptr, clock);
            }
            else
            {
                return 2;
            }
            std::cout << "0 0";
            for (u8 byte : body)
            {
                std::cout << ' ' << u32(byte);
            }
            std::cout << '\n';
            continue;
        }
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
