#include "background_part_destruction.h"
#include "../entities/actor_transform.h"
#include "../entities/player_overlay.h"
#include "../graphics/effect_particles.h"
#include "../graphics/point_graph_solver.h"
#include "../graphics/vector_normalize.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr const char* kDebrisEmitter = "aid_fxemitter_ghoulies_background_bits_explode"; // 0x002FD388.

u32 next_random(u32& random)
{
    random = random * 0x19660D + 0x3C6EF35F;
    return random;
}

float random_unit(u32 value)
{
    return std::bit_cast<float>((value & 0x7FFFFF) | 0x3F800000);
}

u8 read_u8(std::span<const u8> bytes, size_t offset)
{
    if (offset >= bytes.size())
    {
        throw Error("background part read past its header");
    }
    return bytes[offset];
}

void store(std::span<u8> bytes, size_t offset, float value)
{
    if (offset + 4 > bytes.size())
    {
        throw Error("background debris particle lies past its emitter");
    }
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

std::span<const u8> background_part_header(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return {};
    }
    const auto cpu = actor.model->cpu;
    const u32 address = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
    if (address == 0)
    {
        return {};
    }
    return cpu.subspan(address);
}

// Xbox 0x00045170
void dispatch_background_part_destruction(Actor& actor, i32 part, const Vec3& source, u32 value,
    BackgroundPartDestruction& context)
{
    auto& rows = context.removal.volumes;
    auto& random = context.dispatch.effects.random_state;
    const auto live_rows = [&]()
    {
        return std::span<const BackgroundAnimationVolume>(rows.data(), size_t(context.row_count));
    };
    const auto volume_at = [&](i32 row) -> BackgroundAnimationVolume&
    {
        if (row < 0 || size_t(row) >= rows.size())
        {
            throw Error("background part row lies outside actor +0x94C");
        }
        return rows[size_t(row)];
    };
    const auto set_row_part = [&](i32 row)
    {
        const u32 stored = u32(part);
        std::memcpy(volume_at(row).fields.data(), &stored, 4);
    };
    Vec3 position{};
    i32 row = 0;
    std::span<const u8> header;
    if (context.graphs.data() == nullptr)
    {
        row = find_active_background_part(live_rows(), part);
        header = background_part_header(actor);
        position = Vec3{0.f, 0.f, 0.f};
        if (row == -1 && context.sample_count > 0)
        {
            row = context.row_count;
            context.row_count = row + 1;
            if (context.effect == nullptr)
            {
                const Vec3 origin = actor_world_position(actor);
                const Vec3 euler = actor_world_euler(actor);
                ActorEffectNode* effect = create_actor_effect_node(nullptr, u16(context.sample_count), origin,
                    euler, context.dispatch.effects.models.registry, context.dispatch.effects.models.error);
                context.effect = effect;
                interpret_named_actor_effect(effect, kDebrisEmitter, nullptr, context.dispatch.effects);
                if (context.effect == nullptr)
                {
                    throw Error("background debris emitter reset reads address zero");
                }
                reset_actor_effect_to_debris(*context.effect, context.dispatch.effects.models);
            }
            set_row_part(row);
            const size_t metadata = u32(part) * 12;
            const size_t samples = read_u16(header, metadata + 0x14);
            for (i32 index = 0; index < i32(read_u8(header, metadata + 0x16)); ++index)
            {
                ActorEffectNode& effect = *context.effect;
                const u32 script = effect.script.data() == nullptr ? 0 : context.address_of(effect.script.data());
                allocate_actor_effect_particle(effect, context.address_of(&effect), script, random, context.pal,
                    context.pal_override);
                Vec3 sample{};
                Vec3 euler{};
                sample_actor_model_matrix(actor, i32(read_u8(header, samples + size_t(index))), sample, euler, 0,
                    context.sampling);
                const u32 first = next_random(random);
                const u32 second = next_random(random);
                const u32 third = next_random(random);
                const u32 fourth = next_random(random);
                const u32 fifth = next_random(random);
                const u32 sixth = next_random(random);
                constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
                constexpr float kHalfTurn = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
                // DAT_0032B0F8 1, DAT_0032B260 3, DAT_0032B184 5, DAT_0032B120 -1, DAT_0032B1B8 40, DAT_0032B234 20.
                const float drift_x = -1.f / ((random_unit(third) - 1.f) * 3.f + 5.f) * sample.x;
                const float rise = (random_unit(second) - 1.f) * 40.f + 20.f;
                const float drift_z = -1.f / ((random_unit(first) - 1.f) * 3.f + 5.f) * sample.z;
                const float spin_x = (random_unit(sixth) - 1.f) * kTurn - kHalfTurn;
                const float spin_y = (random_unit(fifth) - 1.f) * kTurn - kHalfTurn;
                const float spin_z = (random_unit(fourth) - 1.f) * kTurn - kHalfTurn;
                const size_t particle = size_t(i32(i16(u16(u32(context.particle_count))))) * 0x150 + 0x390;
                const auto bytes = context.effect->bytes;
                store(bytes, particle, sample.x);
                store(bytes, particle + 4, sample.y);
                store(bytes, particle + 8, sample.z);
                store(bytes, particle + 0xC, euler.x);
                store(bytes, particle + 0x10, euler.y);
                store(bytes, particle + 0x14, euler.z);
                store(bytes, particle + 0x18, drift_x);
                store(bytes, particle + 0x1C, rise);
                store(bytes, particle + 0x20, drift_z);
                store(bytes, particle + 0x24, spin_x);
                store(bytes, particle + 0x28, spin_y);
                store(bytes, particle + 0x2C, spin_z);
                ++context.particle_count;
            }
            remove_background_part_volume(actor, part, value, position, context.removal);
        }
    }
    else
    {
        row = find_active_background_part(live_rows(), part);
        header = background_part_header(actor);
        float cleared = 0.f;
        position = Vec3{0.f, 0.f, 0.f};
        if (row == -1 && context.sample_count > 0)
        {
            row = context.row_count;
            context.row_count = row + 1;
            set_row_part(row);
            const size_t metadata = u32(part) * 12;
            const size_t graphs = read_u16(header, metadata + 0x14);
            set_row_part(row);
            const u32 names = read_u32(header, 4);
            const char* name = names == 0 ? "unknown" // DAT_00322F70.
                : reinterpret_cast<const char*>(header.data() + names + read_u8(header, metadata + 0x17));
            const i32 settings = find_background_part_settings(name, context.dispatch.settings,
                context.dispatch.setting_count);
            const char* effect_name = settings == -1 ? nullptr
                : reinterpret_cast<const char*>(context.dispatch.settings.data() + size_t(settings) * 0x22C + 0xA0);
            if (effect_name == nullptr || *effect_name == '\0')
            {
                context.settings_part = -1;
            }
            else
            {
                size_t length = 0;
                do
                {
                    if (length >= context.settings_name.size())
                    {
                        throw Error("background part settings name overflows actor +0x958");
                    }
                    context.settings_name[length] = effect_name[length];
                }
                while (effect_name[length++] != '\0');
                context.settings_part = part;
            }
            for (i32 index = 0; index < i32(read_u8(header, metadata + 0x16)); ++index)
            {
                const u8 graph = read_u8(header, graphs + size_t(index));
                Vec3 sample{};
                Vec3 euler{};
                sample_actor_model_matrix(actor, i32(graph), sample, euler, 0, context.sampling);
                if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
                {
                    throw Error("background part sphere lookup reads the model block through address zero");
                }
                const auto cpu = actor.model->cpu;
                const u32 records = read_u32(cpu, read_u32(cpu, 0x10) + 0x20);
                const u8* sphere = nullptr;
                if (records != 0 && read_u32(cpu, records) != 0)
                {
                    sphere = find_runtime_sphere_record(sample, cpu.subspan(records));
                }
                Vec3 impulse{};
                if (sphere == nullptr)
                {
                    const u32 first = next_random(random);
                    const u32 second = next_random(random);
                    const u32 third = next_random(random);
                    // DAT_0032B284 7, DAT_0032B260 3, DAT_0032B120 -1, DAT_0032B1B8 40, DAT_0032B234 20.
                    impulse.x = -1.f / ((random_unit(third) - 1.f) * 7.f + 3.f) * sample.x;
                    impulse.y = (random_unit(second) - 1.f) * 40.f + 20.f;
                    impulse.z = -1.f / ((random_unit(first) - 1.f) * 7.f + 3.f) * sample.z;
                }
                else
                {
                    float centre[3];
                    std::memcpy(centre, sphere, 12);
                    const u32 first = next_random(random);
                    const u32 second = next_random(random);
                    const u32 third = next_random(random);
                    // DAT_0032B234 20, DAT_0032B10C 10.
                    impulse.x = (((random_unit(first) - 1.f) * 20.f + centre[0]) - 10.f) - sample.x;
                    impulse.y = (((random_unit(second) - 1.f) * 20.f + centre[1]) - 10.f) - sample.y;
                    impulse.z = (((random_unit(third) - 1.f) * 20.f + centre[2]) - 10.f) - sample.z;
                    normalize_vector_in_place(&impulse.x);
                    // DAT_0032B218 6, DAT_0032B284 7.
                    const float scale = (random_unit(next_random(random)) - 1.f) * 6.f + 7.f;
                    impulse.x = impulse.x * scale;
                    impulse.y = impulse.y * scale;
                    impulse.z = impulse.z * scale;
                }
                normalize_runtime_graphics_vector(impulse, impulse);
                // DAT_0032B268 0.3, DAT_0032B0F4 0.5.
                const float scale = (random_unit(next_random(random)) - 1.f) * std::bit_cast<float>(0x3E99999Au) + .5f;
                impulse.x = scale * impulse.x;
                impulse.y = impulse.y * scale;
                impulse.z = impulse.z * scale;
                push_background_graph(impulse, graph, context.push);
                cleared = 0.f;
                ++context.particle_count;
                if (size_t(graph) >= context.graphs.size())
                {
                    throw Error("background part graph lies outside actor +0x950");
                }
                auto fields = context.graphs[graph].fields;
                std::memcpy(fields.data() + 0x1D0, &cleared, 4);
                fields[0x1D5] = 1;
            }
            // Retail releases the row indexed by the part, not the row just filled.
            auto& volume = volume_at(part);
            const u32 zero = 0;
            position = Vec3{cleared, cleared, cleared};
            if (volume.attachment != nullptr)
            {
                const auto& shape = volume.attachment->shape;
                position.z = read_f32(shape, 0xC);
                position.x = read_f32(shape, 4);
                position.y = read_f32(shape, 0x14) * .5f + read_f32(shape, 8);
                release_actor_live_attachment(*volume.attachment);
                volume.attachment = nullptr;
                std::memcpy(volume.fields.data() + 0x14, &zero, 4);
            }
            if (volume.anchor != nullptr)
            {
                release_actor_model_anchor(*volume.anchor);
                volume.anchor = nullptr;
                std::memcpy(volume.fields.data() + 0x18, &zero, 4);
            }
            remove_background_part_volume(actor, part, value, position, context.removal);
        }
    }
    advance_background_part_stage(actor, context.secondary, rows, row, part, header, position, source,
        context.shared_states, context.dispatch);
}
}
