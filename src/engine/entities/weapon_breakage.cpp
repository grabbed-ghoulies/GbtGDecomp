#include "weapon_breakage.h"
#include "weapon_actor_detail.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_model_pieces.h"
#include "actor_render_registration.h"
#include "actor_spawn_eligibility.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "ghouly_box_decoder.h"
#include "../scene/actor_loading.h"
#include "gbtg/sound_runtime.h"
#include "../scripts/script_actions.h"

namespace gbtg
{
using namespace weapon_detail;

// Xbox 0x001486C0
void load_runtime_weapon_stat_tables(RuntimeWeaponStatTables& tables, AnimationResourceRegistry& registry,
    ResourceErrorState& error)
{
    tables.stats = acquire_animation_resource(registry, "aid_misc_ghoulies_weapon_stattable"); // 0x00035E90
    if (tables.stats.data() == nullptr)
    {
        resource_error(error, 2); // 0x00032700
    }
    tables.count = u32(tables.stats.size()) / 0x44; // 0x00035D90's size.
    tables.text = acquire_animation_resource(registry, "aid_loctext_ghoulies_weapon_weaponstattable");
    if (tables.text.data() == nullptr)
    {
        resource_error(error, 2);
    }
}

// Xbox 0x00148640, EAX tag
i32 runtime_weapon_stat_row(const RuntimeWeaponStatTables& tables, const char* tag_name)
{
    for (i32 row = 0; row < i32(tables.count); ++row)
    {
        const char* name = reinterpret_cast<const char*>(tables.stats.data() + size_t(row) * 0x44);
        for (size_t index = 0;; ++index)
        {
            u8 first = u8(tag_name[index]);
            if (first > 0x20)
            {
                first &= 0xDF;
            }
            u8 second = u8(name[index]);
            if (second > 0x20)
            {
                second &= 0xDF;
            }
            if (first != second)
            {
                break;
            }
            if (first == 0)
            {
                return row;
            }
        }
    }
    return -1;
}

// Xbox 0x00148740
i32 runtime_weapon_stat_points(RuntimeWeaponActor& weapon, std::span<char16_t> text)
{
    auto& services = weapon.services;
    const char* name = services.tag_name(weapon.actor.class_key); // DAT_0042A850[object +6].
    const i32 row = runtime_weapon_stat_row(services.stats, name);
    if (row == -1)
    {
        return -1;
    }
    // The caption key is the tag after its last underscore.
    size_t last = 0;
    for (size_t index = 0; name[index] != 0; ++index)
    {
        if (name[index] == '_')
        {
            last = index;
        }
    }
    if (lookup_borrowed_localtext(services.stats.text, "weaponstattable", name + last + 1, text.data(),
        services.text) == 0) // 0x000FF120
    {
        return -1;
    }
    return i32(read_u32(services.stats.stats, size_t(row) * 0x44 + 0x40));
}

// Xbox 0x001456D0, ESI actor
void award_runtime_weapon_points(RuntimeWeaponActor& weapon, u16 category)
{
    if (read_u32(weapon.bytes, 0xDA4) == 0)
    {
        return;
    }
    const i32 points = runtime_weapon_stat_points(weapon, weapon.services.award_text); // 0x00148740
    if (points < 0)
    {
        return;
    }
    const Actor* owner = weapon.actor.spawn_owner; // +0xD8C.
    if (owner == nullptr || owner->mask_mode == 2 || owner->mask_flag != 0)
    {
        ++weapon.services.broken_parts; // DAT_00545978.
        weapon.services.award(points, category, weapon.services.award_text.data(),
            i32(read_u32(weapon.bytes, 0x9A4))); // 0x0012DE90
    }
}

// Xbox 0x001457A0, ESI actor
void set_runtime_weapon_piece_pairs(RuntimeWeaponActor& weapon, float fraction)
{
    const Actor& actor = weapon.actor;
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return;
    }
    const auto cpu = actor.model->cpu;
    const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
    if (header == 0 || cpu[header + 0x19] == 0xFF)
    {
        return;
    }
    const u8 pair = cpu[header + 0x19];
    const long double scaled = static_cast<long double>(i32(cpu[header + 0x1A])) *
        (static_cast<long double>(1.f) - fraction); // DAT_0032B0F8.
    const u8 value = u8(i32(scaled)); // 0x001673D8.
    entity_store_pair(std::span<u8>(weapon.view.pose.node_flags), pair, value); // 0x000F96F0 on +0x204's +0x294
    if (read_u32(weapon.bytes, 0x310) != 0)
    {
        throw Error("0x001457A0 stores the pair on actor +0x310's model, which has no native view");
    }
}

// Xbox 0x001454F0, EAX actor
bool break_runtime_weapon(RuntimeWeaponActor& weapon)
{
    Actor& actor = weapon.actor;
    auto bytes = weapon.bytes;
    u32 groups = 0;
    u32 header = 0;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
        if (header != 0)
        {
            groups = cpu[header];
        }
    }
    bool launched = false;
    if (i32(groups) > 0 && weapon.matrices.matrices.count > 0)
    {
        const auto cpu = actor.model->cpu;
        dispatch_actor_resource_callout(weapon.view.startup.resource, 0xD, nullptr, weapon.base().callouts); // 0x0003BE20
        bytes[0x298] = 0;
        if (i32(read_u32(bytes, 0xD90)) < i32(groups))
        {
            launched = true;
            do
            {
                const u32 group = read_u32(bytes, 0xD90);
                RuntimeActorMatrixSampling sampling{{weapon.matrices.allocation,
                    size_t(weapon.matrices.matrices.count) * 0x48}, weapon.matrices.matrices.node,
                    weapon.services.sampling_scratch, weapon.services.sampling_active, actor_transform_origin,
                    weapon.model_source};
                launch_actor_model_piece_group(actor, i32(group), weapon.pieces, weapon.matrices, sampling,
                    weapon.model_source, weapon.random); // 0x0003F820
                store32(bytes, 0xD94, read_u32(bytes, 0xD94) + cpu[header + 0x16 + group * 0xC]);
                store32(bytes, 0xD90, group + 1);
            }
            while (i32(read_u32(bytes, 0xD90)) < i32(groups));
        }
    }
    set_runtime_weapon_piece_pairs(weapon, 0.f); // 0x001457A0
    bytes[0x93C] = 0;
    actor.mask_flag = i32(u32(actor.mask_flag) & ~0xFFu);
    if (bytes[0xD08] != 0)
    {
        const u16 voice = read_u16(bytes, 0xD88);
        bytes[0xD08] = 0;
        if (voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
            store16(bytes, 0xD88, 0xFFFF);
        }
    }
    if (read_u32(bytes, 0x310) != 0)
    {
        store32(bytes, 0x30C, 0);
    }
    if (launched)
    {
        auto* node = static_cast<entity_detail::ListHead*>(weapon.view.anchors.next); // Actor +0x1F8.
        while (node != nullptr)
        {
            auto* next = static_cast<entity_detail::ListHead*>(node->next);
            release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(node)); // 0x0011BDA0
            node = next;
        }
        weapon.services.release_frames(actor, weapon.view.frame.updates); // 0x0003DE90
        request_runtime_weapon_variant(weapon, reinterpret_cast<const char*>(bytes.data() + 0xAEC),
            reinterpret_cast<const char*>(bytes.data() + 0xB6C)); // 0x00148430
    }
    actor.spawn_model_blocked = 1; // +0x9A0.
    store32(bytes, 0x9A0, 1);
    const u16 category = read_u16(bytes, 0xAA8);
    if (category != 0 && read_u32(bytes, 0x9A4) == 0)
    {
        award_runtime_weapon_points(weapon, category); // 0x001456D0
    }
    if (read_u32(bytes, 0x9A4) == 0)
    {
        ++weapon.services.broken_parts; // DAT_00545978.
        const std::array<i32, 2> message{0x8F, i32(weapon.self_address())};
        broadcast_actor_pulse_message(message.data()); // 0x00122C70
    }
    return launched;
}

// Xbox 0x00148430, EAX name, ECX actor
bool request_runtime_weapon_variant(RuntimeWeaponActor& weapon, const char* name, const char* attributes)
{
    auto bytes = weapon.bytes;
    u32 head;
    std::memcpy(&head, name, 4);
    if (head != kAidPrefix)
    {
        return false;
    }
    if (read_u32(bytes, 0xC00) != 0 && !actor_can_supply_ghoulybox(weapon.actor)) // 0x00146150
    {
        return false;
    }
    if (weapon.actor.spawn_model_blocked != 0) // +0x9A0.
    {
        return false;
    }
    auto* record = load_ghoulybox(name, weapon.registry, weapon.error, weapon.random); // 0x000F39A0
    if (record == nullptr)
    {
        throw Error("0x00148430 writes the ghoulybox record 0x000F39A0 did not load");
    }
    weapon.spawn_record = record;
    record->controller_24 = &weapon.controller;
    record->flags_2c = 0;
    if (attributes != nullptr)
    {
        std::strcpy(record->attributes_30, attributes);
    }
    link_actor_load_record(&g_side_list, *record); // 0x000F3C30 on DAT_00510568
    if (bytes[0x93C] != 0)
    {
        if (read_u32(bytes, 0xC00) != 0)
        {
            weapon.services.runtime_flags |= 2; // DAT_0050EA9C.
            activate_actor_bind_tree(weapon.actor); // 0x00112CC0
        }
        else
        {
            bytes[0xAEC] = 0;
            std::array<u32, 0x42> message{};
            message[0] = 0x13;
            weapon.message(message); // Vtable +0xC.
        }
        scare_near_runtime_weapon(weapon, read_f32(bytes, 0xBF0)); // 0x0013B660
    }
    return weapon.spawn_record != nullptr;
}

// Xbox 0x0013B660, ESI actor
void scare_near_runtime_weapon(RuntimeWeaponActor& weapon, float radius)
{
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const std::array<u32, 4> message{0x37, std::bit_cast<u32>(position.x), std::bit_cast<u32>(position.y),
        std::bit_cast<u32>(position.z)};
    float reach = 60.f; // DAT_0032B28C.
    if (!(reach >= radius))
    {
        reach = radius;
    }
    // DAT_00511B38's player nodes, in list order, for 0x00122D80.
    const auto marks = weapon.marks();
    std::vector<RuntimeActorProximityNode> nodes;
    nodes.reserve(marks.size());
    for (const auto& mark : marks)
    {
        nodes.push_back(RuntimeActorProximityNode{nullptr, *mark.actor,
            [&weapon, address = mark.address](std::span<const u32> words)
            {
                std::array<u32, 0x42> buffer{};
                std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
                weapon.send(address, buffer);
            }});
    }
    for (size_t index = 1; index < nodes.size(); ++index)
    {
        nodes[index - 1].next = &nodes[index];
    }
    broadcast_runtime_actor_proximity(nodes.empty() ? nullptr : nodes.data(), message, position, reach); // 0x00122D80
}
}
