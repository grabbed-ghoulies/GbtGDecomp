#include "background_parts.h"
#include "../navigation/navigation.h"
#include "../navigation/grid_regions.h"
#include "../entities/player_mind_events.h"
#include "gbtg/model_runtime.h"

#include <cstring>
#include <bit>

namespace gbtg
{
// Xbox 0x00044D20
void dispatch_background_part_effects(Actor& actor, const char* name,
    const Vec3& position, const Vec3& source, BackgroundPartDispatch& context)
{
    const i32 index = find_background_part_settings(name, context.settings, context.setting_count);
    if (index != -1)
    {
        const i32 old_slot = context.sound_slot;
        context.sound_slot = std::bit_cast<i32>(u32(context.sound_slot) + 1u);
        if (context.sound_slot >= 5)
        {
            context.sound_slot = 0;
        }
        const size_t offset = u32(index) * 0x22C;
        const char* sound = reinterpret_cast<const char*>(context.settings.data() + offset + 0x20);
        if (*sound != '\0')
        {
            queue_actor_sound(actor.sound_slots[old_slot], context.sound_groups[old_slot],
                &actor, u16(old_slot), &position, nullptr, 1.f, sound, context.sounds);
        }
        if (read_u32(context.settings, offset + 0x120) == 0x5F646961u)
        {
            auto& options = g_actor_creation_options;
            options.flags = 0;
            const Vec3 direction{position.x - source.x, position.y - source.y, position.z - source.z};
            float angle;
            runtime_direction_angle(direction, angle);
            options.position = position;
            options.flags |= 5;
            options.euler = {0.f, angle, 0.f};
            load_actor_effect_stream(reinterpret_cast<const char*>(context.settings.data() + offset + 0x120),
                &options, context.effects);
        }
    }
}

// Xbox 0x000450A0
void advance_background_part_stage(Actor& actor, RuntimeModelInstance* secondary,
    std::span<BackgroundAnimationVolume> rows, i32 row, i32 part, std::span<const u8> header,
    const Vec3& position, const Vec3& source,
    std::span<u32> shared_states, BackgroundPartDispatch& context)
{
    const size_t metadata = u32(part) * 12;
    if (header[metadata + 0x19] != 0xFF)
    {
        if (row < 0 || size_t(row) >= rows.size())
        {
            throw Error("background part stage row lies outside actor +0x94C");
        }
        auto& fields = rows[size_t(row)].fields;
        u32 stage = read_u32(fields, 4);
        if (std::bit_cast<i32>(stage) < i32(header[metadata + 0x1A]))
        {
            ++stage;
            std::memcpy(fields.data() + 4, &stage, 4);
        }
        const u8 slot = header[metadata + 0x19];
        if (actor.model != nullptr)
        {
            set_runtime_model_node_state(*actor.model->runtime, slot, u8(stage), shared_states);
        }
        if (secondary != nullptr)
        {
            set_runtime_model_node_state(*secondary, slot, u8(stage), shared_states);
        }
        const u32 time = 0x40200000u; // DAT_0032B20C, 2.5f.
        std::memcpy(fields.data() + 8, &time, 4);
    }
    const u32 strings = read_u32(header, 4);
    const char* name = strings != 0 ? reinterpret_cast<const char*>(header.data() + strings + header[metadata + 0x17]) : "unknown"; // DAT_00322F70.
    dispatch_background_part_effects(actor, name, position, source, context);
}

// Xbox 0x00044CA0
void load_background_part_tables(BackgroundPartTables& tables, AnimationResourceRegistry& registry,
    std::span<const RuntimeBodyLoadedResourceSize> loaded, ResourceErrorState& error)
{
    const auto settings = acquire_animation_resource(registry, "aid_misc_ghoulies_background_parttable");
    if (settings.data() == nullptr)
    {
        resource_error(error, 2);
    }
    tables.settings = settings;
    const u32 size = runtime_body_resource_size(settings.data(), registry.records, loaded);
    tables.setting_count = i32(u32((u64(size) * 0x75DED953u) >> 40)); // size / 0x22C.
    const auto text = acquire_animation_resource(registry, "aid_loctext_ghoulies_background_backgroundparttable");
    if (text.data() == nullptr)
    {
        resource_error(error, 2);
    }
    tables.text = text;
}

// Xbox 0x00044EA0
i32 find_active_background_part(std::span<const BackgroundAnimationVolume> rows, i32 part)
{
    size_t row = 0;
    while (true)
    {
        if (row >= rows.size())
        {
            return -1;
        }
        if (std::bit_cast<i32>(read_u32(rows[row].fields, 0)) == part)
        {
            break;
        }
        ++row;
    }
    return i32(row);
}

// Xbox 0x00045CB0
bool background_part_can_be_destroyed(const Actor& actor, std::span<const BackgroundAnimationVolume> rows,
    i32 part, std::span<const u8> zero)
{
    size_t row = 0;
    while (true)
    {
        if (row >= rows.size())
        {
            return true;
        }
        if (std::bit_cast<i32>(read_u32(rows[row].fields, 0)) == part)
        {
            break;
        }
        ++row;
    }
    const auto& fields = rows[row].fields;
    auto header = zero;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 address = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
        header = address != 0 ? cpu.subspan(address) : zero;
    }
    const u32 selected = read_u32(fields, 0);
    if (header[0x19 + selected * 12] != 0xFF && read_f32(fields, 8) == 0.f)
    {
        return std::bit_cast<i32>(read_u32(fields, 4)) < i32(header[0x1A + u32(part) * 12]);
    }
    return false;
}

// Xbox 0x00044E70
u32 background_part_settings_value(const char* name, std::span<const u8> settings, i32 count)
{
    const i32 index = find_background_part_settings(name, settings, count);
    if (index != -1)
    {
        return read_u32(settings, u32(index) * 0x22C + 0x220);
    }
    return 0xFFFFFFFFu;
}

// Xbox 0x00044C20
i32 find_background_part_settings(const char* name, std::span<const u8> settings, i32 count)
{
    i32 index = 0;
    if (count > 0)
    {
        do
        {
            const char* other = reinterpret_cast<const char*>(settings.data() + index * 0x22C);
            size_t character = 0;
            do
            {
                u8 left = u8(name[character]);
                u8 right = u8(other[character]);
                if (left > 0x20)
                {
                    left &= 0xDF;
                }
                if (right > 0x20)
                {
                    right &= 0xDF;
                }
                if (left != right)
                {
                    break;
                }
                ++character;
                if (left == 0)
                {
                    return index;
                }
            }
            while (true);
            ++index;
        }
        while (index < count);
    }
    return -1;
}

// Xbox 0x00046470
const char* background_part_rumble(const Actor& actor, i32 part,
    std::span<const u8> settings, i32 count, const char* fallback, std::span<const u8> zero)
{
    auto header = zero;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 offset = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
        header = offset != 0 ? cpu.subspan(offset) : zero;
    }
    const u32 strings = read_u32(header, 4);
    const char* name;
    if (strings == 0)
    {
        name = "unknown";
    }
    else
    {
        name = reinterpret_cast<const char*>(header.data() + strings + header[0x17 + u32(part) * 12]);
    }
    const i32 index = find_background_part_settings(name, settings, count);
    if (index == -1)
    {
        return fallback;
    }
    return reinterpret_cast<const char*>(settings.data() + index * 0x22C + 0x1A0);
}

// Xbox 0x00045EC0
void prepare_background_volume_navigation(const Actor& actor, std::span<BackgroundAnimationVolume> records)
{
    using namespace entity_detail;
    u32 count = 0;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const auto cpu = actor.model->cpu;
        const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
        if (header != 0)
        {
            count = cpu[header];
        }
    }
    for (u32 index = 0; index < count; ++index)
    {
        auto& record = records[index];
        const u16 tag = read_u16(record.fields, 0x10);
        if (tag != 0 && g_nav_count != 0 && g_nav_count > 0)
        {
            for (i32 floor = 0; floor < g_nav_count; ++floor)
            {
                set_navigation_region_blocked(*g_nav_grids[floor], tag, 1);
            }
        }
        if (record.attachment != nullptr)
        {
            Vec3 point;
            std::memcpy(&point, record.attachment->shape.data() + 4, 12);
            record.attachment->flag = u8(navigation_floor_index(point, -1, 0));
        }
    }
}
}
