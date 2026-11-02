#include "baron_plane_strategy.h"
#include "mind_fields.h"
#include "actor_clock.h"
#include "actor_transform.h"
#include "body_message_helpers.h"
#include "body_timers.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_parent_lifecycle.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "player_strategy_goals.h"
#include "../frontend/scoreboard_update.h"
#include "../scene/cutscene_scene_runtime.h"
#include "../scene/scene_markers.h"
#include "../scripts/script_conditions.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = baron_plane_strategy_field;
namespace parameter = baron_plane_parameter;

RuntimePlayerStrategyGoals& goals_of(RuntimeBaronPlaneStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeBaronPlaneStrategy& context)
{
    return goals_of(context).entry.strategy;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeBaronPlaneStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeBaronPlaneStrategy& context)
{
    return goals_of(context).entry.body;
}

Actor& actor_of(RuntimeBaronPlaneStrategy& context)
{
    return goals_of(context).entry.actor;
}

EntityBitBlock& bits_of(RuntimeBaronPlaneStrategy& context, const char* reader)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string("Baron plane strategy ") + reader + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

void put_u32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeBaronPlaneStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

// The inlined 0x0002B8D0.
void change_state(u16 state, RuntimeBaronPlaneStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeBaronPlaneStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

bool target_found(RuntimeBaronPlaneStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    return select_runtime_strategy_target(target, 0, goals_of(context)); // 0x00139AA0
}

bool can_resume(RuntimeBaronPlaneStrategy& context)
{
    return runtime_player_can_resume_strategy(goals_of(context).entry.actor_bytes, goals_of(context).resume)
        != 0; // 0x00028D50, EAX actor
}

u16 energy_of(RuntimeBaronPlaneStrategy& context)
{
    return read_u16(body_of(context), 0x94);
}

// The actor's pause group is held (DAT_0050E768) or its decay entry is off.
bool paused(RuntimeBaronPlaneStrategy& context)
{
    const RuntimePlayerStrategyResume& resume = goals_of(context).resume;
    return resume.references[1] != 0 || resume.decay_enabled[resume.decay_index] == 0;
}

// The inlined "mode 1" of 0x000A9430: the effect on.
void effect_on(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_mode) == 1)
    {
        return;
    }
    set(strategy, field::effect_mode, 1u);
    start_runtime_baron_plane_effect(context); // 0x000A8AD0
}

// The inlined "mode 2" of 0x000A9430 at 60 energy or less: the effect by body bit 0x35.
void effect_when_low(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (energy_of(context) > 0x3C || get(strategy, field::effect_mode) == 2)
    {
        return;
    }
    set(strategy, field::effect_mode, 2u);
    if (!runtime_body_bit_set(bits_of(context, "0x000A9430"), 0x35))
    {
        start_runtime_baron_plane_effect(context); // 0x000A8AD0
        return;
    }
    end_runtime_baron_plane_effect(context); // 0x000A89C0
}

// The inlined mode end of 0x000A9980 and message 0x3B: the effect lingers 0.25 s (DAT_0032B104).
void effect_off(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_mode) == 0)
    {
        return;
    }
    set(strategy, field::effect_mode, 0u);
    if (get(strategy, field::effect) != 0)
    {
        set(strategy, field::linger, 0.25f);
    }
}

// The dives done: more to come, or the wait.
void after_dive(RuntimeBaronPlaneStrategy& context)
{
    change_state(get(strategy_of(context), field::dives) != 0 ? 0x3EE : 0x40C, context);
}

SceneCutsceneRuntime& cutscenes_of(RuntimeBaronPlaneStrategy& context)
{
    return context.cutscenes();
}
}

// Xbox 0x000A8BF0
void baron_plane_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A8C60
bool initialize_runtime_baron_plane_strategy(std::span<u8> strategy, std::span<const u8> parameters)
{
    const auto copy = [&](u32 to, const char* from)
    {
        std::memcpy(strategy.data() + to, from, std::strlen(from) + 1);
    };
    copy(field::effect_name.offset, field_data(parameters, parameter::effect));
    copy(field::sound_name.offset, field_data(parameters, parameter::sound));
    copy(field::cutscene_name.offset, field_data(parameters, parameter::cutscene));
    set(strategy, field::spawns, 4u);
    return true;
}

// Xbox 0x000A88C0
void tick_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u32 mode = get(strategy, field::effect_mode);
    if (mode == 0)
    {
        float linger = get(strategy, field::linger);
        const bool ended = count_down_runtime_strategy_timer(linger, goals); // The inlined 0x0002B9F0.
        set(strategy, field::linger, linger);
        if (ended)
        {
            end_runtime_baron_plane_effect(context); // 0x000A89C0
        }
        return;
    }
    if (mode != 2)
    {
        return;
    }
    if (!runtime_body_bit_set(bits_of(context, "0x000A88C0"), 0x35))
    {
        start_runtime_baron_plane_effect(context); // The inlined 0x000A8AD0.
        return;
    }
    end_runtime_baron_plane_effect(context); // 0x000A89C0
}

// Xbox 0x000A8960
void set_runtime_baron_plane_effect_mode(u32 mode, RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect_mode) == mode)
    {
        return;
    }
    set(strategy, field::effect_mode, mode);
    if (mode == 0)
    {
        if (get(strategy, field::effect) != 0)
        {
            set(strategy, field::linger, 0.25f); // DAT_0032B104.
        }
        return;
    }
    if (mode == 2 && runtime_body_bit_set(bits_of(context, "0x000A8960"), 0x35))
    {
        end_runtime_baron_plane_effect(context); // 0x000A89C0
        return;
    }
    if (mode == 1 || mode == 2)
    {
        start_runtime_baron_plane_effect(context); // The tail jump to 0x000A8AD0.
    }
}

// Xbox 0x000A89C0
void end_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 effect = get(strategy, field::effect);
    if (effect == 0)
    {
        return;
    }
    const std::array<u32, 2> message{5, 0};
    context.object_message(effect, message); // Vtable +0xC.
    decrement_runtime_body_latches(body_of(context), actor_of(context), context.latches); // The inlined 0x0001FD20.
    set(strategy, field::effect, 0u);
}

// Xbox 0x000A8AD0
void start_runtime_baron_plane_effect(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::effect) != 0)
    {
        return;
    }
    const std::span<u8> body = body_of(context);
    const char* name = field_data(std::span<const u8>{strategy}, field::effect_name);
    set(strategy, field::effect, context.create_effect(name, read_u32(body, 8))); // The inlined 0x0003DF40.
    increment_runtime_body_latches(body, actor_of(context), context.latches); // The inlined 0x0001FC40.
}


namespace
{
// DAT_00511B48's actors with energy (their body +0x94).
u32 live_actives(RuntimeBaronPlaneStrategy& context)
{
    u32 count = 0;
    for (const RuntimeActiveActorLink* link = context.checks.active; link != nullptr; link = link->next)
    {
        if (link->body->energy != 0)
        {
            ++count;
        }
    }
    return count;
}
}

// Xbox 0x000A9AA0
const std::array<float, 4>& runtime_baron_plane_row(std::span<const u8> body, RuntimeBaronPlaneStrategy& context)
{
    // DAT_00302AA0 (another ghouly out) and DAT_00302AF0 (alone): {minimum, maximum, dives (a word), the answer to
    // message 0x58}.
    static const std::array<std::array<float, 4>, 5> crowded_rows{{
        {10.f, 10.f, std::bit_cast<float>(1u), 140.f},
        {10.f, 10.f, std::bit_cast<float>(1u), 160.f},
        {10.f, 10.f, std::bit_cast<float>(1u), 180.f},
        {10.f, 10.f, std::bit_cast<float>(1u), 200.f},
        {1.f, 1.f, std::bit_cast<float>(5u), 250.f},
    }};
    static const std::array<std::array<float, 4>, 5> alone_rows{{
        {8.f, 8.f, std::bit_cast<float>(1u), 140.f},
        {6.f, 6.f, std::bit_cast<float>(1u), 160.f},
        {4.f, 4.f, std::bit_cast<float>(1u), 180.f},
        {2.f, 2.f, std::bit_cast<float>(1u), 200.f},
        {1.f, 1.f, std::bit_cast<float>(5u), 250.f},
    }};
    const auto& rows = live_actives(context) > 1 ? crowded_rows : alone_rows;
    const u16 energy = read_u16(body, 0x94);
    if (energy > 0x78)
    {
        return rows[0];
    }
    if (energy > 100)
    {
        return rows[1];
    }
    if (energy > 0x50)
    {
        return rows[2];
    }
    if (energy < 0x3D)
    {
        return runtime_baron_plane_crowded(context) ? rows[3] : rows[4]; // 0x000A9B20
    }
    return rows[3];
}

// Xbox 0x000A9B20
bool runtime_baron_plane_crowded(RuntimeBaronPlaneStrategy& context)
{
    return live_actives(context) > 1;
}

// Xbox 0x000A9B50
bool runtime_baron_plane_can_fly(RuntimeBaronPlaneStrategy& context)
{
    if (get(strategy_of(context), field::hit_pause) > 0.f) // DAT_0032B100.
    {
        return false;
    }
    const EntityBitBlock& bits = bits_of(context, "0x000A9B50");
    for (const u32 bit : {36u, 53u, 31u}) // DAT_00302B48.
    {
        if (runtime_body_bit_set(bits, bit))
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x000A9BA0
bool runtime_baron_plane_can_sound(RuntimeBaronPlaneStrategy& context)
{
    const EntityBitBlock& bits = bits_of(context, "0x000A9BA0");
    for (const u32 bit : {36u, 53u}) // DAT_00302B54.
    {
        if (runtime_body_bit_set(bits, bit))
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x000A9BE0
void start_runtime_baron_plane_flight(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    if (get(strategy, field::flying) == 0)
    {
        if (context.collision != nullptr)
        {
            activate_runtime_body_contact_shapes(*context.collision, actor_of(context), 0x13, 7, 1.f); // 0x0013F420
            put_u32(body, 0x2CC, 0);
        }
        body[0x6A] = u8(body[0x6A] + 1);
        set(strategy, field::flying, 1u);
    }
    // EDI body +0x5C, EBX 4: the first table's row, else the second's.
    const u8* row = nullptr;
    if (context.attack_resource.data() != nullptr)
    {
        const RuntimeAttachmentRequestTables& tables = *context.attack_tables.at(context.attack_resource.data());
        if (tables.first.rows != nullptr)
        {
            row = find_runtime_attachment_first_request(tables, 4); // 0x0001C560
        }
        if (row == nullptr && tables.second.rows != nullptr)
        {
            row = find_runtime_attachment_second_request(tables, 4); // 0x0001C5A0
        }
    }
    put_u32(body, 0x84, row == nullptr ? 0u : context.address_of(row));
    put_u32(body, 0x88, 0);
    put_u32(body, 0x74C, 1);
    EntityBitBlock& bits = bits_of(context, "0x000A9BE0");
    bits.bytes[1] = u8(bits.bytes[1] & 0xFB);
}

// Xbox 0x000A9C90
void end_runtime_baron_plane_flight(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    if (get(strategy, field::flying) != 0)
    {
        if (context.collision != nullptr)
        {
            deactivate_runtime_body_contact_shapes(*context.collision, 0x13); // 0x0013F4D0, EAX 0x13
        }
        put_u32(body, 0x84, 0);
        put_u32(body, 0x88, 0);
        body[0x6A] = u8(body[0x6A] - 1);
        put_u32(body, 0x74C, 0);
        set(strategy, field::flying, 0u);
    }
    EntityBitBlock& bits = bits_of(context, "0x000A9C90");
    bits.bytes[1] = u8(bits.bytes[1] | 4);
}

// Xbox 0x000A9CF0
void start_runtime_baron_plane_sound(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (get(strategy, field::sounding) != 0)
    {
        return;
    }
    set(strategy, field::sounding, 1u);
    const char* name = field_data(std::span<const u8>{strategy}, field::sound_name);
    if (name[0] == 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f, name,
        context.sounds); // 0x0012A260, EBX actor +0x614
}

// Xbox 0x000A9D40
void land_runtime_baron_plane(RuntimeBaronPlaneStrategy& context)
{
    Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const u8* markers = context.markers();
    const u16 id = nearest_runtime_marker(position, 0x1B, markers, position); // 0x0011EFA0, EAX 0x1B
    if (id != 0)
    {
        runtime_marker_position(id, position, markers); // 0x0011F260
    }
    const std::span<u8> mind = mind_of(context);
    set(mind, mind_field::goal_point, position);
    load_goal(context.goal_land, context);
}

// Xbox 0x0011F260
void runtime_marker_position(u16 id, Vec3& position, const u8* records)
{
    const u8* row = find_runtime_marker(records, i16(id));
    if (row == nullptr)
    {
        return;
    }
    std::memcpy(&position, row + 0xC, 12);
}

// Xbox 0x000A9DB0
void follow_runtime_baron_plane_cutscene(RuntimeBaronPlaneStrategy& context)
{
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const Vec3 euler = actor_world_euler(actor_of(context)); // 0x00041820
    SceneCutsceneRuntime& cutscenes = cutscenes_of(context);
    if (cutscenes.index < 0 || cutscenes.instances[size_t(cutscenes.index)] == nullptr)
    {
        throw Error("0x000A9DB0 reads the current cutscene (DAT_004E1C38[DAT_004E1C34]) with none playing");
    }
    // The inlined 0x0004EF20 and 0x0004EF80.
    place_scene_cutscene(position, cutscenes);
    turn_scene_cutscene(euler, cutscenes);
}

// Xbox 0x000A9EA0
bool wait_runtime_baron_plane_crowd(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (live_actives(context) > 1)
    {
        return false;
    }
    const float time = get(strategy, field::crowd_time)
        - runtime_actor_rate(actor_of(context)) * runtime_frame_step(goals_of(context).clock);
    set(strategy, field::crowd_time, time);
    if (0.f < time)
    {
        return false;
    }
    set(strategy, field::crowd_wait, 0u);
    if (energy_of(context) <= 0x3C && get(strategy, field::spawns) == 0 && get(strategy, field::spinning) == 0)
    {
        change_state(0x414, context);
        return true;
    }
    set_runtime_baron_plane_effect_mode(energy_of(context) > 0x3C ? 0u : 2u, context); // 0x000A8960
    change_state(0x3EE, context);
    return true;
}

// Xbox 0x000A9F90
bool runtime_baron_plane_spin_ready(RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    return energy_of(context) < 0x3D && get(strategy, field::spawns) == 0 && get(strategy, field::spinning) == 0;
}

// Xbox 0x000A8CD0
void update_runtime_baron_plane_strategy(RuntimeBaronPlaneStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = get(strategy, strategy_field::state);
    alert_runtime_goal_listener(goals); // 0x0013D400
    update_runtime_base_strategy(goals); // 0x0002C130
    tick_runtime_baron_plane_effect(context); // 0x000A88C0
    if (paused(context))
    {
        // +0x40 runs down by the frame step alone while the actor is paused.
        const float step = runtime_frame_step(goals.clock);
        if (get(strategy, field::hit_pause) > 0.f)
        {
            const float left = get(strategy, field::hit_pause) - step;
            set(strategy, field::hit_pause, left);
            if (!(0.f < left))
            {
                set(strategy, field::hit_pause, 0.f);
            }
        }
    }
    if (get(strategy, field::fly) != 0 && runtime_baron_plane_can_fly(context)) // 0x000A9B50
    {
        start_runtime_baron_plane_flight(context); // 0x000A9BE0
    }
    else
    {
        end_runtime_baron_plane_flight(context); // 0x000A9C90
    }
    if (get(strategy, field::sound) != 0 && runtime_baron_plane_can_sound(context)) // 0x000A9BA0
    {
        start_runtime_baron_plane_sound(context); // 0x000A9CF0
    }
    else if (get(strategy, field::sounding) != 0)
    {
        set(strategy, field::sounding, 0u);
        release_actor_sound(actor_of(context).sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    if (get(strategy, field::flying) != 0 && context.collision != nullptr && context.collision->overrides != nullptr)
    {
        expire_runtime_body_contact_overrides(context.collision->overrides, 0.5f); // 0x0013EB60, DAT_0032B0F4
    }
    const auto far = [&]()
    {
        if (!requests_pending(context))
        {
            load_goal(context.goal_far, context);
        }
    };
    switch (state)
    {
    case 0x3EE:
        if (!target_found(context))
        {
            return;
        }
        if (get(strategy, field::crowd_wait) != 0 && wait_runtime_baron_plane_crowd(context)) // 0x000A9EA0
        {
            return;
        }
        if (requests_pending(context))
        {
            return;
        }
        if (get(strategy, field::hit_target) != 0)
        {
            change_state(0x40E, context);
            return;
        }
        after_dive(context);
        return;
    case 0x401:
        if (target_found(context))
        {
            goals.room = nullptr; // Actor +0xCA0.
            change_state(0x40C, context);
        }
        far();
        return;
    case 0x404:
        if (get(strategy, field::cutscene) != 0)
        {
            follow_runtime_baron_plane_cutscene(context); // 0x000A9DB0
        }
        if (requests_pending(context))
        {
            return;
        }
        if (get(strategy, field::landed) == 0)
        {
            // The script hears {0xB9, his tag}.
            const std::array<u32, 2> event{0xB9, read_u16(goals.entry.actor_bytes, 6)};
            script_queue_event(goals.game, {reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810
            if (get(strategy, field::cutscene) != 0)
            {
                release_scene_cutscenes(cutscenes_of(context)); // 0x0004ECE0
                set(strategy, field::cutscene, 0u);
            }
            set(strategy, field::landed, get(strategy, field::landed) + 1);
            set(strategy, field::spawns, get(strategy, field::spawns) - 1);
        }
        if (cutscenes_of(context).index > -1) // DAT_004E1C34.
        {
            return;
        }
        change_state(0x3EE, context);
        return;
    case 0x409:
        if (!target_found(context) || requests_pending(context) || !can_resume(context))
        {
            return;
        }
        if (runtime_baron_plane_spin_ready(context)) // 0x000A9F90
        {
            change_state(0x414, context);
            return;
        }
        if (get(strategy, field::knocked) != 0 && get(strategy, field::spawns) != 0)
        {
            change_state(0x404, context);
            return;
        }
        change_state(0x3EE, context);
        return;
    case 0x40C:
    {
        if (!target_found(context))
        {
            return;
        }
        if (get(strategy, field::crowd_wait) != 0 && wait_runtime_baron_plane_crowd(context)) // 0x000A9EA0
        {
            return;
        }
        float wait = get(strategy, field::wait);
        tick_runtime_hud_timer(wait); // 0x0012B2F0, ECX +0x44
        set(strategy, field::wait, wait);
        const bool low = energy_of(context) <= 0x3C && get(strategy, field::spinning) != 0;
        const bool alone = !runtime_baron_plane_crowded(context); // 0x000A9B20
        if ((get(strategy, field::wait) == 0.f || (low && alone)) && (!low || alone)) // DAT_0032B100.
        {
            const u32 dives = std::bit_cast<u32>(runtime_baron_plane_row(body_of(context), context)[2]); // 0x000A9AA0
            set(strategy, field::dives, dives);
            if (dives != 0)
            {
                change_state(0x3EE, context);
                return;
            }
        }
        far();
        return;
    }
    case 0x40E:
        if (!target_found(context) || requests_pending(context))
        {
            return;
        }
        if (runtime_baron_plane_spin_ready(context)) // 0x000A9F90
        {
            change_state(0x414, context);
            return;
        }
        after_dive(context);
        return;
    case 0x414:
        if (!target_found(context))
        {
            return;
        }
        if (get(strategy, field::cutscene) != 0)
        {
            follow_runtime_baron_plane_cutscene(context); // 0x000A9DB0
        }
        if (requests_pending(context))
        {
            return;
        }
        if (get(strategy, field::cutscene) != 0)
        {
            release_scene_cutscenes(cutscenes_of(context)); // 0x0004ECE0
            set(strategy, field::cutscene, 0u);
        }
        change_state(0x40C, context);
        return;
    default:
        return;
    }
}

// Xbox 0x000A91A0
void dispatch_runtime_baron_plane_message(std::span<u32> message, RuntimeBaronPlaneStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    const u32 actor = read_u32(strategy, 8);
    if (message[0] == 0x26 || message[0] == 0x27)
    {
        if (paused(context))
        {
            set(strategy, field::hit_pause, 1.f); // DAT_0032B0F8.
        }
        const u16 state = get(strategy, strategy_field::state);
        if (state == 0x404 || state == 0x409 || state == 0x414)
        {
            return;
        }
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
    switch (message[0])
    {
    case 0x14:
        if (get(strategy, field::effect) == message[1])
        {
            set(strategy, field::effect, 0u);
        }
        return;
    case 0x31:
        react_runtime_strategy_watched_actor(message, context.checks, context.views); // 0x001397B0
        return;
    case 0x3B:
        effect_off(context);
        start_scene_cutscene(field_data(std::span<const u8>{strategy}, field::cutscene_name), 0, 0,
            cutscenes_of(context)); // 0x0004EDA0
        follow_runtime_baron_plane_cutscene(context); // 0x000A9DB0
        set(strategy, field::cutscene, 1u);
        return;
    case 0x58:
        message[2] = std::bit_cast<u32>(runtime_baron_plane_row(body, context)[3]); // 0x000A9AA0
        message[1] = 1;
        return;
    case 0x6E:
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490, EBX strategy
        {
            return;
        }
        if (message[1] == actor && runtime_strategy_reactions_enabled(goals) && message[4] != 0)
        {
            if (message[3] != 0)
            {
                set(strategy, field::dives,
                    std::bit_cast<u32>(runtime_baron_plane_row(body, context)[2])); // 0x000A9AA0
            }
            set(strategy, field::knocked, message[3]);
            change_state(0x409, context);
        }
        if (runtime_message_from_mind_target(message, actor, mind_of(context), goals.target_at) // 0x001408E0
            && message[4] != 0)
        {
            set(strategy, field::hit_target, 1u);
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000A9430
void enter_runtime_baron_plane_state(u16 state, u16 previous, RuntimeBaronPlaneStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    body[0x328] = 2;
    strategy[0x1A] = 0;
    set(strategy, field::fly, 0u);
    set(strategy, field::sound, 0u);
    const auto fly_and_sound = [&]()
    {
        set(strategy, field::fly, 1u);
        set(strategy, field::sound, 1u);
    };
    switch (state)
    {
    case 0x3E8:
        set(strategy, field::sound, 1u);
        effect_when_low(context);
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    case 0x3EE:
        set(strategy, field::hit_target, 0u);
        fly_and_sound();
        if (get(strategy, field::dives) != 0)
        {
            set(strategy, field::dives, get(strategy, field::dives) - 1);
        }
        if (runtime_baron_plane_crowded(context)) // 0x000A9B20
        {
            effect_on(context);
            set(strategy, field::crowd_wait, 1u);
            set(strategy, field::crowd_time, 2.f); // DAT_0032B110.
        }
        else
        {
            effect_when_low(context);
        }
        body[0x328] = 5;
        strategy[0x1A] = 1;
        set(strategy, strategy_field::unk_18, u16(0x401));
        load_goal(context.goal_attack, context);
        return;
    case 0x3FB:
        set(strategy, field::sound, 1u);
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    case 0x401:
        body[0x328] = 2;
        load_goal(context.goal_wait_for_stand, context);
        load_goal(context.goal_far, context);
        if (runtime_baron_plane_crowded(context)) // 0x000A9B20
        {
            effect_on(context);
            set(strategy, field::crowd_wait, 1u);
            fly_and_sound();
            return;
        }
        effect_when_low(context);
        fly_and_sound();
        return;
    case 0x404:
        set(strategy, field::landed, 0u);
        set(strategy, field::cutscene, 0u);
        effect_on(context);
        body[0x328] = 2;
        strategy[0x1C] = u8(strategy[0x1C] + 1);
        land_runtime_baron_plane(context); // 0x000A9D40, EAX actor, EBX mind
        increment_runtime_body_latches(body, actor_of(context), context.latches); // 0x0001FC40, ECX body
        set(strategy, field::sound, 1u);
        change_runtime_actor_pause(u8(0xFFFF), 0, context.pause); // 0x00117400
        return;
    case 0x409:
        load_goal(context.goal_wait_for_stand, context);
        effect_on(context);
        fly_and_sound();
        return;
    case 0x40C:
    {
        body[0x328] = 2;
        load_goal(context.goal_wait_for_stand, context);
        load_goal(context.goal_far, context);
        const std::array<float, 4>& row = runtime_baron_plane_row(body, context); // 0x000A9AA0
        if (runtime_baron_plane_crowded(context)) // 0x000A9B20
        {
            effect_on(context);
            set(strategy, field::crowd_wait, 1u);
        }
        else
        {
            effect_when_low(context);
        }
        goals.random = goals.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
        const float roll = std::bit_cast<float>((goals.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        fly_and_sound();
        set(strategy, field::wait, (row[1] - row[0]) * roll + row[0]);
        return;
    }
    case 0x40E:
        fly_and_sound();
        effect_when_low(context);
        load_goal(context.goal_celebrate, context);
        return;
    case 0x414:
        set(strategy, field::cutscene, 0u);
        set(strategy, field::spinning, 1u);
        effect_on(context);
        increment_runtime_body_latches(body, actor_of(context), context.latches); // 0x0001FC40, ECX body
        load_goal(context.goal_spin, context);
        change_runtime_actor_pause(u8(0xFFFF), 0, context.pause); // 0x00117400
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        return;
    }
}

// Xbox 0x000A9980
void leave_runtime_baron_plane_state(u16 state, RuntimeBaronPlaneStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    switch (state)
    {
    case 0x3EE:
        detach_runtime_strategy_link(context.base.link); // The tail jump to 0x00112A20, EAX strategy +0x20.
        return;
    case 0x401:
        return;
    case 0x404:
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause); // 0x00117400
        strategy[0x1C] = u8(strategy[0x1C] - 1);
        decrement_runtime_body_latches(body, actor_of(context), context.latches); // The tail jump to 0x0001FD20.
        return;
    case 0x409:
    case 0x40C:
        effect_off(context);
        return;
    case 0x414:
        effect_off(context);
        decrement_runtime_body_latches(body, actor_of(context), context.latches); // 0x0001FD20, ECX body
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause); // 0x00117400
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            strategy[0x1C] = u8(strategy[0x1C] - 1);
        }
        return;
    }
}
}
