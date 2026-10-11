// Host side of script_nearest_oracle.py. script_actor_query.cpp is linked alone; positions, the path
// walk and the predicate are logging stubs consuming the retail hooks' scripted queues.
#include "scripts/script_actor_query.h"
#include "entities/actor_transform.h"
#include "navigation/navigation.h"
#include "gbtg/anim.h"
#include <bit>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace gbtg;

namespace
{
std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;
std::map<const Actor*, std::pair<std::string, Vec3>> g_actors;
std::vector<u32> g_words;
size_t g_at = 0;

u32 next()
{
    return g_words.at(g_at++);
}

float next_float()
{
    return std::bit_cast<float>(next());
}

u32 pop(const char* name)
{
    auto& queue = g_queues[name];
    if (queue.empty())
    {
        return 0;
    }
    const u32 value = queue.front();
    queue.pop_front();
    return value;
}

void logf(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_log.emplace_back(buffer);
}

u32 bits(float value)
{
    return std::bit_cast<u32>(value);
}

const char* name_of(const Actor* actor)
{
    const auto found = g_actors.find(actor);
    return found != g_actors.end() ? found->second.first.c_str() : "?";
}

u32 predicate(Actor* requester, Actor* candidate)
{
    const u32 value = pop("predicate");
    logf("predicate %s %s", name_of(requester), name_of(candidate));
    return value;
}
}

// The real 0x00152B30 refinement, wrapped the way anim.cpp includes it.
namespace gbtg
{
namespace
{
#include "../../src/gbtg/animation/anim_math.inl"
}
}

namespace gbtg
{
Vec3 actor_world_position(const Actor& actor)
{
    logf("position %s", name_of(&actor));
    return g_actors.at(&actor).second;
}

long double retail_square_root(float value)
{
    return animation_sqrt(value);
}

// Not reached by 0x00128DF0 with flags 0 or 0x4000; a call here is a harness failure.
void engine_heap_free(void*)
{
    std::abort();
}

bool actor_spawn_owner_unavailable(const Actor&)
{
    std::abort();
}

long double animation_remainder(float, float)
{
    std::abort();
}

bool runtime_direction_angle(const Vec3&, float&)
{
    std::abort();
}

namespace entity_detail
{
bool navigation_path_clear(const Vec3& source, const Vec3& destination, i32 region_mode, i32 allow_tagged,
    i32 query_occupancy, i32 reject_marked, const OccupancyRecord* allowed, i32* last_cell, i16 group)
{
    const u32 value = pop("path");
    logf("path %08x %08x %08x %08x %08x %08x %x %x %x %x %x %x %d", bits(source.x), bits(source.y), bits(source.z),
        bits(destination.x), bits(destination.y), bits(destination.z), region_mode, allow_tagged, query_occupancy,
        reject_marked, allowed != nullptr ? 1 : 0, u16(group), last_cell != nullptr ? 1 : 0);
    return value != 0;
}
}
}

int main()
{
    int count;
    std::cin >> count;
    for (int test = 0; test < count; ++test)
    {
        g_queues.clear();
        g_log.clear();
        g_actors.clear();
        g_words.clear();
        g_at = 0;
        // Read a whole line of words.
        std::string line;
        while (line.empty())
        {
            std::getline(std::cin, line);
        }
        size_t position = 0;
        while (position < line.size())
        {
            size_t end = line.find(' ', position);
            if (end == std::string::npos)
            {
                end = line.size();
            }
            if (end > position)
            {
                g_words.push_back(u32(std::stoul(line.substr(position, end - position))));
            }
            position = end + 1;
        }
        for (const char* queue : {"path", "predicate"})
        {
            for (int index = 0; index < 8; ++index)
            {
                g_queues[queue].push_back(next());
            }
        }
        const size_t n = g_words.size() - g_at - 14;
        const size_t candidate_count = n / 4;
        std::vector<std::unique_ptr<Actor>> actors;
        std::vector<u16> tags(candidate_count);
        std::vector<scene_detail::CamFocus> hosts(candidate_count);
        std::vector<scene_detail::RangeLink> links(candidate_count);
        std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> views;
        for (size_t index = 0; index < candidate_count; ++index)
        {
            tags[index] = u16(next());
            const Vec3 at{next_float(), next_float(), next_float()};
            actors.push_back(std::make_unique<Actor>());
            actors.back()->class_key = tags[index];
            g_actors[actors.back().get()] = {"c" + std::to_string(index), at};
        }
        for (size_t index = 0; index < candidate_count; ++index)
        {
            links[index].host = &hosts[index];
            links[index].next = index + 1 < candidate_count ? &links[index + 1] : nullptr;
            views.emplace(&hosts[index], RuntimeScriptActorQueryView{*actors[index], tags[index], nullptr, nullptr,
                nullptr});
        }
        const i32 requester_index = i32(next());
        const u16 requester_class = u16(next());
        const Vec3 requester_at{next_float(), next_float(), next_float()};
        auto separate = std::make_unique<Actor>();
        separate->class_key = requester_class;
        Actor* requester = separate.get();
        if (requester_index >= 0)
        {
            requester = actors[size_t(requester_index)].get();
        }
        else
        {
            g_actors[separate.get()] = {"r", requester_at};
        }
        const u32 has_point = next();
        const Vec3 point{next_float(), next_float(), next_float()};
        const i16 tag = i16(next());
        const float radius = next_float();
        const u32 flags = next();
        const u32 has_predicate = next();
        const u32 has_distance = next();
        std::array<u8, 4> address_zero{};
        RuntimeScriptActorQueryContext context{candidate_count != 0 ? &links[0] : nullptr, views, address_zero.data()};
        float distance = std::bit_cast<float>(0xA5A5A5A5u);
        Actor* found = find_nearest_runtime_script_actor(has_point != 0 ? &point : nullptr, *requester, tag, radius,
            has_distance != 0 ? &distance : nullptr, flags, has_predicate != 0 ? predicate : nullptr, context);
        std::string out = found != nullptr ? name_of(found) : "-";
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), " %08x |", has_distance != 0 ? bits(distance) : 0xA5A5A5A5u);
        out += buffer;
        for (size_t index = 0; index < g_log.size(); ++index)
        {
            out += (index == 0 ? " " : " ; ") + g_log[index];
        }
        std::printf("%s\n", out.c_str());
    }
    return 0;
}
