// Host side of body_joint_oracle.py, using the actual reconstructed dependencies.
#include "entities/player_body_motion.h"
#include "entities/body_channels.h"
#include "entities/body_timers.h"
#include "entities/body_wash.h"
#include "entities/body_shape_push.h"
#include "entities/body_action_ids.h"
#include "entities/body_animation_commands.h"
#include "entities/body_damage_tables.h"
#include "navigation/spawn_candidates.h"
#include "entities/entity_heap.h"
#include "entities/segment_records.h"
#include "entities/body_hit_messages.h"
#include "entities/contact_sweep.h"
#include "entities/player_body_collision.h"
#include "entities/player_mind_feedback.h"
#include "entities/player_mind_control.h"
#include "entities/body_attachment_requests.h"
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
        if (kind == "inputgesture")
        {
            std::array<u32, 13> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 32> tracking;
            std::memcpy(tracking.data(), words.data() + 1, 32);
            if (words[0] == 0)
            {
                update_runtime_player_input_gesture(tracking,
                    std::bit_cast<float>(words[9]), std::bit_cast<float>(words[11]));
            }
            else
            {
                const std::array<float, 2> axes{std::bit_cast<float>(words[9]), std::bit_cast<float>(words[10])};
                update_runtime_player_input_direction(tracking, axes,
                    std::bit_cast<float>(words[11]), words[12]);
            }
            for (size_t offset = 0; offset < tracking.size(); offset += 4)
            {
                std::cout << read_u32(tracking, offset) << ' ';
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "inputtracking")
        {
            std::array<u32, 16> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 24> tracking;
            std::memcpy(tracking.data(), words.data(), 24);
            std::array<float, 2> axes{std::bit_cast<float>(words[6]), std::bit_cast<float>(words[7])};
            std::array<u8, 16> preset;
            std::memcpy(preset.data(), words.data() + 8, 16);
            const float freeze = std::bit_cast<float>(words[12]);
            const float dt = std::bit_cast<float>(words[13]);
            const float scale = std::bit_cast<float>(words[14]);
            const u32 zero = 0;
            const std::array<u8, 1> decay{1};
            const RuntimeActorClock clock{freeze, dt, scale, zero, decay, zero};
            update_runtime_player_input_tracking(tracking, axes, preset, words[15], clock);
            for (size_t offset = 0; offset < tracking.size(); offset += 4)
            {
                std::cout << read_u32(tracking, offset) << ' ';
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "attachmentlookup")
        {
            u32 operation, count, key, present, category;
            std::cin >> operation >> count >> key >> present >> category;
            const size_t stride = operation == 2 ? 0x28 : operation == 3 ? 0x2C : 0x24;
            std::vector<u8> rows(size_t(count) * stride);
            for (u32 index = 0; index < count; ++index)
            {
                u32 value;
                std::cin >> value;
                const u16 half = u16(value);
                std::memcpy(rows.data() + size_t(index) * stride, &half, 2);
            }
            const RuntimeAttachmentRequestRows view{rows.data(), u16(count)};
            RuntimeAttachmentRequestTables tables{view, view, view, view, view};
            std::array<u8, 0x950> actor{};
            actor[0x93D] = u8(category);
            RuntimeBodyAttachedActor attached{actor, 1, {}};
            const u8* found;
            switch (operation)
            {
            case 0:
                found = find_runtime_attachment_first_request(tables, u16(key));
                break;
            case 1:
                found = find_runtime_attachment_second_request(tables, u16(key));
                break;
            case 2:
                found = find_runtime_attachment_pair_request(tables, u16(key));
                break;
            case 3:
                found = find_runtime_attachment_category_request(tables, u16(key));
                break;
            default:
                found = find_runtime_attachment_special_request(&tables, present != 0 ? &attached : nullptr, u16(key));
                break;
            }
            std::cout << (found == nullptr ? 0xFFFFFFFFu : u32((found - rows.data()) / stride)) << '\n';
            continue;
        }
        if (kind == "mindhelper")
        {
            u32 operation;
            std::array<u32, 12> words{};
            std::cin >> operation;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 0x1A4> mind{};
            Actor actor;
            actor.position = {std::bit_cast<float>(words[5]), std::bit_cast<float>(words[6]),
                std::bit_cast<float>(words[7])};
            const Vec3 position{std::bit_cast<float>(words[8]), std::bit_cast<float>(words[9]),
                std::bit_cast<float>(words[10])};
            std::memcpy(mind.data() + 0x124, &words[0], 4);
            std::memcpy(mind.data() + 0x120, &words[1], 4);
            std::memcpy(mind.data() + 0x134, &words[0], 4);
            std::memcpy(mind.data() + 0x138, words.data() + 2, 12);
            if (operation == 0)
            {
                words[0] = runtime_mind_alert_level(std::bit_cast<float>(words[11]));
            }
            else if (operation == 1)
            {
                record_runtime_player_mind_distance(mind, std::bit_cast<float>(words[11]));
                std::memcpy(&words[0], mind.data() + 0x124, 4);
                std::memcpy(&words[1], mind.data() + 0x120, 4);
            }
            else
            {
                record_runtime_player_mind_position(mind, actor, words[11], position);
                std::memcpy(&words[0], mind.data() + 0x134, 4);
                std::memcpy(words.data() + 2, mind.data() + 0x138, 12);
            }
            for (u32 word : words)
            {
                std::cout << word << ' ';
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "bodycollisionempty")
        {
            std::array<u32, 19> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 0xA50> bytes{};
            std::span<u8> body = bytes;
            Actor actor;
            actor.position = {std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]),
                std::bit_cast<float>(words[2])};
            actor.radius_xz = std::bit_cast<float>(words[6]);
            actor.animation_rate = std::bit_cast<float>(words[12]);
            actor.animation_pause = std::bit_cast<float>(words[13]);
            actor.xform_valid = 0xA5A5A5A5;
            std::memcpy(bytes.data() + 0x18, words.data() + 3, 12);
            std::memcpy(bytes.data() + 0x894, words.data() + 7, 4);
            std::memcpy(bytes.data() + 0x8A4, words.data() + 8, 4);
            std::memcpy(bytes.data() + 0x340, words.data() + 14, 4);
            bytes[0x874] = u8(words[15]);
            bytes[0x19E] = u8(words[18]);
            std::array<u8, 8> bit_bytes{};
            EntityBitBlock bits{bit_bytes.data()};
            std::memcpy(bits.bytes, words.data() + 16, 8);
            EntityBitBlock* flags = &bits;
            EntityMaskHost mask;
            i32 floor = 0;
            RuntimeBodyShapeStorage shapes{};
            entity_detail::ShapeProbe* first = nullptr;
            i32 count = 0;
            std::array<entity_detail::ShapeProbe*, 64> contacts{};
            RuntimeBodyShapePush push{first, count, contacts};
            RuntimePlayerCollisionAction* action = nullptr;
            std::array<entity_detail::SlideRecord, 5> slides{};
            const float freeze = std::bit_cast<float>(words[9]);
            const float dt = std::bit_cast<float>(words[10]);
            const float scale = std::bit_cast<float>(words[11]);
            const u32 disabled = 0;
            std::array<u8, 1> decay{};
            RuntimeActorClock clock{freeze, dt, scale, disabled, decay, disabled};
            const Vec3 stationary{};
            RuntimePlayerBodyCollision context{body, actor, flags, mask, floor, shapes, push,
                action, {}, slides, stationary, clock};
            const bool result = update_runtime_player_body_collision(context);
            std::cout << u32(result);
            for (u8 byte : bytes)
            {
                std::cout << ' ' << u32(byte);
            }
            std::cout << ' ' << std::bit_cast<u32>(actor.position.x) << ' '
                << std::bit_cast<u32>(actor.position.y) << ' ' << std::bit_cast<u32>(actor.position.z)
                << ' ' << actor.xform_valid;
            for (size_t index = 0; index < 8; ++index)
            {
                std::cout << ' ' << u32(bits.bytes[index]);
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "contactseparate")
        {
            u32 size;
            std::array<u32, 4> words;
            std::cin >> size;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::vector<u8> rows(size);
            for (u8& byte : rows)
            {
                u32 value;
                std::cin >> value;
                byte = u8(value);
            }
            entity_detail::SegmentPool pool;
            pool.begin = rows.data();
            pool.cursor = pool.begin + size;
            const Vec3 position{std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]),
                std::bit_cast<float>(words[2])};
            Vec3 output;
            separate_runtime_segment_contacts(pool, position, std::bit_cast<float>(words[3]),
                output, Vec3{0.f, 0.f, 0.f});
            std::cout << std::bit_cast<u32>(output.x) << ' ' << std::bit_cast<u32>(output.y)
                << ' ' << std::bit_cast<u32>(output.z) << '\n';
            continue;
        }
        if (kind == "sweeproot" || kind == "sweepspheres" || kind == "sweepedge" ||
            kind == "sweeppoint" || kind == "sweeptriangle" || kind == "sweepray")
        {
            std::array<u32, 23> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::array<float, 23> values;
            std::memcpy(values.data(), words.data(), sizeof(words));
            const auto vector = [&](size_t offset)
            {
                return Vec3{values[offset], values[offset + 1], values[offset + 2]};
            };
            Vec3 normal = vector(19);
            float fraction = values[22];
            bool hit;
            if (kind == "sweeproot")
            {
                hit = select_runtime_collision_root(values[0], values[1], values[2], fraction);
            }
            else if (kind == "sweepspheres")
            {
                hit = sweep_runtime_collision_spheres(vector(0), vector(3), vector(6),
                    values[9], vector(10), values[13], fraction);
            }
            else if (kind == "sweepedge")
            {
                hit = sweep_runtime_collision_edge(vector(0), vector(3), vector(6),
                    vector(9), values[12], normal, fraction);
            }
            else if (kind == "sweeppoint")
            {
                hit = sweep_runtime_collision_point(vector(0), vector(3), values[6],
                    vector(7), normal, fraction);
            }
            else if (kind == "sweeptriangle")
            {
                hit = sweep_runtime_collision_triangle(vector(0), vector(3), vector(6),
                    vector(9), vector(12), values[15], normal, fraction, vector(16));
            }
            else
            {
                hit = intersect_runtime_collision_triangle(vector(0), vector(3), vector(6),
                    vector(9), vector(12), vector(15), values[18], normal.x, normal.y, normal.z);
            }
            std::cout << u32(hit) << ' ' << std::bit_cast<u32>(normal.x) << ' '
                << std::bit_cast<u32>(normal.y) << ' ' << std::bit_cast<u32>(normal.z)
                << ' ' << std::bit_cast<u32>(fraction) << '\n';
            continue;
        }
        if (kind == "contactposition")
        {
            std::array<u32, 8> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 0x68> damage{};
            std::array<u8, 0x54> row{};
            std::memcpy(damage.data() + 0x18, words.data(), 12);
            std::memcpy(row.data() + 0x2C, words.data() + 3, 16);
            entity_detail::SegmentPool pool;
            pool.begin = row.data();
            Vec3 position;
            runtime_body_damage_contact_position(damage, words[7] == 0 ? nullptr : &pool, position);
            std::cout << std::bit_cast<u32>(position.x) << ' ' << std::bit_cast<u32>(position.y)
                << ' ' << std::bit_cast<u32>(position.z) << '\n';
            continue;
        }
        if (kind == "contactdirection")
        {
            u32 size;
            u32 stride;
            std::cin >> size >> stride;
            std::vector<u8> rows(size);
            for (u8& byte : rows)
            {
                u32 value;
                std::cin >> value;
                byte = u8(value);
            }
            entity_detail::SegmentPool pool;
            pool.begin = rows.data();
            pool.cursor = pool.begin + size;
            pool.stride = u16(stride);
            Vec3 direction;
            normalized_segment_contact_direction(pool, direction);
            std::cout << std::bit_cast<u32>(direction.x) << ' ' << std::bit_cast<u32>(direction.y)
                << ' ' << std::bit_cast<u32>(direction.z) << '\n';
            continue;
        }
        if (kind == "spawncandidates")
        {
            std::array<u32, 10> words;
            for (u32& word : words)
            {
                std::cin >> word;
            }
            entity_detail::NavGrid grid;
            grid.width = i32(words[0]);
            grid.depth = i32(words[1]);
            grid.cell = i32(words[2]);
            grid.cells.resize(size_t(grid.width * grid.depth));
            auto* bytes = reinterpret_cast<u8*>(grid.cells.data());
            for (size_t index = 0; index < grid.cells.size() * sizeof(entity_detail::NavCell); ++index)
            {
                u32 byte;
                std::cin >> byte;
                bytes[index] = u8(byte);
            }
            entity_detail::g_nav_grids[0] = &grid;
            entity_detail::g_nav_origin_x = std::bit_cast<i32>(words[3]);
            entity_detail::g_nav_origin_z = std::bit_cast<i32>(words[4]);
            const Vec3 center{std::bit_cast<float>(words[5]), 0.f, std::bit_cast<float>(words[6])};
            auto* pool = entity_detail::collect_navigation_spawn_candidates(0, nullptr, center,
                std::bit_cast<float>(words[7]), std::bit_cast<float>(words[8]),
                nullptr, nullptr, u8(words[9]), 0, 0.f);
            if (pool == nullptr)
            {
                std::cout << "0\n";
            }
            else
            {
                std::cout << (pool->cursor - pool->begin) / pool->stride;
                for (auto* row = pool->begin; row < pool->cursor; row += pool->stride)
                {
                    std::cout << ' ' << read_u32({row, pool->stride}, 0) << ' ' << read_u32({row, pool->stride}, 4);
                }
                std::cout << '\n';
                engine_heap_free(pool);
            }
            entity_detail::g_nav_grids[0] = nullptr;
            continue;
        }
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
