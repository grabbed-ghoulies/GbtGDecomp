#include "avatar_motion_blur.h"
#include "actor_transform.h"
#include "actor_shape_bounds.h"
#include "actor_runtime_queries.h"
#include "actor_render_registration.h"
#include "actor_shape_push.h"
#include "actor_world_lists.h"
#include "actor_model_anchors.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/resource_error.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void copy_string(char* destination, const char* source)
{
    char value;
    do
    {
        value = *source++;
        *destination++ = value;
    } while (value != '\0');
}
}

// Xbox 0x000668E0
void avatar_motion_blur_object_sizes(ObjectSizes& output)
{
    output.instance = 0xA50;
    output.parameters = 0x4B8;
}

// Xbox 0x00066B90
void begin_runtime_avatar_glow(const Actor& actor, RuntimeAvatarGlow& context)
{
    if (context.mirror_pass != 0)
    {
        return;
    }
    Vec3 position;
    float height = 0.f;
    float radius = 0.f;
    runtime_actor_cylinder_metrics(actor, position, height, radius); // 0x0003DD50.
    std::array<float, 2> minimum{};
    std::array<float, 2> maximum{};
    if (!runtime_screen_cylinder_rect(position, radius, height, minimum.data(), maximum.data(),
        *entity_detail::g_display_view))
    {
        begin_runtime_glow_capture(nullptr, nullptr, context.pass);
        return;
    }
    begin_runtime_glow_capture(minimum.data(), maximum.data(), context.pass);
}

// Xbox 0x00066C00
void end_runtime_avatar_glow(std::span<const u8> avatar, RuntimeAvatarGlow& context)
{
    if (context.mirror_pass != 0)
    {
        return;
    }
    const auto clamp_unit = [](float value)
    {
        return value > 1.f ? 1.f : (0.f > value ? 0.f : value);
    };
    const auto alpha = [](float value) -> u32
    {
        const float clamped = 0.f > value ? 0.f : (value > 255.f ? 255.f : value);
        return u32(u8(truncate_runtime_float(clamped))); // 0x001673D8.
    };
    const u32 base = context.word_at(0x00314F48);
    const float fade = read_f32(avatar, 0x93C);
    float level = 0.f;
    float rise = 1.f;
    u32 first = 0;
    u32 second = 0;
    bool low = true;
    if (0.5f > fade) // DAT_0032B0F4.
    {
        rise = clamp_unit(fade * 2.f);
        level = 0.f;
        if (0.5f > rise)
        {
            first = scale_runtime_color(base, clamp_unit(rise * 2.f));
            second = context.word_at(0x00314F4C);
        }
        else
        {
            first = base;
            second = scale_runtime_color(base, clamp_unit((rise - 0.5f) * 2.f));
            low = 0.5f > level;
        }
    }
    else
    {
        level = clamp_unit((fade - 0.5f) * 2.f);
        first = base;
        second = scale_runtime_color(base, clamp_unit((rise - 0.5f) * 2.f));
        low = 0.5f > level;
    }
    if (low)
    {
        first = (first & 0x00FFFFFFu) | 0xFF000000u;
        second = (second & 0x00FFFFFFu) | (alpha(255.f - level * 2.f * 255.f) << 24);
    }
    else
    {
        first = (first & 0x00FFFFFFu) | (alpha(255.f - (level - 0.5f) * 2.f * 255.f) << 24);
        second = second & 0x00FFFFFFu;
    }
    composite_runtime_glow(read_u32(avatar, 0x94C), read_u32(avatar, 0x948), 0, first, second, context.pass);
}

// Xbox 0x00040CA0
bool copy_runtime_actor_model(RuntimeAvatarActorViews& actor, RuntimeAvatarActorViews& source, const char* name,
    const char* default_name, RuntimeActorStartupContext& startup)
{
    // 0x00040C00 with DAT_00546208 releases the current model.
    set_runtime_actor_model(actor.state.models, default_name, startup.registry,
        startup.error, startup.models, startup.deferred, startup.fence, startup.types, startup.shaders,
        startup.scratch, startup.heap_free);
    bind_actor_model_view(actor.state, startup);
    if (name != nullptr && std::memcmp(name, "aid_", 4) == 0)
    {
        copy_string(actor.state.models.name.data(), name);
        throw Error("untranslated Xbox 0x000F8A10 reached from the actor model copy 0x00040CA0");
    }
    copy_string(actor.state.models.name.data(), source.state.models.name.data());
    if (source.state.models.model != nullptr)
    {
        actor.state.models.model = clone_runtime_model_instance(*source.state.models.model, startup.types,
            startup.registry, startup.shaders, startup.scratch);
    }
    else
    {
        actor.state.models.model = nullptr;
    }
    bind_actor_model_view(actor.state, startup);
    return true;
}

// Xbox 0x0013AE30
void copy_runtime_actor_appearance(RuntimeAvatarActorViews& actor, RuntimeAvatarActorViews& source,
    const char* name, RuntimeActorStartupContext& startup)
{
    Actor& self = actor.actor;
    const Actor& from = source.actor;
    actor_set_world_position(self, actor_world_position(from));
    normalize_euler_angles(self.euler, actor_world_euler(from));
    self.xform_valid = 0;
    if (from.scale_x != self.scale_x || from.radius_y != self.radius_y || from.radius_xz != self.radius_xz)
    {
        self.scale_x = from.scale_x;
        self.radius_y = from.radius_y;
        self.radius_xz = from.radius_xz;
        self.xform_valid = 0;
    }
    if (self.scale_200 != from.scale_200)
    {
        self.scale_200 = from.scale_200;
        self.xform_valid = 0;
    }
    copy_runtime_actor_model(actor, source, name, name, startup);
    actor.bytes[0x19B] = source.bytes[0x19B];
    actor.state.color = (actor.state.color & 0x00FFFFFFu) | (source.state.color & 0xFF000000u);
    self.model_size_y = from.model_size_y;
    self.model_size_z = from.model_size_z;
    self.model_size_x = from.model_size_x;
    std::memcpy(actor.bytes.data() + 0x1D0, source.bytes.data() + 0x1D0, 4);
    self.model_factor = from.model_factor;
    std::array<char, 0x100> resource{};
    copy_string(resource.data(), source.state.resource.name.data());
    if (std::memcmp(resource.data(), "aid_", 4) == 0)
    {
        set_runtime_actor_resource(actor.state.resource, resource.data(), startup.registry, startup.error);
    }
}

// Xbox 0x00066AB0
void start_runtime_avatar_effect(RuntimeAvatarMotionBlur& context, const char* name, float scale)
{
    g_actor_creation_options.position = context.effects.default_position;
    g_actor_creation_options.euler = context.effects.default_euler;
    g_actor_effect_binding.mode = 0;
    g_actor_effect_binding.index = -1;
    g_actor_creation_options.scale = scale;
    g_actor_creation_options.user = &g_actor_effect_binding;
    g_actor_creation_options.flags = 0x4D;
    g_actor_effect_binding.actor = &context.self.actor;
    if (load_actor_effect_stream(name, &g_actor_creation_options, context.effects) != 0)
    {
        const i16 index = i16(g_last_actor_effect);
        g_last_actor_effect = -1;
        if (index != -1)
        {
            g_actor_effect_nodes[index]->bytes[0x18] |= 8;
        }
    }
}

// Xbox 0x00066E20
bool initialize_runtime_avatar_motion_blur(std::span<const u8> parameters, RuntimeAvatarMotionBlur& context)
{
    const auto bytes = context.self.bytes;
    put_word(bytes, 0x944, 0);
    put_float(bytes, 0x938, std::bit_cast<float>(0x3F000000u)); // DAT_0032B0F4.
    copy_string(reinterpret_cast<char*>(bytes.data() + 0x950), reinterpret_cast<const char*>(parameters.data() + 0x3B8));
    copy_string(reinterpret_cast<char*>(bytes.data() + 0x9D0), reinterpret_cast<const char*>(parameters.data() + 0x438));
    auto source = context.actor_at(read_u32(parameters, 0x334));
    copy_runtime_actor_appearance(context.self, source, context.default_name, context.startup);
    actor_refresh_transform(context.self.actor);
    put_float(bytes, 0x934, context.self.actor.radius_y);
    const long double radius = actor_model_sweep_radius(context.self.actor);
    start_runtime_avatar_effect(context, reinterpret_cast<const char*>(bytes.data() + 0x950),
        float(radius * static_cast<long double>(std::bit_cast<float>(0x3DCCCCCDu)))); // DAT_0032B254.
    append_runtime_actor_snapshot(context.self.actor, context.snapshots);
    put_word(bytes, 0x1A0, 0x00066B90);
    put_word(bytes, 0x1A4, 0x00066C00);
    put_word(bytes, 0x1A8, context.address);
    const char* resource = reinterpret_cast<const char*>(parameters.data() + 0x338);
    if (std::memcmp(resource, "aid_", 4) == 0)
    {
        context.resource = acquire_animation_resource(context.startup.registry, resource);
        if (context.resource.data() == nullptr)
        {
            resource_error(context.startup.error, 2);
        }
    }
    else
    {
        context.resource = {};
    }
    put_word(bytes, 0x94C, context.resource.data() == nullptr ? 0 : context.resource_address(context.resource.data()));
    put_word(bytes, 0x940, 0xFFFFFFFFu);
    put_word(bytes, 0x948, 7u - (source.bytes[0x196] != 0 ? 1u : 0u));
    return true;
}

// Xbox 0x00066940
long double runtime_avatar_pulse(const RuntimeAvatarMotionBlur& context)
{
    const auto bytes = context.self.bytes;
    if (read_u32(bytes, 0x944) != 3)
    {
        return 1.L; // DAT_0032B0F8.
    }
    const float timer = read_f32(bytes, 0x938);
    float clamped = 1.f;
    if (!(timer > 1.f))
    {
        clamped = 0.f > timer ? 0.f : timer;
    }
    constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    constexpr float half_pi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
    const long double first = retail_sin(clamped * pi - half_pi);
    const float eased = float((((first + 1.L) * 0.5L) * pi) - half_pi);
    return (retail_sin(eased) + 1.L) * 0.5L;
}

// Xbox 0x00066F70
void ignite_runtime_avatar(RuntimeAvatarMotionBlur& context)
{
    const long double radius = actor_model_sweep_radius(context.self.actor);
    start_runtime_avatar_effect(context, reinterpret_cast<const char*>(context.self.bytes.data() + 0x9D0),
        float(radius * static_cast<long double>(std::bit_cast<float>(0x3D638E39u)))); // DAT_0032B5E4.
    Actor& actor = context.self.actor;
    queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f, "XACT_SOUNDBANK_GMISC_ONFIRE",
        context.sounds); // 0x002FF778.
    queue_actor_sound(actor.sound_slots[5], 0, &actor, 5, nullptr, nullptr, 1.f, "XACT_SOUNDBANK_GMISC_BURNDITTY",
        context.sounds); // 0x002FF7F8.
}

// Xbox 0x000669D0
void light_runtime_avatar(RuntimeAvatarMotionBlur&)
{
    throw Error("untranslated Xbox 0x00123720 (a scene point light) reached from the avatar light 0x000669D0");
}

// Xbox 0x00066A80
void release_runtime_avatar_light(RuntimeAvatarMotionBlur& context)
{
    if (-1 < i32(read_u32(context.self.bytes, 0x940)))
    {
        throw Error("avatar light release 0x00066A80 needs the DAT_00511B98 point light records");
    }
}

// Xbox 0x00066FE0
void tick_runtime_avatar_motion_blur(RuntimeAvatarMotionBlur& context)
{
    // DAT_002FF768: each phase's length.
    static constexpr std::array<u32, 6> lengths{0x3F000000u, 0x3F000000u, 0x3FA00000u, 0x3F800000u, 0x54434158u,
        0x554F535Fu}; // Words 4 and 5 are the "XACT_SOU" of the name at 0x002FF778.
    const auto length = [](u32 phase)
    {
        return std::bit_cast<float>(lengths.at(phase));
    };
    const auto bytes = context.self.bytes;
    Actor& actor = context.self.actor;
    float step = 0.f;
    if (!(context.clock.freeze > 0.f))
    {
        step = context.clock.dt * context.clock.scale;
    }
    float rate = 0.f;
    if (!(actor.animation_pause > 0.f))
    {
        rate = actor.animation_rate;
    }
    const float timer = read_f32(bytes, 0x938);
    bool expired = false;
    if (timer > 0.f)
    {
        const float left = timer - rate * step;
        put_float(bytes, 0x938, left);
        expired = left <= 0.f; // JC on 0 < left, unordered included.
    }
    if (expired)
    {
        put_float(bytes, 0x938, 0.f);
        const u32 phase = read_u32(bytes, 0x944);
        put_word(bytes, 0x944, phase + 1);
        if (phase == 0)
        {
            ignite_runtime_avatar(context);
            if (context.lights_enabled != 0 || context.script_lights != 0)
            {
                light_runtime_avatar(context);
            }
        }
        else if (phase == 2)
        {
            release_actor_sound(actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI avatar +0x614.
        }
        else if (phase == 3)
        {
            release_runtime_avatar_light(context);
            activate_actor_bind_tree(actor); // 0x00112CC0.
            return;
        }
        put_float(bytes, 0x938, length(read_u32(bytes, 0x944)));
    }
    else
    {
        const float scale_x = actor.scale_x;
        const float scale_z = actor.radius_xz;
        const long double pulse = runtime_avatar_pulse(context);
        float factor = std::bit_cast<float>(0x38D1B717u); // DAT_0032B200.
        if (!(static_cast<long double>(factor) >= pulse))
        {
            factor = float(pulse);
        }
        const float scale_y = read_f32(bytes, 0x934) * factor;
        if (scale_x != actor.scale_x || scale_y != actor.radius_y || scale_z != actor.radius_xz)
        {
            actor.radius_y = scale_y;
            actor.scale_x = scale_x;
            actor.radius_xz = scale_z;
            actor.xform_valid = 0;
        }
        if (-1 < i32(read_u32(bytes, 0x940)))
        {
            throw Error("avatar light radius needs the DAT_00511B8C point light records");
        }
    }
    const float span = length(read_u32(bytes, 0x944));
    float fade = 1.f;
    if (span != 0.f)
    {
        const float value = (read_f32(bytes, 0x938) - span) / (0.f - span);
        if (value <= 1.f)
        {
            fade = value < 0.f ? 0.f : value;
        }
    }
    put_float(bytes, 0x93C, fade);
    switch (read_u32(bytes, 0x944))
    {
    case 1:
        fade = fade + 1.f;
        [[fallthrough]];
    case 0:
        fade = fade * 0.25f; // DAT_0032B104.
        break;
    case 2:
        fade = fade * 0.25f + 0.5f; // DAT_0032B0F4.
        break;
    case 3:
        fade = fade * 0.25f + 0.75f; // DAT_0032B2AC.
        break;
    case 4:
        activate_actor_bind_tree(actor); // 0x00112CC0.
        return;
    default:
        update_actor_frame(context.frame, context.frame_context); // 0x000420D0.
        return;
    }
    put_float(bytes, 0x93C, fade);
    update_actor_frame(context.frame, context.frame_context); // 0x000420D0.
}

// Xbox 0x00066F50
void destroy_runtime_avatar_motion_blur(RuntimeAvatarMotionBlur& context)
{
    if (context.resource.data() == nullptr)
    {
        throw Error("avatar motion blur 0x00035C00 over a null +0x94C resource");
    }
    release_animation_resource(context.startup.registry, context.resource);
}

// Xbox 0x00069080
void avatar_afterimage_object_sizes(ObjectSizes& output)
{
    output.instance = 0x968;
    output.parameters = 0x354;
}

// Xbox 0x000690F0
bool initialize_runtime_avatar_afterimage(std::span<const u8> parameters, RuntimeAvatarAfterimage& context)
{
    const auto bytes = context.self.bytes;
    put_word(bytes, 0x938, read_u32(parameters, 0x338));
    put_word(bytes, 0x934, read_u32(parameters, 0x338));
    put_word(bytes, 0x940, parameters[0x33C]);
    put_word(bytes, 0x93C, parameters[0x33C]);
    put_word(bytes, 0x964, read_u32(parameters, 0x34C));
    auto source = context.actor_at(read_u32(parameters, 0x334));
    copy_runtime_actor_appearance(context.self, source, context.default_name, context.startup); // 0x0013AE30
    const Actor& actor = context.self.actor;
    put_float(bytes, 0x944, actor.scale_x); // +0x30.
    put_float(bytes, 0x948, actor.radius_y); // +0x34.
    put_float(bytes, 0x94C, actor.radius_xz); // +0x38.
    put_word(bytes, 0x950, read_u32(parameters, 0x340));
    put_word(bytes, 0x954, read_u32(parameters, 0x344));
    put_word(bytes, 0x958, read_u32(parameters, 0x348));
    const u32 sound = read_u32(parameters, 0x350);
    if (sound != 0 && read_u32(context.bytes_at(sound), 8) == 0x5F646961) // "aid_"
    {
        throw Error("untranslated Xbox 0x000F8540 reached from the after-image 0x000690F0 (parameter +0x350 names "
            "an aid_ sound)");
    }
    return true;
}

// Xbox 0x00069220
void destroy_runtime_avatar_afterimage(RuntimeAvatarAfterimage& context)
{
    if (read_u32(context.self.bytes, 0x95C) != 0)
    {
        throw Error("untranslated Xbox 0x000F85D0 reached from the after-image 0x00069220 (+0x95C set)");
    }
    if (read_u32(context.self.bytes, 0x960) != 0)
    {
        throw Error("after-image 0x00069220 releases a +0x960 resource it has no view of");
    }
}

// Xbox 0x00069250
void tick_runtime_avatar_afterimage(RuntimeAvatarAfterimage& context)
{
    const auto bytes = context.self.bytes;
    Actor& actor = context.self.actor;
    if (read_u32(bytes, 0x95C) != 0)
    {
        throw Error("untranslated Xbox 0x000F8640 reached from the after-image 0x00069250 (+0x95C set)");
    }
    const float left = read_f32(bytes, 0x934);
    if (left > 0.f)
    {
        const float next = left - runtime_actor_step(actor, context.clock);
        put_float(bytes, 0x934, next);
        if (next <= 0.f) // JC on 0 < next, unordered included.
        {
            put_float(bytes, 0x934, 0.f);
            activate_actor_bind_tree(actor); // 0x00112CC0
        }
    }
    const float phase = read_f32(bytes, 0x934) / read_f32(bytes, 0x938);
    constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    constexpr float half_pi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
    const long double wave = retail_sin(phase * pi - half_pi); // 0x00153320
    const long double alpha = (wave + 1.L) * static_cast<long double>(i32(read_u32(bytes, 0x940))) * 0.5L;
    put_word(bytes, 0x93C, u32(truncate_runtime_float(alpha))); // 0x001673D8
    const float towards = 1.f - phase; // DAT_0032B0F8.
    const float x = (read_f32(bytes, 0x950) - read_f32(bytes, 0x944)) * towards + read_f32(bytes, 0x944);
    const float y = (read_f32(bytes, 0x954) - read_f32(bytes, 0x948)) * towards + read_f32(bytes, 0x948);
    const float z = (read_f32(bytes, 0x958) - read_f32(bytes, 0x94C)) * towards + read_f32(bytes, 0x94C);
    if (x != actor.scale_x || y != actor.radius_y || z != actor.radius_xz)
    {
        actor.scale_x = x;
        actor.radius_y = y;
        actor.radius_xz = z;
        actor.xform_valid = 0; // +0x2A8.
    }
    update_actor_frame(context.frame, context.frame_context); // 0x000420D0
}

// Xbox 0x00069540
void message_runtime_avatar_afterimage(u32 id, RuntimeAvatarAfterimage& context)
{
    if (id != 0x23)
    {
        return;
    }
    unregister_actor_segment_probes(context.render); // 0x0003E850
    clear_actor_shape_probe(context.self.actor, context.shape); // 0x0003C700
    auto* node = static_cast<entity_detail::ListHead*>(context.anchors.next);
    while (node != nullptr)
    {
        auto* next = static_cast<entity_detail::ListHead*>(node->next);
        release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(node)); // 0x0011BDA0
        node = next;
    }
}

// Xbox 0x00069410
u32 draw_runtime_avatar_afterimage(i32 kind, RuntimeAvatarAfterimageDraw& context)
{
    if (kind != 2)
    {
        return 0;
    }
    const bool glow = read_u32(context.bytes, 0x964) != 0;
    if (glow)
    {
        Vec3 position;
        float height = 0.f;
        float radius = 0.f;
        runtime_actor_cylinder_metrics(context.actor, position, height, radius); // 0x0003DD50
        std::array<float, 2> minimum{};
        std::array<float, 2> maximum{};
        if (!runtime_screen_cylinder_rect(position, radius, height, minimum.data(), maximum.data(),
            *entity_detail::g_display_view)) // 0x0013BCC0
        {
            return 0;
        }
        begin_runtime_glow_capture(minimum.data(), maximum.data(), context.pass); // 0x00062220
    }
    if (context.model == nullptr)
    {
        throw Error("after-image draw 0x00069410 has no actor +0x204 model");
    }
    auto& model = *context.model;
    if (!glow)
    {
        // +0x1C4 is the kind argument's stack word with the alpha stored in its top byte.
        model.material_enabled = 1;
        model.material_first = (u32(context.bytes[0x93C]) << 24) | (u32(kind) & 0x00FFFFFFu);
        model.material_second = context.material_second;
    }
    AnimationMatrix world;
    std::memcpy(world.data(), context.actor.xform, sizeof(context.actor.xform)); // Actor +0x2AC.
    if ((context.draw.image.byte(model.cpu, 8) & 0x10) != 0)
    {
        draw_runtime_scene_model(model.instance, model.cpu, world, 0, context.draw); // 0x000F9080
    }
    draw_runtime_scene_model(model.instance, model.cpu, world, 1, context.draw);
    if ((context.draw.image.byte(model.cpu, 8) & 8) != 0)
    {
        draw_runtime_scene_model(model.instance, model.cpu, world, 2, context.draw);
    }
    if (glow)
    {
        // White with the +0x93C alpha in the top byte.
        stamp_runtime_glow((u32(context.bytes[0x93C]) << 24) | 0x00FFFFFFu, 1, context.pass); // 0x00062490, EAX 1
    }
    return 1;
}
}
