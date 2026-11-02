#include "player_body_collision.h"
#include "entity_collision.h"
#include "entity.h"
#include "entity_heap.h"
#include "actor_transform.h"
#include "../navigation/navigation.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00074670
bool update_runtime_player_body_collision(RuntimePlayerBodyCollision& context)
{
    auto& body = context.body;
    auto& actor = context.actor;
    const u8 surface = get(body, body_field::surface);
    const bool was_grounded = (context.flags->bytes[5] & 2) != 0;
    const bool was_supported = (context.flags->bytes[2] & 0x20) != 0;
    i32 maximum_passes = 3;
    if (context.scene_rows.data() != nullptr)
    {
        for (size_t offset = 0; offset < context.scene_rows.size(); offset += 0x154)
        {
            if (read_u32(context.scene_rows, offset + 0xB8) != 0)
            {
                separate_runtime_player_world_contacts(body, actor, context.mask, context.stationary);
                break;
            }
        }
    }
    const Vec3 start = actor_world_position(actor);
    const float actor_radius = actor.radius_xz;
    Vec3 proposed = get(body, body_field::position);
    push_runtime_player_body_shapes(body, actor, proposed, context.shapes, context.push);
    const float radius = read_f32(body, 0x894) * actor_radius;
    const float height = (read_f32(body, 0x8A4) * actor_radius + radius) + 0.5f;
    Vec3 reference{start.x, start.y + height, start.z};
    body[0x874] = 0;
    context.flags->bytes[1] &= 0xDF;
    bool had_contact = false;
    bool handled = false;
    bool flatten = false;
    bool commit = false;
    Vec3 final = start;
    for (i32 pass = 0; pass < maximum_passes; ++pass)
    {
        auto& contact = context.contacts[pass];
        auto* record = reinterpret_cast<u8*>(&contact);
        Vec3 upper{proposed.x, proposed.y + height, proposed.z};
        const bool floor_hit = sweep_runtime_player_body_floor(body, actor, *context.flags,
            context.mask, context.floor, start, proposed, upper, radius, height);
        const u32 mask = camera_collision_mask(context.mask);
        const i32 hit = entity_record_movement_probe(upper, reference,
            std::bit_cast<u32>(radius), 4, mask);
        if (hit == 0)
        {
            record[0x26] = 0;
            handled = true;
            bool backwards = false;
            if (had_contact && pass >= 2)
            {
                constexpr float low = std::bit_cast<float>(0xBC23D70Au);
                constexpr float high = std::bit_cast<float>(0x3C23D70Au);
                for (i32 index = 1; index < pass; ++index)
                {
                    const auto& other = context.contacts[index];
                    const auto& first = context.contacts[0];
                    const float dx = first.x - other.x;
                    const float dy = first.y - other.y;
                    const float dz = first.z - other.z;
                    if (low > dx || dx > high || low > dy || dy > high || low > dz || dz > high)
                    {
                        Vec3 desired{get(body, body_field::position).x - start.x, 0.f,
                                     get(body, body_field::position).z - start.z};
                        Vec3 actual{upper.x - start.x, 0.f, upper.z - start.z};
                        float squared = desired.x * desired.x + desired.z * desired.z;
                        if (squared != 0.f)
                        {
                            const long double inverse = retail_reciprocal_sqrt(squared);
                            desired.x = float(static_cast<long double>(desired.x) * inverse);
                            desired.y = float(0.L * inverse);
                            desired.z = float(static_cast<long double>(desired.z) * inverse);
                        }
                        squared = actual.x * actual.x + actual.z * actual.z;
                        if (squared != 0.f)
                        {
                            const long double inverse = retail_reciprocal_sqrt(squared);
                            actual.x = float(static_cast<long double>(actual.x) * inverse);
                            actual.y = float(0.L * inverse);
                            actual.z = float(static_cast<long double>(actual.z) * inverse);
                        }
                        if (0.f > ((actual.x * desired.x + actual.z * desired.z) + desired.y * actual.y))
                        {
                            handled = false;
                            backwards = true;
                        }
                        break;
                    }
                }
            }
            if (backwards)
            {
                break;
            }
            if (had_contact)
            {
                context.flags->bytes[2] |= 0x20;
            }
            else
            {
                context.flags->bytes[2] &= 0xDF;
            }
            final = {upper.x, upper.y - height, upper.z};
            commit = true;
            break;
        }
        entity_copy_match_block(record, entity_detail::g_segment_slots[0].pool.begin);
        had_contact = true;
        if (pass == 0)
        {
            flatten = was_grounded && floor_hit;
        }
        bool slide = !notify_runtime_player_collision_action(proposed, start, reference,
            upper, record, context.action);
        if (pass == 2 && entity_match_blocks_equal(context.contacts[2].match, context.contacts[0].match) != 0)
        {
            contact.x = context.contacts[1].x + contact.x;
            contact.y = context.contacts[1].y + contact.y;
            contact.z = context.contacts[1].z + contact.z;
            const float squared = (contact.x * contact.x + contact.y * contact.y) + contact.z * contact.z;
            if (squared != 0.f)
            {
                const long double inverse = retail_reciprocal_sqrt(squared);
                contact.x = float(inverse * static_cast<long double>(contact.x));
                contact.y = float(inverse * static_cast<long double>(contact.y));
                contact.z = float(inverse * static_cast<long double>(contact.z));
            }
            maximum_passes = 5;
            slide = true;
            context.flags->bytes[1] |= 0x20;
        }
        entity_copy_match_block(body.data() + 0x1CC, record);
        if (slide)
        {
            if (flatten && animation_absolute(contact.y) > std::bit_cast<float>(0x3456BF95u))
            {
                contact.y = 0.f;
                const float squared = (contact.x * contact.x + 0.f) + contact.z * contact.z;
                if (squared != 0.f)
                {
                    const long double inverse = retail_reciprocal_sqrt(squared);
                    contact.x = float(inverse * static_cast<long double>(contact.x));
                    contact.y = float(inverse * static_cast<long double>(contact.y));
                    contact.z = float(inverse * static_cast<long double>(contact.z));
                }
            }
            const float x = (proposed.x - start.x) - (upper.x - reference.x);
            const float z = (proposed.z - start.z) - (upper.z - reference.z);
            const float y = (proposed.y - start.y) - (upper.y - reference.y);
            float push = 0.f - ((contact.x * x + z * contact.z) + y * contact.y);
            if (entity_detail::kSlideFloor > push)
            {
                push = entity_detail::kSlideFloor;
            }
            if (!(0.f > push))
            {
                proposed.x = contact.x * push + proposed.x;
                proposed.z = push * contact.z + proposed.z;
                proposed.y = push * contact.y + proposed.y;
            }
        }
        else
        {
            proposed = {upper.x, upper.y - height, upper.z};
        }
    }
    if (commit)
    {
        if (actor.bind == nullptr)
        {
            actor.position = final;
        }
        else if (actor.bind->parent == nullptr && (actor.link == nullptr || actor.link->head == nullptr))
        {
            engine_heap_free(actor.bind);
            actor.bind = nullptr;
            actor.position = final;
        }
        else
        {
            const float* matrix = actor.bind->m;
            actor.position.x = ((matrix[1] * final.y + matrix[2] * final.z) + matrix[0] * final.x) + matrix[3];
            actor.position.y = ((matrix[4] * final.x + matrix[5] * final.y) + matrix[6] * final.z) + matrix[7];
            actor.position.z = ((matrix[8] * final.x + matrix[9] * final.y) + matrix[10] * final.z) + matrix[11];
        }
        actor.xform_valid = 0;
        const Vec3 world = actor_world_position(actor);
        std::memcpy(body.data() + 0x18, &world, 12);
    }
    else
    {
        if (was_grounded)
        {
            context.flags->bytes[5] |= 2;
        }
        else
        {
            context.flags->bytes[5] &= 0xFD;
        }
        if (was_supported)
        {
            context.flags->bytes[2] |= 0x20;
        }
        else
        {
            context.flags->bytes[2] &= 0xDF;
        }
        context.flags->bytes[1] &= 0xDF;
        body[0x874] = surface;
    }
    if ((context.flags->bytes[5] & 2) == 0 && body[0x19E] != 0)
    {
        const float floor_y = read_f32(body, 0x17C);
        const Vec3 world = actor_world_position(actor);
        if (floor_y + 5.f >= world.y)
        {
            u8 kind = 0;
            const auto* grid = entity_detail::g_nav_grids[context.floor];
            if (grid != nullptr)
            {
                const i32 cell = entity_detail::navigation_cell_index(context.floor, world);
                if (cell != -1)
                {
                    const u8* record = reinterpret_cast<const u8*>(&grid->cells[cell]);
                    kind = record[0xB];
                    if (kind == 0)
                    {
                        kind = record[0xA];
                    }
                }
            }
            body[0x874] = kind;
        }
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(actor);
    const Vec3 delta{final.x - start.x, final.y - start.y, final.z - start.z};
    size_t sample = 0x344 + size_t(read_u32(body, 0x340)) * 12;
    std::memcpy(body.data() + sample, &delta, 12);
    if (rate * step > 0.f)
    {
        const float inverse = 1.f / (rate * step);
        sample = 0x344 + size_t(read_u32(body, 0x340)) * 12;
        const Vec3 velocity{inverse * read_f32(body, sample), inverse * read_f32(body, sample + 4),
            inverse * read_f32(body, sample + 8)};
        std::memcpy(body.data() + sample, &velocity, 12);
    }
    const i32 index = std::bit_cast<i32>(read_u32(body, 0x340)) + 1;
    const u32 next = u32(index % 0x50);
    std::memcpy(body.data() + 0x340, &next, 4);
    std::memcpy(body.data() + 0x704, &delta, 12);
    return handled;
}
}
