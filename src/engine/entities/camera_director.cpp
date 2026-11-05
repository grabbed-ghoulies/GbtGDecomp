#include "camera_director.h"
#include <cstddef>
#include "strategy_fields.h"
#include "camera_actor_classes.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "shape_copy.h"
#include "../camera/display_view.h"
#include "../navigation/navigation.h"
#include "../runtime/crt_sort.h"
#include "../scripts/script_conditions.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <climits>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr size_t kEntrySize = 0x2C; // Key +0, priority +4, distance +8, camera target record +0xC.
constexpr i32 kEntryLimit = 0x14;

void put(u8* bytes, size_t offset, u32 value)
{
    std::memcpy(bytes + offset, &value, 4);
}

void put_float(u8* bytes, size_t offset, float value)
{
    std::memcpy(bytes + offset, &value, 4);
}

u32 word(const u8* bytes, size_t offset)
{
    u32 value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

float real(const u8* bytes, size_t offset)
{
    float value;
    std::memcpy(&value, bytes + offset, 4);
    return value;
}

// CVTTSS2SI: out-of-range and unordered inputs give the integer indefinite value.
i32 truncate_float(float value)
{
    if (!(value >= -2147483648.f && value < 2147483648.f))
    {
        return INT_MIN;
    }
    return i32(value);
}

// The ground height under (x, z) on the actor's floor grid DAT_0050E240[+0xCEC]; otherwise the fallback.
float director_ground_height(const RuntimeDirectorActor& actor, float x, float z, float fallback)
{
    const u32 floor = player_record(actor.bytes).floor;
    if (floor >= u32(entity_detail::kNavSlots))
    {
        throw Error("camera director floor index is outside DAT_0050E240");
    }
    const entity_detail::NavGrid* grid = entity_detail::g_nav_grids[floor];
    if (grid != nullptr)
    {
        const i32 column = (truncate_float(x) - entity_detail::g_nav_origin_x) / grid->cell;
        const i32 row = (truncate_float(z) - entity_detail::g_nav_origin_z) / grid->cell;
        if (column >= 0 && column < grid->width && row >= 0 && row < grid->depth)
        {
            const i32 index = grid->width * row + column;
            if (index != -1)
            {
                return float(grid->cells.at(size_t(index)).bottom);
            }
        }
    }
    return fallback;
}

float director_planar_distance(const Vec3& origin, const Vec3& position)
{
    const float dz = origin.z - position.z;
    const float dx = origin.x - position.x;
    return float(retail_square_root(dz * dz + dx * dx));
}
}

// Xbox 0x00028CC0
bool runtime_actor_strategy_waiting(const RuntimeDirectorActor& actor)
{
    if (!actor.strategy.empty() && strategy_base(actor.strategy).attacking != 0)
    {
        if ((body_record(actor.body).move_holds == 0 && body_record(actor.body).move_hold_time == 0.f)
            || body_record(actor.body).unk_6A != 0)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x000AA480
u32 camera_target_watched_elsewhere(u32 self, u32 target, const RuntimeDirectorTargets& context)
{
    if ((context.runtime_flags & 1) != 0)
    {
        return 1;
    }
    for (const RuntimeDirectorActor& candidate : context.marks())
    {
        if (candidate.address != self && candidate.address != target && !candidate.mind.empty()
            && get(candidate.mind, mind_field::target) == target)
        {
            return 1;
        }
    }
    return 0;
}

// Xbox 0x000AA4D0
u8* reserve_camera_director_entry(u8* entries, i32& count, u32 actor, i32 priority, i32 limit)
{
    u8* end = entries + size_t(count) * kEntrySize;
    u8* entry = entries;
    while (entry < end)
    {
        const u32 key = word(entry, 0);
        if (key != 0 && key == actor)
        {
            if (i32(word(entry, 4)) > priority)
            {
                return nullptr;
            }
            put(entry, 4, u32(priority));
            put(entry, 0, actor);
            return entry;
        }
        entry += kEntrySize;
    }
    if (count >= limit)
    {
        return nullptr;
    }
    ++count;
    put(end, 4, u32(priority));
    put(end, 0, actor);
    return end;
}

// Xbox 0x000AA520
u32 add_camera_director_entry(u8* entries, i32& count, u32 actor, i32 priority, i32 limit, float distance,
    const RuntimeDirectorTargets& context)
{
    u8* entry = reserve_camera_director_entry(entries, count, actor, priority, limit);
    if (entry == nullptr)
    {
        return 0;
    }
    put_float(entry, 8, distance);
    context.camera_target(std::span<u8, 0x20>(entry + 0xC, 0x20), actor);
    return 1;
}

// Xbox 0x000AA560
int compare_camera_director_entries(const void* left, const void* right)
{
    const u8* first = static_cast<const u8*>(left);
    const u8* second = static_cast<const u8*>(right);
    const i32 first_priority = i32(word(first, 4));
    const i32 second_priority = i32(word(second, 4));
    if (first_priority == second_priority)
    {
        if (real(second, 8) > real(first, 8))
        {
            return -1;
        }
        return 1;
    }
    return second_priority - first_priority;
}

// Xbox 0x000AA590
i32 collect_camera_director_targets(u32 camera, u32 target, std::span<u8> output, i32 limit,
    const RuntimeDirectorTargets& context)
{
    const RuntimeDirectorTunables& tunables = context.tunables;
    const float extended_range = tunables.near_range + tunables.range_extension;
    std::array<u8, kEntryLimit * kEntrySize> chosen{}; // Stack +0x64.
    std::array<u8, kEntryLimit * kEntrySize> grouped{}; // Stack +0x3D4.
    i32 chosen_count = 0;
    i32 grouped_count = 0;
    const RuntimeDirectorActor focus = context.actor_at(target);
    const Vec3 origin = actor_world_position(*focus.actor);
    const u32 focus_target = get(focus.mind, mind_field::target);
    if (tunables.skip_candidates == 0)
    {
        const std::vector<RuntimeDirectorActor> marks = context.marks();
        if (!marks.empty())
        {
            for (const RuntimeDirectorActor& candidate : marks)
            {
                if (candidate.address == camera)
                {
                    continue;
                }
                const Vec3 position = actor_world_position(*candidate.actor);
                const float distance = director_planar_distance(origin, position);
                if (candidate.mind.empty())
                {
                    continue;
                }
                const u32 watched = get(candidate.mind, mind_field::target);
                i32 last_cell = 0;
                const bool clear = navigation_path_clear(origin, position, tunables.region_mode, 0, 0, 0, nullptr,
                    &last_cell, 0);
                const bool idle = body_record(candidate.body).energy == 0;
                if (candidate.address == target)
                {
                    continue;
                }
                if (candidate.address == focus_target && tunables.focus_targets != 0)
                {
                    // Inlined in retail with the same find-or-append rule as 0x000AA4D0.
                    add_camera_director_entry(chosen.data(), chosen_count, candidate.address, 5, kEntryLimit,
                        distance, context);
                }
                else if (tunables.near_range > distance && clear
                    && player_record(candidate.bytes).goal_volume == 0 && !idle)
                {
                    add_camera_director_entry(grouped.data(), grouped_count, candidate.address, 2, kEntryLimit,
                        distance, context);
                }
                else if (runtime_actor_strategy_waiting(candidate) && watched == target && extended_range > distance)
                {
                    add_camera_director_entry(chosen.data(), chosen_count, candidate.address, 4, kEntryLimit,
                        distance, context);
                }
            }
            if (tunables.skip_candidates == 0 && chosen_count == 0 && grouped_count == 0)
            {
                for (const RuntimeDirectorActor& candidate : context.marks())
                {
                    if (candidate.address == camera || candidate.address == target)
                    {
                        continue;
                    }
                    const Vec3 position = actor_world_position(*candidate.actor);
                    const float distance = director_planar_distance(origin, position);
                    if (candidate.mind.empty())
                    {
                        continue;
                    }
                    const u32 watched = get(candidate.mind, mind_field::target);
                    i32 last_cell = 0;
                    navigation_path_clear(origin, position, 0, 0, 0, 0, nullptr, &last_cell, 0);
                    if (!candidate.strategy.empty() && strategy_base(candidate.strategy).unk_1B != 0
                        && watched == target && extended_range > distance)
                    {
                        add_camera_director_entry(chosen.data(), chosen_count, candidate.address, 3, kEntryLimit,
                            distance, context);
                    }
                    else if ((candidate.body_bits->bytes[4] & 0x10) != 0
                        && body_record(candidate.body).attacker == target
                        && extended_range > distance && tunables.carried_targets != 0)
                    {
                        add_camera_director_entry(chosen.data(), chosen_count, candidate.address, 1, kEntryLimit,
                            distance, context);
                    }
                }
            }
        }
    }
    const i32 total = grouped_count + chosen_count;
    if (grouped_count != 0)
    {
        float sum_x = 0.f;
        float sum_y = 0.f;
        float sum_z = 0.f;
        float sum_20 = 0.f;
        float sum_28 = 0.f;
        for (i32 index = 0; index < grouped_count; ++index)
        {
            const u8* entry = grouped.data() + size_t(index) * kEntrySize;
            const RuntimeDirectorActor member = context.actor_at(word(entry, 0));
            if ((total <= 1 && tunables.ground_heights == 0) || (member.body_bits->bytes[4] & 0x10) == 0)
            {
                sum_x = real(entry, 0x10) + sum_x;
                sum_y = real(entry, 0x14) + sum_y;
            }
            else
            {
                const float height = director_ground_height(member, real(entry, 0x10), real(entry, 0x18), origin.y);
                sum_x = real(entry, 0x10) + sum_x;
                sum_y = sum_y + height;
            }
            sum_z = real(entry, 0x18) + sum_z;
            sum_20 = real(entry, 0x20) + sum_20;
            sum_28 = real(entry, 0x28) + sum_28;
        }
        const float scale = 1.f / float(grouped_count); // DAT_0032B0F8.
        const float average_20 = scale * sum_20;
        const float average_28 = scale * sum_28;
        const Vec3 center{scale * sum_x, scale * sum_y, scale * sum_z};
        if (chosen_count >= kEntryLimit)
        {
            throw Error("camera director writes its grouped entry through address zero");
        }
        u8* entry = chosen.data() + size_t(chosen_count) * kEntrySize;
        ++chosen_count;
        put(entry, 0, 0);
        put(entry, 4, 2);
        const double dx = double(center.x) - double(origin.x);
        const double dy = double(center.y) - double(origin.y);
        const double dz = double(center.z) - double(origin.z);
        put_float(entry, 8, float(std::sqrt(dx * dx + dz * dz + dy * dy)));
        entry[0xC] = 4;
        entry[0xD] = 0x20;
        entry[0xE] = 1;
        put_float(entry, 0x10, center.x);
        put_float(entry, 0x14, center.y);
        put_float(entry, 0x18, center.z);
        put_float(entry, 0x1C, 0.f);
        put_float(entry, 0x20, average_20);
        put_float(entry, 0x24, 0.f);
        put_float(entry, 0x28, average_28);
    }
    if (total == 0 && tunables.lead_distance > 0.f)
    {
        std::array<u8, 0x20> lead{};
        context.camera_target(lead, target);
        const Vec3 euler = actor_world_euler(*focus.actor);
        const float pitch_sin = float(retail_sin(euler.x));
        const float pitch_cos = float(retail_cos(euler.x));
        const float forward_x = float(double(retail_sin(euler.y)) * pitch_cos);
        const float rise = 0.f - pitch_sin;
        const double forward_z = double(retail_cos(euler.y)) * pitch_cos;
        const float distance = tunables.lead_distance;
        put_float(lead.data(), 4, float(double(forward_x) * distance + real(lead.data(), 4)));
        put_float(lead.data(), 8, rise * distance + real(lead.data(), 8));
        put_float(lead.data(), 0xC, float(forward_z * distance + real(lead.data(), 0xC)));
        if (chosen_count < kEntryLimit)
        {
            u8* entry = chosen.data() + size_t(chosen_count) * kEntrySize;
            put(entry, 0, 0);
            put(entry, 4, 5);
            put_float(entry, 8, distance);
            ++chosen_count;
            copy_actor_shape(entry + 0xC, lead.data());
        }
    }
    u8* own = reserve_camera_director_entry(chosen.data(), chosen_count, target, 6, kEntryLimit);
    if (own != nullptr)
    {
        put_float(own, 8, 0.f);
        context.camera_target(std::span<u8, 0x20>(own + 0xC, 0x20), target);
    }
    runtime_quick_sort(chosen.data(), size_t(chosen_count), kEntrySize, compare_camera_director_entries);
    i32 index = 0;
    if (limit > 0)
    {
        if (output.size() < size_t(limit) * 0x20)
        {
            throw Error("camera director output is smaller than its limit");
        }
        do
        {
            if (index >= chosen_count)
            {
                return index;
            }
            const u8* entry = chosen.data() + size_t(index) * kEntrySize;
            if (i32(word(entry, 4)) < tunables.minimum_priority && index >= tunables.minimum_count)
            {
                return index;
            }
            u8* record = output.data() + size_t(index) * 0x20;
            copy_actor_shape(record, entry + 0xC);
            const u32 key = word(entry, 0);
            if ((total > 2 || tunables.ground_heights != 0) && key != 0)
            {
                const RuntimeDirectorActor member = context.actor_at(key);
                if ((member.body_bits->bytes[4] & 0x10) != 0)
                {
                    put_float(record, 8, director_ground_height(member, real(entry, 0x10), real(entry, 0x18),
                        origin.y));
                }
            }
            ++index;
        } while (index < limit);
    }
    return index;
}

// Xbox 0x0013DD80
float camera_director_input_heading(const RuntimeDirectorActor& actor, const RuntimeDirectorTargets& context)
{
    float heading = actor.actor->euler.y; // Actor +0x18.
    if (!actor.mind.empty() && get(actor.mind, mind_field::target_mode) != 0 && (actor.body_bits->bytes[6] & 0x80) != 0)
    {
        const u32 watched = get(actor.mind, mind_field::target);
        if (watched != 0)
        {
            const Vec3 self = actor_world_position(*actor.actor);
            const Vec3 other = actor_world_position(*context.actor_at(watched).actor);
            const Vec3 direction{other.x - self.x, other.y - self.y, other.z - self.z};
            float angle = 0.f;
            if (runtime_direction_angle(direction, angle))
            {
                heading = angle;
            }
        }
    }
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const long double remainder = animation_remainder(heading - kPi, kTurn);
    float result = float(remainder);
    if (std::bit_cast<float>(0xC0490FDBu) > remainder) // DAT_0032B29C.
    {
        result = result + kTurn;
    }
    if (result >= kPi)
    {
        result = result - kTurn;
    }
    return result;
}

// Xbox 0x000AADE0
u32 read_camera_director_input_heading(const RuntimeDirectorActor& actor, float& heading,
    const RuntimeDirectorTargets& context)
{
    if (context.tunables.input_heading != 0)
    {
        const u32 device_address = player_record(actor.bytes).input_device;
        if (device_address != 0)
        {
            const RuntimeDialogueInputDevice* device = context.input_device(device_address);
            if (read_u32(device->record, 4) != 0)
            {
                constexpr float kHalf = std::bit_cast<float>(0x3F000000u); // DAT_0032B0F4.
                if (runtime_input_first_analog_level(*device, context.input) > kHalf
                    && runtime_input_second_analog_level(*device, context.input) > kHalf)
                {
                    heading = camera_director_input_heading(actor, context);
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Xbox 0x0013D1E0
bool camera_director_scene_start_pending()
{
    const EntityCamNode* camera = g_cam_active[0];
    const char* name = camera == nullptr ? "" : camera->name;
    if (std::strcmp(name, "cameron") != 0)
    {
        return false;
    }
    for (const LiveScript* node = g_live_scripts; node != nullptr; node = node->next)
    {
        if (node->type == 0x7D)
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x00020000
void release_runtime_body_hold(std::span<u8> body, const std::function<void(std::span<const u32>)>& message)
{
    const u32 holds = body_record(body).holds - 1;
    body_record(body).holds = holds;
    if (holds == 0)
    {
        const u32 release[1]{0x4A};
        message(release);
    }
}

// Xbox 0x000AA250
void update_runtime_director_strategy(RuntimeDirectorUpdate& context)
{
    std::span<u8> strategy{context.strategy.bytes.get(), context.strategy.size};
    DirectorStrategyState& director = strategy_view<DirectorStrategyState>(strategy);
    const u16 state = director.base.state;
    const u32 target = get(context.mind, mind_field::target);
    RuntimeDirectorTargets& targets = context.targets;
    if (target == 0 && director.unk_38 == 0)
    {
        activate_actor_bind_tree(context.actor);
        return;
    }
    update_runtime_base_strategy(context.goals);
    if (state == 0x401)
    {
        if (targets.tunables.keep_state == 0
            && camera_target_watched_elsewhere(context.actor_address, target, targets) != 0)
        {
            change_runtime_player_strategy_state(context.strategy, 0x40C, context.setup);
        }
    }
    else if (state == 0x40C)
    {
        if (targets.tunables.keep_state == 0
            && camera_target_watched_elsewhere(context.actor_address, target, targets) == 0)
        {
            change_runtime_player_strategy_state(context.strategy, 0x401, context.setup);
            director.first_update = 0;
            return;
        }
        RuntimePlayerMindRequest* request = allocate_runtime_mind_request(context.requests, 1,
            context.request_sizes);
        if (request == nullptr)
        {
            throw Error("camera director request 1 allocation returned address zero");
        }
        std::span<u8> bytes = request->bytes;
        const i32 count = collect_camera_director_targets(context.actor_address, target, bytes.subspan(0x14, 0x280),
            0x14, targets);
        std::memcpy(bytes.data() + 0x294, &count, 4);
        float heading = read_f32(bytes, 0x29C);
        const u32 steered = read_camera_director_input_heading(targets.actor_at(target), heading, targets);
        std::memcpy(bytes.data() + 0x29C, &heading, 4);
        std::memcpy(bytes.data() + 0x298, &steered, 4);
        const u32 starting = camera_director_scene_start_pending() && director.first_update == 0 ? 1 : 0;
        std::memcpy(bytes.data() + 0x2A0, &starting, 4);
        if (director.starting != starting)
        {
            if (starting != 0)
            {
                release_runtime_body_hold(context.body, context.body_message);
            }
            else
            {
                const u32 holds = body_record(context.body).holds + 1;
                body_record(context.body).holds = holds;
            }
            director.starting = read_u32(bytes, 0x2A0);
            director.first_update = 0;
            return;
        }
    }
    director.first_update = 0;
}
}
