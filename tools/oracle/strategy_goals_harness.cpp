// Host side of strategy_goals_oracle.py. player_strategy_goals.cpp is linked alone; every callee
// outside the batch is a logging stub that consumes the same scripted queues as the retail hooks.
#include "entities/player_strategy_goals.h"
#include "gbtg/game.h"
#include "gbtg/model_runtime.h"
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
constexpr u32 kActor = 0x10000000;
constexpr u32 kRing = 0x10003900;
constexpr u32 kTarget = 0x10008000;
constexpr u32 kRecords = 0x10009100;
constexpr u32 kOther1 = 0x1000D000;
constexpr u32 kOther2 = 0x1000E000;
constexpr u32 kTarget2 = 0x10008800;
const char* const kQueues[] = {"select", "lookup", "effect", "bit", "target", "sphere", "box", "spawn", "path",
    "classify", "range", "decay", "controlled", "differs", "saved", "resume", "timer", "key", "mask", "player",
    "latchkind"};

std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;

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

void logf(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_log.emplace_back(buffer);
}

// Objects the stubs recognise.
Actor* g_player = nullptr;
Actor* g_attached_actor = nullptr;
RuntimeMindTarget* g_target = nullptr;
const OccupancyRecord* g_allowed = nullptr;
const OccupancyRecord* g_volume = nullptr;
std::vector<OccupancyRecord>* g_pool_records = nullptr;
std::vector<std::unique_ptr<RuntimeMindEvent>>* g_events = nullptr;
RuntimeMindEvent* g_last_event = nullptr;
std::vector<std::unique_ptr<std::array<u8, 0x40>>> g_request_storage;
std::vector<std::unique_ptr<RuntimePlayerMindRequest>> g_requests;
std::vector<std::unique_ptr<RuntimeMindEvent>> g_new_events;
RuntimePlayerGoalRing** g_ring = nullptr;
std::span<u8> g_strategy;
std::span<u8> g_mind;
std::array<u8, 0x100>* g_device = nullptr;
RuntimeStrategyLink* g_link = nullptr;
std::map<const Actor*, const char*> g_actor_names;

u32 allowed_offset(const OccupancyRecord* record)
{
    return record == g_allowed ? 0x40 : 0xFFFF;
}
}

namespace gbtg
{
void* g_occupancy_pool = nullptr; // DAT_00512344
RuntimeActorCreationOptions g_actor_creation_options;
i32 g_last_actor_effect = -1;
std::array<ActorEffectNode*, 150> g_actor_effect_nodes{};

bool select_runtime_body_action(std::span<u8>, u16 key, RuntimeBodyActionSelection&)
{
    const u32 value = pop("select");
    logf("select %x %d", key, value);
    return value != 0;
}
const u8* find_runtime_script_table_row(const RuntimeScriptLookupTable*, u16 key, u32&)
{
    static const u8 row = 0;
    const u32 value = pop("lookup");
    logf("lookup %x %d", key, value);
    return value != 0 ? &row : nullptr;
}
void clear_runtime_player_mind_requests(RuntimePlayerMindRequests&)
{
    logf("clear");
}
void update_actor_animation_pose(Actor& actor, ActorAnimationState&, ModelAnimationState&, u16,
    AnimationResourceRegistry&, const SkeletonDispatchContext*, ModelNodeFlagCache&, const AnimationMatrix&)
{
    logf("pose %s", &actor == g_player ? "actor" : &actor == g_attached_actor ? "attached" : "?");
}
Vec3 actor_world_position(const Actor& actor)
{
    return actor.position;
}
Vec3 actor_world_euler(const Actor& actor)
{
    return actor.euler;
}
void increment_runtime_body_latches(std::span<u8>, Actor&, entity_detail::LatchNode*&)
{
    logf("latch");
}
void decrement_runtime_body_latches(std::span<u8>, Actor&, entity_detail::LatchNode*&)
{
    logf("unlatch");
}
void release_animation_controller(std::unique_ptr<AnimationController>& controller, bool send_end,
    AnimationResourceRegistry&)
{
    logf("release %d", send_end ? 1 : 0);
    controller.reset();
}
RuntimeMindEvent* allocate_runtime_mind_event(RuntimeMindEvents&, u16 kind, float duration)
{
    logf("event %x %08x", kind, bits_of(duration));
    auto event = std::make_unique<RuntimeMindEvent>();
    static std::vector<std::unique_ptr<std::array<u8, 0x40>>> storage;
    storage.push_back(std::make_unique<std::array<u8, 0x40>>());
    event->bytes = *storage.back();
    g_last_event = event.get();
    g_new_events.push_back(std::move(event));
    return g_last_event;
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
u32 load_actor_effect_stream(const char* name, const RuntimeActorCreationOptions* options, ActorEffectStreamContext&)
{
    const u32 value = pop("effect");
    logf("effect %s %08x %08x %08x %08x %08x %08x %08x %d", name, options->flags, bits_of(options->position.x),
        bits_of(options->position.y), bits_of(options->position.z), bits_of(options->euler.x),
        bits_of(options->euler.y), bits_of(options->euler.z), value);
    return value;
}
void queue_actor_sound(RuntimeActorSound& sound, i32 group, Actor* owner, u16, const Vec3*, const Vec3*, float volume,
    const char* name, RuntimeActorSoundQueues&)
{
    logf("sound %s %x %x %08x %d", name, &sound == &g_player->sound_slots[0] ? 0x334 : 0xFFFF,
        owner == g_player ? 1 : 0, bits_of(volume), group);
}
bool runtime_body_bit_set(const EntityBitBlock& flags, u32 index)
{
    const u32 value = (flags.bytes[index >> 3] >> (index & 7)) & 1;
    logf("bit %x %d", index, value);
    return value != 0;
}
RuntimeMindTarget* find_runtime_mind_target(const Actor&, const OccupancyRecord* allowed, u16 include_class, u16 exclude_class, float* distance,
    u32 category_mask, u32 predicate_flags, RuntimeMindTarget*)
{
    const u32 value = pop("target");
    logf("target %08x %08x %08x %x %x", predicate_flags, allowed_offset(allowed), category_mask, include_class, exclude_class);
    *distance = 3.5f;
    return value != 0 ? g_target : nullptr;
}
bool occupancy_sphere_contains(const OccupancyRecord&, const Vec3& point, float& distance)
{
    const u32 value = pop("sphere");
    logf("sphere %08x %08x %08x %08x", bits_of(point.x), bits_of(point.y), bits_of(point.z), bits_of(distance));
    return value != 0;
}
bool occupancy_box_contains(const OccupancyRecord&, const Vec3& point, float& distance)
{
    const u32 value = pop("box");
    logf("box %08x %08x %08x %08x", bits_of(point.x), bits_of(point.y), bits_of(point.z), bits_of(distance));
    return value != 0;
}
void set_runtime_mind_target(RuntimeMindEvents&, u32 target)
{
    logf("settarget %08x", target);
    std::memcpy(g_mind.data() + 0x10, &target, 4);
}
RuntimePlayerMindRequest* allocate_runtime_mind_request(RuntimePlayerMindRequest&, u16 kind, std::span<const u8>)
{
    logf("request %x", kind);
    g_request_storage.push_back(std::make_unique<std::array<u8, 0x40>>());
    auto request = std::make_unique<RuntimePlayerMindRequest>();
    request->bytes = *g_request_storage.back();
    g_requests.push_back(std::move(request));
    return g_requests.back().get();
}
u16 classify_runtime_attached_actor(std::span<const u8>, u16 tag)
{
    const u32 value = pop("classify");
    logf("classify %x", tag);
    return u16(value);
}
bool runtime_attached_actor_allows_range(const RuntimeBodyAttachedActor&)
{
    const u32 value = pop("range");
    logf("range");
    return value != 0;
}
bool load_runtime_mind_goal(const char* name, RuntimeMindGoalContext&)
{
    logf("goal %s", name);
    return true;
}
void remove_runtime_player_goal(RuntimePlayerGoalCleanup&)
{
    logf("remove");
    RuntimePlayerGoalRing*& ring = *g_ring;
    if (ring == nullptr)
    {
        return;
    }
    if (ring->tail == ring->capacity || ring->head == ring->tail)
    {
        ring = nullptr;
    }
    else
    {
        ring->head = u16((ring->head + 1) % ring->capacity);
    }
}
void activate_actor_bind_tree(Actor&)
{
    logf("bind");
}
u8 runtime_player_strategy_decay_enabled(std::span<const u8>, const RuntimePlayerStrategyResume&)
{
    const u32 value = pop("decay");
    logf("decay 1");
    return u8(value);
}
bool runtime_mind_target_is_controlled_player(const RuntimeMindTarget& target)
{
    const u32 value = pop("controlled");
    logf("controlled %d", &target == g_target ? 1 : 0);
    return value != 0;
}
bool finish_runtime_player_goal(RuntimePlayerGoalCleanup&, EntityBitBlock*&, RuntimeBodyActionSelection&,
    const std::function<u16()>&, Game&, u32 success)
{
    logf("finish %d", success);
    return true;
}
void reset_runtime_input_device(std::span<u8> device, const RuntimeInputReleaseContext&)
{
    logf("input %x", device.data() == g_device->data() ? 0x7000 : 0);
}
void change_runtime_player_strategy_state(FactoryObject&, u16 state, RuntimePlayerStrategySetup&)
{
    logf("state %x", state);
}
bool runtime_mind_target_occupancy_differs(const RuntimeMindTarget& target, u16 owner_class, const OccupancyRecord* allowed)
{
    const u32 value = pop("differs");
    logf("differs %d %x %x", &target == g_target ? 1 : 0, owner_class, allowed_offset(allowed));
    return value != 0;
}
bool runtime_player_mind_saved_path_clear(const Actor&, std::span<const u8>, const OccupancyRecord*,
    const OccupancyRecord*, u16)
{
    const u32 value = pop("saved");
    logf("saved");
    return value != 0;
}
u8 runtime_player_can_resume_strategy(std::span<const u8>, const RuntimePlayerStrategyResume&)
{
    const u32 value = pop("resume");
    logf("resume 1");
    return u8(value);
}
u16 script_actor_current_mask(const Actor& actor, const RuntimeScriptActorQueryContext&)
{
    const u32 value = pop("mask");
    logf("mask %d", &actor == g_player ? 1 : 0);
    return u16(value);
}
void drain_runtime_player_goals(RuntimePlayerGoalCleanup&)
{
    logf("drain");
    *g_ring = nullptr;
}
void detach_runtime_strategy_link(RuntimeStrategyLink& link)
{
    logf("detach %d", &link == g_link ? 1 : 0);
}
bool runtime_actor_is_player(const Actor& actor)
{
    const u32 value = pop("player");
    const auto found = g_actor_names.find(&actor);
    logf("player %s", found == g_actor_names.end() ? "?" : found->second);
    return value != 0;
}
bool runtime_goal_requires_latches(u16 kind)
{
    const u32 value = pop("latchkind");
    logf("latchkind %x", kind);
    return value != 0;
}
void* engine_heap_allocate(i32 size)
{
    logf("alloc");
    return std::calloc(1, size_t(size));
}
bool actor_tick_timer(const Actor&, float& timer, float, float, float)
{
    const u32 value = pop("timer");
    logf("timer %08x", bits_of(timer));
    timer = value != 0 ? 0.0f : 0.5f;
    return false;
}
namespace entity_detail
{
bool select_navigation_spawn_position(i32 floor, const Vec3*, const Vec3& center, float minimum, float maximum,
    const OccupancyRecord*, const OccupancyRecord* allowed, u8 flags, i16 group, Vec3& output, u32&, float)
{
    const u32 value = pop("spawn");
    logf("spawn %x %08x %08x %08x %x %08x %08x %08x %x", flags, bits_of(center.x), bits_of(center.y),
        bits_of(center.z), floor, bits_of(minimum), bits_of(maximum), allowed_offset(allowed), u16(group));
    if (value != 0)
    {
        output = Vec3{11.0f, 12.0f, 13.0f};
    }
    return value != 0;
}
bool navigation_path_clear(const Vec3& source, const Vec3& destination, i32 region_mode, i32 allow_tagged,
    i32 query_occupancy, i32 reject_marked, const OccupancyRecord* allowed, i32*, i16 group)
{
    const u32 value = pop("path");
    logf("path %08x %08x %08x %08x %08x %08x %d %d %d %d %x %x", bits_of(source.x), bits_of(source.y),
        bits_of(source.z), bits_of(destination.x), bits_of(destination.y), bits_of(destination.z), region_mode,
        allow_tagged, query_occupancy, reject_marked, allowed_offset(allowed), u16(group));
    return value != 0;
}
}
}

namespace
{
u32 next()
{
    u32 value;
    std::cin >> value;
    return value;
}
float next_float()
{
    const u32 bits = next();
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void latch_message(entity_detail::LatchNode&, const Message& message)
{
    logf("message %x %08x %08x %08x %08x %08x %08x %08x %08x", message.id, u32(message.arg_i),
        bits_of(message.hit_center.x), bits_of(message.hit_center.y), bits_of(message.hit_center.z), 0u,
        bits_of(message.hit_extent_y), 0u, bits_of(message.hit_radius));
}

void run_case()
{
    const u32 entry_kind = next();
    g_queues.clear();
    for (const char* name : kQueues)
    {
        for (int index = 0; index < 12; ++index)
        {
            g_queues[name].push_back(next());
        }
    }
    g_log.clear();
    g_requests.clear();
    g_request_storage.clear();
    g_new_events.clear();
    g_last_event = nullptr;

    // Strategy.
    FactoryObject strategy;
    strategy.bytes = std::make_unique<u8[]>(0x40);
    strategy.size = 0x40;
    std::span<u8> strategy_span{strategy.bytes.get(), 0x40};
    std::memset(strategy.bytes.get(), 0, 0x40);
    g_strategy = strategy_span;
    put_u16(strategy_span, 0x10, u16(next()));
    put_u16(strategy_span, 0x12, u16(next()));
    put_u16(strategy_span, 0x18, u16(next()));
    put(strategy_span, 0x34, next());
    // Actor.
    auto actor = std::make_unique<Actor>();
    g_player = actor.get();
    std::vector<u8> actor_bytes(0xD00);
    put_u16(actor_bytes, 6, u16(next()));
    actor->position = Vec3{next_float(), next_float(), next_float()};
    actor->euler = Vec3{next_float(), next_float(), next_float()};
    actor->radius_y = 1.5f;
    actor->radius_xz = 2.5f;
    actor->animation_rate = 1.0f;
    actor->animation_pause = 0.0f;
    ActorAnimationState animation{};
    if (next() != 0)
    {
        animation.controller = std::make_unique<AnimationController>();
    }
    put_u16(actor_bytes, 0x1FC, u16(next()));
    put(actor_bytes, 0x960, next()); // Input device token.
    put(actor_bytes, 0x984, next());
    actor_bytes[0x988] = u8(next());
    actor_bytes[0x989] = u8(next());
    actor_bytes[0x98A] = u8(next());
    const u32 volume_word = next();
    put(actor_bytes, 0x98C, volume_word);
    put_u16(actor_bytes, 0x990, u16(next()));
    put(actor_bytes, 0x994, next());
    const char* names[]{"aid_fx_a", "plain", "aid_fx_c"};
    const u32 name_count = next();
    for (u32 index = 0; index < name_count; ++index)
    {
        std::strcpy(reinterpret_cast<char*>(actor_bytes.data() + 0x998 + 0x80 * index), names[index]);
    }
    if (next() != 0)
    {
        std::strcpy(reinterpret_cast<char*>(actor_bytes.data() + 0xB18), "snd_win");
    }
    OccupancyRecord allowed_record{};
    OccupancyRecord current_record{};
    g_allowed = &allowed_record;
    const OccupancyRecord* allowed = &allowed_record;
    const OccupancyRecord* current = next() != 0 ? &current_record : nullptr;
    put(actor_bytes, 0xCAC, next());
    put(actor_bytes, 0xCB0, next());
    put(actor_bytes, 0xCB4, next());
    put(actor_bytes, 0xCB8, next());
    put_u16(actor_bytes, 0xCDE, u16(next()));
    put(actor_bytes, 0xCEC, next());
    // Goal ring.
    std::array<u8, 0x200> rows{}; // Retail ROWS has 0x200 zero bytes before BITS.
    RuntimePlayerGoalRing ring_storage{8, 4, 0, 0, rows};
    RuntimePlayerGoalRing* ring = nullptr;
    if (next() != 0)
    {
        ring_storage.head = u16(next());
        ring_storage.tail = u16(next());
        for (int index = 0; index < 4; ++index)
        {
            put_u16(rows, size_t(index) * 8, u16(next()));
        }
        ring = &ring_storage;
    }
    g_ring = &ring;
    std::array<u8, 4> address_zero{};
    put_u16(address_zero, 0, u16(next()));
    // Body.
    std::vector<u8> body(0x900);
    std::array<u8, 8> bit_bytes{};
    for (u8& value : bit_bytes)
    {
        value = u8(next());
    }
    EntityBitBlock bits{bit_bytes.data()};
    EntityBitBlock* bits_pointer = &bits;
    const bool attached_present = next() != 0;
    std::vector<u8> attached_bytes(0xA00);
    attached_bytes[0x93D] = u8(next());
    auto attached_actor = std::make_unique<Actor>();
    g_attached_actor = attached_actor.get();
    RuntimeBodyAttachedActor attached{attached_bytes, 0x10004000, nullptr, nullptr, attached_actor.get()};
    RuntimeBodyAttachedActor* attached_pointer = attached_present ? &attached : nullptr;
    body[0x6A] = u8(next());
    RuntimeScriptLookupTable table{0, nullptr};
    const RuntimeScriptLookupTable* table_pointer = next() != 0 ? &table : nullptr;
    put_u16(body, 0x748, u16(next()));
    const float scale_y = 2.0f;
    const float scale_xz = 3.0f;
    std::memcpy(body.data() + 0x884, &scale_y, 4);
    std::memcpy(body.data() + 0x888, &scale_xz, 4);
    // Mind.
    std::vector<u8> mind(0x100);
    g_mind = mind;
    put(mind, 0x10, next());
    RuntimePlayerMindRequest pending{};
    RuntimePlayerMindRequest* first_request = next() != 0 ? &pending : nullptr;
    put(mind, 0x64, next());
    put(mind, 0x68, next());
    put(mind, 0xC, next());
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
    // Target.
    std::vector<u8> target_bytes(0x40);
    put(target_bytes, 0x20, next());
    put(target_bytes, 0x24, next());
    put(target_bytes, 0x28, next());
    target_bytes[0x14] = u8(next());
    put(target_bytes, 0x10, next());
    auto target_actor = std::make_unique<Actor>();
    RuntimeScriptMindQuery target_query{};
    RuntimeMindTarget target{nullptr, target_bytes, kTarget, *target_actor, {}, target_query, nullptr};
    g_target = &target;
    // Goal volume.
    OccupancyRecord volume_record{};
    volume_record.kind = i16(next());
    g_volume = &volume_record;
    const OccupancyRecord* goal_volume = volume_word != 0 ? &volume_record : nullptr;
    // Latch listeners.
    const u32 latch_count = next();
    std::vector<entity_detail::LatchNode> latches(latch_count);
    for (u32 index = 0; index < latch_count; ++index)
    {
        latches[index].tag = u16(next());
        latches[index].handle_message = latch_message;
        latches[index].next = index + 1 < latch_count ? &latches[index + 1] : nullptr;
    }
    entity_detail::LatchNode* first_latch = latch_count != 0 ? &latches[0] : nullptr;
    const u32 runtime_flags = next();
    // Occupancy pool and script records.
    const u32 pool_present = next();
    const u32 pool_count = next();
    std::vector<OccupancyRecord> pool_records(std::max<u32>(pool_count, 1));
    for (u32 index = 0; index < pool_count; ++index)
    {
        const u16 id = u16(next());
        std::memcpy(reinterpret_cast<u8*>(&pool_records[index]) + 0xA, &id, 2);
    }
    g_pool_records = &pool_records;
    OccupancyPool pool{0x38, 4, pool_records.data(), pool_records.data() + pool_count,
        pool_records.data() + pool_count};
    g_occupancy_pool = pool_present != 0 ? &pool : nullptr;
    const u32 script_count = next();
    std::vector<u8> script(8 + 0xC * std::max<u32>(script_count, 1));
    put(script, 0, script_count);
    for (u32 index = 0; index < script_count; ++index)
    {
        const size_t row = 8 + 0xC * index;
        put(script, row, 0xC);
        put_u16(script, row + 6, u16(next()));
        put_u16(script, row + 8, u16(next()));
    }
    const void* script_records = script.data();
    g_last_actor_effect = i32(next());
    std::array<u8, 0x40> effect_bytes{};
    ActorEffectNode effect_node{effect_bytes};
    g_actor_effect_nodes[2] = &effect_node;
    // Message actors, targets and the carried object.
    std::vector<u8> target2_bytes(0x40);
    put(target2_bytes, 0x10, 0x5678);
    RuntimeMindTarget target2{nullptr, target2_bytes, kTarget2, *target_actor, {}, target_query, nullptr};
    std::array<std::vector<u8>, 2> other_bytes{std::vector<u8>(0xA00), std::vector<u8>(0xA00)};
    std::array<std::unique_ptr<Actor>, 2> other_actors{std::make_unique<Actor>(), std::make_unique<Actor>()};
    std::array<RuntimeStrategyMessageActor, 3> actor_views{};
    actor_views[0] = RuntimeStrategyMessageActor{kActor, actor.get(), actor_bytes, nullptr, nullptr};
    u32 holder_codes[2];
    for (int index = 0; index < 2; ++index)
    {
        put_u16(other_bytes[index], 0x1FC, u16(next()));
        put_u16(other_bytes[index], 0x1FE, u16(next()));
        const u32 target_code = next();
        holder_codes[index] = next();
        actor_views[index + 1] = RuntimeStrategyMessageActor{index == 0 ? kOther1 : kOther2, other_actors[index].get(),
            other_bytes[index], target_code == 1 ? &target : target_code == 2 ? &target2 : nullptr, nullptr};
    }
    for (int index = 0; index < 2; ++index)
    {
        actor_views[index + 1].holder = holder_codes[index] == 0 ? nullptr : &actor_views[holder_codes[index]];
    }
    g_actor_names = {{actor.get(), "actor"}, {other_actors[0].get(), "other1"}, {other_actors[1].get(), "other2"}};
    target_bytes[0x18] = u8(next());
    target2_bytes[0x18] = u8(next());
    target2_bytes[0x14] = u8(next());
    const u32 carried_code = next();
    std::array<u8, 0x200> carried_bytes{};
    carried_bytes[0x199] = carried_code == 2 ? 1 : 0;
    const u8* carried = carried_code != 0 ? carried_bytes.data() : nullptr;
    strategy_span[0x1C] = u8(next());
    put_u16(strategy_span, 0xC, u16(next()));
    put_u16(strategy_span, 0x14, u16(next()));
    put_u16(strategy_span, 0x16, u16(next()));
    put_u16(actor_bytes, 0xCDC, u16(next()));
    put(actor_bytes, 0x93C, next());
    actor_bytes[0x969] = u8(next());
    put(actor_bytes, 0x978, next());
    put_u16(actor_bytes, 0x97C, u16(next()));
    put_u16(actor_bytes, 0x97E, u16(next()));
    u32 references[2];
    references[0] = next();
    references[1] = next();
    std::array<u8, 4> resume_table{};
    resume_table[0] = u8(next());
    std::array<u32, 9> message{};
    if (entry_kind == 9)
    {
        for (u32 index = 0; index < 9; ++index)
        {
            u32 word = next();
            if (index == 1 || index == 2)
            {
                const u32 addresses[]{0, kActor, kOther1, kOther2};
                word = word < 4 ? addresses[word] : word - 4;
            }
            message[index] = word;
        }
    }
    const u32 argument = next();

    // Context.
    static float freeze = 0.0f;
    static float dt = 0.25f;
    static float scale = 2.0f;
    static u32 block_decay = 0;
    static u32 decay_index = 0;
    static std::array<u8, 4> decay_enabled{};
    const RuntimeActorClock clock{freeze, dt, scale, block_decay, decay_enabled, decay_index};
    std::array<u8, 0x100> device{};
    g_device = &device;
    std::unordered_map<u32, std::span<u8>> release_map;
    RuntimeInputReleaseContext release{release_map};
    RuntimeMindEvents mind_events{anchor, mind, {}, nullptr};
    RuntimePlayerMindRequest request_anchor{};
    std::string scratch;
    u32 random = 1;
    // Only referenced: the stubs replace every registry user.
    alignas(64) static u8 registry_storage[4096];
    AnimationResourceRegistry& registry = *reinterpret_cast<AnimationResourceRegistry*>(registry_storage);
    ResourceErrorState error{};
    RuntimeMindGoalContext goals{nullptr, request_anchor, {}, nullptr, scratch, random, registry, error};
    std::span<u8> body_span{body};
    RuntimeMindTarget* targets = nullptr;
    RuntimePlayerStrategyEntry entry{strategy_span, *actor, actor_bytes, body_span, mind_events, goals, ring,
        address_zero, allowed, targets, "actionenableddelay", "celebrate", "coward", "worried", "taunt"};
    ObjectFactoryRegistry factory;
    RuntimePlayerChildContext child{factory, {}, kActor};
    RuntimePlayerMindRequests mind_requests{first_request, nullptr};
    RuntimePlayerMindRequests* mind_pointer = &mind_requests;
    RuntimePlayerStrategySetup setup{child, mind_pointer,
        [](FactoryObject&, u16 from, u16 to)
        {
            logf("leaving %x %x", from, to);
        },
        [](FactoryObject&, u16 to, u16 from)
        {
            logf("entering %x %x", to, from);
        }};
    u32 override_words[8]{};
    RuntimePlayerScriptOverrides overrides{override_words[0], override_words[1], override_words[2], override_words[3],
        override_words[4], override_words[5], override_words[6], override_words[7]};
    std::vector<u8> self_bytes(0x10);
    RuntimeBodyAttachedActor self{actor_bytes, kActor, [](std::span<const u32> notice)
        {
            logf("notice %x", notice[0]);
        },
        nullptr, actor.get()};
    RuntimePlayerGoalCleanup cleanup{self, ring, body_span, *actor, first_latch, address_zero, overrides};
    const u32 resume_index = 0;
    RuntimePlayerStrategyResume resume{std::span<const u32, 2>(references), resume_table, resume_index};
    RuntimeBodyAnimationReceiver* first_receiver = nullptr;
    RuntimeBodyAnimationReceiver* second_receiver = nullptr;
    FactoryObject first_action;
    FactoryObject second_action;
    const u8* action_row = nullptr;
    const std::unordered_map<u32, const char*> action_names;
    u32 action_random = 1;
    RuntimeBodyActionSelection actions{animation, attached_pointer, first_receiver, second_receiver, first_action,
        second_action, action_row, table_pointer, action_names, child, registry, error, action_random, 0x10002000};
    alignas(16) static u8 game_storage[16];
    Game& game = *reinterpret_cast<Game*>(game_storage);
    RuntimeBodyAnimationReceiver receiver;
    ModelAnimationState model{};
    ModelNodeFlagCache cache{};
    const AnimationMatrix reset{};
    RuntimePlayerAttachmentActor pose{self, *actor, receiver, animation, model, 0, nullptr, cache, reset};
    ActorAnimationState attached_animation{};
    RuntimePlayerAttachmentActor attached_pose{attached, *attached_actor, receiver, attached_animation, model, 0,
        nullptr, cache, reset};
    std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attached_poses{
        {&attached, &attached_pose}};
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
    const OccupancyRecord* room = nullptr;
    RuntimePlayerStrategyGoals context{entry, strategy, setup, cleanup, resume, bits_pointer, actions,
        []()
        {
            return u16(0);
        },
        game,
        [](u32 slot)
        {
            const u32 key = pop("key");
            logf("key %x %x", slot, key);
            return u16(key);
        },
        [](u32 address) -> RuntimeMindTarget*
        {
            return address == kTarget ? g_target : nullptr;
        },
        [](const void* record) -> u32
        {
            if (record == g_volume)
            {
                return kRecords;
            }
            const auto* begin = g_pool_records->data();
            return kRecords + 0x38 * u32(static_cast<const OccupancyRecord*>(record) - begin + 1);
        },
        pose, attached_poses, effects, device, release, room, current, goal_volume, script_records, runtime_flags,
        random, clock, "ghouly_intro", "drone_waitforscare"};

    std::string out;
    char buffer[256];
    switch (entry_kind)
    {
    case 0:
        put_u16(strategy_span, 0xE, u16(argument));
        update_runtime_base_strategy(context);
        out = "-";
        break;
    case 1:
    {
        u32 output = 0;
        const u32 result = select_runtime_goal_kind_action(u16(argument), output, context);
        std::snprintf(buffer, sizeof(buffer), "%x %x", result, output);
        out = buffer;
        break;
    }
    case 2:
    {
        u32 output = 0;
        const u32 result = select_runtime_player_goal_action(output, context);
        std::snprintf(buffer, sizeof(buffer), "%x %x", result, output);
        out = buffer;
        break;
    }
    case 3:
    {
        const OccupancyRecord* found = find_runtime_scripted_occupancy(i16(argument), script_records);
        out = std::to_string(found == nullptr ? -1 : int(found - pool_records.data()) + 1);
        break;
    }
    case 4:
        play_runtime_player_goal_effects(context);
        out = "-";
        break;
    case 5:
        std::snprintf(buffer, sizeof(buffer), "%x", update_runtime_player_goal_state(context));
        out = buffer;
        break;
    case 6:
        std::snprintf(buffer, sizeof(buffer), "%x", complete_runtime_player_goal_step(context));
        out = buffer;
        break;
    case 7:
        std::snprintf(buffer, sizeof(buffer), "%x", begin_runtime_player_goal(context));
        out = buffer;
        break;
    case 8:
        std::snprintf(buffer, sizeof(buffer), "%x", select_runtime_carry_action(context));
        out = buffer;
        break;
    default:
    {
        std::vector<u8> query_zero(4);
        std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> query_views;
        RuntimeScriptActorQueryContext query{nullptr, query_views, query_zero.data()};
        RuntimeStrategyLink link{};
        g_link = &link;
        static RuntimeStrategyMessageActor unknown{};
        RuntimeBaseStrategyMessages messages{context, query, link,
            [&actor_views](u32 address) -> RuntimeStrategyMessageActor*
            {
                for (auto& view : actor_views)
                {
                    if (view.address == address)
                    {
                        return &view;
                    }
                }
                static std::array<u8, 0x200> zero{};
                unknown = RuntimeStrategyMessageActor{address, nullptr, zero, nullptr, nullptr};
                return &unknown;
            },
            carried};
        dispatch_runtime_base_strategy_message(message, messages);
        out = "-";
        break;
    }
    }
    // Snapshot.
    std::vector<u32> ring_words{ring != nullptr ? 1u : 0u};
    if (ring != nullptr)
    {
        ring_words.insert(ring_words.end(), {ring->stride, ring->capacity, ring->head, ring->tail});
        if (ring->tail != ring->capacity)
        {
            const size_t row = size_t(ring->stride) * ring->tail;
            ring_words.push_back(read_u16(ring->rows, row));
            for (size_t offset = 4; offset < 0x20; offset += 4)
            {
                ring_words.push_back(read_u32(ring->rows, row + offset));
            }
        }
    }
    out += " |";
    for (const u32 word : ring_words)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", word);
        out += buffer;
    }
    const u32 words[]{read_u32(actor_bytes, 0x984), read_u32(actor_bytes, 0x98C),
        read_u32(actor_bytes, 0x994), read_u32(actor_bytes, 0xCA0), read_u32(actor_bytes, 0xCAC),
        read_u32(actor_bytes, 0xCB0), read_u32(actor_bytes, 0xCB4), read_u32(actor_bytes, 0xCB8),
        actor_bytes[0x988], actor_bytes[0x989], actor_bytes[0x98A], read_u16(strategy_span, 0xE),
        read_u32(strategy_span, 0x34), read_u32(mind, 0x10), read_u32(mind, 0xC), read_u32(mind, 0x70),
        read_u32(mind, 0x74), read_u32(mind, 0x78), u32(g_last_actor_effect), effect_bytes[0x18],
        animation.controller != nullptr ? 1u : 0u};
    for (const u32 word : words)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", word);
        out += buffer;
    }
    out += " R";
    for (const auto& request : g_requests)
    {
        std::string text;
        for (size_t offset = 0x10; offset < 0x2C; offset += 4)
        {
            std::snprintf(buffer, sizeof(buffer), offset == 0x10 ? "%08x" : "|%08x", read_u32(request->bytes, offset));
            text += buffer;
        }
        out += " " + text;
    }
    out += " E";
    if (g_last_event != nullptr)
    {
        std::snprintf(buffer, sizeof(buffer), " %08x", read_u32(g_last_event->bytes, 0x20));
        out += buffer;
    }
    out += " |";
    for (size_t index = 0; index < g_log.size(); ++index)
    {
        out += (index == 0 ? " " : " ; ") + g_log[index];
    }
    std::printf("%s\n", out.c_str());
    std::fflush(stdout);
    g_actor_effect_nodes[2] = nullptr;
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
