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
#include "entities/player_mind_targets.h"
#include "entities/player_mind_target_control.h"
#include "entities/body_damage_motion.h"
#include "entities/player_body_messages.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"
#include "navigation/path_routes.h"
#include "navigation/player_navigation.h"
#include "entities/body_attachment_requests.h"
#include "entities/body_request_selection.h"
#include "navigation/player_avoidance.h"
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
        if (kind == "avoidanceheading")
        {
            std::array<u32, 12> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto value = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            u32 random = words[4];
            const Vec3 origin{value(5), value(6), value(7)};
            Vec3 output{std::bit_cast<float>(0xA5A5A5A5u), std::bit_cast<float>(0xA5A5A5A5u),
                std::bit_cast<float>(0xA5A5A5A5u)};
            RuntimeNavigationHeadingSearch context{origin, 0, value(10), value(11), nullptr,
                nullptr, words[8] != 0 ? &output : nullptr, words[9], 9, 0, random, 0.f};
            entity_detail::g_nav_grids[0] = nullptr;
            u32 result;
            if (words[0] == 0)
            {
                result = std::bit_cast<u32>(randomize_runtime_navigation_heading(value(1), value(2), random));
            }
            else if (words[0] == 1)
            {
                result = probe_runtime_navigation_heading(context, value(1));
            }
            else
            {
                result = search_runtime_navigation_headings(context, value(1), value(2),
                    std::bit_cast<i32>(words[3]));
            }
            std::cout << result << ' ' << random << ' ' << std::bit_cast<u32>(output.x)
                << ' ' << std::bit_cast<u32>(output.y) << ' ' << std::bit_cast<u32>(output.z) << '\n';
            continue;
        }
        if (kind == "requestmatch")
        {
            std::array<u32, 23> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            Actor actor{};
            actor.position = Vec3{std::bit_cast<float>(words[0]), std::bit_cast<float>(words[1]),
                std::bit_cast<float>(words[2])};
            actor.radius_xz = std::bit_cast<float>(words[3]);
            std::array<u8, 0xA00> body{};
            std::array<u8, 0x24> row{};
            std::array<u8, 0x40> target{};
            std::array<u8, 32> shape{};
            row[6] = u8(words[4]);
            row[7] = u8(words[5]);
            row[8] = u8(words[6]);
            std::memcpy(row.data() + 0xC, &words[7], 4);
            const i16 low = i16(words[8]), high = i16(words[9]);
            std::memcpy(row.data() + 0x10, &low, 2);
            std::memcpy(row.data() + 0x12, &high, 2);
            std::memcpy(row.data() + 0x20, &words[10], 4);
            std::memcpy(target.data() + 0x20, words.data() + 11, 12);
            std::memcpy(target.data() + 0x38, &words[14], 4);
            target[0x16] = u8(words[15]);
            body[0x2D0] = u8(words[16]);
            body[0x2D2] = u8(words[17]);
            std::memcpy(shape.data() + 8, &words[19], 4);
            std::memcpy(shape.data() + 0x14, &words[20], 4);
            std::cout << u32(runtime_attachment_target_row_matches(body, actor, row, target,
                words[18] != 0 ? shape.data() : nullptr, words[21], u8(words[22]))) << '\n';
            continue;
        }
        if (kind == "requestbits")
        {
            std::array<u32, 8> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 16> bytes;
            std::memcpy(bytes.data(), words.data() + 4, bytes.size());
            RuntimeRequestBits bits{words[0], {bytes.data(), words[1]}};
            u32 random = words[2];
            u32 index = words[3];
            const i32 count = count_runtime_request_bits(bits);
            const bool result = select_random_runtime_request_bit(bits, index, random);
            std::cout << u32(count) << ' ' << u32(result) << ' ' << index << ' ' << random << '\n';
            continue;
        }
        if (kind == "requestlength")
        {
            std::array<u32, 10> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            std::array<u8, 0xA00> body{};
            std::array<u8, 0xD00> owner{};
            std::array<u8, 0x200> model{};
            std::array<u8, 8> flag_bytes{};
            std::vector<u8> table((words[1] + 1) * 0x87);
            const u16 classification = u16(words[1]);
            std::memcpy(owner.data() + 0xCF4, &classification, 2);
            std::memcpy(body.data() + 0x16C, &words[3], 4);
            std::memcpy(flag_bytes.data(), &words[4], 4);
            model[0x1A4] = u8(words[2]);
            if (classification != 0)
            {
                table[u32(classification) * 0x87 - 0x38] = u8(words[9]);
            }
            EntityBitBlock flags{};
            flags.bytes = flag_bytes.data();
            RuntimeBodyChainLength context{body, owner, words[0] != 0 ? model.data() : nullptr,
                flags, u8(words[5]), u8(words[6]), std::bit_cast<float>(words[7]),
                std::bit_cast<float>(words[8]), table};
            std::cout << u32(runtime_body_target_chain_length(context)) << '\n';
            continue;
        }
        if (kind == "playeractionquery")
        {
            std::array<u32, 41> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto value = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            const auto put = [](auto& bytes, size_t offset, u32 word)
            {
                std::memcpy(bytes.data() + offset, &word, 4);
            };
            Actor owner{}, actor{}, focus{};
            owner.position = Vec3{value(1), value(2), value(3)};
            actor.position = Vec3{value(4), value(5), value(6)};
            focus.position = Vec3{value(7), value(8), value(9)};
            owner.euler.y = value(10);
            owner.radius_xz = value(11);
            owner.radius_y = value(11);
            std::array<u8, 0xA00> body{}, other{};
            std::array<u8, 0xD00> actor_bytes{};
            std::array<u8, 0x200> mind{};
            std::array<u8, 0x40> target_bytes{};
            std::array<u8, 8> own_bytes{}, other_bytes{};
            EntityBitBlock own_flags{}, other_flags{};
            own_flags.bytes = own_bytes.data();
            other_flags.bytes = other_bytes.data();
            put(own_bytes, 0, words[34]);
            put(own_bytes, 4, words[35]);
            put(other_bytes, 0, words[17]);
            put(other_bytes, 4, words[18]);
            put(body, 0x884, words[12]);
            put(body, 0x888, words[13]);
            body[0x6A] = u8(words[19]);
            body[0x6B] = u8(words[20]);
            body[0x6C] = u8(words[21]);
            put(body, 0x70, words[22]);
            const u16 state = u16(words[23]);
            std::memcpy(body.data() + 0x30, &state, 2);
            put(body, 0x34, words[24]);
            put(other, 0x23C, words[26]);
            const u16 energy = u16(words[27]);
            std::memcpy(other.data() + 0x94, &energy, 2);
            put(other, 0x174, words[28]);
            put(other, 0x16C, words[29]);
            put(mind, 0x88, words[30] != 0 ? 0x10005000 : 0);
            mind[0x90] = u8(words[31]);
            put(mind, 0x80, words[32]);
            target_bytes[0x14] = u8(words[15]);
            target_bytes[0x16] = u8(words[16]);
            for (size_t index = 0; index < 3; ++index)
            {
                put(target_bytes, 0x20 + index * 4, words[4 + index]);
            }
            put(target_bytes, 0x30, words[14]);
            put(target_bytes, 0x38, words[14]);
            put(actor_bytes, 0xCE0, words[36]);
            const u16 mask = u16(words[37]);
            std::memcpy(actor_bytes.data() + 0xCDC, &mask, 2);
            put(actor_bytes, 0x93C, words[38]);
            RuntimeScriptMindQuery query{};
            query.bits = &other_flags;
            RuntimeMindTarget target{nullptr, target_bytes, 0x10005000, actor, actor_bytes, query, nullptr};
            RuntimePlayerAttachmentEligibility eligibility{body, own_flags, owner, nullptr};
            u32 result = 0, within = 0xA5A5A5A5;
            switch (words[0])
            {
            case 0:
                result = runtime_body_target_direction(owner, target_bytes, value(10));
                break;
            case 1:
                result = u32(runtime_body_target_turn_direction(owner, target_bytes, &within));
                break;
            case 2:
                result = runtime_body_accepts_target_request(body, own_flags, u16(words[25]));
                break;
            case 3:
                result = runtime_body_target_state_active(other);
                break;
            case 4:
                result = runtime_mind_target_requires_action(target, other);
                break;
            case 5:
                result = runtime_mind_target_requires_special_action(target, other, actor_bytes, words[39], words[40]);
                break;
            case 6:
                result = runtime_body_target_height(body, owner, target_bytes, nullptr);
                break;
            case 7:
                result = select_runtime_player_target_action_direction(mind, eligibility, owner,
                    target, other, words[33] == 0 ? nullptr : words[33] == 1 ? &actor : &focus);
                break;
            default:
                result = std::bit_cast<u32>(runtime_actor_squared_distance(owner, focus));
                break;
            }
            std::cout << result << ' ' << within << ' ' << u32(mind[0x90]) << '\n';
            continue;
        }
        if (kind == "playersearch")
        {
            std::array<u32, 34> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto value = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            const auto put = [](auto& bytes, size_t offset, u32 word)
            {
                std::memcpy(bytes.data() + offset, &word, 4);
            };
            Actor owner{}, actor{}, held_actor{};
            owner.position = Vec3{value(2), value(3), value(4)};
            owner.radius_xz = value(18);
            actor.class_key = u16(words[15]);
            std::array<u8, 0xD00> owner_bytes{}, actor_bytes{};
            std::array<u8, 0xA00> body{}, held_bytes{};
            std::array<u8, 0xC0> policy{};
            std::array<u8, 0x40> target_bytes{};
            std::array<u8, 8> own_flag_bytes{}, target_flag_bytes{}, zero{};
            EntityBitBlock own_flags{}, target_flags{};
            own_flags.bytes = own_flag_bytes.data();
            target_flags.bytes = target_flag_bytes.data();
            own_flag_bytes[5] = u8(words[23]);
            put(body, 0x888, words[19]);
            put(body, 0x78, words[20]);
            put(owner_bytes, 0x978, words[29]);
            owner_bytes[0x969] = u8(words[30]);
            const u16 included = u16(words[31]), excluded = u16(words[32]);
            std::memcpy(owner_bytes.data() + 0x97C, &included, 2);
            std::memcpy(owner_bytes.data() + 0x97E, &excluded, 2);
            held_bytes[0x93D] = u8(words[17]);
            RuntimeBodyAttachedActor held{held_bytes, 0x10008000, {}, nullptr, &held_actor};
            RuntimePlayerAttachmentEligibility eligibility{body, own_flags, owner,
                words[17] != 0 ? &held : nullptr};
            put(policy, 0x64, words[12]);
            put(policy, 0x68, words[11]);
            put(policy, 0xB0, words[24]);
            target_bytes[0x14] = u8(words[13]);
            target_bytes[0x18] = u8(words[14]);
            target_bytes[0x16] = u8(words[33]);
            for (size_t index = 0; index < 3; ++index)
            {
                put(target_bytes, 0x20 + index * 4, words[5 + index]);
            }
            put(target_bytes, 0x38, words[16]);
            RuntimeScriptMindQuery target_body{};
            target_body.energy = i16(words[22]);
            target_body.bits = &target_flags;
            RuntimeMindTarget target{nullptr, target_bytes, 0x10005000, actor, actor_bytes,
                target_body, nullptr};
            float angle = value(27), distance = value(28), retained = value(26);
            RuntimeMindTargetScore score{};
            RuntimeMindTarget* selected = nullptr;
            if (words[0] == 0)
            {
                score_runtime_mind_target(owner.position, Vec3{value(5), value(6), value(7)},
                    value(8), value(9), value(10), score);
                angle = score.angle;
                distance = score.distance;
            }
            else if (words[0] == 1)
            {
                selected = find_runtime_mind_target_in_angle(owner, nullptr, value(8), value(9),
                    included, excluded, &angle, &distance, words[11], words[12], value(10),
                    words[1] != 0 ? &target : nullptr);
            }
            else
            {
                RuntimePlayerTargetSearch context{owner, owner_bytes, eligibility,
                    words[1] != 0 ? &target : nullptr, zero, retained, value(25)};
                selected = select_runtime_player_shape_target(context, policy, &angle,
                    words[21] != 0 ? &target : nullptr, value(8));
            }
            std::cout << (selected != nullptr ? selected->address : 0) << ' '
                << std::bit_cast<u32>(angle) << ' ' << std::bit_cast<u32>(distance)
                << ' ' << std::bit_cast<u32>(score.score) << ' '
                << std::bit_cast<u32>(score.relative_angle) << '\n';
            continue;
        }
        if (kind == "playerfocus")
        {
            std::array<u32, 25> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto value = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            Actor owner{}, actor{};
            owner.position = Vec3{value(2), value(3), value(4)};
            actor.position = Vec3{value(5), value(6), value(7)};
            owner.euler.y = value(8);
            owner.radius_xz = value(16);
            actor.radius_xz = value(17);
            actor.mask_mode = 1;
            std::array<u8, 0x200> mind{};
            std::array<u8, 0xA00> body{}, other_body{};
            std::array<u8, 8> flag_bytes{};
            flag_bytes[3] = u8(words[14]);
            flag_bytes[7] = u8(words[15]);
            EntityBitBlock own_flags{}, flags{};
            std::array<u8, 8> own_flag_bytes{};
            own_flags.bytes = own_flag_bytes.data();
            flags.bytes = flag_bytes.data();
            const auto put = [](auto& bytes, size_t offset, u32 word)
            {
                std::memcpy(bytes.data() + offset, &word, 4);
            };
            put(body, 0x888, words[18]);
            put(other_body, 0x888, words[19]);
            put(other_body, 0x78, words[20]);
            put(mind, 0x34, words[22]);
            put(mind, 0x38, words[23]);
            mind[0x20] = u8(words[13]);
            Actor* target = words[12] != 0 ? &actor : nullptr;
            RuntimePlayerAttachmentEligibility own{body, own_flags, owner, nullptr};
            RuntimePlayerAttachmentEligibility other{other_body, flags, actor, nullptr};
            RuntimeScriptMindQuery query_mind{};
            query_mind.bits = &flags;
            scene_detail::CamFocus host{};
            scene_detail::RangeLink link{nullptr, &host};
            u16 tag = 0;
            std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> views;
            views.emplace(&host, RuntimeScriptActorQueryView{actor, tag, &query_mind, nullptr, nullptr});
            std::array<u8, 32> zero{};
            RuntimeScriptActorQueryContext query{words[1] != 0 ? &link : nullptr, views, zero.data()};
            float distance = value(24);
            float angle = value(21);
            float retained = value(21);
            Actor* selected;
            if (words[0] == 0)
            {
                selected = find_nearest_runtime_script_actor_in_angle(owner, value(8), value(9),
                    value(10), &angle, &distance, words[11], nullptr, query);
            }
            else if (words[0] == 1)
            {
                selected = select_runtime_player_near_actor(owner, distance, retained, query);
            }
            else
            {
                std::unordered_map<Actor*, const RuntimePlayerAttachmentEligibility*> bodies{{&actor, &other}};
                std::unordered_map<Actor*, u32> addresses{{&actor, 0x10003000}};
                RuntimePlayerMindFocus focus{mind, owner, target, own, bodies, addresses, query, retained};
                selected = update_runtime_player_mind_focus(focus);
            }
            std::cout << (selected != nullptr ? 0x10003000u : 0u) << ' '
                << std::bit_cast<u32>(angle) << ' ' << std::bit_cast<u32>(distance)
                << ' ' << u32(mind[0x20]) << ' ' << read_u32(mind, 0x34) << '\n';
            continue;
        }
        if (kind == "playertarget")
        {
            std::array<u32, 31> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            ActorLiveAttachment attachment{};
            for (auto* shape : {&attachment.shape, &attachment.secondary_shape})
            {
                for (auto& byte : *shape)
                {
                    u32 value;
                    std::cin >> value;
                    byte = u8(value);
                }
            }
            const auto vector = [&words](size_t index)
            {
                return Vec3{std::bit_cast<float>(words[index]), std::bit_cast<float>(words[index + 1]),
                    std::bit_cast<float>(words[index + 2])};
            };
            Actor actor{}, target_actor{};
            actor.position = vector(13);
            actor.radius_xz = std::bit_cast<float>(words[16]);
            target_actor.position = vector(19);
            target_actor.radius_xz = std::bit_cast<float>(words[22]);
            std::array<u8, 0x900> body{}, target_body{};
            body[0x6A] = u8(words[1]);
            body[0x6B] = u8(words[2]);
            std::memcpy(body.data() + 0x70, &words[3], 4);
            const u16 key = u16(words[4]);
            std::memcpy(body.data() + 0x30, &key, 2);
            std::memcpy(body.data() + 0x34, &words[5], 4);
            std::memcpy(body.data() + 0x78, &words[17], 4);
            std::memcpy(body.data() + 0x888, &words[18], 4);
            std::memcpy(target_body.data() + 0x888, &words[23], 4);
            std::array<u8, 8> flag_bytes;
            std::memcpy(flag_bytes.data(), words.data() + 6, 8);
            EntityBitBlock flags{flag_bytes.data()};
            std::array<u8, 0x200> mind;
            mind.fill(0xA5);
            std::memcpy(mind.data() + 0x14, &words[9], 4);
            const u32 actor_address = 0x10003000;
            const u32 event_address = words[29] != 0 ? 0x1000B000u : 0;
            std::memcpy(mind.data() + 8, &actor_address, 4);
            std::memcpy(mind.data() + 0x28, &event_address, 4);
            std::array<u8, 0x40> candidate{};
            candidate[0x14] = u8(words[24]);
            const Vec3 point = vector(25);
            std::memcpy(candidate.data() + 0x20, &point, 12);
            std::memcpy(candidate.data() + 0x38, &words[28], 4);
            const i32 floor = std::bit_cast<i32>(words[12]);
            RuntimePlayerMindShapeTarget shape{mind, body, actor, words[8] != 0 ? &attachment : nullptr,
                flags, floor, 0x10001000};
            RuntimePlayerAttachmentEligibility eligibility{body, flags, actor, nullptr};
            u32 result;
            if (words[0] < 2)
            {
                const auto returned = words[0] == 0
                    ? std::span<const u8>(refresh_runtime_player_shape_target(shape,
                        std::bit_cast<float>(words[10]), std::bit_cast<float>(words[11])))
                    : redirect_runtime_player_shape_target(shape, eligibility, candidate,
                        std::bit_cast<float>(words[10]));
                result = returned.data() == nullptr ? 0
                    : returned.data() == candidate.data() ? 0x10009000u : 0x100010B8u;
            }
            else
            {
                std::array<u8, 0x24> event_bytes{};
                std::memcpy(event_bytes.data() + 0x18, &words[30], 4);
                LiveScript events;
                RuntimeMindEvent event;
                event.type = 3;
                event.bytes = event_bytes;
                events.next = words[29] != 0 ? &event : nullptr;
                std::array<u8, 0x24> address_zero{};
                result = u32(runtime_player_mind_accepts_target(eligibility, target_body,
                    target_actor, candidate, events, address_zero));
            }
            std::cout << result;
            for (size_t offset = 0; offset < mind.size(); offset += 4)
            {
                std::cout << ' ' << read_u32(mind, offset);
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "playergaze")
        {
            std::array<u32, 43> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto vector = [&words](size_t index)
            {
                return Vec3{std::bit_cast<float>(words[index]), std::bit_cast<float>(words[index + 1]),
                    std::bit_cast<float>(words[index + 2])};
            };
            std::array<Actor, 3> actors{};
            actors[0].position = vector(5);
            actors[0].euler.y = std::bit_cast<float>(words[8]);
            actors[0].radius_y = std::bit_cast<float>(words[4]);
            std::array<ActorModelAnchor, 4> anchors{};
            std::unordered_map<ActorModelAnchor*, u32> addresses;
            g_actor_model_anchors = {};
            for (size_t index = 0; index < anchors.size(); ++index)
            {
                auto& anchor = anchors[index];
                anchor.owner = &actors[words[23 + index]];
                anchor.output = vector(27 + index * 3);
                anchor.radius = std::bit_cast<float>(words[39 + index]);
                anchor.global_link.next = index + 1 < anchors.size() ? &anchors[index + 1].global_link : nullptr;
                addresses[&anchor] = 0x10006000u + u32(index) * 0x100;
            }
            g_actor_model_anchors.next = &anchors[0].global_link;
            std::array<u8, 0x900> body{};
            std::array<u8, 0x200> mind{};
            const u16 key = u16(words[1]);
            std::memcpy(body.data() + 0x30, &key, 2);
            std::memcpy(body.data() + 0x34, &words[2], 4);
            std::memcpy(body.data() + 0x884, &words[3], 4);
            mind[0x20] = u8(words[9]);
            Actor* target = words[10] != 0 ? &actors[2] : nullptr;
            ActorModelAnchor* anchor = words[11] < anchors.size() ? &anchors[words[11]] : nullptr;
            const u32 address = anchor == nullptr ? 0 : addresses.at(anchor);
            std::memcpy(mind.data() + 0x30, &address, 4);
            std::memcpy(mind.data() + 0x34, words.data() + 12, 12);
            const Vec3 explicit_point = vector(15);
            std::memcpy(mind.data() + 0x40, &explicit_point, 12);
            std::array<u8, 0x80> keyboard{};
            std::memcpy(keyboard.data() + 8, words.data() + 18, 8);
            std::array<u32, 4> device_words{3, 1, 0, 0};
            const auto device_bytes = std::span<const u8>(reinterpret_cast<const u8*>(device_words.data()), 16);
            RuntimeDialogueInputDevice input{device_bytes, {}};
            RuntimeDialogueInputState state{keyboard, {}, 0, 0, 0, nullptr, 0};
            u32 random = words[0];
            RuntimePlayerMindGaze gaze{mind, body, actors[0], actors[0], target, anchor,
                input, state, addresses, random};
            for (auto& grid : entity_detail::g_nav_grids)
            {
                grid = nullptr;
            }
            update_runtime_player_mind_gaze(gaze, i32(words[20]), std::bit_cast<float>(words[21]),
                std::bit_cast<float>(words[22]));
            std::cout << random;
            for (size_t offset = 0x30; offset <= 0x3C; offset += 4)
            {
                std::cout << ' ' << read_u32(mind, offset);
            }
            for (size_t offset = 0x2FC; offset <= 0x304; offset += 4)
            {
                std::cout << ' ' << read_u32(body, offset);
            }
            std::cout << ' ' << u32(body[0x308]) << '\n';
            g_actor_model_anchors = {};
            continue;
        }
        if (kind == "playeridle" || kind == "playerinactivity")
        {
            std::vector<u32> words(kind == "playeridle" ? 5 : 24);
            for (auto& word : words)
            {
                std::cin >> word;
            }
            constexpr std::array<u16, 6> keys{135, 136, 153, 154, 162, 165};
            std::vector<u8> rows;
            for (size_t index = 0; index < keys.size(); ++index)
            {
                if ((words[1] & (1u << index)) != 0)
                {
                    const size_t offset = rows.size();
                    rows.resize(offset + 0x24);
                    std::memcpy(rows.data() + offset, &keys[index], 2);
                }
            }
            RuntimeAttachmentRequestTables tables{};
            tables.first = {rows.data(), u16(rows.size() / 0x24)};
            const RuntimeAttachmentRequestTables* table = &tables;
            ActorAnimationState animation;
            RuntimeBodyAttachedActor* attached = nullptr;
            RuntimeBodyAnimationReceiver* first_receiver = nullptr;
            RuntimeBodyAnimationReceiver* second_receiver = nullptr;
            FactoryObject first, second;
            const u8* action_row = nullptr;
            const RuntimeScriptLookupTable* actions = nullptr;
            std::unordered_map<u32, const char*> names;
            ObjectFactoryRegistry factory{};
            RuntimePlayerChildContext child{factory, {}, 0x10003000};
            AnimationResourceRegistry registry;
            ResourceErrorState error{};
            u32 random = words[0];
            RuntimeBodyActionSelection selection{animation, attached, first_receiver, second_receiver,
                first, second, action_row, actions, names, child, registry, error, random, 0x10001000};
            std::array<u8, 0x800> body{};
            std::array<u8, 0x200> mind{};
            std::memcpy(mind.data() + 0x14, &words[2], 4);
            std::memcpy(body.data() + 0x84, &words[3], 4);
            std::memcpy(body.data() + 0x88, &words[4], 4);
            RuntimeBodyAttachedActor* secondary = nullptr;
            const u8* state = nullptr;
            RuntimeBodyAttachmentRequests request{body, table, selection, secondary, 0, state,
                [&rows](const u8* row)
                {
                    return 0x10008000u + u32(row - rows.data());
                }};
            bool result = false;
            if (kind == "playeridle")
            {
                result = request_runtime_player_idle_action(mind, request, random);
            }
            else
            {
                std::memcpy(mind.data() + 0x144, words.data() + 5, 20);
                mind[0x20] = u8(words[10]);
                const u16 state_key = u16(words[11]);
                std::memcpy(body.data() + 0x30, &state_key, 2);
                std::memcpy(body.data() + 0x34, &words[12], 4);
                std::memcpy(body.data() + 0x48, &words[13], 4);
                std::memcpy(body.data() + 0x50, &words[14], 4);
                const u16 action = u16(words[15]);
                std::memcpy(body.data() + 0x748, &action, 2);
                std::array<u8, 16> flag_bytes{};
                flag_bytes[6] = u8(words[16]);
                EntityBitBlock flags{flag_bytes.data()};
                const float freeze = std::bit_cast<float>(words[17]);
                const float dt = std::bit_cast<float>(words[18]);
                const float scale = std::bit_cast<float>(words[19]);
                const u32 disabled = 0;
                RuntimeActorClock clock{freeze, dt, scale, disabled, {}, disabled};
                std::array<u32, 4> device_words{3, 1, 0, 0};
                const auto device_bytes = std::span<const u8>(reinterpret_cast<const u8*>(device_words.data()), 16);
                RuntimeDialogueInputDevice input{device_bytes, {}};
                std::array<u8, 0x80> keyboard{};
                std::memcpy(keyboard.data(), words.data() + 20, 16);
                RuntimeDialogueInputState input_state{keyboard, {}, 0, 0, 0, nullptr, 0};
                RuntimePlayerMindInactivity control{mind, flags, clock, input, input_state};
                update_runtime_player_mind_inactivity(control, request, animation, random);
            }
            std::cout << u32(result) << ' ' << random << ' ' << read_u32(mind, 0x14)
                << ' ' << read_u32(body, 0x84) << ' ' << read_u32(body, 0x88);
            if (kind == "playerinactivity")
            {
                for (size_t offset = 0x144; offset <= 0x154; offset += 4)
                {
                    std::cout << ' ' << read_u32(mind, offset);
                }
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "animationcrossing")
        {
            std::array<u32, 6> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            ActorAnimationState animation;
            if (words[0] != 0)
            {
                animation.controller = std::make_unique<AnimationController>();
                animation.controller->slots.resize(1);
                if (words[1] != 0)
                {
                    auto player = std::make_unique<MinaPlayer>();
                    player->position = std::bit_cast<float>(words[2]);
                    player->previous = std::bit_cast<float>(words[3]);
                    player->speed = std::bit_cast<float>(words[4]);
                    animation.controller->slots[0].player = std::move(player);
                }
            }
            std::cout << u32(runtime_actor_animation_crossed_position(animation, 0,
                std::bit_cast<float>(words[5]))) << '\n';
            continue;
        }
        if (kind == "playerbodycontrol")
        {
            std::array<u32, 12> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            Actor actor{};
            actor.animation_rate = std::bit_cast<float>(words[10]);
            u8 hidden = u8(words[8]);
            ActorLiveAttachment attachment{};
            attachment.retained_18 = u8(words[9]);
            ActorLiveAttachment* attached = words[11] != 0 ? &attachment : nullptr;
            entity_detail::ListHead anchors;
            ActorAttachmentNode* first = nullptr;
            ActorAttachmentNode* second = nullptr;
            RuntimeBodyAttachmentLists lists{first, second, {}};
            ActorAttachmentList attachments;
            AnimationResourceRegistry registry;
            ResourceErrorState error{};
            RuntimeModelResourceViews model_views;
            RuntimeModelDeferredPool deferred{};
            u32 fence = 0;
            RuntimeShaderRegistrationState shaders;
            RuntimeShaderAnimationScratch scratch;
            const std::function<void(u8*)> free_memory = [](u8* memory)
            {
                engine_heap_free(memory);
            };
            ActorEffectModelContext models{registry, error, model_views, deferred, fence,
                {}, shaders, scratch, free_memory};
            RuntimeActorSoundQueues sounds{};
            u32 random = 0;
            Vec3 position{}, euler{};
            ActorEffectStreamContext effects{models, sounds, random, position, euler};
            ObjectFactoryRegistry factory{};
            std::vector<FactoryObject> objects;
            std::unordered_map<const u8*, const u8*> linked;
            ActorAttachmentEventContext events{actor, attachments, effects, factory, {},
                objects, 0x10003000, linked, {}};
            RuntimeActorResourceReference resource;
            std::array<u8, 16> flag_bytes{};
            EntityBitBlock flags{flag_bytes.data()};
            std::array<u8, 0xA00> body{};
            std::array<u32, 5> message{words[0], words[1], words[2], words[3], words[4]};
            const u32 address = 0x10003000;
            std::memcpy(body.data() + 8, &address, 4);
            std::memcpy(body.data() + 0x94, &words[5], 4);
            std::memcpy(body.data() + 0x948, &words[6], 4);
            std::memcpy(body.data() + 0x94C, &words[7], 4);
            RuntimePlayerBodyControlMessages context{actor, hidden, anchors, attached,
                resource, events, lists, flags, 0x10001000};
            const auto packet = std::span<const u8>(reinterpret_cast<const u8*>(message.data()), 20);
            if (words[0] == 0x86)
            {
                begin_runtime_player_body_control(body, packet, context);
            }
            else
            {
                end_runtime_player_body_control(body, packet, context);
            }
            std::cout << read_u32(body, 0x94) << ' ' << read_u32(body, 0x948)
                << ' ' << read_u32(body, 0x94C) << ' ' << u32(hidden)
                << ' ' << u32(attachment.retained_18) << ' '
                << std::bit_cast<u32>(actor.animation_rate) << ' ' << u32(flags.bytes[0])
                << ' ' << read_u32(body, 0x924) << '\n';
            continue;
        }
        if (kind == "playerrequest")
        {
            std::array<u32, 27> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto vector = [&words](size_t index)
            {
                return Vec3{std::bit_cast<float>(words[index]), std::bit_cast<float>(words[index + 1]),
                    std::bit_cast<float>(words[index + 2])};
            };
            Actor actor;
            actor.position = vector(2);
            actor.euler = vector(5);
            const Vec3 reference = vector(8);
            std::array<u8, 0x200> mind;
            mind.fill(0xA5);
            std::array<u8, 64> message;
            std::memcpy(message.data(), words.data() + 11, 64);
            u32 random = words[0];
            const u32 result = prepare_runtime_player_mind_request(mind, actor,
                words[1] != 0 ? &reference : nullptr, message, random);
            std::cout << result << ' ' << random << ' ';
            for (size_t index = 0; index < mind.size() / 4; ++index)
            {
                if (index != 2 && index != 4)
                {
                    std::cout << read_u32(mind, index * 4) << ' ';
                }
            }
            for (size_t index = 0; index < message.size() / 4; ++index)
            {
                std::cout << read_u32(message, index * 4) << ' ';
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "navigationdrive")
        {
            using namespace gbtg::entity_detail;
            std::array<u32, 15> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto scalar = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            Actor actor;
            actor.position = Vec3{scalar(1), 0, scalar(2)};
            actor.animation_rate = 1;
            std::array<u8, 0x900> body{};
            const u16 gate = u16(words[10]);
            std::memcpy(body.data() + 0x30, &gate, 2);
            std::memcpy(body.data() + 0x34, words.data() + 11, 4);
            std::memcpy(body.data() + 0x878, words.data() + 9, 4);
            NavGrid grid;
            grid.width = grid.depth = 4;
            grid.cell = 16;
            grid.cost_scale = 1;
            grid.cells.resize(16);
            for (i32 index = 0; index < 16; ++index)
            {
                auto& cell = grid.cells[index];
                cell.x = i16((index % 4) * 16);
                cell.z = i16((index / 4) * 16);
                cell.value_8 = 16;
                cell.connections = 255;
                cell.flags_7 = (words[14] & (1u << index)) != 0 ? 2 : 0;
            }
            g_nav_grids[0] = &grid;
            g_nav_origin_x = g_nav_origin_z = 0;
            std::array<u8, 16 * 0x14> nodes{};
            NavigationPathSearch search;
            reset_navigation_path_search(search, nodes.data(), &grid, 16);
            search.word_20 = words[12];
            NavigationPathSearch* pointer = words[13] != 0 ? &search : nullptr;
            NavigationCellRegistration* registration = nullptr;
            i32 floor = 0;
            float zero = 0, one = 1, step = scalar(8);
            u32 index = 0;
            RuntimeActorClock clock{zero, step, one, index, {}, index};
            RuntimePlayerNavigation navigation{pointer, actor, body, floor, registration, clock};
            std::array<u8, 8> flag_bytes{};
            EntityBitBlock flags{flag_bytes.data()};
            std::array<u8, 0x90> device{};
            const u32 kind = 5;
            std::memcpy(device.data(), &kind, 4);
            RuntimeDialogueInputDevice input{device, {}};
            RuntimeDialogueInputState state{};
            RuntimeInputAxisWriteDevice writable{device, {}};
            RuntimePlayerNavigationControl control{navigation, actor, flags, input, state, writable, {}, nullptr};
            const Vec3 target{scalar(3), 0, scalar(4)};
            Vec3 direction{123, 456, 789};
            u32 failed = 77;
            u32 result;
            if (words[0] == 0)
            {
                result = request_runtime_player_navigation_path(navigation, target, 0, direction, failed);
            }
            else
            {
                result = drive_runtime_player_navigation(control, target, 0, scalar(5), scalar(6), scalar(7), 1, failed);
            }
            std::cout << result << ' ' << failed << ' ' << read_u32(device, 0x10) << ' '
                << read_u32(device, 0x14) << ' ' << std::bit_cast<u32>(direction.x) << ' '
                << std::bit_cast<u32>(direction.y) << ' ' << std::bit_cast<u32>(direction.z) << ' '
                << search.state << ' ' << search.result_found << '\n';
            if (search.scratch != nullptr)
            {
                engine_heap_free(search.scratch);
            }
            g_nav_grids[0] = nullptr;
            continue;
        }
        if (kind == "navigationmotion")
        {
            using namespace gbtg::entity_detail;
            std::array<u32, 27> words;
            for (auto& word : words)
            {
                std::cin >> word;
            }
            const auto scalar = [&words](size_t index)
            {
                return std::bit_cast<float>(words[index]);
            };
            const auto vector = [&scalar](size_t index)
            {
                return Vec3{scalar(index), scalar(index + 1), scalar(index + 2)};
            };
            Actor actor;
            actor.anchor = vector(5);
            actor.position = vector(14);
            actor.radius_xz = scalar(20);
            std::array<u8, 0x900> body{};
            std::memcpy(body.data() + 0x1D8, words.data() + 8, 12);
            std::memcpy(body.data() + 0x248, words.data() + 11, 12);
            std::memcpy(body.data() + 0x888, words.data() + 21, 4);
            std::array<u8, 8> flag_bytes{};
            std::memcpy(flag_bytes.data(), words.data() + 1, 4);
            EntityBitBlock flags{flag_bytes.data()};
            std::array<u8, 0x90> device{};
            const u32 kind = 5;
            std::memcpy(device.data(), &kind, 4);
            std::memcpy(device.data() + 0x10, words.data() + 25, 8);
            RuntimeDialogueInputDevice input{device, {}};
            RuntimeDialogueInputState state{};
            RuntimeInputAxisWriteDevice writable{device, {}};
            NavigationPathSearch* search = nullptr;
            NavigationCellRegistration* registration = nullptr;
            i32 floor = 0;
            float zero = 0;
            u32 index = 0;
            RuntimeActorClock clock{zero, zero, zero, index, {}, index};
            RuntimePlayerNavigation navigation{search, actor, body, floor, registration, clock};
            RuntimePlayerNavigationControl context{navigation, actor, flags, input, state, writable, {}, nullptr};
            NavGrid grid;
            grid.cell = i32(words[23]);
            g_nav_grids[0] = words[22] != 0 ? &grid : nullptr;
            u32 forced = words[2];
            float motion_alignment = scalar(3);
            float input_alignment = scalar(4);
            bool result;
            switch (words[0])
            {
            case 0:
                result = runtime_player_motion_allows_navigation(context, forced, motion_alignment);
                break;
            case 1:
                result = runtime_player_input_aligns_with_motion(context, input_alignment);
                break;
            case 2:
                result = runtime_player_navigation_motion(context, forced, motion_alignment, input_alignment);
                break;
            default:
                result = runtime_player_navigation_target_is_near(context, vector(17));
                break;
            }
            std::cout << result << ' ' << forced << ' ' << std::bit_cast<u32>(motion_alignment)
                << ' ' << std::bit_cast<u32>(input_alignment) << '\n';
            g_nav_grids[0] = nullptr;
            continue;
        }
        if (kind == "navigationfloor")
        {
            using namespace gbtg::entity_detail;
            u32 source, target, index, count;
            std::cin >> source >> target >> index >> count;
            Vec3 point;
            for (float* component : {&point.x, &point.y, &point.z})
            {
                u32 word;
                std::cin >> word;
                *component = std::bit_cast<float>(word);
            }
            NavGrid grid;
            grid.width = grid.depth = 4;
            grid.cells.resize(16);
            auto* bytes = reinterpret_cast<u8*>(grid.cells.data());
            for (size_t at = 0; at < 16 * sizeof(NavCell); ++at)
            {
                u32 value;
                std::cin >> value;
                bytes[at] = u8(value);
            }
            std::vector<u32> rows(count * 3);
            for (u32 row = 0; row < count; ++row)
            {
                std::cin >> rows[row * 3];
            }
            auto* begin = reinterpret_cast<u8*>(rows.data());
            NavigationCellRegistration connections{begin, begin + rows.size() * 4};
            g_nav_grids[0] = g_nav_grids[1] = &grid;
            g_nav_floor_connections[0] = g_nav_floor_connections[1] = &connections;
            Vec3 nearest{};
            const i32 found = navigation_nearest_open_cell(index, i32(source), &nearest);
            Vec3 converted{};
            const u32 selected = navigation_floor_connection_point(i32(source), i32(target), point, converted);
            std::cout << u32(found) << ' ' << std::bit_cast<u32>(nearest.x) << ' '
                << std::bit_cast<u32>(nearest.y) << ' ' << std::bit_cast<u32>(nearest.z) << ' '
                << selected << ' ' << std::bit_cast<u32>(converted.x) << ' '
                << std::bit_cast<u32>(converted.y) << ' ' << std::bit_cast<u32>(converted.z) << '\n';
            g_nav_grids[0] = g_nav_grids[1] = nullptr;
            g_nav_floor_connections[0] = g_nav_floor_connections[1] = nullptr;
            continue;
        }
        if (kind == "navigationsearch")
        {
            using namespace gbtg::entity_detail;
            u32 start, finish, mask, rate, elapsed, steps;
            std::cin >> start >> finish >> mask >> rate >> elapsed >> steps;
            NavGrid grid;
            grid.width = 4;
            grid.depth = 4;
            grid.cell = 16;
            grid.cost_scale = 1.0f;
            grid.cells.resize(16);
            auto* bytes = reinterpret_cast<u8*>(grid.cells.data());
            for (size_t at = 0; at < 16 * sizeof(NavCell); ++at)
            {
                u32 value;
                std::cin >> value;
                bytes[at] = u8(value);
            }
            std::array<u8, 16 * 0x14> nodes{};
            NavigationPathSearch search;
            reset_navigation_path_search(search, nodes.data(), &grid, 16);
            begin_navigation_path_search(search, i32(finish), i32(start));
            search.word_20 = rate;
            for (u32 step = 0; step < steps; ++step)
            {
                std::cout << update_navigation_path_search(search, mask, std::bit_cast<float>(elapsed)) << ' ';
            }
            std::cout << search.result_found << ' ' << search.word_24 << ' '
                << search.word_0 << ' ' << search.word_4 << ' ';
            for (size_t at = 0; at < nodes.size(); at += 4)
            {
                std::cout << read_u32({nodes.data() + at, 4}, 0) << ' ';
            }
            for (size_t at = 0; at < 16 * sizeof(NavCell); ++at)
            {
                std::cout << u32(bytes[at]) << ' ';
            }
            std::cout << '\n';
            continue;
        }
        if (kind == "navigationroute")
        {
            using namespace gbtg::entity_detail;
            u32 operation, count;
            std::cin >> operation >> count;
            NavGrid grid;
            grid.width = 4;
            grid.depth = 4;
            grid.cell = 16;
            grid.cells.resize(16);
            auto* bytes = reinterpret_cast<u8*>(grid.cells.data());
            for (size_t at = 0; at < 16 * sizeof(NavCell); ++at)
            {
                u32 value;
                std::cin >> value;
                bytes[at] = u8(value);
            }
            std::vector<u32> cells(count);
            for (auto& cell : cells)
            {
                std::cin >> cell;
            }
            std::array<float, 9> points;
            for (auto& point : points)
            {
                u32 value;
                std::cin >> value;
                point = std::bit_cast<float>(value);
            }
            std::array<u8, 16 * 0x14> nodes{};
            for (u32 index = 0; index < count; ++index)
            {
                const u16 previous = index + 1 < count ? u16(cells[index + 1]) : 0xFFFF;
                const u16 cell = u16(cells[index]);
                std::memcpy(nodes.data() + cells[index] * 0x14 + 8, &previous, 2);
                std::memcpy(nodes.data() + cells[index] * 0x14 + 0xA, &cell, 2);
            }
            NavigationPathSearch search;
            search.grid = &grid;
            search.nodes = nodes.data();
            search.first = i32(cells.back());
            search.last = i32(cells.front());
            const Vec3 start{points[0], points[1], points[2]};
            const Vec3 finish{points[3], points[4], points[5]};
            GhoulySpawnPool* route = nullptr;
            build_navigation_search_route(search, route, start, finish);
            if (operation == 1)
            {
                append_navigation_route_point(route, i32(cells.front()), Vec3{points[6], points[7], points[8]});
            }
            if (operation == 2)
            {
                set_navigation_route_endpoints(*route, finish, start);
            }
            search.scratch = route;
            u32 retargeted = 0;
            if (operation == 3)
            {
                retargeted = retarget_navigation_search_route(search,
                    i32(std::bit_cast<u32>(points[6]) % 16),
                    i32(std::bit_cast<u32>(points[7]) % 16));
                route = static_cast<GhoulySpawnPool*>(search.scratch);
            }
            std::cout << retargeted << ' ' << u32(search.first) << ' '
                << u32(search.last) << ' ' << search.word_30 << ' ';
            grid.cost_scale = points[0];
            std::cout << navigation_search_cells_connected(search, cells.front(), i32(cells.back())) << ' '
                << std::bit_cast<u32>(navigation_cell_distance_cost(grid, cells.front(), cells.back())) << ' ';
            std::cout << navigation_path_search_route_is_open(search) << ' '
                << (route->cursor - route->begin) / route->stride << ' ';
            for (const u8* row = route->begin; row < route->cursor; row += 4)
            {
                std::cout << read_u32({row, 4}, 0) << ' ';
            }
            Vec3 direction;
            Vec3 sampled;
            navigation_route_direction(*route, direction, &sampled, points[8]);
            std::cout << std::bit_cast<u32>(direction.x) << ' ' << std::bit_cast<u32>(direction.y) << ' '
                << std::bit_cast<u32>(direction.z) << ' ' << std::bit_cast<u32>(sampled.x) << ' '
                << std::bit_cast<u32>(sampled.y) << ' ' << std::bit_cast<u32>(sampled.z) << ' ';
            std::cout << std::bit_cast<u32>(float(navigation_route_length(*route))) << ' ';
            std::cout << '\n';
            engine_heap_free(route);
            continue;
        }
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
