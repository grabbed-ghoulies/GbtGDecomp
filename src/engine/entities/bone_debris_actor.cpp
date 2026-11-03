#include "bone_debris_actor.h"
#include "actor_transform.h"
#include "euler_wrapping.h"
#include "fx_anim_emitter.h"
#include "../audio/listener_settings.h"
#include "../graphics/effect_particles.h"
#include "../graphics/effect_placement.h"
#include "../graphics/shader_parameters.h"
#include "../scene/background_part_destruction.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = bone_debris_field;
namespace parameter = bone_debris_parameter;

i32 carried_effect(std::span<const u8> bytes, u32 index)
{
    return get(bytes, field::carried_effects).at(index);
}

void set_carried_effect(std::span<u8> bytes, u32 index, i32 slot)
{
    auto slots = get(bytes, field::carried_effects);
    slots.at(index) = slot;
    set(bytes, field::carried_effects, slots);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

Vec3 read_vector(std::span<const u8> bytes, size_t offset)
{
    return Vec3{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}

// Actor +0x288 record `index`.
ActorMatrixRecord& matrix_row(RuntimeBoneDebris& context, u32 index)
{
    if (context.matrices.allocation == nullptr || i32(index) >= context.matrices.matrices.count)
    {
        throw Error("bone debris joint row lies outside actor +0x288");
    }
    return actor_matrix_records(context.matrices)[index];
}

ActorEffectNode& debris_effect(RuntimeBoneDebris& context)
{
    if (context.effect == nullptr)
    {
        throw Error("bone debris reads its +0x934 effect through address zero");
    }
    return *context.effect;
}

// Effect +0x390 + 0x150 * index: a debris particle.
std::span<u8> debris_particle(RuntimeBoneDebris& context, i32 index)
{
    auto& effect = debris_effect(context);
    const size_t offset = size_t(i32(i16(u16(u32(index))))) * 0x150 + 0x390;
    if (offset + 0x150 > effect.bytes.size())
    {
        throw Error("bone debris particle lies past its effect node");
    }
    return effect.bytes.subspan(offset, 0x150);
}

RuntimeActorMatrixSampling matrix_sampling(RuntimeBoneDebris& context)
{
    return RuntimeActorMatrixSampling{actor_matrix_records(context.matrices),
        context.matrices.matrices.node, context.sampling_scratch, context.sampling_active, context.origin,
        context.model_source};
}

// The particle's push away from `source`, `strength` long (x87 from the reciprocal square root on).
void push_debris_particle(std::span<u8> particle, const Vec3& position, const Vec3* source, float strength)
{
    if (source == nullptr)
    {
        return;
    }
    float dx = position.x - source->x;
    float dy = position.y - source->y;
    float dz = position.z - source->z;
    const float length = dz * dz + dy * dy + dx * dx;
    if (length != 0.f) // DAT_0032B100.
    {
        const long double scale = static_cast<long double>(engine_reciprocal_sqrt(length)) * strength;
        dx = float(dx * scale);
        dy = float(dy * scale);
        dz = float(dz * scale);
    }
    put_vector(particle, 0x18, Vec3{dx, dy, dz});
}

// The effect a piece may carry (name at +0x954, chance +0x9D4), at most eight.
void carry_debris_effect(const Vec3& position, u32 particle, RuntimeBoneDebris& context)
{
    const auto bytes = context.self.bytes;
    const u32 carried = get(bytes, field::carried_count);
    if (i32(carried) >= 8)
    {
        return;
    }
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    if (!(get(bytes, field::carry_chance) >= roll))
    {
        return;
    }
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    if (load_actor_effect_stream(field_data(bytes, field::carried_effect), &g_actor_creation_options,
        context.effects) != 0)
    {
        set_carried_effect(bytes, carried, i32(g_last_actor_effect));
        auto particles = get(bytes, field::carried_particles);
        particles.at(carried) = particle;
        set(bytes, field::carried_particles, particles);
        g_last_actor_effect = -1;
        set(bytes, field::carried_count, carried + 1);
    }
}

// 0x000EFE30 (ESI the effect, EDX its +0x134 script) for the next debris particle.
void allocate_debris_particle(RuntimeBoneDebris& context)
{
    auto& effect = debris_effect(context);
    const u32 script = effect.script.data() == nullptr ? 0 : context.resource_address(effect.script.data());
    allocate_actor_effect_particle(effect, context.effect_address(effect), script, context.random, context.pal,
        context.pal_override);
}
}

// Xbox 0x00067E40
void bone_debris_object_sizes(ObjectSizes& output)
{
    output.instance = 0xAAC;
    output.parameters = 0x55C;
}

// Xbox 0x00068970
void spawn_runtime_bone_debris_group(std::span<const u8> header, i32 group, i32& rows, i32& particles,
    const Vec3* source, float strength, RuntimeBoneDebris& context)
{
    Actor& actor = context.self.actor;
    const size_t entry = size_t(group) * 0xC;
    const u32 count = header[entry + 0x16];
    const auto joints = header.subspan(read_u16(header, entry + 0x14));
    auto sampling = matrix_sampling(context);
    if (header[entry + 0x1B] != 0 && count != 0)
    {
        // The group moves as one piece about the mean of its joints.
        Vec3 sum{0.f, 0.f, 0.f};
        for (u32 index = 0; index < count; ++index)
        {
            Vec3 position{};
            Vec3 euler{};
            sample_actor_model_matrix(actor, joints[index], position, euler, 0, sampling); // 0x00040640
            sum.x = position.x + sum.x;
            sum.y = position.y + sum.y;
            sum.z = position.z + sum.z;
        }
        const float share = 1.f / float(i32(count)); // DAT_0032B0F8.
        const Vec3 center{share * sum.x, sum.y * share, sum.z * share};
        for (u32 index = 0; index < count; ++index)
        {
            Vec3 position{};
            Vec3 euler{};
            sample_actor_model_matrix(actor, joints[index], position, euler, 0, sampling); // 0x00040640
            ActorMatrixRecord& row = matrix_row(context, joints[index]);
            row.center = context.origin; // DAT_0050DD80.
            row.center = Vec3{position.x - center.x, position.y - center.y, position.z - center.z};
            row.second_euler = euler;
            row.has_second_euler = 1;
            if (index == 0)
            {
                allocate_debris_particle(context);
                const auto particle = debris_particle(context, particles);
                put_vector(particle, 0, position);
                put_vector(particle, 0xC, euler);
                initialize_actor_effect_velocity(particle, debris_effect(context).bytes, context.random); // 0x000EE5C0
                push_debris_particle(particle, position, source, strength);
                carry_debris_effect(position, 0, context);
                ++particles;
            }
            ++rows;
        }
        return;
    }
    for (u32 index = 0; index < count; ++index)
    {
        Vec3 position{};
        Vec3 euler{};
        sample_actor_model_matrix(actor, joints[index], position, euler, 1, sampling); // 0x00040640
        ActorMatrixRecord& row = matrix_row(context, joints[index]);
        row.center = context.origin; // DAT_0050DD80.
        row.has_second_euler = 0;
        allocate_debris_particle(context);
        const auto particle = debris_particle(context, particles);
        put_vector(particle, 0, position);
        put_vector(particle, 0xC, euler);
        initialize_actor_effect_velocity(particle, debris_effect(context).bytes, context.random); // 0x000EE5C0
        push_debris_particle(particle, position, source, strength);
        carry_debris_effect(position, index, context);
        ++particles;
        ++rows;
    }
}

// Xbox 0x00068850
bool create_runtime_bone_debris_effect(std::span<const u8> parameters, RuntimeBoneDebris& context)
{
    Actor& actor = context.self.actor;
    initialize_actor_model_matrices(actor, context.matrices, context.model_types,
        context.matrix_bindings(actor)); // 0x000400D0
    // 0x000400D0 stores the rows at actor +0x288 and their count at +0x28C.
    context.actor_matrices = context.matrices.allocation;
    context.self.state.word_28c = u32(context.matrices.matrices.count);
    put_word(context.self.bytes, 0x28C, u32(context.matrices.matrices.count));
    const auto header = background_part_header(actor);
    const u32 groups = header.data() == nullptr ? 0 : header[0];
    const i32 count = context.matrices.matrices.count;
    if (i32(groups) <= 0 || count <= 0)
    {
        return false;
    }
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(actor); // 0x00041820
    auto& models = context.effects.models;
    context.effect = create_actor_effect_node(nullptr, u16(count), position, euler, models.registry,
        models.error); // 0x000ED1C0
    interpret_named_actor_effect(context.effect, field_data(parameters, parameter::effect), nullptr,
        context.effects); // 0x000ED990
    reset_actor_effect_to_debris(debris_effect(context), models); // 0x000ED6E0
    i32 rows = 0;
    i32 particles = 0;
    const Vec3* source = get(parameters, parameter::has_push_source) == 0 ? nullptr
        : field_data(parameters, parameter::push_source);
    for (i32 group = 0; group < i32(groups); ++group)
    {
        spawn_runtime_bone_debris_group(header, group, rows, particles, source,
            get(parameters, parameter::push_strength),
            context);
    }
    return true;
}

// Xbox 0x00067EB0
bool initialize_runtime_bone_debris(std::span<const u8> parameters, RuntimeBoneDebris& context)
{
    const auto bytes = context.self.bytes;
    auto source = context.actor_at(get(parameters, parameter::dead_actor));
    copy_runtime_actor_appearance(context.self, source, field_data(parameters, parameter::appearance),
        context.startup); // 0x0013AE30
    actor_refresh_transform(context.self.actor); // 0x00040DE0
    set(bytes, field::carry_chance, get(parameters, parameter::carry_chance));
    const char* name = field_data(parameters, parameter::carried_effect);
    char* copy = field_data(bytes, field::carried_effect);
    char character;
    do
    {
        character = *name++;
        *copy++ = character;
    } while (character != '\0');
    if (!create_runtime_bone_debris_effect(parameters, context))
    {
        return false;
    }
    const u32 record = get(parameters, parameter::shader_texture);
    if (record != 0 && read_u32(context.bytes_at(record), 8) == 0x5F646961) // "aid_"
    {
        const ActorModel* model = context.self.actor.model; // EBX, actor +0x204.
        const std::span<const u8> words = context.bytes_at(record);
        std::memcpy(field_data(bytes, field::shader_texture), words.data(), field::shader_texture.size); // 0x00140560
        if (model == nullptr || model->runtime == nullptr)
        {
            throw Error("bone debris 0x00067EB0 reads the shader texture's model through actor +0x204 with no model");
        }
        context.texture_parameter = create_shader_parameter(*model->runtime,
            reinterpret_cast<const char*>(context.bytes_at(read_u32(words, 0)).data()),
            reinterpret_cast<const char*>(context.bytes_at(read_u32(words, 4)).data())); // 0x000F8540
        set(bytes, field::texture_parameter, context.texture_parameter != nullptr ? 1u : 0u);
        context.texture = acquire_named_animation_resource(context.startup.registry,
            reinterpret_cast<const char*>(words.data() + 8), context.startup.error); // 0x00035B60
        const u32 texture = context.texture.data() == nullptr ? 0u : context.resource_address(context.texture.data());
        set(bytes, field::texture, texture);
        if (context.texture_parameter == nullptr)
        {
            throw Error("bone debris 0x00067EB0 binds its texture through a null +0x9D8 parameter (0x000F86F0)");
        }
        set_shader_parameter_texture(*context.texture_parameter, texture, 0, context.texture_bytes); // 0x000F86F0
    }
    set(bytes, field::stay_time, get(parameters, parameter::stay_time));
    set(bytes, field::fade_timer, get(parameters, parameter::fade_length));
    set(bytes, field::fade_length, get(parameters, parameter::fade_length));
    set(bytes, field::fade_alpha, 0xFF);
    set(bytes, field::resting, 0);
    set(bytes, field::dead_actor, get(parameters, parameter::dead_actor));
    dispatch_actor_attachment_records(context.self.state.resource.resource.data(), 0x18, nullptr, nullptr,
        context.callouts, 0); // 0x0003BE20 over actor +0x8C
    const char* sound = field_data(parameters, parameter::sound);
    if (sound[0] != 0)
    {
        queue_actor_sound(context.self.actor.sound_slots[1], 0, &context.self.actor, 1, nullptr, nullptr, 1.f, sound,
            context.sounds); // 0x0012A260, EBX actor +0x3EC
    }
    return true;
}

// Xbox 0x00068010
void destroy_runtime_bone_debris(RuntimeBoneDebris& context)
{
    if (context.effect != nullptr)
    {
        release_actor_effect_node(*context.effect, context.effect_release()); // 0x000ED2E0
        context.effect = nullptr;
    }
    if (get(context.self.bytes, field::texture_parameter) != 0)
    {
        release_shader_parameter(context.texture_parameter); // 0x000F85D0
        context.texture_parameter = nullptr;
    }
    if (get(context.self.bytes, field::texture) != 0)
    {
        release_animation_resource(context.startup.registry, context.texture); // 0x00035C00, EBX the texture
        context.texture = {};
    }
}

// Xbox 0x00041D90
void take_runtime_actor_sounds(RuntimeAvatarActorViews& source, RuntimeBoneDebris& context)
{
    for (i32 slot = 0; slot < 8; ++slot)
    {
        if (slot == 4 || slot == 7)
        {
            continue;
        }
        RuntimeActorSound& from = source.actor.sound_slots[size_t(slot)];
        if (from.previous == nullptr)
        {
            continue;
        }
        if (actor_sound_is_active(from, context.sounds) && from.enabled == 0) // 0x0012A970, +0x92.
        {
            continue;
        }
        RuntimeActorSound& to = context.self.actor.sound_slots[size_t(slot)];
        if (to.previous != nullptr)
        {
            release_actor_sound(to, context.sounds); // 0x0012A3D0
        }
        move_actor_sound(to, from, &context.self.actor); // 0x0012A440, EBX `to`
    }
}

// Xbox 0x00068700
void message_runtime_bone_debris(std::span<const u32> message, RuntimeBoneDebris& context)
{
    const auto bytes = context.self.bytes;
    if (message[0] == 0x23)
    {
        unregister_actor_segment_probes(context.render); // 0x0003E850
        clear_actor_shape_probe(context.self.actor, context.shape); // 0x0003C700
        auto* node = static_cast<entity_detail::ListHead*>(context.anchors.next);
        while (node != nullptr)
        {
            auto* next = static_cast<entity_detail::ListHead*>(node->next);
            release_actor_model_anchor(*reinterpret_cast<ActorModelAnchor*>(node)); // 0x0011BDA0
            node = next;
        }
        return;
    }
    if (message[0] == 0x7C && get(bytes, field::dead_actor) == message[1])
    {
        auto source = context.actor_at(message[1]);
        take_runtime_actor_sounds(source, context); // 0x00041D90, ECX the dead actor
        set(bytes, field::dead_actor, 0);
    }
}

// Xbox 0x0013AF80
bool spawn_runtime_actor_fade(float duration, u32 sound, u32 glow, const float* tint, RuntimeActorFade& context)
{
    if (!(duration > 0.f))
    {
        return false;
    }
    std::array<u8, 0x354> parameters{};
    build_runtime_emitter_burst_parameters(parameters, context.actor, context.address, duration,
        context.default_name); // 0x00069590, ESI actor
    parameters[0x33C] = context.bytes[0x19B];
    put_word(parameters, 0x350, sound);
    if (tint != nullptr)
    {
        std::memcpy(parameters.data() + 0x340, tint, 12);
    }
    put_word(parameters, 0x34C, glow);
    return context.keep(create_factory_object(context.factory, parameters)) != nullptr; // 0x00107520
}

// Xbox 0x00068060
void update_runtime_bone_debris(RuntimeBoneDebris& context)
{
    const auto bytes = context.self.bytes;
    Actor& actor = context.self.actor;
    const float step = runtime_actor_rate(actor) * runtime_frame_step(context.clock);
    update_actor_frame(context.frame, context.frame_context); // 0x000420D0
    if (get(bytes, field::texture_parameter) != 0)
    {
        advance_shader_parameter(*context.texture_parameter, step, context.texture_bytes); // 0x000F8640
    }
    if (get(bytes, field::resting) != 0)
    {
        // At rest: the pieces stay for +0x93C, then fade out as an after-image.
        const float stay = get(bytes, field::stay_time);
        if (stay > 0.f)
        {
            const float left = stay - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            set(bytes, field::stay_time, left);
            if (!(0.f < left))
            {
                set(bytes, field::stay_time, 0.f);
                // 0x0013AF80, EAX actor, with +0x9DC by address.
                RuntimeActorFade fade{actor, bytes, context.address, context.factory, context.keep, context.default_name};
                spawn_runtime_actor_fade(2.f, context.address + field::shader_texture.offset, 0, nullptr, fade); // EDI 0
                activate_actor_bind_tree(actor); // 0x00112CC0
            }
        }
        return;
    }
    if (get(bytes, field::fade_timer) > 0.f)
    {
        const float fade = get(bytes, field::fade_timer);
        const float rate = runtime_actor_rate(actor) * runtime_frame_step(context.clock);
        bool alpha = true;
        if (fade > 0.f)
        {
            const float left = fade - rate;
            set(bytes, field::fade_timer, left);
            if (!(0.f < left))
            {
                set(bytes, field::fade_timer, 0.f);
                activate_actor_bind_tree(actor); // 0x00112CC0
                alpha = false;
            }
        }
        if (alpha)
        {
            constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
            constexpr float half_pi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
            const float phase = float(static_cast<long double>(get(bytes, field::fade_timer))
                / get(bytes, field::fade_length)
                * pi - half_pi);
            const long double wave = (retail_sin(phase) + 1.0L) * 127.5L; // 0x00153320, DAT_0032B678.
            set(bytes, field::fade_alpha, u32(truncate_runtime_float(wave))); // 0x001673D8
        }
    }
    if (!(step > 0.f))
    {
        return;
    }
    auto& effect = debris_effect(context);
    context.tick_effect(effect, runtime_frame_step(context.clock)); // 0x000EF2D0
    const auto header = background_part_header(actor);
    const u32 groups = header.data() == nullptr ? 0 : header[0];
    set(bytes, field::resting, 1);
    u32 carried = 0;
    i32 particle = 0;
    for (u32 group = 0; group < groups; ++group)
    {
        const size_t entry = size_t(group) * 0xC;
        const auto joints = header.subspan(read_u16(header, entry + 0x14));
        const u32 count = header[entry + 0x16];
        const bool together = header[entry + 0x1B] != 0;
        Vec3 group_position{};
        Vec3 group_euler{};
        for (u32 index = 0; index < count; ++index)
        {
            const auto piece = debris_particle(context, particle);
            const u8 resting = piece[0x54];
            set(bytes, field::resting, get(bytes, field::resting) != 0 && resting != 0 ? 1 : 0);
            const Vec3 position = read_vector(piece, 0);
            const Vec3 euler = read_vector(piece, 0xC);
            ActorMatrixRecord& row = matrix_row(context, joints[index]);
            if (together)
            {
                if (index == 0)
                {
                    group_position = position;
                    group_euler = euler;
                }
                row.translation = group_position;
                context.matrices.progress = 0.f; // +0x29C.
                context.matrices.flags = 0; // +0x2A0.
                row.active = 1;
                row.euler = group_euler;
            }
            else
            {
                row.translation = position;
                context.matrices.flags = 0; // +0x2A0.
                context.matrices.progress = 0.f; // +0x29C.
                row.active = 1;
                row.euler = euler;
            }
            Vec3 wrapped = row.euler;
            wrap_runtime_euler_angles(wrapped); // 0x001059B0, ESI the row's euler
            row.euler = wrapped;
            row.active = 1;
            if (get(bytes, field::landed) == 0 && read_u32(piece, 0x5C) != 0 && piece[0x55] == 0
                && context.collision_normal.y > 0.75f) // DAT_00506928, DAT_0032B2AC.
            {
                set(bytes, field::landed, 1);
                dispatch_actor_attachment_records(context.self.state.resource.resource.data(), 0x16, nullptr,
                    nullptr, context.callouts, 0); // 0x0003BE20 over actor +0x8C
            }
            if (i32(get(bytes, field::carried_count)) > particle
                && i32(get(bytes, field::carried_particles).at(carried)) == particle)
            {
                const i32 slot = carried_effect(bytes, carried);
                if (slot != -1)
                {
                    ActorEffectNode* follower = g_actor_effect_nodes.at(size_t(slot));
                    if (follower == nullptr)
                    {
                        throw Error("bone debris carried effect slot is empty in DAT_00506930");
                    }
                    if (resting == 0)
                    {
                        put_vector(follower->bytes, 0x1C, position);
                    }
                    else
                    {
                        follower->bytes[0x18] |= 8;
                        put_float(follower->bytes, 8, -1.f); // DAT_0032B120.
                        set_carried_effect(bytes, carried, -1);
                    }
                }
                ++carried;
            }
            if (!together)
            {
                ++particle;
            }
        }
        if (together)
        {
            ++particle;
        }
    }
    if (get(bytes, field::resting) != 0)
    {
        refresh_actor_model_matrices(actor, context.matrices.matrices, 1, context.boundaries); // 0x00040200
        bytes[0x298] = 1;
        refresh_actor_model_matrices(actor, context.matrices.matrices, 0, context.boundaries); // 0x00040200
    }
}

// Xbox 0x00068610
u32 draw_runtime_bone_debris_fade(i32 kind, RuntimeBoneDebrisDraw& draw, RuntimeBoneDebris& context)
{
    if (kind != 2)
    {
        return 0;
    }
    if (draw.model == nullptr)
    {
        throw Error("bone debris fade draw has no actor +0x204 model");
    }
    auto& model = *draw.model;
    // The stack word holds the kind with the alpha in its top byte.
    model.material_enabled = 1;
    model.material_first = (u32(context.self.bytes[0x948]) << 24) | (u32(kind) & 0x00FFFFFFu);
    model.material_second = draw.material_second;
    AnimationMatrix world;
    std::memcpy(world.data(), context.self.actor.xform, sizeof(context.self.actor.xform)); // Actor +0x2AC.
    if ((draw.draw.image.byte(model.cpu, 8) & 0x10) != 0)
    {
        draw_runtime_scene_model(model.instance, model.cpu, world, 0, draw.draw); // 0x000F9080
    }
    draw_runtime_scene_model(model.instance, model.cpu, world, 1, draw.draw);
    if ((draw.draw.image.byte(model.cpu, 8) & 8) != 0)
    {
        draw_runtime_scene_model(model.instance, model.cpu, world, 2, draw.draw);
    }
    return 1;
}

// Xbox 0x000686A0
u32 draw_runtime_bone_debris(i32 kind, RuntimeBoneDebrisDraw& draw, RuntimeBoneDebris& context)
{
    Actor& actor = context.self.actor;
    if (context.self.bytes[0x298] == 0)
    {
        refresh_actor_model_matrices(actor, context.matrices.matrices, 0, context.boundaries); // 0x00040200
    }
    if (get(context.self.bytes, field::fade_alpha) != 0xFF)
    {
        return draw_runtime_bone_debris_fade(kind, draw, context); // Tail jump, EAX actor, ECX kind.
    }
    if (draw.model == nullptr)
    {
        throw Error("bone debris draw has no actor +0x204 model");
    }
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, sizeof(actor.xform)); // Actor +0x2AC.
    draw_runtime_scene_model(draw.model->instance, draw.model->cpu, world, u32(kind), draw.draw); // 0x000F9080
    return 1;
}
}
