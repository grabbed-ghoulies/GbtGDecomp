// Host side of player_strategy_oracle.py. player_strategy.cpp, brew_meter.cpp and dialogue_input.cpp
// are linked with this file first (/FORCE:MULTIPLE), so the stubs below replace the input reads that
// dialogue_input.cpp also defines; the three axis writers stay real on both sides.
#include "entities/player_strategy.h"
#include "frontend/frontend_slots.h"
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
constexpr u32 kChildren = kHeap + 0x5000;
const char* const kQueues[] = {"edge", "stick", "chapter", "idle", "timer", "resource", "nearest"};

std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;
std::vector<u32> g_words;
size_t g_at = 0;
const u8* g_strategy = nullptr;
const u8* g_device = nullptr;
const u8* g_body = nullptr;
const Actor* g_actor = nullptr;
const RuntimeDialogueInputDevice* g_input_device = nullptr;
const RuntimePlayerMindRequest* g_anchor = nullptr;
const RuntimeMindGoalContext* g_mind_goals = nullptr;
const RuntimeHudCounter* g_brew_counter = nullptr;
const u8* g_preset = nullptr;
const RuntimeDialogueWidget* g_widget = nullptr;
std::span<const u8> g_texture;
Actor* g_found = nullptr;
RuntimePlayerMindRequest g_request{};

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

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

template <typename T>
T& junk()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}

const char* short_name(const char* name)
{
    const std::pair<const char*, const char*> names[] = {
        {"aid_texture_ghoulies_scoreboard_energy_butlersbrew", "brew"},
        {"aid_texture_ghoulies_scoreboard_eventicons_cross", "cross"},
        {"aid_texture_ghoulies_scoreboard_eventicons_tick", "tick"}, {"idle", "idle"},
        {"XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", "right"}, {"XACT_SOUNDBANK_GSUPERSCARYSTUF_WRONGBUTTON", "wrong"}};
    for (const auto& [full, label] : names)
    {
        if (std::strcmp(name, full) == 0)
        {
            return label;
        }
    }
    return "?";
}
}

namespace gbtg
{
void change_runtime_player_strategy_state(FactoryObject& strategy, u16 state, RuntimePlayerStrategySetup&)
{
    logf("state %x %d", state, strategy.bytes.get() == g_strategy ? 1 : 0);
}

void drain_runtime_player_goals(RuntimePlayerGoalCleanup&)
{
    logf("drain");
}

void dispatch_runtime_base_strategy_message(std::span<const u32> message, RuntimeBaseStrategyMessages& context)
{
    logf("base %x %d", message[0], context.goals.entry.strategy.data() == g_strategy ? 1 : 0);
}

void update_runtime_base_strategy(RuntimePlayerStrategyGoals& context)
{
    logf("baseupdate %d", context.entry.strategy.data() == g_strategy ? 1 : 0);
}

void enter_runtime_base_strategy_state(RuntimePlayerStrategyEntry& context, u16 state)
{
    logf("baseenter %x %d", state, context.strategy.data() == g_strategy ? 1 : 0);
}

void reset_runtime_input_device(std::span<u8> device, const RuntimeInputReleaseContext&)
{
    logf("input %d", device.data() == g_device ? 1 : 0);
}

u32 runtime_input_button_edge(const RuntimeDialogueInputDevice& device, u16 mask, const RuntimeDialogueInputState&)
{
    const u32 value = pop("edge");
    logf("edge %x %d", mask, &device == g_input_device ? 1 : 0);
    return value;
}

void runtime_input_axes(const RuntimeDialogueInputDevice& device, std::array<float, 2>& output,
    const RuntimeDialogueInputState&)
{
    const u32 value = pop("stick");
    output = value == 1 ? std::array<float, 2>{0.2f, 0.1f}
        : value == 2 ? std::array<float, 2>{0.1f, 0.05f} : std::array<float, 2>{0.05f, 0.025f};
    logf("stick %d", &device == g_input_device ? 1 : 0);
}

void runtime_input_secondary_axes(const RuntimeDialogueInputDevice& device, std::array<float, 2>& output,
    const RuntimeDialogueInputState&)
{
    const u32 value = pop("stick");
    output = value == 1 ? std::array<float, 2>{0.2f, 0.1f}
        : value == 2 ? std::array<float, 2>{0.1f, 0.05f} : std::array<float, 2>{0.05f, 0.025f};
    logf("stick2 %d", &device == g_input_device ? 1 : 0);
}

namespace frontend_detail
{
int script_chapter_index(std::string_view script)
{
    logf("chapter %d", script == "scene" ? 1 : 0);
    return int(pop("chapter") * 9);
}
}

RuntimePlayerMindRequest* allocate_runtime_mind_request(RuntimePlayerMindRequest& anchor, u16 kind, std::span<const u8>)
{
    logf("request %x %d", kind, &anchor == g_anchor ? 1 : 0);
    return &g_request;
}

bool load_runtime_mind_goal(const char* name, RuntimeMindGoalContext& context)
{
    logf("goal %s %d", short_name(name), &context == g_mind_goals ? 1 : 0);
    return true;
}

void change_runtime_actor_pause(u8 mask, i32 release, RuntimeActorPauseGroups&)
{
    logf("pause %x %d", mask, release);
}

i16 request_spatial_audio_event_voice(const char* name, u32 stop, float distance, AudioEventVoices&)
{
    logf("sound %s %d %08x", short_name(name), stop, bits(distance));
    return 0;
}

void dispatch_actor_resource_callout(const RuntimeActorResourceReference&, i32 id, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext&)
{
    logf("callout %x %d 1", id, options != nullptr ? 1 : 0);
}

void set_runtime_player_health_display(RuntimePlayerHealthDisplay&, i32 health, u32 mode)
{
    logf("health %d %d", health, mode);
}

void acquire_runtime_player_health_textures(RuntimePlayerHealthDisplay&)
{
    logf("hearts");
}

void update_runtime_hud_counter_digits(RuntimeHudCounter& counter, std::span<const u8> preset)
{
    logf("digits %d %d %d %d", counter.value, counter.target, &counter == g_brew_counter ? 1 : 0,
        preset.data() == g_preset ? 1 : 0);
}

void set_dialogue_widget_mode(RuntimeDialogueWidget& widget, i32 mode, RuntimeDialogueWidgetContext&)
{
    logf("widget %d %d", mode, &widget == g_widget ? 1 : 0);
}

std::span<const u8> acquire_animation_resource(AnimationResourceRegistry&, const char* name)
{
    const u32 value = pop("resource");
    logf("resource %s", short_name(name));
    return value != 0 ? g_texture : std::span<const u8>();
}

void resource_error(ResourceErrorState&, int code)
{
    logf("error %d", code);
}

bool runtime_body_timer_is_idle(std::span<const u8> body)
{
    const u32 value = pop("idle");
    logf("idle %d", body.data() == g_body ? 1 : 0);
    return value != 0;
}

bool actor_tick_timer(const Actor& actor, float& timer, float, float, float)
{
    const u32 value = pop("timer");
    logf("timer %08x %d", bits(timer), &actor == g_actor ? 1 : 0);
    return value != 0;
}

Actor* find_nearest_runtime_script_actor(const Vec3* point, Actor& requester, i16 tag, float radius, float* distance,
    u32 flags, RuntimeScriptActorPredicate predicate, const RuntimeScriptActorQueryContext&)
{
    const u32 value = pop("nearest");
    logf("nearest %x %d %x %08x %d %x %x", point != nullptr ? 1 : 0, &requester == g_actor ? 1 : 0, u16(tag),
        bits(radius), distance != nullptr ? 1 : 0, flags, predicate != nullptr ? 1 : 0);
    return value != 0 ? g_found : nullptr;
}
}

namespace
{
void run_case()
{
    g_queues.clear();
    g_log.clear();
    const u32 entry_kind = next();
    for (const char* name : kQueues)
    {
        for (int index = 0; index < 12; ++index)
        {
            g_queues[name].push_back(next());
        }
    }
    // Strategy.
    FactoryObject strategy_object;
    strategy_object.bytes = std::make_unique<u8[]>(0x60);
    strategy_object.size = 0x60;
    std::span<u8> strategy{strategy_object.bytes.get(), 0x60};
    std::memset(strategy.data(), 0, strategy.size());
    g_strategy = strategy.data();
    put_u16(strategy, 0xE, u16(next()));
    for (size_t offset : {0x12, 0x14, 0x16, 0x18})
    {
        put_u16(strategy, offset, u16(next()));
    }
    strategy[0x1C] = u8(next());
    for (size_t offset : {0x38, 0x3C, 0x40, 0x48, 0x4C, 0x50})
    {
        put(strategy, offset, next());
    }
    // Actor, body and mind.
    auto actor = std::make_unique<Actor>();
    auto found = std::make_unique<Actor>();
    g_actor = actor.get();
    g_found = found.get();
    actor->animation_pause = next_float();
    actor->animation_rate = next_float();
    std::vector<u8> actor_bytes(0xD00);
    std::array<u8, 8> bit_bytes{};
    for (u8& value : bit_bytes)
    {
        value = u8(next());
    }
    EntityBitBlock bit_block{bit_bytes.data()};
    EntityBitBlock* body_bits = &bit_block;
    std::vector<u8> body(0x200);
    g_body = body.data();
    put_u16(body, 0x30, u16(next()));
    put_u16(body, 0x94, u16(next()));
    put_u16(body, 0x96, u16(next()));
    RuntimePlayerMindRequest pending{};
    RuntimePlayerMindRequest* first_request = next() != 0 ? &pending : nullptr;
    // Input device and children.
    std::array<std::array<u8, 0x100>, 4> child_records{};
    std::array<u8, 0x100> device_record{};
    g_device = device_record.data();
    put(device_record, 0, next());
    std::array<RuntimeInputAxisWriteDevice, 4> axis_children{};
    std::array<RuntimeDialogueInputDevice, 4> input_children{};
    for (size_t index = 0; index < 4; ++index)
    {
        put(device_record, 8 + 4 * index, kChildren + 0x100 * u32(index));
        put(child_records[index], 0, next());
        put(child_records[index], 8, next());
        axis_children[index] = RuntimeInputAxisWriteDevice{child_records[index], {}};
        input_children[index] = RuntimeDialogueInputDevice{child_records[index], {}};
    }
    RuntimeInputAxisWriteDevice axis_device{device_record,
        {&axis_children[0], &axis_children[1], &axis_children[2], &axis_children[3]}};
    RuntimeDialogueInputDevice input_device{device_record,
        {&input_children[0], &input_children[1], &input_children[2], &input_children[3]}};
    g_input_device = &input_device;
    std::array<u8, 0x100> keyboard{};
    // Globals.
    const i32 prompt_lock = i32(next());
    const u32 prompt_owner = next();
    const i32 difficulty = i32(next());
    const i32 chapter_mode = i32(next());
    const u8 unlock_flags = u8(next());
    const u8 scene_flags = u8(next());
    u8 story_flags = u8(next());
    const u32 runtime_flags = next();
    const i32 brews_owned = i32(next());
    const i32 brews_required = i32(next());
    u32 intro_pending = next();
    const u32 case_present = next();
    std::array<u8, 8> case_bytes{};
    case_bytes[3] = u8(next());
    EntityBitBlock case_bits{case_bytes.data()};
    entity_detail::ListHead case_node;
    entity_detail::ListHead cases;
    cases.next = case_present != 0 ? &case_node : nullptr;
    u32 pause_count = next();
    u32 brews_taken = next();
    u32 brew_accepted = 0;
    const u32 widget_present = next();
    RuntimeDialogueWidget widget_storage{};
    static const u8 sentinel = 0;
    widget_storage.requested_set = &sentinel;
    RuntimeDialogueWidget* widget = widget_present != 0 ? &widget_storage : nullptr;
    g_widget = &widget_storage;
    std::array<u8, 0x40> texture{};
    std::array<std::span<const u8>, 3> textures{};
    for (auto& slot : textures)
    {
        slot = next() != 0 ? std::span<const u8>(texture) : std::span<const u8>();
    }
    put_u16(texture, 8, u16(next()));
    g_texture = texture;
    const float freeze = next_float();
    const float dt = next_float();
    const float scale = next_float();
    // Script records.
    const u32 row_count = next();
    std::vector<u8> script(8 + 0x24 * std::max<u32>(row_count, 1));
    put(script, 0, row_count);
    for (u32 index = 0; index < row_count; ++index)
    {
        const size_t row = 8 + 0x24 * index;
        put(script, row, 0x24);
        put_u16(script, row + 6, u16(next()));
        put_u16(script, row + 8, u16(next()));
        for (size_t value = 0; value < 6; ++value)
        {
            put(script, row + 0xC + 4 * value, next());
        }
    }
    const void* script_records = script.data();

    // Contexts.
    static const u32 block_decay = 0;
    static const u32 decay_index = 0;
    static const std::array<u8, 4> decay_enabled{};
    const RuntimeActorClock clock{freeze, dt, scale, block_decay, decay_enabled, decay_index};
    std::array<u8, 0x40> address_zero{};
    RuntimePlayerMindRequest anchor{};
    g_anchor = &anchor;
    std::string scratch;
    u32 random = 1;
    RuntimeMindGoalContext mind_goals{nullptr, anchor, {}, nullptr, scratch, random, junk<AnimationResourceRegistry>(),
        junk<ResourceErrorState>()};
    g_mind_goals = &mind_goals;
    RuntimePlayerGoalRing* ring = nullptr;
    const OccupancyRecord* allowed = nullptr;
    RuntimeMindTarget* targets = nullptr;
    std::span<u8> body_span{body};
    RuntimePlayerStrategyEntry entry{strategy, *actor, actor_bytes, body_span, junk<RuntimeMindEvents>(), mind_goals,
        ring, address_zero, allowed, targets, "a", "b", "c", "d", "e"};
    RuntimePlayerMindRequests mind_requests{first_request, nullptr};
    RuntimePlayerMindRequests* mind_pointer = &mind_requests;
    RuntimePlayerStrategySetup setup{junk<RuntimePlayerChildContext>(), mind_pointer, nullptr, nullptr};
    RuntimeBodyAttachedActor self{actor_bytes, kHeap, [](std::span<const u32> message)
        {
            std::string text = "actormsg 1";
            for (size_t index = 0; index < 9; ++index)
            {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), " %x", message[index]);
                text += buffer;
            }
            g_log.push_back(text);
        },
        nullptr, actor.get()};
    entity_detail::LatchNode* first_latch = nullptr;
    RuntimePlayerGoalCleanup cleanup{self, ring, body_span, *actor, first_latch, address_zero,
        junk<RuntimePlayerScriptOverrides>()};
    const OccupancyRecord* room = nullptr;
    const OccupancyRecord* current = nullptr;
    const OccupancyRecord* goal_volume = nullptr;
    const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attached_poses;
    RuntimePlayerStrategyGoals goals{entry, strategy_object, setup, cleanup, junk<RuntimePlayerStrategyResume>(),
        body_bits, junk<RuntimeBodyActionSelection>(), nullptr, junk<Game>(), nullptr, nullptr, nullptr,
        junk<RuntimePlayerAttachmentActor>(), attached_poses, junk<ActorEffectStreamContext>(), device_record,
        junk<RuntimeInputReleaseContext>(), room, current, goal_volume, script_records, runtime_flags, random, clock,
        "intro", "wait"};
    const u8* carried = nullptr;
    RuntimeBaseStrategyMessages base{goals, junk<RuntimeScriptActorQueryContext>(), junk<RuntimeStrategyLink>(), nullptr,
        carried};
    u32 brew_state = 0;
    float brew_intensity = 0.0f;
    RuntimeHudCounter brew_counter{};
    g_brew_counter = &brew_counter;
    std::array<float, 3> half_widths{};
    static const std::array<u8, 0x14> preset{};
    g_preset = preset.data();
    RuntimeBrewMeter brew{brew_state, brew_intensity, brew_counter, textures, half_widths, widget,
        junk<RuntimeDialogueWidgetContext>(), preset, junk<AnimationResourceRegistry>(), junk<ResourceErrorState>(),
        address_zero};
    float health_intensity = 0.0f;
    std::array<u32, 5> health_widths{};
    RuntimePlayerHealthDisplay health{junk<i32>(), junk<i32>(), health_intensity, junk<RuntimeHudCounter>(), junk<u32>(),
        junk<std::array<std::span<const u8>, 5>>(), health_widths, junk<u32>(), junk<u8>(), {}, {}, {},
        junk<AnimationResourceRegistry>(), junk<ResourceErrorState>()};
    std::array<char, 0x40> intro_name{};
    RuntimePlayerStrategyFlow flow{base, junk<RuntimeActorPauseGroups>(), pause_count, brew, health, axis_device,
        input_device, junk<RuntimeDialogueInputState>(), keyboard, junk<AudioEventVoices>(),
        junk<RuntimeActorResourceReference>(), junk<ActorAttachmentEventContext>(), brews_taken, brew_accepted,
        prompt_lock, prompt_owner, difficulty, chapter_mode, unlock_flags, scene_flags, "scene", brews_owned,
        brews_required, story_flags, intro_pending, intro_name, cases,
        [&case_bits](const entity_detail::ListHead*) -> const EntityBitBlock* { return &case_bits; }, "idle"};

    std::string out = "-";
    char buffer[64];
    std::array<u32, 9> message{};
    switch (entry_kind)
    {
    case 0:
    {
        message[1] = next();
        message[0] = next();
        dispatch_runtime_player_strategy_message(message, flow);
        break;
    }
    case 1:
        enter_runtime_player_strategy_state(u16(next()), flow);
        break;
    case 2:
        next();
        update_runtime_player_strategy(flow);
        break;
    case 5:
        next();
        std::snprintf(buffer, sizeof(buffer), "%x", runtime_brew_prompt_available(flow));
        out = buffer;
        break;
    case 6:
        message[1] = next();
        next();
        queue_runtime_scripted_goals(message, flow);
        break;
    case 7:
        next();
        std::snprintf(buffer, sizeof(buffer), "%x", check_runtime_player_knockout(goals));
        out = buffer;
        break;
    case 8:
        next();
        std::snprintf(buffer, sizeof(buffer), "%x", runtime_player_idle_elapsed(flow));
        out = buffer;
        break;
    case 9:
        next();
        std::snprintf(buffer, sizeof(buffer), "%x", runtime_player_wants_to_move(flow));
        out = buffer;
        break;
    case 10:
        next();
        unlock_runtime_brew_intro(flow);
        break;
    case 11:
        show_runtime_brew_count(i32(next()), brew);
        break;
    case 12:
        next();
        close_runtime_brew_meter(brew);
        break;
    case 13:
        next();
        accept_runtime_brew_meter(brew);
        break;
    default:
    {
        const std::array<float, 2> axes{0.25f, -0.75f};
        const u32 writer = next();
        if (writer == 0)
        {
            set_runtime_input_trigger_axes(axis_device, axes, keyboard);
        }
        else if (writer == 1)
        {
            set_runtime_input_axes(axis_device, axes, keyboard);
        }
        else
        {
            set_runtime_input_secondary_axes(axis_device, axes, keyboard);
        }
        break;
    }
    }
    std::vector<u32> words{read_u16(strategy, 0xE), read_u16(strategy, 0x12), read_u16(strategy, 0x14),
        read_u16(strategy, 0x16), read_u16(strategy, 0x18), strategy[0x1C]};
    for (size_t offset : {0x38, 0x3C, 0x40, 0x48, 0x4C, 0x50})
    {
        words.push_back(read_u32(strategy, offset));
    }
    words.insert(words.end(), {read_u16(body, 0x30), read_u16(body, 0x94), pause_count, brews_taken, brew_accepted,
        brew_state, bits(brew_intensity), u32(brew_counter.value), u32(brew_counter.target)});
    for (const auto& slot : textures)
    {
        words.push_back(slot.data() != nullptr ? 1 : 0);
    }
    words.insert(words.end(), {bits(half_widths[0]), bits(half_widths[1]), bits(half_widths[2]), bits(health_intensity),
        intro_pending, widget_storage.requested_set == &sentinel ? 0x1234u : 0u, story_flags});
    std::string text;
    for (u32 value : words)
    {
        std::snprintf(buffer, sizeof(buffer), "%x ", value);
        text += buffer;
    }
    text += intro_name[0] != '\0' ? std::string(intro_name.data()) : std::string("-");
    text += " D";
    for (size_t offset = 8; offset < 0x60; offset += 4)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", read_u32(device_record, offset));
        text += buffer;
    }
    for (const auto& record : child_records)
    {
        for (size_t offset = 8; offset < 0x60; offset += 4)
        {
            std::snprintf(buffer, sizeof(buffer), " %x", read_u32(record, offset));
            text += buffer;
        }
    }
    for (size_t offset = 0; offset < 0x60; offset += 4)
    {
        std::snprintf(buffer, sizeof(buffer), " %x", read_u32(keyboard, offset));
        text += buffer;
    }
    out += " | " + text + " |";
    for (size_t index = 0; index < g_log.size(); ++index)
    {
        out += (index == 0 ? " " : " ; ") + g_log[index];
    }
    std::printf("%s\n", out.c_str());
}
}

int main()
{
    int count;
    std::cin >> count;
    std::string line;
    std::getline(std::cin, line);
    for (int test = 0; test < count; ++test)
    {
        std::getline(std::cin, line);
        g_words.clear();
        g_at = 0;
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
        run_case();
    }
    return 0;
}
