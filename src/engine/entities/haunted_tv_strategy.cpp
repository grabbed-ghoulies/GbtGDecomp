#include "haunted_tv_strategy.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_live_attachments.h"
#include "actor_transform.h"
#include "body_contacts.h"
#include "ghouly_reference_names.h"
#include "ghouly_spawn_record.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_mind_requests.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "../frontend/dialogue_resources.h"
#include "../graphics/shader_parameters.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_vm.h"
#include "gbtg/model_runtime.h"
#include "body_fields.h"
#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = haunted_tv_strategy_field;
namespace parameter = haunted_tv_parameter;

// The host halves of the strategy's resource words: the shader parameter (+0x58), the six screen textures
// (+0x5C..), the ghouly's face (+0x74) and the timeline (+0x108), by the strategy's bytes.
struct HauntedTvResources
{
    RuntimeShaderTextureParameter* screen = nullptr;
    std::array<std::span<const u8>, 6> screens{};
    std::span<const u8> ghouly{};
    std::span<const u8> timeline{};
};
std::unordered_map<const u8*, HauntedTvResources> tv_resources;

// DAT_00303038: the screen modes that loop the static sound (2 and 3).
constexpr std::array<u8, 8> kStaticModes{0, 0, 1, 1, 0, 0, 0, 0};
constexpr const char* kStaticSound = "XACT_SOUNDBANK_GHAUNTEDTV_STATIC"; // 0x00302FB8.
constexpr const char* kClickSound = "XACT_SOUNDBANK_GHAUNTEDTV_CLICK"; // 0x00302F38.
constexpr const char* kTurnOffSound = "XACT_SOUNDBANK_GHAUNTEDTV_TURNOFF"; // 0x00302EB8.

RuntimePlayerStrategyGoals& goals_of(RuntimeHauntedTvStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeHauntedTvStrategy& context)
{
    return goals_of(context).entry.strategy;
}

std::span<u8> body_of(RuntimeHauntedTvStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeHauntedTvStrategy& context)
{
    return goals_of(context).entry.actor;
}

HauntedTvResources& resources_of(RuntimeHauntedTvStrategy& context)
{
    return tv_resources[strategy_of(context).data()];
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeHauntedTvStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeHauntedTvStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_byte(std::span<u8> bytes, size_t offset, i32 step)
{
    bytes[offset] = u8(bytes[offset] + step);
}

bool aid_name(const char* name)
{
    u32 prefix;
    std::memcpy(&prefix, name, 4);
    return prefix == 0x5F646961; // "aid_".
}

// 0x00035E90 with error 2 (0x00032700) when the AID is missing; nothing for a name without the prefix.
std::span<const u8> acquire_named(const char* name, RuntimeHauntedTvStrategy& context)
{
    if (!aid_name(name))
    {
        return {};
    }
    const std::span<const u8> resource = acquire_animation_resource(context.registry, name);
    if (resource.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    return resource;
}

u32 address_of_resource(std::span<const u8> resource, RuntimeHauntedTvStrategy& context)
{
    return resource.data() == nullptr ? 0u : context.address_of(resource.data());
}

// Body +0x328, the body's mode byte the states set.
void set_body_mode(u8 mode, RuntimeHauntedTvStrategy& context)
{
    body_of(context)[0x328] = mode;
}

// The actor's frame step: actor +0x92C (zero while +0x930 is above zero) by the frame's.
float actor_step(RuntimeHauntedTvStrategy& context)
{
    return runtime_actor_step(actor_of(context), goals_of(context).clock);
}

// Xbox 0x00042450, EAX the counter, ECX the actor: true each time the counter runs out, when it starts again from
// `reset`.
bool count_down_repeat(float& counter, float reset, RuntimeHauntedTvStrategy& context)
{
    const float left = counter - actor_step(context);
    counter = left;
    if (left <= 0.f)
    {
        counter = left + reset;
        return true;
    }
    return false;
}

// Xbox 0x0015B2B0's caller 0x000B1CE0, EAX strategy, ECX texture, EDX mode: 0x000F86F0, inlined, when the screen
// parameter exists.
void show_screen_texture(u32 texture, u32 mode, RuntimeHauntedTvStrategy& context)
{
    RuntimeShaderTextureParameter* screen = resources_of(context).screen;
    if (screen != nullptr)
    {
        set_shader_parameter_texture(*screen, texture, mode, context.texture_bytes);
    }
}

// Xbox 0x000B1A90, EAX strategy: the attack may run (no recovery, and body bits 0x32, 0x1B and 0x11 clear).
bool runtime_haunted_tv_attack_open(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::recovery) > 0.f) // DAT_0032B100.
    {
        return false;
    }
    const auto& bits = goals_of(context).body_bits->bytes; // Body +0x15C -> +8.
    if ((bits[6] & 4) != 0 || (bits[3] & 2) != 0)
    {
        return false;
    }
    return ((u8(~bits[2]) >> 1) & 1) != 0;
}

// Xbox 0x000B17B0: the attack switches on.
void start_runtime_haunted_tv_attack(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::attacking) != 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    increment_runtime_body_latch_mask(body, 0x27, actor, context.latches); // The inlined 0x0001F3F0, bytes 0..2, 5.
    if (context.collision != nullptr)
    {
        activate_runtime_body_contact_shapes(*context.collision, actor, 0, 1, 1.f); // 0x0013F420, DAT_0032B0F8
        put_word(body, 0x2CC, 0);
    }
    // EAX body +0x5C, EBX 0x65.
    if (context.attack_resource.data() == nullptr)
    {
        throw Error("0x000B17B0 reads the body +0x5C attack tables through address zero");
    }
    const u8* row = find_runtime_attachment_first_request(*context.attack_tables.at(context.attack_resource.data()),
        0x65); // 0x0001C560
    set(body, body_field::hit_record, row == nullptr ? 0u : context.address_of(row));
    put_word(body, 0x88, 0);
    step_byte(body, 0x6A, 1);
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + 1); // Actor +0x8F8's records, +0x15.
    }
    step_byte(body, 0x6D, 1);
    put_word(body, 0x74C, 1);
    // The inlined 0x0003DF40: the parameters' object, cloned with this actor at +8 and made.
    set(strategy, field::effect_object,
        context.create_effect(field_data(strategy, field::effect), read_u32(strategy, 8)));
    set(strategy, field::attacking, 1);
}

// Xbox 0x000B1940: the attack switches off.
void end_runtime_haunted_tv_attack(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::attacking) == 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    decrement_runtime_body_latch_mask(body, 0x27, actor, context.latches); // The inlined 0x0001F2F0.
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, 0); // 0x0013F4D0, EAX 0
    }
    set(body, body_field::hit_record, 0);
    put_word(body, 0x88, 0);
    step_byte(body, 0x6A, -1);
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold - 1);
    }
    step_byte(body, 0x6D, -1);
    put_word(body, 0x74C, 0);
    const u32 object = get(strategy, field::effect_object);
    if (object != 0)
    {
        context.release_frame_object(object); // 0x0003DE30, ECX actor, ESI object
        set(strategy, field::effect_object, 0);
    }
    set(strategy, field::attacking, 0);
}

// The timeline row at the strategy's +0x110: {on, seconds}.
std::span<const u8> timeline_row(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<const u8> timeline = resources_of(context).timeline;
    if (timeline.data() == nullptr)
    {
        throw Error("the haunted TV reads its attack timeline through address zero");
    }
    const u32 offset = get(strategy, field::timeline_row) - get(strategy, field::timeline);
    return timeline.subspan(offset, 8);
}

// Xbox 0x000B1710, ESI strategy, EBX hold: the attack timeline starts from its first row once the gate allows.
void begin_runtime_haunted_tv_timeline(u32 hold, RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::playing) != 0)
    {
        return;
    }
    set(strategy, field::pending, 1);
    set(strategy, field::held, hold);
    if (actor_of(context).animation_pause > 0.f)
    {
        return;
    }
    if (!runtime_haunted_tv_attack_open(context)) // 0x000B1A90
    {
        return;
    }
    set(strategy, field::playing, 1);
    set(strategy, field::timeline_row, get(strategy, field::timeline));
    set(strategy, field::row_time, 0.f);
    if (read_u32(timeline_row(context), 0) == 0 && hold == 0)
    {
        end_runtime_haunted_tv_attack(context); // 0x000B1940
        return;
    }
    start_runtime_haunted_tv_attack(context); // 0x000B17B0
}

// Xbox 0x000B1780, EAX strategy
void stop_runtime_haunted_tv_timeline(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    set(strategy, field::playing, 0);
    set(strategy, field::pending, 0);
    set(strategy, field::held, 0);
    if (actor_of(context).animation_pause > 0.f) // DAT_0032B100.
    {
        return;
    }
    end_runtime_haunted_tv_attack(context); // 0x000B1940
}

// Xbox 0x000B1590, EAX strategy: the timeline follows the attack gate, then steps its rows.
void tick_runtime_haunted_tv_timeline(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    if (actor.animation_pause == 0.f)
    {
        if (get(strategy, field::playing) != 0)
        {
            if (!runtime_haunted_tv_attack_open(context)) // 0x000B1A90
            {
                set(strategy, field::playing, 0);
                set(strategy, field::pending, 0);
                set(strategy, field::held, 0);
                if (!(actor.animation_pause > 0.f))
                {
                    end_runtime_haunted_tv_attack(context); // 0x000B1940
                }
                set(strategy, field::pending, 1);
            }
        }
        else if (get(strategy, field::pending) != 0 && runtime_haunted_tv_attack_open(context))
        {
            begin_runtime_haunted_tv_timeline(get(strategy, field::held), context); // 0x000B1710, EBX +0x118
        }
    }
    if (get(strategy, field::playing) == 0)
    {
        return;
    }
    if (context.collision != nullptr && context.collision->overrides != nullptr)
    {
        expire_runtime_body_contact_overrides(context.collision->overrides, 0.5f); // 0x0013EB60, DAT_0032B0F4
    }
    if (get(strategy, field::playing) == 0 || get(strategy, field::held) != 0)
    {
        return;
    }
    const float time = runtime_frame_step(goals_of(context).clock) * runtime_actor_rate(actor)
        + get(strategy, field::row_time);
    set(strategy, field::row_time, time);
    if (!(time >= read_f32(timeline_row(context), 4)))
    {
        return;
    }
    u32 row = get(strategy, field::timeline_row) + 8;
    set(strategy, field::row_time, 0.f);
    set(strategy, field::timeline_row, row);
    if (!(row < get(strategy, field::timeline_end)))
    {
        set(strategy, field::timeline_row, get(strategy, field::timeline));
    }
    const u32 on = read_u32(timeline_row(context), 0);
    if (get(strategy, field::attacking) == on)
    {
        return;
    }
    if (on != 0)
    {
        start_runtime_haunted_tv_attack(context); // 0x000B17B0
        return;
    }
    end_runtime_haunted_tv_attack(context); // 0x000B1940
}

// Xbox 0x000B1D30, EAX actor: the screen shows the face of the ghouly the actor's box (+0xBA0) holds,
// "aid_texture_ghoulies_actor_hauntedtv_" and its tag's last word.
void show_runtime_haunted_tv_ghouly(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> actor = goals_of(context).entry.actor_bytes;
    const char* box = reinterpret_cast<const char*>(actor.data() + 0xBA0);
    if (!aid_name(box) && context.spawn_record != nullptr)
    {
        GhoulySpawnRecord& record = *context.spawn_record; // Actor +0xB98.
        const i32 result = dispatch_next_ghouly_spawn(record, 1, context.spawns()); // 0x000F40A0
        if (result != -1 && result == 1 && record.remaining_c != -1)
        {
            --record.remaining_c;
        }
    }
    const u16 tag = runtime_executable_actor_type(box, context.resources); // The inlined 0x0013E820.
    const char* name = context.object_tags[tag];
    size_t last = 0;
    for (size_t index = 0; name[index] != 0; ++index)
    {
        if (name[index] == '_')
        {
            last = index;
        }
    }
    std::array<char, 0x80> texture_name{};
    format_dialogue_resource_name(texture_name.data(), "aid_texture_ghoulies_actor_hauntedtv_%s",
        name + last + 1); // 0x00035830, 0x003242CC
    HauntedTvResources& resources = resources_of(context);
    if (get(strategy, field::ghouly_texture) != 0)
    {
        release_animation_resource(context.registry, resources.ghouly); // 0x00035C00
        resources.ghouly = {};
        set(strategy, field::ghouly_texture, 0);
    }
    resources.ghouly = acquire_named(texture_name.data(), context);
    const u32 texture = address_of_resource(resources.ghouly, context);
    set(strategy, field::ghouly_texture, texture);
    show_screen_texture(texture, 1, context); // The inlined 0x000F86F0, mode 1.
}

// Xbox 0x000B1BA0, EAX strategy: the screen's mode, its texture and its sounds (the static loop in actor sound slot 6,
// the click and the turn-off in slot 0).
void set_runtime_haunted_tv_screen(u32 mode, RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const u32 current = get(strategy, field::screen);
    if (kStaticModes.at(mode) != 0)
    {
        if (kStaticModes.at(current) == 0)
        {
            queue_actor_sound(actor.sound_slots[6], 0, &actor, 6, nullptr, nullptr, 1.f, kStaticSound,
                context.sounds); // 0x0012A260, EBX actor +0x784
        }
    }
    else if (kStaticModes.at(current) != 0)
    {
        release_actor_sound(actor.sound_slots[6], context.sounds); // 0x0012A3D0, ESI actor +0x784
    }
    set(strategy, field::screen, mode);
    const u32 texture = read_u32(strategy, field::screen_texture_0.offset + mode * 4);
    switch (mode)
    {
    case 1:
        show_screen_texture(texture, 0, context); // 0x000B1CE0
        request_actor_sound(actor, 0, kClickSound, context.sounds); // 0x00041D00
        return;
    case 2:
    case 3:
        show_screen_texture(texture, 1, context); // 0x000B1CE0
        return;
    case 4:
        show_runtime_haunted_tv_ghouly(context); // 0x000B1D30
        return;
    case 5:
        show_screen_texture(texture, 0, context); // 0x000B1CE0
        request_actor_sound(actor, 0, kTurnOffSound, context.sounds); // 0x00041D00
        return;
    default:
        show_screen_texture(texture, 1, context); // The inlined 0x000F86F0, mode 1.
        return;
    }
}

// Xbox 0x00141530, ECX target: the target stands idle (body +0x30 and +0x34 clear) and is after no one but `actor`.
bool runtime_haunted_tv_target_free(const RuntimeMindTarget& target, RuntimeHauntedTvStrategy& context)
{
    if (get(target.bytes, mind_target_field::kind) != 0)
    {
        return false;
    }
    const std::span<const u8> body = context.views.body_of(target.actor).bytes; // Target +0x10 actor's +0x95C.
    if (get(body, body_field::move_holds) != 0 || get(body, body_field::move_hold_time) != 0.f) // DAT_0032B100.
    {
        return false;
    }
    const u32 mind = read_u32(target.actor_bytes, 0x958);
    if (mind == 0)
    {
        return true;
    }
    const u32 aim = get(context.views.mind_bytes_at(mind), mind_field::target_record); // The target's mind +0x10.
    if (aim == 0)
    {
        return true;
    }
    const RuntimeMindTarget* aimed = goals_of(context).target_at(aim);
    if (aimed == nullptr)
    {
        throw Error("0x00141530 reads a mind's +0x10 target record that has no view");
    }
    return read_u32(aimed->bytes, 0x10) == read_u32(strategy_of(context), 8);
}

// Xbox 0x000B29D0, EBX target, EAX actor: 1 near and free, 2 near (within 50, or 100 when busy), 3 far or none.
u32 runtime_haunted_tv_target_range(const RuntimeMindTarget* target, RuntimeHauntedTvStrategy& context)
{
    if (target == nullptr)
    {
        return 3;
    }
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const float dx = position.x - get(target->bytes, mind_target_field::shape_position).x;
    const float dy = position.y - get(target->bytes, mind_target_field::shape_position).y;
    const float dz = position.z - get(target->bytes, mind_target_field::shape_position).z;
    const float distance = (dz * dz + dx * dx) + dy * dy;
    if (2500.f > distance) // DAT_0032B40C.
    {
        return runtime_haunted_tv_target_free(*target, context) ? 1u : 2u; // 0x00141530
    }
    if (10000.f > distance) // DAT_0032B4D0.
    {
        return 2;
    }
    return 3;
}

// Xbox 0x000B2A60, ESI strategy, ECX target: within reach, the TV starts generating (state 0x3FE) and asks its mind
// for request 0xD.
bool begin_runtime_haunted_tv_generate(const RuntimeMindTarget* target, u32 jolt, RuntimeHauntedTvStrategy& context)
{
    if (runtime_haunted_tv_target_range(target, context) == 3) // 0x000B29D0
    {
        return false;
    }
    change_state(0x3FE, context); // The inlined 0x0002B8D0.
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    RuntimePlayerMindRequest* request = allocate_runtime_mind_request(mind.anchor, 0xD, mind.sizes); // 0x00029D90
    put_float(request->bytes, 0x10, 20.f); // DAT_0032B234.
    put_float(request->bytes, 0x14, 25.f); // DAT_0032B170.
    put_float(request->bytes, 0x20, 1.f); // DAT_0032B0F8.
    put_float(request->bytes, 0x18, 100.f); // DAT_0032B12C.
    put_float(request->bytes, 0x1C, 100.f);
    if (jolt != 0)
    {
        put_float(body_of(context), 0x32C, 2.f); // DAT_0032B110.
    }
    return true;
}

// Xbox 0x000B2B30, ESI strategy: the inlined 0x0002B8D0 to state 0x3EF.
void begin_runtime_haunted_tv_release(RuntimeHauntedTvStrategy& context)
{
    change_state(0x3EF, context);
}

// Story bit 0xE, **(DAT_00506C6C + 8) byte 1 bit 6.
bool story_bit_14(RuntimeHauntedTvStrategy& context)
{
    return test_runtime_story_bit(context.story_bits(), 0xE); // 0x000F29A0, ECX 0xE
}

RuntimeMindTarget* select_target(bool& found, RuntimeHauntedTvStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    found = select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
    return target;
}
}

// Xbox 0x000B1EA0
void haunted_tv_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000B1F10
bool initialize_runtime_haunted_tv_strategy(std::span<const u8> parameters, RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    HauntedTvResources& resources = tv_resources[strategy.data()];
    resources = HauntedTvResources{};
    // 0x000B1AD0, EAX strategy: the screen parameter and the six screen textures.
    Actor& actor = actor_of(context);
    if (actor.model != nullptr) // Actor +0x204.
    {
        if (actor.model->runtime == nullptr)
        {
            throw Error("0x000B1AD0 reads the haunted TV's model instance through no runtime view");
        }
        resources.screen = create_shader_parameter(*actor.model->runtime, "shader_blinn22", "colour0"); // 0x000F8540
        set(strategy, field::screen_parameter, resources.screen != nullptr ? 1u : 0u);
    }
    for (u32 index = 0; index < 6; ++index)
    {
        const char* name = reinterpret_cast<const char*>(parameters.data() + parameter::screen_0.offset
            + index * parameter::kScreenStride);
        if (!aid_name(name))
        {
            continue;
        }
        resources.screens[index] = acquire_named(name, context);
        put_word(strategy, field::screen_texture_0.offset + index * 4,
            address_of_resource(resources.screens[index], context));
    }
    // 0x000B1490, EAX parameters, ESI strategy: the attack timeline and the attack's object.
    resources.timeline = acquire_named(field_data(parameters, parameter::timeline), context);
    set(strategy, field::timeline, address_of_resource(resources.timeline, context));
    const char* effect = field_data(parameters, parameter::effect);
    char* target = field_data(strategy, field::effect);
    size_t index = 0;
    do
    {
        target[index] = effect[index];
    }
    while (effect[index++] != 0);
    if (get(strategy, field::timeline) != 0)
    {
        const u32 size = runtime_body_resource_size(resources.timeline.data(), context.registry.records,
            context.loaded_sizes); // 0x00035D90
        set(strategy, field::timeline_end, get(strategy, field::timeline) + (size >> 3) * 8);
        set(strategy, field::timeline_row, get(strategy, field::timeline));
    }
    if (context.collision != nullptr) // Body +0x2C8.
    {
        context.collision->forced = 1;
    }
    return true;
}

// Xbox 0x000B1F40
void destroy_runtime_haunted_tv_strategy(RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    HauntedTvResources& resources = resources_of(context);
    // 0x000B1B50, EDI strategy.
    if (get(strategy, field::screen_parameter) != 0)
    {
        release_shader_parameter(resources.screen); // 0x000F85D0
        resources.screen = nullptr;
        set(strategy, field::screen_parameter, 0);
    }
    for (u32 index = 0; index < 6; ++index)
    {
        if (read_u32(strategy, field::screen_texture_0.offset + index * 4) != 0)
        {
            release_animation_resource(context.registry, resources.screens[index]); // 0x00035C00
            resources.screens[index] = {};
            put_word(strategy, field::screen_texture_0.offset + index * 4, 0);
        }
    }
    if (get(strategy, field::ghouly_texture) != 0)
    {
        release_animation_resource(context.registry, resources.ghouly); // 0x00035C00
        resources.ghouly = {};
        set(strategy, field::ghouly_texture, 0);
    }
    // 0x000B1530, ESI strategy.
    set(strategy, field::playing, 0);
    set(strategy, field::pending, 0);
    set(strategy, field::held, 0);
    if (!(actor_of(context).animation_pause > 0.f)) // DAT_0032B100.
    {
        end_runtime_haunted_tv_attack(context); // 0x000B1940
    }
    if (get(strategy, field::timeline) != 0)
    {
        release_animation_resource(context.registry, resources.timeline); // 0x00035C00
    }
    set(strategy, field::timeline, 0);
    set(strategy, field::timeline_end, 0);
    set(strategy, field::timeline_row, 0);
    tv_resources.erase(strategy.data());
}

// Xbox 0x000B1F60
void update_runtime_haunted_tv_strategy(RuntimeHauntedTvStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const u16 state = read_u16(strategy, 0xE);
    const float frame = runtime_frame_step(goals.clock);
    const float rate = runtime_actor_rate(actor);
    const float recovery = get(strategy, field::recovery);
    if (recovery > 0.f)
    {
        const float left = recovery - rate * frame;
        set(strategy, field::recovery, left);
        if (left <= 0.f)
        {
            set(strategy, field::recovery, 0.f);
        }
    }
    const bool boxed = aid_name(reinterpret_cast<const char*>(goals.entry.actor_bytes.data() + 0xBA0));
    update_runtime_base_strategy(goals); // 0x0002C130
    RuntimeMindGoalContext& mind = goals.entry.goals;
    bool found = false;
    switch (state)
    {
    case 0x3EE:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            break;
        }
        const bool free = target != nullptr && runtime_haunted_tv_target_free(*target, context); // 0x00141530
        if (!requests_pending(context))
        {
            RuntimePlayerMindRequest* turn = allocate_runtime_mind_request(mind.anchor, 0x27, mind.sizes); // 0x00029D90
            put_float(turn->bytes, 0x10, std::bit_cast<float>(0x3E32B8C3u)); // DAT_0032B508, 10 degrees.
        }
        const u32 range = runtime_haunted_tv_target_range(target, context); // 0x000B29D0
        if (range == 2)
        {
            const float near = runtime_actor_step(actor, goals.clock) + get(strategy, field::near_time); // 0x00042320
            set(strategy, field::near_time, near);
            if (near >= 0.5f || !free) // DAT_0032B0F4.
            {
                begin_runtime_haunted_tv_generate(target, free ? 0u : 1u, context); // 0x000B2A60
            }
        }
        else if (range == 3)
        {
            if (target != nullptr && boxed)
            {
                begin_runtime_haunted_tv_release(context); // 0x000B2B30
            }
            else
            {
                change_state(0x416, context);
            }
        }
        else
        {
            set(strategy, field::near_time, 0.f);
            if (!free)
            {
                begin_runtime_haunted_tv_generate(target, 1, context); // 0x000B2A60
            }
        }
        break;
    }
    case 0x3EF:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found || requests_pending(context))
        {
            break;
        }
        if (runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) == 0) // 0x0002B7F0
        {
            change_state(0x3E8, context);
            break;
        }
        if (!begin_runtime_haunted_tv_generate(target, 0, context)) // 0x000B2A60
        {
            change_state(0x416, context);
        }
        break;
    }
    case 0x3FE:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            break;
        }
        float repeat = get(strategy, field::repeat);
        const bool again = count_down_repeat(repeat, 0.5f, context); // 0x00042450, DAT_0032B0F4
        set(strategy, field::repeat, repeat);
        if (again || !requests_pending(context))
        {
            begin_runtime_haunted_tv_generate(target, 0, context); // 0x000B2A60
        }
        const u32 range = runtime_haunted_tv_target_range(target, context); // 0x000B29D0
        if (range == 1)
        {
            change_state(0x3EE, context);
        }
        else if (range == 3)
        {
            if (target != nullptr && boxed)
            {
                begin_runtime_haunted_tv_release(context); // 0x000B2B30
            }
            else
            {
                change_state(0x416, context);
            }
        }
        break;
    }
    case 0x409:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (requests_pending(context))
        {
            break;
        }
        goals.room = nullptr; // Actor +0xCA0.
        if (target != nullptr && boxed)
        {
            begin_runtime_haunted_tv_release(context); // 0x000B2B30
        }
        else
        {
            change_state(0x416, context);
        }
        break;
    }
    case 0x40F:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (target == nullptr)
        {
            change_state(0x416, context);
            break;
        }
        notice_runtime_strategy_target(0xE, context.checks); // 0x0013A430
        break;
    }
    case 0x413:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!found)
        {
            break;
        }
        if (3600.f > runtime_squared_target_distance(actor, target->bytes)) // 0x000415A0, DAT_0032B410
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        break;
    }
    case 0x416:
    {
        RuntimeMindTarget* target = select_target(found, context);
        if (!requests_pending(context))
        {
            load_runtime_mind_goal(context.goal_wait, mind); // 0x0002A8E0
        }
        float repeat = get(strategy, field::repeat);
        const bool again = count_down_repeat(repeat, 0.5f, context); // 0x00042450
        set(strategy, field::repeat, repeat);
        if (again && begin_runtime_haunted_tv_generate(target, 0, context)) // 0x000B2A60
        {
            break;
        }
        const u32 range = runtime_haunted_tv_target_range(target, context); // 0x000B29D0
        if (range == 1)
        {
            change_state(0x3EE, context);
        }
        else if (range == 3 && target != nullptr && boxed)
        {
            begin_runtime_haunted_tv_release(context); // 0x000B2B30
        }
        break;
    }
    case 0x417:
        if (requests_pending(context))
        {
            break;
        }
        if (!story_bit_14(context))
        {
            change_state(0x40F, context);
            break;
        }
        if (runtime_player_strategy_decay_enabled(goals.entry.actor_bytes, goals.resume) != 0) // 0x0002B7F0
        {
            begin_runtime_haunted_tv_release(context); // 0x000B2B30
            break;
        }
        change_state(0x3E8, context);
        break;
    default:
        break;
    }
    if (RuntimeShaderTextureParameter* screen = resources_of(context).screen; screen != nullptr)
    {
        advance_shader_parameter(*screen, rate * frame, context.texture_bytes); // 0x000F8640
    }
    tick_runtime_haunted_tv_timeline(context); // 0x000B1590
}

// Xbox 0x000B23F0
void dispatch_runtime_haunted_tv_message(std::span<const u32> message, RuntimeHauntedTvStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = read_u16(strategy, 0xE);
    const u32 actor = read_u32(strategy, 8);
    switch (message[0])
    {
    case 1:
    {
        if (state == 0x3E9)
        {
            if (get(strategy, field::announced) != 0)
            {
                return;
            }
        }
        else if (state != 0x413)
        {
            dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
            return;
        }
        RuntimeMindTarget* target = nullptr;
        if (select_runtime_strategy_hostile_target(target, 0, goals)) // 0x00139C70, EDX 0
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x417, context);
        }
        return;
    }
    case 0x27:
        if (state != 0x417 && get(strategy, field::generating) == 0)
        {
            dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        }
        return;
    case 0x35:
    {
        const u32 screen = get(strategy, field::screen);
        if (screen == 0 || screen == 5)
        {
            return;
        }
        Actor& own = actor_of(context);
        if (kStaticModes.at(screen) != 0)
        {
            release_actor_sound(own.sound_slots[6], context.sounds); // 0x0012A3D0, ESI actor +0x784
        }
        set(strategy, field::screen, 5);
        show_screen_texture(read_u32(strategy, field::screen_texture_0.offset + 5 * 4), 0, context); // 0x000B1CE0
        request_actor_sound(own, 0, kTurnOffSound, context.sounds); // 0x00041D00
        return;
    }
    case 0x38:
        set_runtime_haunted_tv_screen(2, context); // 0x000B1BA0
        return;
    case 0x3A:
        put_u16(strategy, 0x12, 0x3EF);
        put_u16(strategy, 0x18, 0x3EF);
        put_u16(strategy, 0x14, 0x416);
        put_u16(strategy, 0x16, 0x416);
        return;
    case 0x3B:
    {
        set(strategy, field::announced, 1);
        // 0x0013A400, EAX strategy: the script hears {0xB4, the actor's tag}.
        const std::array<u32, 2> event{0xB4, read_u16(goals.entry.actor_bytes, 6)};
        script_queue_event(goals.game, {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810
        return;
    }
    case 0x6E:
    {
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        if (check_runtime_player_knockout(goals) != 0 || message[1] != actor) // 0x0013A490, EBX strategy
        {
            return;
        }
        retaliate_runtime_strategy_contact(message, -1, context.checks, context.views); // 0x00139390
        if (!story_bit_14(context))
        {
            begin_runtime_haunted_tv_timeline(1, context); // 0x000B1710, EBX 1
            change_state(0x40F, context);
            return;
        }
        if (runtime_message_lands_on_actor(message, actor)) // 0x001408A0
        {
            change_state(0x409, context);
            return;
        }
        if (read_u16(strategy, 0xE) == 0x413)
        {
            change_state(0x416, context);
        }
        return;
    }
    default:
        dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
        return;
    }
}

// Xbox 0x000B2650
void enter_runtime_haunted_tv_state(u16 state, u16 previous, RuntimeHauntedTvStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    RuntimeMindGoalContext& mind = goals.entry.goals;
    (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
    set_body_mode(2, context);
    strategy[0x1A] = 0;
    strategy[0x1B] = 0;
    switch (state)
    {
    case 0x3EE:
        set_body_mode(5, context);
        strategy[0x1A] = 1;
        set(strategy, field::near_time, 0.f);
        set_runtime_haunted_tv_screen(3, context); // 0x000B1BA0
        begin_runtime_haunted_tv_timeline(0, context); // 0x000B1710, EBX 0
        return;
    case 0x3EF:
        set_body_mode(6, context);
        put_u16(strategy, 0x12, 0x416);
        put_u16(strategy, 0x18, 0x416);
        put_u16(strategy, 0x14, 0x416);
        put_u16(strategy, 0x16, 0x416);
        strategy[0x1B] = 1;
        load_runtime_mind_goal(context.goal_generate, mind); // 0x0002A8E0
        if (get(strategy, field::generated) == 0)
        {
            begin_runtime_haunted_tv_timeline(1, context); // 0x000B1710, EBX 1
            set(strategy, field::generated, get(strategy, field::generated) + 1);
            set(strategy, field::generating, get(strategy, field::generating) + 1);
        }
        if (kStaticModes.at(get(strategy, field::screen)) != 0)
        {
            release_actor_sound(actor_of(context).sound_slots[6], context.sounds); // 0x0012A3D0
        }
        set(strategy, field::screen, 4);
        show_runtime_haunted_tv_ghouly(context); // 0x000B1D30
        return;
    case 0x3FC:
    {
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        if (context.spawn_record != nullptr) // Actor +0xB98.
        {
            release_ghouly_spawn_record(*context.spawn_record); // 0x000F3A00
            context.spawn_record = nullptr;
        }
        goals.entry.actor_bytes[0xBA0] = 0;
        context.spawn_active = 0; // The native alias of actor +0xBA0's first byte.
        return;
    }
    case 0x3FE:
        set_body_mode(3, context);
        set(strategy, field::repeat, 0.5f); // DAT_0032B0F4.
        set_runtime_haunted_tv_screen(2, context); // 0x000B1BA0
        return;
    case 0x409:
        set(strategy, field::recovery, 1.f); // DAT_0032B0F8.
        begin_runtime_haunted_tv_timeline(1, context); // 0x000B1710, EBX 1
        load_runtime_mind_goal(context.goal_knocked_down, mind); // 0x0002A8E0
        return;
    case 0x40F:
        put_u16(strategy, 0x12, 0x40F);
        put_u16(strategy, 0x18, 0x40F);
        set_runtime_haunted_tv_screen(2, context); // 0x000B1BA0
        return;
    case 0x413:
        load_runtime_mind_goal(context.goal_sleep, mind); // 0x0002A8E0
        strategy[0x1C] = u8(strategy[0x1C] + 1);
        set_runtime_haunted_tv_screen(0, context); // 0x000B1BA0
        return;
    case 0x416:
        set(strategy, field::repeat, 0.5f); // DAT_0032B0F4.
        load_runtime_mind_goal(context.goal_wait, mind); // 0x0002A8E0
        set_runtime_haunted_tv_screen(2, context); // 0x000B1BA0
        return;
    case 0x417:
        // Story bit 0xE (**(DAT_00506C6C + 8) byte 1 bit 6) picks 0x416 over 0x40F as the state after waking.
        put_u16(strategy, 0x18, (context.story_bits()[1] & 0x40) != 0 ? 0x416 : 0x40F);
        set_runtime_haunted_tv_screen(3, context); // 0x000B1BA0
        strategy[0x1B] = 1;
        load_runtime_mind_goal(context.goal_wakeup, mind); // 0x0002A8E0
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000B2900
void leave_runtime_haunted_tv_state(u16 state, u16 next, RuntimeHauntedTvStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EF:
        stop_runtime_haunted_tv_timeline(context); // 0x000B1780
        if (get(strategy, field::generating) != 0)
        {
            set(strategy, field::generating, get(strategy, field::generating) - 1);
        }
        return;
    case 0x409:
        if (next == 0x3EF)
        {
            return;
        }
        stop_runtime_haunted_tv_timeline(context); // 0x000B1780
        return;
    case 0x3EE:
        stop_runtime_haunted_tv_timeline(context); // 0x000B1780
        return;
    case 0x3FE:
        put_float(body_of(context), 0x32C, 1.f); // DAT_0032B0F8.
        return;
    case 0x413:
    case 0x3FC:
    case 0x3FF:
    case 0x415:
        strategy[0x1C] = u8(strategy[0x1C] - 1);
        return;
    default:
        return;
    }
}
}
