#include "player_health_display.h"
#include "gbtg/retail_field.h"
#include <bit>

namespace gbtg
{
// Xbox 0x0012F990
void update_runtime_hud_counter_digits(RuntimeHudCounter& counter, std::span<const u8> preset)
{
    i32 value = counter.value;
    auto update = [&](size_t index)
    {
        const i32 digit = value % 10;
        value /= 10;
        if (digit != counter.digits[index])
        {
            counter.digits[index] = digit;
            counter.timer = std::bit_cast<u32>(retail_view<HudCounterPreset>(preset).swell_time);
        }
    };
    update(7);
    update(6);
    update(5);
    update(4);
    update(3);
    update(2);
    update(1);
    update(0);
}

// Xbox 0x0012FE10
void acquire_runtime_player_health_textures(RuntimePlayerHealthDisplay& context)
{
    if (context.textures[0].data() == nullptr)
    {
        const char* name;
        if ((context.story_flags & 0x40) == 0)
        {
            name = "aid_texture_ghoulies_scoreboard_energy_heart";
        }
        else
        {
            name = "aid_texture_ghoulies_scoreboard_energy_heartgold";
        }
        const auto texture = acquire_animation_resource(context.registry, name);
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[0] = texture;
        context.widths[0] = read_u16(texture, 8);
    }
    if (context.textures[1].data() == nullptr)
    {
        const char* name;
        if ((context.story_flags & 0x40) == 0)
        {
            name = "aid_texture_ghoulies_scoreboard_energy_heartbroken";
        }
        else
        {
            name = "aid_texture_ghoulies_scoreboard_energy_heartgoldbroken";
        }
        const auto texture = acquire_animation_resource(context.registry, name);
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[1] = texture;
        context.widths[1] = read_u16(texture, 8);
    }
    if (context.textures[2].data() == nullptr)
    {
        const auto texture = acquire_animation_resource(context.registry, "aid_texture_ghoulies_powerups_invunerability");
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[2] = texture;
        context.widths[2] = read_u16(texture, 8);
    }
    if (context.textures[3].data() == nullptr)
    {
        const auto texture = acquire_animation_resource(context.registry, "aid_texture_particles_impheadglow");
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[3] = texture;
        context.widths[3] = read_u16(texture, 8);
    }
    if (context.textures[4].data() == nullptr)
    {
        const auto texture = acquire_animation_resource(context.registry, "aid_texture_ghoulies_scoreboard_dialogheads_baron");
        if (texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.textures[4] = texture;
        context.widths[4] = read_u16(texture, 8);
    }
}

// Xbox 0x00130710
void set_runtime_player_health_display(RuntimePlayerHealthDisplay& context, i32 health, u32 mode)
{
    acquire_runtime_player_health_textures(context);
    if (context.counter.value != health || context.counter.target != health)
    {
        context.intensity = 2.f;
    }
    context.active = 1;
    context.mode = mode;
    const auto preset = mode != 0 ? context.active_preset : context.normal_preset;
    if (health < 0)
    {
        health = 0;
    }
    context.counter.target = health;
    if (context.counter.value == 0)
    {
        context.counter.value = health;
        update_runtime_hud_counter_digits(context.counter, preset);
    }
    if (context.override_health == 0 && context.style != 1)
    {
        i32 index = 0;
        size_t row = 0;
        do
        {
            if (context.counter.value <= retail_view<HealthBandRow>(context.thresholds.subspan(row)).limit)
            {
                if (index == 0)
                {
                    context.intensity = 2.f;
                    acquire_runtime_player_health_textures(context);
                    return;
                }
                break;
            }
            row += 0x18;
            ++index;
        } while (row < 0x78);
    }
    if (mode == 0)
    {
        return;
    }
    context.intensity = 2.f;
    acquire_runtime_player_health_textures(context);
}
}
