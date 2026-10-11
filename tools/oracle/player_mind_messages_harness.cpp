// Host side of player_mind_messages_oracle.py. player_mind_messages.cpp and camera_pulse.cpp are
// linked alone; every other callee is a logging stub consuming the retail hooks' scripted queues.
#include "entities/player_mind_messages.h"
#include "entities/actor_transform.h"
#include "scripts/script_conditions.h"
#include "scripts/script_vm.h"
#include "gbtg/game.h"
#include <bit>
#include <cstdarg>
#include <cstdio>
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
constexpr u32 kHeap = 0x10000000;
constexpr u32 kMind = kHeap + 0x1000;
constexpr u32 kActor = kHeap + 0x2000;
constexpr u32 kOther = kHeap + 0x5000;
constexpr u32 kSource = kHeap + 0x6000;
constexpr u32 kNames = kHeap + 0x8000;
constexpr u32 kPosition[3] = {0x3FC00000, 0xC0100000, 0x40400000};
constexpr u32 kEuler[3] = {0x3E000000, 0xBF000000, 0x40000000};
constexpr u32 kDuration = 0x40300000;
const char* const kQueues[] = {"special", "resume", "active", "sceneend"};
const char* const kNameText[] = {"aid_camerashake_hit", "aidx", "xyz"};

std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;
std::span<u8> g_mind;
std::span<u8> g_body;
std::span<u8> g_actor_bytes;
Actor* g_actor = nullptr;
EntityBitBlock* g_flags = nullptr;
const std::span<const u8>* g_action_target = nullptr;

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

int is_mind(std::span<const u8> mind)
{
    return mind.data() == g_mind.data() ? 1 : 0;
}

template <typename T>
T& junk()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}
}

namespace gbtg
{
void dispatch_runtime_parent_mind_message(RuntimeMindEvents& context, std::span<const u32> message,
    std::span<const u8>)
{
    logf("base %x %d", message[0], is_mind(context.mind));
}

void register_runtime_controlled_player(RuntimePlayerInputBinding&)
{
    logf("register 1");
}

void reset_runtime_player_mind_body_event(RuntimePlayerMindFeedback& context, std::span<const u8>,
    const std::span<const u8>*)
{
    logf("reset %d", is_mind(context.mind));
}

void notify_runtime_player_mind_impact(RuntimePlayerMindFeedback& context)
{
    logf("impact %d", is_mind(context.mind));
}

void notify_runtime_player_mind_release(RuntimePlayerMindFeedback& context)
{
    logf("release %d", is_mind(context.mind));
}

void notify_runtime_player_mind_activation(RuntimePlayerMindFeedback& context)
{
    logf("activation %d", is_mind(context.mind));
}

void notify_runtime_player_mind_event(RuntimePlayerMindFeedback& context, i32 kind)
{
    logf("event %d %d", kind, is_mind(context.mind));
}

void script_queue_event(Game&, std::span<const u8> message)
{
    u32 words[4];
    std::memcpy(words, message.data(), 16);
    logf("queue %x %x %x %x", words[0], words[1], words[2], words[3]);
}

void notify_runtime_player_mind_attachment_callout(RuntimeParentMindRequests&, const std::span<const u8>* action_target,
    u32 sender)
{
    logf("callout %x %d", sender, action_target == g_action_target ? 1 : 0);
}

bool notify_runtime_player_turn(RuntimeMindBoost&, RuntimePlayerMindFeedback& feedback, const Vec3& point)
{
    logf("turn %x %x %x %d", bits(point.x), bits(point.y), bits(point.z), is_mind(feedback.mind));
    return false;
}

void record_runtime_player_mind_distance(std::span<u8> mind, float distance)
{
    logf("distance %x %d", bits(distance), is_mind(mind));
}

void track_runtime_player_mind_source(std::span<u8> mind, const std::span<const u8>*, u32 present, u32 source)
{
    logf("track %x %x %d", present, source, is_mind(mind));
}

void shake_runtime_player_mind_camera(RuntimePlayerMindFeedback& context, const char* name)
{
    logf("shake %s %d", name, is_mind(context.mind));
}

bool notify_runtime_player_boost(RuntimeMindBoost&, RuntimePlayerMindFeedback& feedback)
{
    logf("boost %d", is_mind(feedback.mind));
    return false;
}

void notify_runtime_player_mind_contact(RuntimePlayerMindFeedback& context, u32 payload, u32 target)
{
    logf("contact %x %x %d", payload, target, is_mind(context.mind));
}

void notify_runtime_player_mind_source(RuntimePlayerMindFeedback& context, std::span<const u8> message)
{
    logf("source %x %d", read_u32(message, 0), is_mind(context.mind));
}

void notify_runtime_player_mind_target(RuntimePlayerMindFeedback& context, std::span<const u8> message)
{
    logf("target %x %d", read_u32(message, 0), is_mind(context.mind));
}

void clear_runtime_player_mind_body_event(std::span<u8> body)
{
    logf("clearbody %d", body.data() == g_body.data() ? 1 : 0);
}

bool clear_runtime_player_mind_reference(std::span<u8>, size_t offset, u32 actor)
{
    logf("reference %x %x", u32(offset), actor);
    return false;
}

bool initialize_runtime_player_mind_navigation(std::span<u8> mind, i32 floor, std::span<entity_detail::NavGrid* const>,
    u8*, RuntimeMindStorage*&, const std::function<u32(const void*)>&)
{
    logf("navigation %d %x", is_mind(mind), u32(floor));
    return false;
}

Vec3 actor_world_position(const Actor& actor)
{
    logf("position %d", &actor == g_actor ? 1 : 0);
    return {std::bit_cast<float>(kPosition[0]), std::bit_cast<float>(kPosition[1]), std::bit_cast<float>(kPosition[2])};
}

Vec3 actor_world_euler(const Actor& actor)
{
    logf("euler %d", &actor == g_actor ? 1 : 0);
    return {std::bit_cast<float>(kEuler[0]), std::bit_cast<float>(kEuler[1]), std::bit_cast<float>(kEuler[2])};
}

float runtime_actor_animation_duration(const ActorAnimationState& animation, u32 slot)
{
    logf("duration %d %d", &animation == &junk<ActorAnimationState>() ? 1 : 0, slot);
    return std::bit_cast<float>(kDuration);
}

bool runtime_player_mind_uses_special_event(u32 kind)
{
    logf("special %x", kind);
    return pop("special") != 0;
}

u8 runtime_player_can_resume_strategy(std::span<const u8> actor, const RuntimePlayerStrategyResume&)
{
    logf("resume %d", actor.data() == g_actor_bytes.data() ? 1 : 0);
    return u8(pop("resume"));
}

u8 runtime_player_mind_active(const std::span<const u8>* mind)
{
    logf("active %d", mind != nullptr && mind->data() == g_mind.data() ? 1 : 0);
    return u8(pop("active"));
}

bool scene_end_flag_set()
{
    logf("sceneend");
    return pop("sceneend") != 0;
}

void record_runtime_player_mind_position(std::span<u8> mind, const Actor& actor, u32 source, const Vec3& position)
{
    logf("record %x %x %x %x %d", source, bits(position.x), bits(position.y), bits(position.z),
        is_mind(mind) && &actor == g_actor ? 1 : 0);
}

void assign_runtime_body_bit(EntityBitBlock& flags, u32 index, u32 value)
{
    logf("bit %x %d %d", index, value, &flags == g_flags ? 1 : 0);
}
}

int main()
{
    int count;
    std::cin >> count;
    for (int test = 0; test < count; ++test)
    {
        std::vector<u32> words;
        for (int index = 0; index < 16 + 12 + 4 + 6; ++index)
        {
            u32 value;
            std::cin >> value;
            words.push_back(value);
        }
        g_queues.clear();
        g_log.clear();
        size_t at = 0;
        for (const char* queue : kQueues)
        {
            for (int index = 0; index < 4; ++index)
            {
                g_queues[queue].push_back(words[at++]);
            }
        }
        std::vector<u32> message(words.begin() + at, words.begin() + at + 12);
        at += 12;
        std::array<u8, 0x200> mind{};
        std::memcpy(mind.data() + 8, &kActor, 4);
        std::memcpy(mind.data() + 0x50, &words[at], 4);
        std::memcpy(mind.data() + 0x7C, &words[at + 1], 4);
        std::memcpy(mind.data() + 0x88, &words[at + 2], 4);
        std::memcpy(mind.data() + 0x150, &words[at + 3], 4);
        const u32 source_word = words[at + 2];
        at += 4;
        const u32 health = words[at];
        const u32 floor = words[at + 1];
        const u16 other_key = u16(words[at + 2]);
        const u32 source_key = words[at + 3];
        const i32 chapter = i32(words[at + 4]);
        const u32 flags_word = words[at + 5];

        std::array<u8, 0x100> body{};
        const u16 health16 = u16(health);
        std::memcpy(body.data() + 0x94, &health16, 2);
        std::array<u8, 0xD00> actor_bytes{};
        std::memcpy(actor_bytes.data() + 0xCEC, &floor, 4);
        std::array<u8, 0x20> source{};
        std::memcpy(source.data() + 0x10, &source_key, 4);
        std::array<u8, 0x20> zeros{};
        auto actor = std::make_unique<Actor>();
        auto other = std::make_unique<Actor>();
        other->class_key = other_key;
        EntityBitBlock flags{};
        g_mind = mind;
        g_body = body;
        g_actor_bytes = actor_bytes;
        g_actor = actor.get();
        g_flags = &flags;
        const std::span<const u8> source_view = source_word == kSource ? std::span<const u8>(source)
                                                                      : std::span<const u8>(zeros);
        const std::span<const u8> self_mind(mind);
        const std::span<const u8> action_target(zeros);
        g_action_target = &action_target;
        RuntimeMindEvents events{junk<LiveScript>(), mind, {}, [](std::span<const u32> goal)
        {
            std::string text = "actormsg 1";
            for (size_t index = 0; index < 9; ++index)
            {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), " %x", goal[index]);
                text += buffer;
            }
            g_log.push_back(text);
        }};
        RuntimePlayerMindFeedback feedback{mind, body, *actor, {}, 0, junk<RuntimeInputActivationContext>(),
            junk<RuntimeBodyActionMessages>(), {}, junk<AnimationResourceRegistry>(), junk<ResourceErrorState>()};
        RuntimeMindStorage* storage = nullptr;
        RuntimeCameraPulse pulse;
        std::memset(&pulse, 0xA5, sizeof(pulse));
        RuntimePlayerMindMessages context{events, feedback, junk<RuntimeMindBoost>(),
            junk<RuntimeParentMindRequests>(), junk<RuntimePlayerInputBinding>(), junk<RuntimePlayerStrategyResume>(),
            junk<Game>(), kActor, actor_bytes, flags, {}, &action_target, &source_view, &self_mind, nullptr, {},
            nullptr, storage, junk<ActorAnimationState>(), chapter, flags_word, pulse, [](const void*) { return 0u; },
            [](u32 address) -> const char*
            {
                if (address >= kNames && address < kNames + 0xC0 && (address - kNames) % 0x40 == 0)
                {
                    return kNameText[(address - kNames) / 0x40];
                }
                return nullptr;
            },
            [&](u32 address) -> const Actor* { return address == kOther ? other.get() : nullptr; }};
        dispatch_runtime_player_mind_message(context, message, {});
        const u32 state[] = {read_u32(mind, 0x50), read_u32(mind, 0x7C), read_u32(mind, 0x88), read_u32(mind, 0x150),
            bits(pulse.remaining), bits(pulse.duration), bits(pulse.first), bits(pulse.second), bits(pulse.third),
            bits(pulse.fourth), pulse.mode, pulse.active};
        std::string out;
        for (u32 value : state)
        {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%x ", value);
            out += buffer;
        }
        out += "|";
        for (size_t index = 0; index < g_log.size(); ++index)
        {
            out += (index == 0 ? " " : " ; ") + g_log[index];
        }
        std::cout << out << "\n";
    }
    return 0;
}
