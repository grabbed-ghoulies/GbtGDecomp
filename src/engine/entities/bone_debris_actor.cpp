#include "bone_debris_actor.h"
#include "actor_transform.h"
#include "euler_wrapping.h"
#include "fx_anim_emitter.h"
#include "../audio/listener_settings.h"
#include "../graphics/effect_particles.h"
#include "../graphics/effect_placement.h"
#include "../scene/background_part_destruction.h"
#include "gbtg/anim.h"

#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
float engine_reciprocal_sqrt(float value); // Xbox 0x001534C0

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

void put_vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

Vec3 read_vector(std::span<const u8> bytes, size_t offset)
{
    return Vec3{read_f32(bytes, offset), read_f32(bytes, offset + 4), read_f32(bytes, offset + 8)};
}

// Actor +0x288 row `index` (stride 0x48).
std::span<u8> matrix_row(RuntimeBoneDebris& context, u32 index)
{
    if (context.matrices.allocation == nullptr || i32(index) >= context.matrices.matrices.count)
    {
        throw Error("bone debris joint row lies outside actor +0x288");
    }
    return {context.matrices.allocation + size_t(index) * 0x48, 0x48};
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
    return RuntimeActorMatrixSampling{{context.matrices.allocation, size_t(context.matrices.matrices.count) * 0x48},
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
    const u32 carried = read_u32(bytes, 0xAA8);
    if (i32(carried) >= 8)
    {
        return;
    }
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    if (!(read_f32(bytes, 0x9D4) >= roll))
    {
        return;
    }
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    if (load_actor_effect_stream(reinterpret_cast<const char*>(bytes.data() + 0x954), &g_actor_creation_options,
        context.effects) != 0)
    {
        put_word(bytes, 0xA68 + size_t(carried) * 4, u32(g_last_actor_effect));
        put_word(bytes, 0xA88 + size_t(carried) * 4, particle);
        g_last_actor_effect = -1;
        put_word(bytes, 0xAA8, carried + 1);
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
            const auto row = matrix_row(context, joints[index]);
            put_vector(row, 0x28, context.origin); // DAT_0050DD80.
            put_vector(row, 0x28, Vec3{position.x - center.x, position.y - center.y, position.z - center.z});
            put_vector(row, 0x38, euler);
            put_word(row, 0x34, 1);
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
        const auto row = matrix_row(context, joints[index]);
        put_vector(row, 0x28, context.origin); // DAT_0050DD80.
        put_word(row, 0x34, 0);
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
    interpret_named_actor_effect(context.effect, reinterpret_cast<const char*>(parameters.data() + 0x338), nullptr,
        context.effects); // 0x000ED990
    reset_actor_effect_to_debris(debris_effect(context), models); // 0x000ED6E0
    i32 rows = 0;
    i32 particles = 0;
    const Vec3* source = read_u32(parameters, 0x544) == 0 ? nullptr
        : reinterpret_cast<const Vec3*>(parameters.data() + 0x548);
    for (i32 group = 0; group < i32(groups); ++group)
    {
        spawn_runtime_bone_debris_group(header, group, rows, particles, source, read_f32(parameters, 0x554),
            context);
    }
    return true;
}

// Xbox 0x00067EB0
bool initialize_runtime_bone_debris(std::span<const u8> parameters, RuntimeBoneDebris& context)
{
    const auto bytes = context.self.bytes;
    auto source = context.actor_at(read_u32(parameters, 0x334));
    copy_runtime_actor_appearance(context.self, source, reinterpret_cast<const char*>(parameters.data() + 0x3B8),
        context.startup); // 0x0013AE30
    actor_refresh_transform(context.self.actor); // 0x00040DE0
    put_word(bytes, 0x9D4, read_u32(parameters, 0x4B8));
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x438);
    char* copy = reinterpret_cast<char*>(bytes.data() + 0x954);
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
    const u32 sound = read_u32(parameters, 0x558);
    if (sound != 0)
    {
        throw Error("untranslated Xbox 0x00140560 / 0x000F8540 reached from the bone debris 0x00067EB0 "
            "(parameter +0x558 set)");
    }
    put_word(bytes, 0x93C, read_u32(parameters, 0x4C0));
    put_word(bytes, 0x940, read_u32(parameters, 0x4BC));
    put_word(bytes, 0x944, read_u32(parameters, 0x4BC));
    put_word(bytes, 0x948, 0xFF);
    put_word(bytes, 0x950, 0);
    put_word(bytes, 0x938, read_u32(parameters, 0x334));
    dispatch_actor_attachment_records(context.self.state.resource.resource.data(), 0x18, nullptr, nullptr,
        context.callouts, 0); // 0x0003BE20 over actor +0x8C
    if (parameters[0x4C4] != 0)
    {
        throw Error("untranslated Xbox 0x0012A260 reached from the bone debris 0x00067EB0 (parameter +0x4C4 set)");
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
    if (read_u32(context.self.bytes, 0x9D8) != 0)
    {
        throw Error("untranslated Xbox 0x000F85D0 reached from the bone debris 0x00068010 (+0x9D8 set)");
    }
    if (read_u32(context.self.bytes, 0xA64) != 0)
    {
        throw Error("untranslated Xbox 0x00035C00 reached from the bone debris 0x00068010 (+0xA64 set)");
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
    if (message[0] == 0x7C && read_u32(bytes, 0x938) == message[1])
    {
        auto source = context.actor_at(message[1]);
        take_runtime_actor_sounds(source, context); // 0x00041D90, ECX the dead actor
        put_word(bytes, 0x938, 0);
    }
}

// Xbox 0x0013AF80
bool spawn_runtime_actor_fade(float duration, u32 sound, u32 glow, RuntimeBoneDebris& context)
{
    if (!(duration > 0.f))
    {
        return false;
    }
    std::array<u8, 0x354> parameters{};
    build_runtime_emitter_burst_parameters(parameters, context.self.actor, context.address, duration,
        context.default_name); // 0x00069590, ESI actor
    parameters[0x33C] = context.self.bytes[0x19B];
    put_word(parameters, 0x350, sound);
    // EDI is zero at the bone debris' call: no colour at parameters +0x340.
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
    if (read_u32(bytes, 0x9D8) != 0)
    {
        throw Error("untranslated Xbox 0x000F8640 reached from the bone debris 0x00068060 (+0x9D8 set)");
    }
    if (read_u32(bytes, 0x950) != 0)
    {
        // At rest: the pieces stay for +0x93C, then fade out as an after-image.
        const float stay = read_f32(bytes, 0x93C);
        if (stay > 0.f)
        {
            const float left = stay - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
            put_float(bytes, 0x93C, left);
            if (!(0.f < left))
            {
                put_float(bytes, 0x93C, 0.f);
                spawn_runtime_actor_fade(2.f, context.address + 0x9DC, 0, context); // 0x0013AF80, EAX actor
                activate_actor_bind_tree(actor); // 0x00112CC0
            }
        }
        return;
    }
    if (read_f32(bytes, 0x940) > 0.f)
    {
        const float fade = read_f32(bytes, 0x940);
        const float rate = runtime_actor_rate(actor) * runtime_frame_step(context.clock);
        bool alpha = true;
        if (fade > 0.f)
        {
            const float left = fade - rate;
            put_float(bytes, 0x940, left);
            if (!(0.f < left))
            {
                put_float(bytes, 0x940, 0.f);
                activate_actor_bind_tree(actor); // 0x00112CC0
                alpha = false;
            }
        }
        if (alpha)
        {
            constexpr float pi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
            constexpr float half_pi = std::bit_cast<float>(0x3FC90FDBu); // DAT_0032B140.
            const float phase = float(static_cast<long double>(read_f32(bytes, 0x940)) / read_f32(bytes, 0x944)
                * pi - half_pi);
            const long double wave = (retail_sin(phase) + 1.0L) * 127.5L; // 0x00153320, DAT_0032B678.
            put_word(bytes, 0x948, u32(truncate_runtime_float(wave))); // 0x001673D8
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
    put_word(bytes, 0x950, 1);
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
            put_word(bytes, 0x950, read_u32(bytes, 0x950) != 0 && resting != 0 ? 1 : 0);
            const Vec3 position = read_vector(piece, 0);
            const Vec3 euler = read_vector(piece, 0xC);
            const auto row = matrix_row(context, joints[index]);
            if (together)
            {
                if (index == 0)
                {
                    group_position = position;
                    group_euler = euler;
                }
                put_vector(row, 4, group_position);
                context.matrices.progress = 0.f; // +0x29C.
                context.matrices.flags = 0; // +0x2A0.
                put_word(row, 0x44, 1);
                put_vector(row, 0x10, group_euler);
            }
            else
            {
                put_vector(row, 4, position);
                context.matrices.flags = 0; // +0x2A0.
                context.matrices.progress = 0.f; // +0x29C.
                put_word(row, 0x44, 1);
                put_vector(row, 0x10, euler);
            }
            Vec3 wrapped = read_vector(row, 0x10);
            wrap_runtime_euler_angles(wrapped); // 0x001059B0, ESI row +0x10
            put_vector(row, 0x10, wrapped);
            put_word(row, 0x44, 1);
            if (read_u32(bytes, 0x94C) == 0 && read_u32(piece, 0x5C) != 0 && piece[0x55] == 0
                && context.collision_normal.y > 0.75f) // DAT_00506928, DAT_0032B2AC.
            {
                put_word(bytes, 0x94C, 1);
                dispatch_actor_attachment_records(context.self.state.resource.resource.data(), 0x16, nullptr,
                    nullptr, context.callouts, 0); // 0x0003BE20 over actor +0x8C
            }
            if (i32(read_u32(bytes, 0xAA8)) > particle && i32(read_u32(bytes, 0xA88 + size_t(carried) * 4)) == particle)
            {
                const i32 slot = i32(read_u32(bytes, 0xA68 + size_t(carried) * 4));
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
                        put_word(bytes, 0xA68 + size_t(carried) * 4, 0xFFFFFFFFu);
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
    if (read_u32(bytes, 0x950) != 0)
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
    if (read_u32(context.self.bytes, 0x948) != 0xFF)
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
