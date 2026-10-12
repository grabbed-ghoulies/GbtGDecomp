// Host side of imp_strategy_oracle.py. The strategy target queue, target selection, imp fire
// strategy, angle and model lookup modules are linked with logging stubs for every other callee;
// the stubs consume the same scripted queues as the retail hooks.
#include "entities/imp_fire_strategy.h"
#include "entities/body_animation_setup.h"
#include "navigation/mind_navigation_angles.h"
#include "navigation/player_avoidance.h"
#include "gbtg/game.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <xmmintrin.h>
using namespace gbtg;

namespace
{
constexpr u32 kActor = 0x10000000;
constexpr u32 kO1 = 0x10003000;
constexpr u32 kO2 = 0x10005000;
constexpr u32 kT0 = 0x10007000;
constexpr u32 kT1 = 0x10007100;
constexpr u32 kObject = 0x1000A000;
const char* const kQueues[] = {"target", "eligible", "knockout", "resume", "search", "direction", "create",
    "unavailable", "surface", "chance", "dist", "randomize"};

std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;
std::vector<u32> g_line;
size_t g_cursor = 0;

u32 next()
{
    return g_line.at(g_cursor++);
}

float next_float()
{
    const u32 bits = next();
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
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

u32 bits_of(float value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

float float_of(u32 bits)
{
    float value;
    std::memcpy(&value, &bits, 4);
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

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

// Objects the stubs recognise.
Actor* g_actor = nullptr;
Actor* g_o1 = nullptr;
Actor* g_o2 = nullptr;
RuntimeMindTarget* g_t0 = nullptr;
RuntimeMindTarget* g_t1 = nullptr;
const OccupancyRecord* g_allowed_record = nullptr;
const OccupancyRecord* g_occ_record = nullptr;
std::vector<std::unique_ptr<RuntimeMindEvent>>* g_events = nullptr;
std::vector<std::unique_ptr<RuntimeMindEvent>> g_new_events;
std::vector<std::unique_ptr<std::array<u8, 0x40>>> g_storage;
std::vector<std::unique_ptr<RuntimePlayerMindRequest>> g_requests;

const char* actor_name(const Actor* actor)
{
    if (actor == nullptr)
    {
        return "none";
    }
    return actor == g_actor ? "actor" : actor == g_o1 ? "o1" : actor == g_o2 ? "o2" : "?";
}

const char* record_name(const OccupancyRecord* record)
{
    return record == nullptr ? "none" : record == g_allowed_record ? "allowed" : record == g_occ_record ? "occ" : "?";
}

const char* target_bytes_name(std::span<const u8> bytes)
{
    if (g_t0 != nullptr && bytes.data() == g_t0->bytes.data())
    {
        return "t0";
    }
    if (g_t1 != nullptr && bytes.data() == g_t1->bytes.data())
    {
        return "t1";
    }
    return "?";
}
}

namespace gbtg
{
// Retail helpers the translated modules call; copies of the verified implementations.
long double animation_remainder(float value, float divisor)
{
    float absolute_divisor = divisor;
    if (divisor < 0.f)
    {
        absolute_divisor = 0.f - divisor;
    }
    float absolute_value = value;
    if (value < 0.f)
    {
        absolute_value = 0.f - value;
    }
    if (absolute_divisor <= absolute_value)
    {
        const float quotient = value / divisor;
        const int integral = _mm_cvtt_ss2si(_mm_set_ss(quotient));
        return static_cast<long double>(value) - static_cast<long double>(integral) * divisor;
    }
    return value;
}
float engine_absolute(float value)
{
    if (0.f <= value)
    {
        return value;
    }
    return 0.f - value;
}
void detach_runtime_strategy_link(RuntimeStrategyLink& link)
{
    RuntimeStrategyLink* first = link.previous;
    RuntimeStrategyLink* anchor;
    if (first == nullptr)
    {
        anchor = nullptr;
    }
    else
    {
        anchor = &link;
        RuntimeStrategyLink* node = first;
        while (node != nullptr)
        {
            anchor = node;
            node = node->previous;
        }
    }
    if (first != nullptr)
    {
        first->next = link.next;
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
    if (anchor != nullptr && anchor->next == nullptr)
    {
        anchor->owner = nullptr;
    }
    link.active = 0;
}
bool runtime_mind_target_occupancy_differs(const RuntimeMindTarget& target, u16 owner_class,
    const OccupancyRecord* allowed)
{
    return target.bytes[0x14] == 0 && target.occupancy != nullptr && target.occupancy != allowed
        && (owner_class == 0 || target.occupancy->group == 0 || u16(target.occupancy->group) == owner_class);
}
u8 runtime_player_strategy_decay_enabled(std::span<const u8> actor, const RuntimePlayerStrategyResume& context)
{
    const size_t group = (read_u16(actor, 0xCDC) == 2 || read_u32(actor, 0x93C) != 0) ? 1 : 0;
    if (context.references[group] != 0)
    {
        return 0;
    }
    return context.decay_enabled[context.decay_index];
}

// Logging stubs.
Vec3 actor_world_position(const Actor& actor)
{
    return actor.position;
}
long double runtime_actor_distance(const Actor& first, const Actor& second)
{
    logf("distance %s %s", actor_name(&first), actor_name(&second));
    return &second == g_o1 ? 3.0f : &second == g_o2 ? 7.0f : &second == g_actor ? 5.0f : 9.0f;
}
RuntimeMindTarget* find_runtime_mind_target(const Actor& source, const OccupancyRecord* allowed, u16 include_class,
    u16 exclude_class, float* distance, u32 category_mask, u32 predicate_flags, RuntimeMindTarget*)
{
    const u32 value = pop("target");
    const u32 range = pop("dist");
    logf("target %s %s %x %x %x %x", actor_name(&source), record_name(allowed), include_class, exclude_class,
        category_mask, predicate_flags);
    const float distances[]{2.0f, 40.0f, 100.0f};
    *distance = distances[range % 3];
    RuntimeMindTarget* targets[]{nullptr, g_t0, g_t1};
    return targets[value % 3];
}
bool runtime_mind_target_is_eligible(std::span<const u8>, std::span<const u8>, const Actor&, u32,
    const RuntimeMindTarget& target, bool adjust)
{
    const u32 value = pop("eligible");
    logf("eligible %s %d", target_bytes_name(target.bytes), adjust ? 1 : 0);
    return value != 0;
}
void refresh_runtime_mind_target_event(RuntimeMindEvents&)
{
    logf("refresh");
}
RuntimeMindEvent* allocate_runtime_mind_event(RuntimeMindEvents&, u16 kind, float duration)
{
    logf("event %x %08x", kind, bits_of(duration));
    auto event = std::make_unique<RuntimeMindEvent>();
    g_storage.push_back(std::make_unique<std::array<u8, 0x40>>());
    event->bytes = *g_storage.back();
    g_new_events.push_back(std::move(event));
    return g_new_events.back().get();
}
void script_release_event(LiveScript* node)
{
    for (size_t index = 0; index < g_events->size(); ++index)
    {
        if ((*g_events)[index].get() == node)
        {
            logf("drop %d", int(index));
            return;
        }
    }
    logf("drop ?");
}
void change_runtime_player_strategy_state(FactoryObject& strategy, u16 state, RuntimePlayerStrategySetup&)
{
    logf("state %x", state);
    std::memcpy(strategy.bytes.get() + 0xE, &state, 2);
}
void dispatch_runtime_base_strategy_message(std::span<const u32> message, RuntimeBaseStrategyMessages&)
{
    logf("base %x", message[0]);
}
void update_runtime_base_strategy(RuntimePlayerStrategyGoals&)
{
    logf("baseupdate");
}
void enter_runtime_base_strategy_state(RuntimePlayerStrategyEntry&, u16 state)
{
    logf("baseenter %x", state);
}
u32 check_runtime_player_knockout(RuntimePlayerStrategyGoals&)
{
    const u32 value = pop("knockout");
    logf("knockout %d", value);
    return value;
}
u8 runtime_player_can_resume_strategy(std::span<const u8>, const RuntimePlayerStrategyResume&)
{
    const u32 value = pop("resume");
    logf("resume %d", value);
    return u8(value);
}
bool load_runtime_mind_goal(const char* name, RuntimeMindGoalContext&)
{
    logf("goal %s", name);
    return true;
}
RuntimePlayerMindRequest* allocate_runtime_mind_request(RuntimePlayerMindRequest&, u16 kind, std::span<const u8>)
{
    logf("request %x", kind);
    g_storage.push_back(std::make_unique<std::array<u8, 0x40>>());
    auto request = std::make_unique<RuntimePlayerMindRequest>();
    request->bytes = *g_storage.back();
    g_requests.push_back(std::move(request));
    return g_requests.back().get();
}
bool search_runtime_navigation_headings(RuntimeNavigationHeadingSearch& context, float heading, float increment,
    i32 attempts)
{
    const u32 value = pop("search");
    logf("search %08x %08x %08x | %08x %d %08x %d %08x %08x %s %s %d %x %x", bits_of(context.origin.x),
        bits_of(context.origin.y), bits_of(context.origin.z), bits_of(heading), context.floor, bits_of(increment),
        attempts, bits_of(context.minimum), bits_of(context.maximum), record_name(context.required),
        record_name(context.allowed), context.keep_sample, context.flags, u16(context.group));
    if (value != 0)
    {
        *context.output = Vec3{1.5f, 2.5f, 3.5f};
    }
    return value != 0;
}
float randomize_runtime_navigation_heading(float heading, float width, u32&)
{
    const u32 value = pop("randomize");
    logf("randomize %08x %08x", bits_of(heading), bits_of(width));
    const float results[]{0.25f, 3.0f, 6.0f};
    return results[value % 3];
}
bool runtime_direction_angle(const Vec3& direction, float& angle)
{
    const u32 value = pop("direction");
    logf("direction %08x %08x %08x", bits_of(direction.x), bits_of(direction.y), bits_of(direction.z));
    if (value != 0)
    {
        angle = 0.75f * float(value);
    }
    return value != 0;
}
void clear_animation_controller_slot(AnimationController&, u32 slot, bool send_end, AnimationResourceRegistry&)
{
    logf("clearslot %d %d", slot, send_end ? 1 : 0);
}
bool runtime_body_bit_set(const EntityBitBlock& flags, u32 index)
{
    const u32 value = (flags.bytes[index >> 3] >> (index & 7)) & 1;
    logf("bit %x %d", index, value);
    return value != 0;
}
bool actor_spawn_owner_unavailable(const Actor& actor)
{
    const u32 value = pop("unavailable");
    logf("unavailable %s %d", actor_name(&actor), value);
    return value != 0;
}
float runtime_body_target_surface_distance(const Actor&, const RuntimePlayerAttachmentEligibility&,
    std::span<const u8> target)
{
    const u32 value = pop("surface");
    logf("surface %s", target_bytes_name(target));
    return value % 2 != 0 ? 80.0f : 10.0f;
}
bool actor_event_chance(u32&, float chance)
{
    const u32 value = pop("chance");
    logf("chance %08x", bits_of(chance));
    return value != 0;
}
// Referenced by unrelated functions in the linked modules; no exercised path reaches them.
std::span<const u8> acquire_animation_resource(AnimationResourceRegistry&, const char*)
{
    throw Error("acquire_animation_resource is not part of this oracle");
}
void resource_error(ResourceErrorState&, int)
{
    throw Error("resource_error is not part of this oracle");
}
void* engine_heap_reallocate(void*, i32)
{
    throw Error("engine_heap_reallocate is not part of this oracle");
}
long double retail_square_root(float)
{
    throw Error("retail_square_root is not part of this oracle");
}
float runtime_actor_bound_angle(std::span<const u8>, const Actor&)
{
    throw Error("runtime_actor_bound_angle is not part of this oracle");
}
float runtime_actor_direction_to_point(const Actor&, const Vec3&)
{
    throw Error("runtime_actor_direction_to_point is not part of this oracle");
}
void* engine_heap_allocate(i32)
{
    return nullptr;
}
void engine_heap_free(void*)
{
}
}

namespace
{
struct OtherActor
{
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();
    std::vector<u8> bytes = std::vector<u8>(0xD00);
    std::vector<u8> body = std::vector<u8>(0x400);
    std::array<u8, 8> bit_bytes{};
    EntityBitBlock bits{};
    RuntimeScriptMindQuery query{};
    const OccupancyRecord* occupancy = nullptr;
    bool model_present = false;
    std::vector<RuntimeBodyModelNode> nodes;
    std::vector<u8> data = std::vector<u8>(0x200);
    RuntimeBodyModelNodes model{};
    u32 address = 0;
    u32 body_address = 0;
};

u32 actor_code_address(u32 code)
{
    const u32 addresses[]{kActor, kO1, kO2};
    return addresses[code];
}

void run_case()
{
    g_log.clear();
    g_queues.clear();
    g_new_events.clear();
    g_storage.clear();
    g_requests.clear();
    g_line.clear();
    g_cursor = 0;
    std::string text;
    std::getline(std::cin >> std::ws, text);
    {
        size_t position = 0;
        while (position < text.size())
        {
            size_t end = text.find(' ', position);
            if (end == std::string::npos)
            {
                end = text.size();
            }
            if (end > position)
            {
                g_line.push_back(u32(std::stoul(text.substr(position, end - position))));
            }
            position = end + 1;
        }
    }
    const u32 entry = next();
    for (const char* name : kQueues)
    {
        for (int index = 0; index < 12; ++index)
        {
            g_queues[name].push_back(next());
        }
    }
    // Actor.
    auto actor = std::make_unique<Actor>();
    g_actor = actor.get();
    std::vector<u8> actor_bytes(0xD00);
    actor->class_key = u16(next());
    put_u16(actor_bytes, 6, actor->class_key);
    put(actor_bytes, 0x18, next());
    actor->position = Vec3{next_float(), next_float(), next_float()};
    for (size_t offset : {0x969, 0x96C, 0x96D, 0x96E, 0x96F, 0x970})
    {
        actor_bytes[offset] = u8(next());
    }
    put(actor_bytes, 0x974, next());
    put(actor_bytes, 0x978, next());
    put_u16(actor_bytes, 0x97C, u16(next()));
    put_u16(actor_bytes, 0x97E, u16(next()));
    put_u16(actor_bytes, 0xCDC, u16(next()));
    put(actor_bytes, 0x93C, next());
    OccupancyRecord allowed_record{};
    OccupancyRecord occ_record{};
    g_allowed_record = &allowed_record;
    g_occ_record = &occ_record;
    const OccupancyRecord* allowed = next() != 0 ? &allowed_record : nullptr;
    const OccupancyRecord* room = next() != 0 ? &occ_record : nullptr;
    put(actor_bytes, 0xCEC, next());
    const bool attributes_present = next() != 0;
    std::vector<u8> attribute_bytes(0x40);
    put(attribute_bytes, 8, next());
    put(attribute_bytes, 0x18, next());
    put(attribute_bytes, 0x2C, next());
    std::span<const u8> attributes = attributes_present ? std::span<const u8>(attribute_bytes) : std::span<const u8>();
    occ_record.group = i16(next());
    // Strategy.
    FactoryObject strategy;
    strategy.bytes = std::make_unique<u8[]>(0x50);
    strategy.size = 0x50;
    std::memset(strategy.bytes.get(), 0, 0x50);
    std::span<u8> strategy_span{strategy.bytes.get(), 0x50};
    put(strategy_span, 8, kActor);
    put_u16(strategy_span, 0xE, u16(next()));
    put_u16(strategy_span, 0x18, u16(next()));
    strategy_span[0x1A] = u8(next());
    strategy_span[0x1C] = u8(next());
    put(strategy_span, 0x38, next());
    const u32 held = next();
    put(strategy_span, 0x40, next());
    put(strategy_span, 0x44, next() != 0 ? kObject : 0);
    put(strategy_span, 0x48, next());
    put(strategy_span, 0x4C, 0x004287BC);
    // Mind.
    std::vector<u8> mind(0x100);
    put(mind, 0xC, next());
    const u32 target_addresses[]{0, kT0, kT1};
    put(mind, 0x10, target_addresses[next()]);
    RuntimePlayerMindRequest pending{};
    RuntimePlayerMindRequest* first_request = next() != 0 ? &pending : nullptr;
    put(mind, 0x64, next());
    put(mind, 0x68, next());
    put(mind, 0x6C, next());
    LiveScript anchor;
    std::vector<std::unique_ptr<RuntimeMindEvent>> events;
    g_events = &events;
    std::vector<std::array<u8, 0x40>> event_bytes(4);
    const u32 event_count = next();
    LiveScript* previous = &anchor;
    for (u32 index = 0; index < event_count; ++index)
    {
        auto event = std::make_unique<RuntimeMindEvent>();
        event->type = next();
        event->bytes = event_bytes[index];
        previous->next = event.get();
        event->prev = previous;
        previous = event.get();
        events.push_back(std::move(event));
    }
    // Body.
    std::vector<u8> body(0x900);
    put_u16(body, 0x30, u16(next()));
    put(body, 0x34, next());
    body[0x6A] = u8(next());
    body[0x6B] = u8(next());
    put(body, 0x70, next());
    put_u16(body, 0x94, u16(next()));
    put_u16(body, 0x96, u16(next()));
    body[0x328] = 0x55;
    std::array<u8, 8> bit_bytes{};
    for (u8& value : bit_bytes)
    {
        value = u8(next());
    }
    EntityBitBlock bits{bit_bytes.data()};
    EntityBitBlock* bits_pointer = &bits;
    // Ring.
    std::array<u8, 0x40> rows{};
    RuntimePlayerGoalRing ring_storage{8, 4, 1, 2, rows};
    RuntimePlayerGoalRing* ring = nullptr;
    if (next() != 0)
    {
        ring_storage.tail = next() != 0 ? 4 : 2;
        put_u16(rows, 8, u16(next()));
        ring = &ring_storage;
    }
    std::array<u8, 4> address_zero{};
    put_u16(address_zero, 0, u16(next()));
    // Others (read after the targets, which name them).
    struct TargetSeed
    {
        u32 actor_code;
        u8 mode;
        u8 flag;
        std::array<u32, 3> position;
    };
    std::array<TargetSeed, 2> target_seeds{};
    for (auto& seed : target_seeds)
    {
        seed.actor_code = next();
        seed.mode = u8(next());
        seed.flag = u8(next());
        for (u32& word : seed.position)
        {
            word = next();
        }
    }
    std::array<OtherActor, 2> others;
    for (int index = 0; index < 2; ++index)
    {
        OtherActor& other = others[index];
        other.address = index == 0 ? kO1 : kO2;
        other.body_address = other.address + 0x1000;
        other.actor->class_key = u16(next());
        put_u16(other.bytes, 6, other.actor->class_key);
        other.actor->position = Vec3{next_float(), next_float(), next_float()};
        other.query.case_actor = next() != 0 ? actor.get() : nullptr;
        const u32 holders[]{0, kActor, kO1};
        put(other.body, 0x50, holders[next()]);
        put(other.body, 0x58, next());
        put(other.body, 8, other.address);
        for (u8& value : other.bit_bytes)
        {
            value = u8(next());
        }
        other.bits.bytes = other.bit_bytes.data();
        other.query.bits = &other.bits;
        const u32 occupancy = next();
        other.occupancy = occupancy == 1 ? &allowed_record : occupancy == 2 ? &occ_record : nullptr;
        other.model_present = next() != 0;
        const u32 node_count = next();
        for (u32 node = 0; node < node_count; ++node)
        {
            const u32 kind = next();
            const bool data_present = next() != 0;
            other.nodes.push_back(RuntimeBodyModelNode{kind, data_present ? other.data.data() : nullptr});
        }
        const u32 record_count = next();
        put(other.data, 0, record_count);
        for (u32 record = 0; record < record_count; ++record)
        {
            put_u16(other.data, 4 + 0x34 * record, u16(next()));
        }
        other.model.nodes = node_count != 0 ? std::span<const RuntimeBodyModelNode>(other.nodes)
                                            : std::span<const RuntimeBodyModelNode>();
    }
    g_o1 = others[0].actor.get();
    g_o2 = others[1].actor.get();
    std::array<std::vector<u8>, 2> target_bytes{std::vector<u8>(0x60), std::vector<u8>(0x60)};
    std::array<ActorLiveAttachment, 2> records{};
    std::vector<std::unique_ptr<RuntimeMindTarget>> targets;
    for (int index = 0; index < 2; ++index)
    {
        const TargetSeed& seed = target_seeds[index];
        OtherActor& other = others[seed.actor_code];
        put(target_bytes[index], 0x10, other.address);
        target_bytes[index][0x14] = seed.mode;
        target_bytes[index][0x19] = seed.flag;
        for (int axis = 0; axis < 3; ++axis)
        {
            put(target_bytes[index], 0x20 + 4 * axis, seed.position[axis]);
            std::memcpy(records[index].shape.data() + 4 + 4 * axis, &seed.position[axis], 4);
        }
        records[index].actor = other.actor.get();
        records[index].mode = seed.mode;
        records[index].flag = seed.flag;
        targets.push_back(std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, target_bytes[index],
            index == 0 ? kT0 : kT1, *other.actor, other.bytes, other.query, other.occupancy, &records[index]}));
    }
    g_t0 = targets[0].get();
    g_t1 = targets[1].get();
    // Target queue slots.
    std::array<ActorAttachmentSlot, 20> slots{};
    std::array<RuntimeStrategyLink, 6> links{};
    size_t link_index = 0;
    u32 owners[3];
    for (u32& owner : owners)
    {
        owner = next();
    }
    for (int index = 0; index < 3; ++index)
    {
        ActorAttachmentSlot& slot = slots[index];
        slot.owner = owners[index] == 0 ? nullptr : &records[owners[index] - 1];
        auto* previous_link = reinterpret_cast<RuntimeStrategyLink*>(&slot);
        const u32 count = next();
        for (u32 link = 0; link < count; ++link)
        {
            RuntimeStrategyLink& node = links[link_index++];
            node.previous = previous_link;
            previous_link->next = &node;
            node.next = nullptr;
            node.owner = others[next()].actor.get();
            node.retained = next();
            node.active = next();
            previous_link = &node;
        }
    }
    RuntimeStrategyLink link{};
    const u32 queued = next();
    if (queued != 0 && owners[queued - 1] != 0)
    {
        auto* last = reinterpret_cast<RuntimeStrategyLink*>(&slots[queued - 1]);
        while (last->next != nullptr)
        {
            last = last->next;
        }
        last->next = &link;
        link.previous = last;
        link.owner = actor.get();
    }
    std::array<u32, 1> limits{next()};
    std::array<u32, 16> class_limits{};
    for (u32 key : {2u, 5u, 9u})
    {
        class_limits[key] = next();
    }
    // Active actor list.
    const u32 active_count = next();
    std::vector<RuntimeActiveActorLink> active_links(std::max<u32>(active_count, 1));
    std::vector<RuntimeScriptMindQuery> active_bodies(std::max<u32>(active_count, 1));
    std::vector<std::array<u8, 0x10>> active_rows(std::max<u32>(active_count, 1));
    std::vector<RuntimePlayerGoalRing> active_rings(std::max<u32>(active_count, 1));
    std::vector<RuntimePlayerGoalRing*> active_ring_pointers(std::max<u32>(active_count, 1));
    for (u32 index = 0; index < active_count; ++index)
    {
        active_bodies[index].energy = i16(next());
        const u32 ring_code = next();
        active_rows[index] = {};
        put_u16(active_rows[index], 0, u16(next()));
        active_rings[index] = RuntimePlayerGoalRing{8, 4, 0, u16(ring_code == 1 ? 4 : 1), active_rows[index]};
        active_ring_pointers[index] = ring_code != 0 ? &active_rings[index] : nullptr;
        active_links[index] = RuntimeActiveActorLink{index + 1 < active_count ? &active_links[index + 1] : nullptr,
            &active_bodies[index], &active_ring_pointers[index]};
    }
    const RuntimeActiveActorLink* active = active_count != 0 ? &active_links[0] : nullptr;
    // Animation controller.
    ActorAnimationState animation{};
    if (next() != 0)
    {
        animation.controller = std::make_unique<AnimationController>();
        animation.controller->slots.resize(2);
    }
    const bool player_present = next() != 0;
    const bool resource_present = next() != 0;
    const bool aid_name = next() != 0;
    static const std::array<u8, 4> resource{};
    if (animation.controller != nullptr && player_present)
    {
        auto player = std::make_unique<MinaPlayer>();
        if (resource_present)
        {
            player->mina = resource;
        }
        player->resource_name = aid_name ? "aid_fire" : "xyz";
        animation.controller->slots[1].player = std::move(player);
    }
    // Globals.
    u32 references[2];
    references[0] = next();
    references[1] = next();
    std::array<u8, 4> decay_enabled{};
    decay_enabled[0] = u8(next());
    const i32 difficulty = i32(next());
    u32 random = next();
    const i32 attachment_limit = i32(next());
    const float freeze = next_float();
    const float dt = next_float();
    const float scale = next_float();
    actor->animation_rate = next_float();
    actor->animation_pause = next_float();
    std::array<u8, 4> seen{};
    seen[2] = u8(next());
    u32 notice_pending = next();
    const Actor* actor_codes[]{actor.get(), g_o1, g_o2};
    const Actor* notice_first = actor_codes[next()];
    u32 notice_id = next();
    std::array<RuntimeTutorialNotice, 4> notice_queue{};
    i32 notice_count = i32(next());
    for (i32 index = 0; index < notice_count; ++index)
    {
        notice_queue[size_t(index)].actor = actor_codes[next()];
        notice_queue[size_t(index)].id = next();
    }
    const u32 player_token = next();
    const Actor* notice_player = player_token == 2 ? g_o1 : nullptr;
    // Message, arguments and extras.
    std::array<u32, 9> message{};
    for (u32 index = 0; index < 9; ++index)
    {
        u32 word = next();
        if (index == 1 || index == 2)
        {
            const u32 addresses[]{0, kActor, kO1, kO2, kObject};
            word = addresses[word];
        }
        message[index] = word;
    }
    const u32 argument = next();
    const u32 argument2 = next();
    std::array<u32, 3> floats{next(), next(), next()};
    u32 extra = 0;
    u32 extra2 = 0;
    if (entry == 0 || entry == 1 || entry == 2 || entry == 3 || entry == 18)
    {
        extra = next();
    }
    if (entry == 19)
    {
        extra = next();
        extra2 = next();
    }
    std::array<u8, 0x40> parameters{};
    for (size_t index = 0; index < 16; ++index)
    {
        put(parameters, 4 * index, next());
    }

    // Context.
    static u32 block_decay = 0;
    static u32 decay_index = 0;
    const RuntimeActorClock clock{freeze, dt, scale, block_decay, decay_enabled, decay_index};
    std::array<u8, 0x100> device{};
    std::unordered_map<u32, std::span<u8>> release_map;
    RuntimeInputReleaseContext release{release_map};
    RuntimeMindEvents mind_events{anchor, mind, {},
        [](std::span<const u32> notice)
        {
            logf("notice %x %x", notice[0], notice[1]);
        }};
    RuntimePlayerMindRequest request_anchor{};
    std::string scratch;
    alignas(64) static u8 registry_storage[4096];
    AnimationResourceRegistry& registry = *reinterpret_cast<AnimationResourceRegistry*>(registry_storage);
    ResourceErrorState error{};
    RuntimeMindGoalContext goals{nullptr, request_anchor, {}, nullptr, scratch, random, registry, error};
    std::span<u8> body_span{body};
    RuntimeMindTarget* target_list = nullptr;
    RuntimePlayerStrategyEntry entry_context{strategy_span, *actor, actor_bytes, body_span, mind_events, goals, ring,
        address_zero, allowed, target_list, "actionenableddelay", "celebrate", "coward", "worried", "taunt"};
    ObjectFactoryRegistry factory;
    RuntimePlayerChildContext child{factory, {}, kActor};
    RuntimePlayerMindRequests mind_requests{first_request, nullptr};
    RuntimePlayerMindRequests* mind_pointer = &mind_requests;
    RuntimePlayerStrategySetup setup{child, mind_pointer, nullptr, nullptr};
    u32 override_words[8]{};
    RuntimePlayerScriptOverrides overrides{override_words[0], override_words[1], override_words[2], override_words[3],
        override_words[4], override_words[5], override_words[6], override_words[7]};
    RuntimeBodyAttachedActor self{actor_bytes, kActor, nullptr, nullptr, actor.get()};
    entity_detail::LatchNode* first_latch = nullptr;
    RuntimePlayerGoalCleanup cleanup{self, ring, body_span, *actor, first_latch, address_zero, overrides};
    const u32 resume_index = 0;
    RuntimePlayerStrategyResume resume{std::span<const u32, 2>(references), decay_enabled, resume_index};
    RuntimeBodyAttachedActor* attached_pointer = nullptr;
    RuntimeBodyAnimationReceiver* first_receiver = nullptr;
    RuntimeBodyAnimationReceiver* second_receiver = nullptr;
    FactoryObject first_action;
    FactoryObject second_action;
    const u8* action_row = nullptr;
    const RuntimeScriptLookupTable* table_pointer = nullptr;
    const std::unordered_map<u32, const char*> action_names;
    u32 action_random = 1;
    RuntimeBodyActionSelection actions{animation, attached_pointer, first_receiver, second_receiver, first_action,
        second_action, action_row, table_pointer, action_names, child, registry, error, action_random, 0x10001000};
    alignas(16) static u8 game_storage[16];
    Game& game = *reinterpret_cast<Game*>(game_storage);
    RuntimeBodyAnimationReceiver receiver;
    ModelAnimationState model{};
    ModelNodeFlagCache cache{};
    const AnimationMatrix reset{};
    RuntimePlayerAttachmentActor pose{self, *actor, receiver, animation, model, 0, nullptr, cache, reset};
    std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attached_poses;
    RuntimeModelResourceViews views;
    RuntimeModelDeferredPool deferred{};
    u32 fence = 0;
    RuntimeShaderRegistrationState shaders;
    RuntimeShaderAnimationScratch shader_scratch;
    std::function<void(u8*)> free_model;
    ActorEffectModelContext models{registry, error, views, deferred, fence, {}, shaders, shader_scratch, free_model};
    RuntimeActorSoundQueues sounds{};
    u32 effect_random = 7;
    Vec3 origin{};
    Vec3 default_euler{};
    ActorEffectStreamContext effects{models, sounds, effect_random, origin, default_euler};
    const OccupancyRecord* current = nullptr;
    const OccupancyRecord* goal_volume = nullptr;
    const void* script_records = nullptr;
    const u32 runtime_flags = 0;
    std::array<OtherActor, 2>* other_pointer = &others;
    RuntimePlayerStrategyGoals context{entry_context, strategy, setup, cleanup, resume, bits_pointer, actions,
        []()
        {
            return u16(0);
        },
        game,
        [](u32)
        {
            return u16(0);
        },
        [](u32 address) -> RuntimeMindTarget*
        {
            return address == kT0 ? g_t0 : address == kT1 ? g_t1 : nullptr;
        },
        [](const void*) -> u32
        {
            return kActor;
        },
        pose, attached_poses, effects, device, release, room, current, goal_volume, script_records, runtime_flags,
        random, clock, "ghouly_intro", "drone_waitforscare"};
    std::vector<u8> query_zero(4);
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> query_views;
    RuntimeScriptActorQueryContext query{nullptr, query_views, query_zero.data()};
    const u8* carried = nullptr;
    RuntimeBaseStrategyMessages base{context, query, link,
        [](u32) -> RuntimeStrategyMessageActor*
        {
            return nullptr;
        },
        carried};
    float occupancy_initial = 0.f;
    RuntimeTutorialNotices notices{seen, notice_pending, notice_first, notice_id, notice_queue, notice_count,
        notice_player};
    RuntimeStrategyTargetChecks checks{context, attributes, difficulty, active, notices, occupancy_initial};
    RuntimeStrategyTargetQueue queue{slots, limits, class_limits,
        [&records](const ActorLiveAttachment* record) -> const RuntimeMindTarget*
        {
            return record == &records[0] ? g_t0 : record == &records[1] ? g_t1 : nullptr;
        },
        [&actor_bytes, other_pointer](const void* owner) -> std::span<const u8>
        {
            if (owner == g_actor)
            {
                return actor_bytes;
            }
            return owner == g_o1 ? (*other_pointer)[0].bytes : (*other_pointer)[1].bytes;
        }};
    auto view_of = [other_pointer](OtherActor& other)
    {
        return RuntimeStrategyBodyView{other.body_address, other.body, &other.bits};
    };
    RuntimeImpFireStrategy imp{base, checks, queue, parameters, animation, registry,
        [](const char* name, u32 data) -> u32
        {
            const u32 value = pop("create");
            logf("create %s %s", name, data == kActor ? "actor" : "?");
            return value != 0 ? kObject : 0;
        },
        [](u32, std::span<const u32> words)
        {
            logf("object %x %x", words[0], words[1]);
        },
        [other_pointer, view_of](u32 address)
        {
            return view_of(address == kO1 + 0x1000 ? (*other_pointer)[0] : (*other_pointer)[1]);
        },
        [other_pointer, view_of](const Actor& owner)
        {
            return view_of(&owner == g_o1 ? (*other_pointer)[0] : (*other_pointer)[1]);
        },
        [other_pointer](const Actor& owner) -> const RuntimeBodyModelNodes*
        {
            OtherActor& other = &owner == g_o1 ? (*other_pointer)[0] : (*other_pointer)[1];
            return other.model_present ? &other.model : nullptr;
        },
        attachment_limit, "aid_objparams_ghoulies_fx_mutatehalo", "attack", "mutateup", "jumpbite", "jumpbiteprepare"};
    put(strategy_span, 0x3C, held == 0 ? 0 : held == 1 ? kO1 + 0x1000 : kO2 + 0x1000);

    std::string out = "-";
    char buffer[256];
    RuntimeMindTarget* target_codes[]{nullptr, g_t0, g_t1};
    switch (entry)
    {
    case 0:
    {
        RuntimeMindTarget* target = target_codes[extra];
        const bool result = join_runtime_strategy_target_queue(link, target != nullptr ? target->record : nullptr,
            actor.get(), queue);
        out = result ? "1" : "0";
        break;
    }
    case 1:
    case 2:
    {
        RuntimeMindTarget* output = nullptr;
        const bool result = entry == 1 ? select_runtime_strategy_target(output, extra, context)
                                       : select_runtime_strategy_hostile_target(output, extra, context);
        std::snprintf(buffer, sizeof(buffer), "%d %d", result ? 1 : 0, output == nullptr ? 0 : output == g_t0 ? 1 : 2);
        out = buffer;
        break;
    }
    case 3:
        out = std::to_string(runtime_strategy_abandons_target(*target_codes[extra], checks));
        break;
    case 4:
        out = std::to_string(react_runtime_strategy_message(message, checks));
        break;
    case 5:
        out = std::to_string(react_runtime_strategy_contact(message, float_of(floats[0]), checks));
        break;
    case 6:
        out = std::to_string(react_runtime_strategy_cower(checks));
        break;
    case 7:
        out = std::to_string(react_runtime_strategy_startle(checks));
        break;
    case 8:
        out = std::to_string(notice_runtime_strategy_hostile_target(0x10, checks));
        break;
    case 9:
        out = request_runtime_strategy_roam(parameters, checks) != nullptr ? "1" : "0";
        break;
    case 10:
        out = std::to_string(retreat_runtime_strategy_from_reached_target(checks));
        break;
    case 11:
        put_u16(strategy_span, 0xE, u16(argument));
        update_runtime_imp_fire_strategy(imp);
        break;
    case 12:
        dispatch_runtime_imp_fire_message(message, imp);
        break;
    case 13:
        enter_runtime_imp_fire_state(u16(argument), u16(argument2), imp);
        break;
    case 14:
        leave_runtime_imp_fire_state(u16(argument), u16(argument2), imp);
        break;
    case 15:
        initialize_runtime_imp_fire_strategy(imp);
        break;
    case 16:
        destroy_runtime_imp_fire_strategy(imp);
        break;
    case 17:
    {
        const float first = reflect_runtime_angle(float_of(floats[0]), float_of(floats[1]));
        const float second = nearest_runtime_angle(float_of(floats[0]), float_of(floats[1]), float_of(floats[2]));
        const float fraction = float(std::fabs(double(float_of(floats[2]))) / 12.0);
        const float third = blend_runtime_imp_fire_heading(fraction, float_of(floats[1]));
        std::snprintf(buffer, sizeof(buffer), "%08x %08x %08x", bits_of(first), bits_of(second), bits_of(third));
        out = buffer;
        break;
    }
    case 18:
        approach_runtime_imp_fire_target(*target_codes[extra], imp);
        break;
    default:
    {
        OtherActor& other = others[extra2 == 2 ? 0 : 1];
        const u8* row = find_runtime_body_model_record(other.model_present ? &other.model : nullptr, 4, i32(extra));
        out = std::to_string(row == nullptr ? -1 : int((row - other.data.data() - 4) / 0x34));
        break;
    }
    }
    // Snapshot.
    auto link_code = [&](const RuntimeStrategyLink* node) -> std::string
    {
        if (node == nullptr)
        {
            return "n";
        }
        for (int index = 0; index < 20; ++index)
        {
            if (node == reinterpret_cast<const RuntimeStrategyLink*>(&slots[index]))
            {
                return "s" + std::to_string(index);
            }
        }
        if (node == &link)
        {
            return "me";
        }
        return "l" + std::to_string(node - links.data());
    };
    auto holder_code = [](u32 address)
    {
        return address == 0 ? 0 : address == kActor ? 1 : address == kO1 ? 2 : 9;
    };
    const u32 held_now = read_u32(strategy_span, 0x3C);
    std::string snapshot;
    std::snprintf(buffer, sizeof(buffer), "%u %u %u %u %u %u %u %u %u", read_u16(strategy_span, 0xE),
        strategy_span[0x1A], strategy_span[0x1C], read_u32(strategy_span, 0x38),
        held_now == 0 ? 0 : held_now == kO1 + 0x1000 ? 1 : held_now == kO2 + 0x1000 ? 2 : 9,
        read_u32(strategy_span, 0x40), read_u32(strategy_span, 0x44) == 0 ? 0 : read_u32(strategy_span, 0x44) == kObject ? 1 : 9,
        read_u32(strategy_span, 0x48), read_u32(strategy_span, 0x4C));
    snapshot = buffer;
    std::snprintf(buffer, sizeof(buffer), " %s %s %s %x %u", link_code(link.previous).c_str(),
        link_code(link.next).c_str(), link.owner == nullptr ? "n" : link.owner == actor.get() ? "a" : "?", link.retained,
        link.active);
    snapshot += buffer;
    const u32 mind_target = read_u32(mind, 0x10);
    std::snprintf(buffer, sizeof(buffer), " %u %u %u %u %u %d %d", read_u32(mind, 0xC),
        mind_target == 0 ? 0 : mind_target == kT0 ? 1 : mind_target == kT1 ? 2 : 9, read_u32(body, 0x70), body[0x328],
        room != nullptr ? 1 : 0, holder_code(read_u32(others[0].body, 0x50)), holder_code(read_u32(others[1].body, 0x50)));
    snapshot += buffer;
    std::snprintf(buffer, sizeof(buffer), " %u %u", random,
        animation.controller != nullptr && animation.controller->dirty ? 1 : 0);
    snapshot += buffer;
    snapshot += " S";
    for (int index = 0; index < 20; ++index)
    {
        const ActorAttachmentSlot& slot = slots[index];
        const int owner = slot.owner == nullptr ? 0 : slot.owner == &records[0] ? 1 : slot.owner == &records[1] ? 2 : 9;
        std::string chain;
        const auto* node = static_cast<const RuntimeStrategyLink*>(slot.records.next);
        int guard = 0;
        while (node != nullptr && guard < 10)
        {
            std::snprintf(buffer, sizeof(buffer), "%s:%s:%x:%u", link_code(node).c_str(),
                link_code(node->previous).c_str(), node->retained, node->active);
            chain += (chain.empty() ? "" : ",") + std::string(buffer);
            node = node->next;
            ++guard;
        }
        if (owner != 0 || !chain.empty())
        {
            snapshot += " " + std::to_string(index) + "=" + std::to_string(owner) + "[" + chain + "]";
        }
    }
    auto notice_code = [&](const Actor* value)
    {
        return value == actor.get() ? 0 : value == g_o1 ? 1 : value == g_o2 ? 2 : 9;
    };
    std::snprintf(buffer, sizeof(buffer), " N %x %x %x %x", notice_pending, notice_code(notice_first), notice_id,
        u32(notice_count));
    snapshot += buffer;
    for (i32 index = 0; index < notice_count; ++index)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", notice_code(notice_queue[size_t(index)].actor));
        snapshot += buffer;
    }
    for (i32 index = 0; index < notice_count; ++index)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", notice_queue[size_t(index)].id);
        snapshot += buffer;
    }
    snapshot += " R";
    for (const auto& request : g_requests)
    {
        std::string words;
        for (size_t offset = 0x10; offset < 0x2C; offset += 4)
        {
            std::snprintf(buffer, sizeof(buffer), offset == 0x10 ? "%08x" : "|%08x", read_u32(request->bytes, offset));
            words += buffer;
        }
        snapshot += " " + words;
    }
    snapshot += " E";
    for (const auto& event : g_new_events)
    {
        std::snprintf(buffer, sizeof(buffer), " %08x", read_u32(event->bytes, 0x20));
        snapshot += buffer;
    }
    out += " | " + snapshot + " |";
    for (size_t index = 0; index < g_log.size(); ++index)
    {
        out += (index == 0 ? " " : " ; ") + g_log[index];
    }
    std::printf("%s\n", out.c_str());
    std::fflush(stdout);
}
}

int main()
{
    int cases = 0;
    std::cin >> cases;
    for (int index = 0; index < cases; ++index)
    {
        try
        {
            run_case();
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "case %d: %s\n", index, error.what());
            return 1;
        }
    }
}
