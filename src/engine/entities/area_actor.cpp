#include "area_actor.h"
#include "actor_transform.h"
#include "player_mind_events.h"
#include "entity_heap.h"
#include "../navigation/spawn_candidates.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
void word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
float uniform(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
}

// Xbox 0x0006DBB0
void runtime_area_storage_sizes(ObjectSizes& sizes)
{
    sizes = {0x95C, 0x348};
}

// Xbox 0x0006DC10
bool initialize_runtime_area_actor(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> bytes{object.bytes.get(), object.size};
    actor_record_view<AreaActorState>(bytes).unk_934 = read_f32(parameters, 0x334);
    actor_record_view<AreaActorState>(bytes).unk_938 = read_f32(parameters, 0x338);
    actor_record_view<AreaActorState>(bytes).unk_93C = read_u32(parameters, 0x33C);
    actor_record_view<AreaActorState>(bytes).unk_940 = read_u32(parameters, 0x340);
    actor_record_view<AreaActorState>(bytes).unk_944 = read_u32(parameters, 0x344);
    actor_record_view<AreaActorState>(bytes).recent_cursor = 0;
    actor_record_view<AreaActorState>(bytes).recent_cells[0] = 0xFFFFFFFFu;
    actor_record_view<AreaActorState>(bytes).recent_cells[1] = 0xFFFFFFFFu;
    actor_record_view<AreaActorState>(bytes).recent_cells[2] = 0xFFFFFFFFu;
    actor_record_view<AreaActorState>(bytes).recent_cells[3] = 0xFFFFFFFFu;
    return true;
}

// Xbox 0x0005B830
bool runtime_area_dialogue_active(const RuntimeAreaDialoguePool* pool)
{
    if (pool != nullptr)
    {
        for (const u8* row = pool->begin; row < pool->end; row += 0x154)
        {
            const std::span<const u8> bytes{row, 0x154};
            if (read_u32(bytes, 0xB8) != 0 && read_u32(bytes, 0x144) != 0)
            {
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00106670
void runtime_random_annulus_position(Vec3& output, const Vec3& origin,
    float maximum, float minimum, u32& random)
{
    const float radius_random = uniform(random);
    const float angle_random = uniform(random);
    const float angle = float(static_cast<long double>(angle_random) *
        static_cast<long double>(std::bit_cast<float>(0x40C90FDBu))); // DAT_0032B298.
    const float radius = (maximum - minimum) * radius_random + minimum;
    output = {0.f, 0.f, 0.f};
    const float sine = float(retail_sin(angle));
    const float cosine = float(retail_cos(angle));
    output.x = sine * radius + output.x;
    output.z = cosine * radius + output.z;
    output.x = origin.x + output.x;
    output.y = origin.y + output.y;
    output.z = origin.z + output.z;
}

// Xbox 0x0006DF50
bool runtime_area_spawn_position(Vec3& output, const Vec3& origin, i32 floor,
    float minimum, float maximum, u32& random, float occupancy_initial)
{
    auto* grid = entity_detail::g_nav_grids[floor];
    auto* pool = entity_detail::collect_navigation_spawn_candidates(floor, nullptr, origin,
        minimum, maximum, nullptr, nullptr, 0, 0, occupancy_initial);
    if (pool != nullptr)
    {
        const i32 count = i32(pool->cursor - pool->begin) / pool->stride;
        const i32 selected = i32(uniform(random) * float(count));
        const u32 index = read_u32({pool->begin + selected * 0xC, 0xC}, 0);
        const auto& cell = grid->cells[index];
        output = {float(cell.x), float(cell.bottom), float(cell.z)};
        engine_heap_free(pool);
        const float half = entity_detail::g_nav_grids[0] == nullptr ? 0.f :
            float(entity_detail::g_nav_grids[0]->cell) * std::bit_cast<float>(0x3F000000u);
        const float spread = half * std::bit_cast<float>(0x3ECCCCCDu); // DAT_0032B11C.
        const float lower = 0.f - spread;
        output.x = ((spread - lower) * uniform(random) + output.x) + lower;
        output.z = ((spread - lower) * uniform(random) + output.z) + lower;
        const i32 nearest = entity_detail::navigation_cell_index(floor, output);
        if (nearest != -1)
        {
            output.y = float(grid->cells[nearest].bottom);
            return true;
        }
    }
    return false;
}

// Xbox 0x0006DCC0
i32 select_runtime_area_position(std::span<u8> area, const Vec3& origin,
    Vec3& output, u32& random, float occupancy_initial)
{
    const i32 floor = entity_detail::navigation_floor_index(origin, -1, 0);
    i32 result;
    i32 attempts = 0;
    do
    {
        result = runtime_area_spawn_position(output, origin, floor, actor_record_view<AreaActorState>(area).unk_934,
            actor_record_view<AreaActorState>(area).unk_938, random, occupancy_initial) ? 1 : 0;
        auto* grid = entity_detail::g_nav_grids[floor];
        if (result != 0 && grid != nullptr)
        {
            const i32 x = (i32(_mm_cvtt_ss2si(_mm_set_ss(output.x))) - entity_detail::g_nav_origin_x) / grid->cell;
            const i32 z = (i32(_mm_cvtt_ss2si(_mm_set_ss(output.z))) - entity_detail::g_nav_origin_z) / grid->cell;
            if (x >= 0 && x < grid->width && z >= 0 && z < grid->depth)
            {
                const i32 index = grid->width * z + x;
                if (index != -1)
                {
                    const i32 cursor = i32(actor_record_view<AreaActorState>(area).recent_cursor);
                    i32 row = i32(u32(cursor) + 1);
                    if (row != cursor)
                    {
                        do
                        {
                            if (i32(actor_record_view<AreaActorState>(area).recent_cells[size_t(row)]) == index)
                            {
                                break;
                            }
                            ++row;
                            if (row >= 4)
                            {
                                row = 0;
                            }
                        }
                        while (row != i32(actor_record_view<AreaActorState>(area).recent_cursor));
                    }
                    if (row == cursor)
                    {
                        actor_record_view<AreaActorState>(area).recent_cells[size_t(cursor)] = u32(index);
                        const i32 next = cursor + 1;
                        actor_record_view<AreaActorState>(area).recent_cursor = next >= 4 ? 0 : u32(next);
                        return result;
                    }
                }
            }
        }
        ++attempts;
    }
    while (attempts < 10);
    return result;
}

// Xbox 0x0006DDF0
i32 query_runtime_area_pose(RuntimeAreaActorContext& context, i32 select, Vec3& position, Vec3& euler)
{
    const Vec3 origin = actor_world_position(context.actor);
    i32 result;
    if (actor_record_view<AreaActorState>(context.bytes).unk_93C == 0 ||
        (context.prompt_lock <= -1 && !runtime_area_dialogue_active(context.dialogue)))
    {
        if (select == 0)
        {
            runtime_random_annulus_position(position, origin, actor_record_view<AreaActorState>(context.bytes).unk_938,
                actor_record_view<AreaActorState>(context.bytes).unk_934, context.random);
            result = 1;
        }
        else
        {
            result = select_runtime_area_position(context.bytes, origin, position, context.random, context.occupancy_initial);
            if (result == 0)
            {
                return 0;
            }
        }
    }
    else
    {
        position = origin;
        result = 1;
    }
    if (actor_record_view<AreaActorState>(context.bytes).unk_944 != 0)
    {
        position.y = origin.y;
    }
    euler = actor_world_euler(context.actor);
    const Vec3 direction{origin.x - position.x, origin.y - position.y, origin.z - position.z};
    float angle;
    if (runtime_direction_angle(direction, angle))
    {
        const u32 mode = actor_record_view<AreaActorState>(context.bytes).unk_940;
        if (mode == 1 || mode == 2)
        {
            if (mode == 2)
            {
                angle -= std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
            }
            euler = {0.f, angle, 0.f};
        }
    }
    return result;
}

// Xbox 0x0006DC80
void dispatch_runtime_area_message(RuntimeAreaActorContext& context, std::span<u32> message)
{
    if (message[0] == 0xA6)
    {
        Vec3 position, euler;
        std::memcpy(&position, message.data() + 8, 12);
        std::memcpy(&euler, message.data() + 11, 12);
        const i32 result = query_runtime_area_pose(context, message[7] == 0 ? 1 : 0, position, euler);
        std::memcpy(message.data() + 8, &position, 12);
        std::memcpy(message.data() + 11, &euler, 12);
        message[14] = u32(result);
    }
    context.base_message(context.base, message);
}

}
