#include "grim_reaper_strategy.h"
#include <cstddef>
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_shape_push.h"
#include "actor_transform.h"
#include "body_animation_commands.h"
#include "body_contacts.h"
#include "body_message_helpers.h"
#include "player_children.h"
#include "player_messages.h"
#include "player_mind_goals.h"
#include "player_strategy_entry.h"
#include "../audio/bank_streams.h"
#include "../scripts/script_actions.h"
#include "gbtg/entity.h"
#include "gbtg/sound_runtime.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
const u32* g_grim_reaper_dizzy = nullptr;

namespace
{
GrimReaperStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<GrimReaperStrategyState>(strategy);
}

const GrimReaperStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<GrimReaperStrategyState>(strategy);
}


void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

constexpr float kOne = 1.f; // DAT_0032B0F8.
constexpr float kEpsilon = 0.001f; // DAT_0032B1A4.
const char* const kStream = "XACT_SOUNDBANK_GSTREAMSDITTIES_GRIMREAPERSTREAM"; // 0x00302DB8.

RuntimePlayerStrategyGoals& goals_of(RuntimeGrimReaperStrategy& context)
{
    return context.base.goals;
}

std::span<u8> strategy_of(RuntimeGrimReaperStrategy& context)
{
    return goals_of(context).entry.strategy;
}

Actor& actor_of(RuntimeGrimReaperStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeGrimReaperStrategy& context)
{
    return goals_of(context).entry.body;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeGrimReaperStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Body +0x15C's bytes.
EntityBitBlock& body_bits(RuntimeGrimReaperStrategy& context)
{
    EntityBitBlock* bits = goals_of(context).body_bits;
    if (bits == nullptr)
    {
        throw Error("the Grim Reaper's body has no +0x15C bit block");
    }
    return *bits;
}

float block(RuntimeGrimReaperStrategy& context, size_t offset)
{
    return read_f32(context.parameters, offset);
}

AudioBankStreams& streams_of(RuntimeGrimReaperStrategy& context)
{
    if (context.streams == nullptr)
    {
        throw Error("the Grim Reaper's stream has no scene audio banks");
    }
    return *context.streams;
}

RuntimeBlobShadows& shadows_of(RuntimeGrimReaperStrategy& context)
{
    if (context.shadows == nullptr)
    {
        throw Error("the Grim Reaper's blob shadow has no scene shadow table");
    }
    return *context.shadows;
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeGrimReaperStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void clear_requests(RuntimeGrimReaperStrategy& context)
{
    clear_runtime_player_mind_requests(*goals_of(context).setup.mind); // 0x00029E80, EBX mind
}

// The inlined 0x0002B8D0 sequence: vtable +0x18 with the old state, the state, 0x00029E80 and vtable +0x14.
void change_state(u16 state, RuntimeGrimReaperStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* goal, RuntimeGrimReaperStrategy& context)
{
    load_runtime_mind_goal(goal, goals_of(context).entry.goals); // 0x0002A8E0
}

RuntimePlayerMindRequest& request(u16 kind, RuntimeGrimReaperStrategy& context)
{
    RuntimeMindGoalContext& mind = goals_of(context).entry.goals;
    return *allocate_runtime_mind_request(mind.anchor, kind, mind.sizes); // 0x00029D90, EBX kind
}

// The body (actor +0x95C) neither turning (+0x30) nor easing (+0x34).
bool body_settled(RuntimeGrimReaperStrategy& context)
{
    const std::span<const u8> body = body_of(context);
    return body_record(body).move_holds == 0 && body_record(body).move_hold_time == 0.f; // DAT_0032B100.
}

// The actor's step: the frame's (none while frozen) by its rate (none while +0x930 holds it).
float actor_step(RuntimeGrimReaperStrategy& context)
{
    return runtime_actor_rate(actor_of(context)) * runtime_frame_step(goals_of(context).clock);
}

// A timer above zero counts down by the actor's step and stops at zero.
void count_down(float& timer, RuntimeGrimReaperStrategy& context)
{
    const float step = runtime_frame_step(goals_of(context).clock);
    const float rate = runtime_actor_rate(actor_of(context));
    const float left = timer;
    if (left > 0.f)
    {
        const float next = left - rate * step;
        timer = next;
        if (!(0.f < next))
        {
            timer = 0.f;
        }
    }
}

u32 next_random(RuntimeGrimReaperStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return random;
}

float random_unit(RuntimeGrimReaperStrategy& context)
{
    return std::bit_cast<float>((next_random(context) & 0x7FFFFFu) | 0x3F800000u) - kOne;
}

// The player kind's pause (DAT_0050E764 / DAT_0050E768 by actor +0xCDC or +0x93C) is clear and decay runs.
bool kind_running(RuntimeGrimReaperStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<const u8> actor = goals.entry.actor_bytes;
    const u32 kind = get(actor, player_field::mode) == 2 || get(actor, player_field::case_link) != 0 ? 1u : 0u;
    return goals.resume.references[kind] == 0 && goals.resume.decay_enabled[goals.resume.decay_index] != 0;
}

// The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
u16 first_goal_kind(RuntimeGrimReaperStrategy& context)
{
    const RuntimePlayerGoalRing* ring = goals_of(context).entry.ring;
    if (ring->tail == ring->capacity)
    {
        return read_u16(goals_of(context).entry.address_zero, 0);
    }
    return read_u16(ring->rows, size_t(ring->stride) * ring->head);
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeGrimReaperStrategy& context)
{
    const u32 target = get(mind_of(context), mind_field::target_record);
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

// Xbox 0x000AE0A0, EAX strategy: he still has a target worth chasing.
bool target_valid(RuntimeGrimReaperStrategy& context)
{
    if (fields_of(strategy_of(context)).fed != 0)
    {
        return false;
    }
    RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    const RuntimePlayerStrategyResume& resume = goals_of(context).resume;
    if (resume.references[1] != 0 || resume.decay_enabled[resume.decay_index] == 0 || resume.references[0] != 0)
    {
        return false;
    }
    const EntityBitBlock* bits = target->body.bits; // Target actor +0x95C -> +0x15C.
    if (bits == nullptr)
    {
        throw Error("0x000AE0A0 reads the bit block of a target with no body");
    }
    if ((bits->bytes[4] & 0x10) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(*bits, 0x25) || runtime_body_bit_set(*bits, 0x1F)) // 0x0001EF10
    {
        return false;
    }
    return !runtime_body_bit_set(*bits, 0x39);
}

// Xbox 0x000ADCC0, EAX actor: chase the target: request 0x2F at his speed within the reach, then 0x27 for 5 s.
void request_chase(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    // Body +8 is his own actor; its +0x38 scales the body's +0x78 reach.
    const float reach = actor_of(context).radius_xz * body_record(body).reach;
    const RuntimeBodyAttachedActor* held = goals_of(context).actions.attached; // Body +0x48.
    float extra = 0.f;
    if (held != nullptr && held->bytes[0x93D] == 1)
    {
        if (held->actor == nullptr)
        {
            throw Error("0x000ADCC0 reads the model of a held actor with no native view");
        }
        if (held->actor->model != nullptr && held->actor->model->cpu.data() != nullptr)
        {
            extra = float(actor_model_sweep_radius(*held->actor)); // 0x0003D9A0, EDI the held actor
        }
    }
    RuntimePlayerMindRequest& chase = request(0x2F, context);
    put_float(chase.bytes, 0x1C, 30.f); // DAT_0032B108.
    put_float(chase.bytes, 0x10, kOne);
    put_float(chase.bytes, 0x14, kOne);
    put_float(chase.bytes, 0x18, extra + reach);
    put_float(chase.bytes, 0x10, fields_of(strategy).speed);
    put_float(chase.bytes, 0x14, fields_of(strategy).speed);
    put_float(chase.bytes, 0x1C, 0.f);
    put_float(body, offsetof(BodyRecord, turn_rates), fields_of(strategy).turn_speed);
   body_record(body).turn_rates[1] = fields_of(strategy).turn_speed;
   body_record(body).turn_rates[2] = kOne;
    RuntimePlayerMindRequest& wait = request(0x27, context);
    put_float(wait.bytes, 0x10, 5.f); // DAT_0032B184.
}

// Xbox 0x000ACE20, EAX target, ECX actor: settled and not flying, the target lies within his reach (block +0).
bool target_within_reach(RuntimeMindTarget* target, RuntimeGrimReaperStrategy& context)
{
    if (target == nullptr || !body_settled(context) || (body_bits(context).bytes[7] & 8) != 0)
    {
        return false;
    }
    const long double distance = runtime_target_distance(actor_of(context), target->bytes); // 0x00041560
    return static_cast<long double>(block(context, 0)) > distance;
}

// Xbox 0x000AD2D0, EBX target, ESI actor: the target has got beyond his reach.
bool target_escaped(RuntimeMindTarget* target, RuntimeGrimReaperStrategy& context)
{
    if (target == nullptr)
    {
        throw Error("0x000AD2D0 reads the point of a missing target (address zero)");
    }
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    const long double dx = static_cast<long double>(position.x) - get(target->bytes, mind_target_field::shape_position).x;
    const long double dy = static_cast<long double>(position.y) - get(target->bytes, mind_target_field::shape_position).y;
    const long double dz = static_cast<long double>(position.z) - get(target->bytes, mind_target_field::shape_position).z;
    return std::sqrt(dz * dz + (dy * dy + dx * dx)) > static_cast<long double>(block(context, 0));
}

void set_model_flag(u8 value, RuntimeGrimReaperStrategy& context)
{
    const Actor& actor = actor_of(context);
    if (actor.model == nullptr)
    {
        throw Error("0x00052F80 reads the Grim Reaper's model data through address zero");
    }
    if (actor_model_record_bit(actor.model->cpu, 0x10)) // 0x00052F80, EDX the model data
    {
        context.model_nodes(0x10, value); // 0x000F96F0 on +0x204 and +0x310
    }
}

// Xbox 0x000AD0D0: the swing grabs: the body's 0xD2 attack row, hands' contact shapes (3 and 0xD) and callout 0x2E.
void start_grab(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).grabbing != 0)
    {
        return;
    }
    Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    u32 row = 0;
    if (context.attack_resource.data() != nullptr && read_u32(context.attack_resource, 8) != 0)
    {
        const u8* found = find_runtime_attachment_second_request(
            *context.attack_tables.at(context.attack_resource.data()), 0xD2); // 0x0001C5A0, EBX 0xD2
        row = found == nullptr ? 0u : context.address_of(found);
    }
    strategy_base(strategy).attacking = 1;
    body_record(body).hit_record = row;
    body_record(body).unk_6A = u8(body_record(body).unk_6A + 1);
    if (context.collision != nullptr)
    {
        activate_runtime_body_contact_shapes(*context.collision, actor, 3, 1, kOne); // 0x0013F420
        body_record(body).contact_count = 0;
    }
    if (context.collision != nullptr)
    {
        activate_runtime_body_contact_shapes(*context.collision, actor, 0xD, 1, kOne);
        body_record(body).contact_count = 0;
    }
   body_record(body).unk_74C = 1;
    set_model_flag(1, context);
    dispatch_actor_attachment_records(context.resource.resource.data(), 0x2E, record_runtime_body_attachment_event, nullptr,
        context.callouts, context.address_of(strategy.data()) + offsetof(GrimReaperStrategyState, events));
        // 0x0003BE20, 0x0013B8B0
    goals_of(context).room = nullptr; // Actor +0x954 -> +8 -> +0xCA0.
    fields_of(strategy).grabbing = 1u;
}

// Xbox 0x000AD200: the grab lets go.
void end_grab(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).grabbing == 0)
    {
        return;
    }
    const std::span<u8> body = body_of(context);
    strategy_base(strategy).attacking = 0;
    body_record(body).hit_record = 0;
   body_record(body).unk_88 = 0;
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, 3); // 0x0013F4D0
    }
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, 0xD);
    }
   body_record(body).unk_74C = 0;
    clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).events), context.lists); // 0x0013B810
    set_model_flag(0, context);
    fields_of(strategy).grabbing = 0u;
}

// Xbox 0x000ACF10, ESI strategy: the swing winds up.
void start_swing(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 swing = fields_of(strategy).swing;
    if (swing == 1)
    {
        return;
    }
    if (swing == 2)
    {
        end_grab(context); // 0x000AD200
    }
    fields_of(strategy).swing = 1u;
    fields_of(strategy).wind_up = block(context, 0x34);
}

// Xbox 0x000ACF40, ESI strategy: the swing ends.
void end_swing(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 swing = fields_of(strategy).swing;
    if (swing == 0)
    {
        return;
    }
    if (swing == 2)
    {
        end_grab(context); // 0x000AD200
    }
    fields_of(strategy).swing = 0u;
}

// Xbox 0x000ADB90, ECX strategy, EDX actor: settled, the cue counts down; at zero it is drawn again and he points or
// laughs (sound slot 1).
void cue_sound(RuntimeGrimReaperStrategy& context)
{
    if (!body_settled(context))
    {
        return;
    }
    const std::span<u8> strategy = strategy_of(context);
    const float low = block(context, 0x20);
    const float high = block(context, 0x24);
    const float roll = random_unit(context);
    const float left = fields_of(strategy).cue - actor_step(context);
    fields_of(strategy).cue = left;
    if (0.f < left)
    {
        return;
    }
    fields_of(strategy).cue = (high - low) * roll + left + low;
    const float pick = random_unit(context);
    const char* sound = 0.66f < pick ? "XACT_SOUNDBANK_GGRIMREAPER_WOOFAK" // DAT_0032B550, 0x00302C38
        : "XACT_SOUNDBANK_GGRIMREAPER_POINT"; // 0x00302BB8
    request_actor_sound(actor_of(context), 1, sound, context.sounds); // 0x00041D00, ECX 1
}

// Xbox 0x000ADDC0, ESI strategy: the fly bit wears off; while he is not appearing, his pace and turn speed grow
// each time the pace time runs out, and a valid target in the chase or the swing is chased afresh.
void tick_pace(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    EntityBitBlock& bits = body_bits(context);
    if ((bits.bytes[7] & 8) != 0)
    {
        count_down(fields_of(strategy).fly, context);
        if (kEpsilon > fields_of(strategy).fly)
        {
            bits.bytes[7] = u8(bits.bytes[7] & 0xF7);
        }
    }
    if (fields_of(strategy).appearing != 0)
    {
        return;
    }
    count_down(fields_of(strategy).pace_time, context);
    if (!(kEpsilon > fields_of(strategy).pace_time))
    {
        return;
    }
    fields_of(strategy).pace_time = block(context, 0x2C);
    if (kOne > fields_of(strategy).speed)
    {
        if (fields_of(strategy).slowed == 0)
        {
            const float speed = block(context, 8) + fields_of(strategy).speed;
            fields_of(strategy).speed = speed;
            if (speed > kOne)
            {
                fields_of(strategy).speed = kOne;
            }
        }
        else
        {
            const float held = fields_of(strategy).held_speed + block(context, 8);
            fields_of(strategy).held_speed = held;
            if (held > kOne)
            {
                fields_of(strategy).held_speed = kOne;
            }
            fields_of(strategy).slow = block(context, 0x30) * block(context, 8) + fields_of(strategy).slow;
            fields_of(strategy).slow_length = block(context, 0x30) * block(context, 8)
                + fields_of(strategy).slow_length;
        }
    }
    if (block(context, 0x14) > fields_of(strategy).turn_speed)
    {
        fields_of(strategy).turn_speed = block(context, 0x10) + fields_of(strategy).turn_speed;
    }
    if (target_valid(context)) // 0x000AE0A0
    {
        const u16 state = strategy_base(strategy).state;
        if (state == 0x401 || state == 0x40C)
        {
            clear_requests(context); // 0x00029E80
            request_chase(context); // 0x000ADCC0
        }
    }
}

// Xbox 0x000ADFB0, ECX strategy: settled, a hit's slow-down wears off and his pace comes back with it.
void recover_pace(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).slowed == 0 || !body_settled(context))
    {
        return;
    }
    const float step = runtime_frame_step(goals_of(context).clock);
    const float rate = runtime_actor_rate(actor_of(context));
    const float slow = fields_of(strategy).slow;
    if (slow > 0.f)
    {
        const float left = slow - rate * step;
        fields_of(strategy).slow = left;
        if (!(0.f < left))
        {
            fields_of(strategy).slow = 0.f;
            fields_of(strategy).speed = fields_of(strategy).held_speed;
            fields_of(strategy).slowed = 0u;
            return;
        }
    }
    const float speed = (kOne - fields_of(strategy).slow / fields_of(strategy).slow_length)
        * fields_of(strategy).held_speed;
    fields_of(strategy).speed = speed;
    if (!(speed < fields_of(strategy).held_speed))
    {
        fields_of(strategy).speed = fields_of(strategy).held_speed;
        fields_of(strategy).slowed = 0u;
    }
    clear_requests(context); // 0x00029E80
}

RuntimeMindTarget* select_target(RuntimeGrimReaperStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    if (select_runtime_strategy_target(target, 0, goals_of(context))) // 0x00139AA0
    {
        notice_runtime_strategy_target(10, context.checks); // 0x0013A430
    }
    return target;
}

// Xbox 0x000ACD40, ECX strategy, EAX actor: the chase.
void update_chase(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimeMindTarget* target = select_target(context);
    if (fields_of(strategy).appearing != 0)
    {
        if (!requests_pending(context))
        {
            fields_of(strategy).appearing = 0u;
        }
        return;
    }
    if (!target_valid(context)) // 0x000AE0A0
    {
        change_state(0x40D, context);
        return;
    }
    if (target_within_reach(target, context)) // 0x000ACE20
    {
        change_state(0x40C, context); // 0x0002B8D0, EDI 0x40C
        return;
    }
    if (!requests_pending(context))
    {
        request_chase(context); // 0x000ADCC0
    }
}

// Xbox 0x000ACEA0, ESI strategy, EDX actor: knocked down, he chases again once his requests are done or the body's
// +0x15C byte 6 bit 0 is set.
void update_knocked_down(RuntimeGrimReaperStrategy& context)
{
    if (!requests_pending(context) || (body_bits(context).bytes[6] & 1) != 0)
    {
        change_state(0x401, context);
    }
}

// Xbox 0x000ACF70, ECX strategy, EAX actor: the swing.
void update_swing(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimeMindTarget* target = select_target(context);
    if (!target_valid(context))
    {
        change_state(0x40D, context);
        return;
    }
    if (target_escaped(target, context)) // 0x000AD2D0
    {
        change_state(0x401, context);
        return;
    }
    if (!requests_pending(context))
    {
        request_chase(context); // 0x000ADCC0
    }
    const u32 swing = fields_of(strategy).swing;
    if (swing == 1)
    {
        float wind_up = fields_of(strategy).wind_up;
        const bool done = count_down_runtime_strategy_timer(wind_up, goals_of(context)); // 0x0002B9F0
        fields_of(strategy).wind_up = wind_up;
        if (done && fields_of(strategy).swing != 2)
        {
            if (fields_of(strategy).swing == 2)
            {
                end_grab(context); // 0x000AD200
            }
            fields_of(strategy).swing = 2u;
            start_grab(context); // 0x000AD0D0
        }
        return;
    }
    if (swing == 2 && context.collision != nullptr && context.collision->overrides != nullptr)
    {
        expire_runtime_body_contact_overrides(context.collision->overrides, 0.5f); // 0x0013EB60, DAT_0032B0F4
    }
}

// Xbox 0x000AD330, ESI strategy: idle, he chases a selected, valid target.
void update_idle(RuntimeGrimReaperStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    if (select_runtime_strategy_target(target, 0, goals_of(context)) && target_valid(context))
    {
        change_state(0x401, context);
    }
}

// The blob shadow at the actor, raised by half its height (0x0003DD50).
Vec3 shadow_point(RuntimeGrimReaperStrategy& context)
{
    Vec3 position{};
    float height = 0.f;
    float radius = 0.f;
    runtime_actor_cylinder_metrics(actor_of(context), position, height, radius); // 0x0003DD50, ECX actor
    return Vec3{position.x, height * 0.5f + position.y, position.z}; // DAT_0032B0F4
}
}

// Xbox 0x000AD390
void grim_reaper_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(GrimReaperStrategyState);
    output.parameters = 0x18;
}

// Xbox 0x000AD400
bool initialize_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    fields_of(strategy).speed = 0.5f; // DAT_0032B0F4
    fields_of(strategy).fed = 0u;
    fields_of(strategy).parameters = context.parameters_address;
    fields_of(strategy).appearing = 1u;
    fields_of(strategy).turn_speed = kOne;
    body_record(body).ease_rate = 10.f; // DAT_0032B10C
    fields_of(strategy).grabbing = 0u;
    EntityBitBlock& bits = body_bits(context);
    bits.bytes[0] = u8(bits.bytes[0] | 0x80);
    fields_of(strategy).fly = block(context, 0x28);
    fields_of(strategy).pace_time = block(context, 0x2C);
    bits.bytes[7] = u8(bits.bytes[7] | 8);
    const float low = block(context, 0x20);
    const float high = block(context, 0x24);
    const float roll = random_unit(context);
    g_flags_ea9c |= 0x10u; // DAT_0050EA9C: no more time limits while he is out.
    fields_of(strategy).cue = (high - low) * roll + low;
    acquire_tracked_audio_bank(kStream, 5, streams_of(context)); // 0x00038910, EAX name
    fields_of(strategy).shadow = register_runtime_blob_shadow(shadow_point(context), 110.f, 140.f,
        shadows_of(context)); // 0x00119A60
    return true;
}

// Xbox 0x000AD570
void destroy_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context)
{
    release_tracked_audio_bank(kStream, streams_of(context)); // The inlined 0x00038970.
    const std::span<u8> strategy = strategy_of(context);
    const i32 slot = fields_of(strategy).shadow;
    if (slot < 0)
    {
        return;
    }
    RuntimeBlobShadows& shadows = shadows_of(context);
    shadows.entries[size_t(slot)].active = 0;
    --shadows.count;
    if (slot < shadows.next_free)
    {
        shadows.next_free = slot;
    }
    fields_of(strategy).shadow = -1;
}

// Xbox 0x000AD600
void update_runtime_grim_reaper_strategy(RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    const i32 slot = fields_of(strategy).shadow;
    if (slot >= 0)
    {
        shadows_of(context).entries[size_t(slot)].position = shadow_point(context);
    }
    if (g_grim_reaper_dizzy == nullptr)
    {
        throw Error("DAT_00512170 (the DizzyReaper powerup row) is read outside a native scene");
    }
    if (state != 0x3EA && *g_grim_reaper_dizzy != 0)
    {
        change_state(0x3EA, context);
    }
    update_runtime_base_strategy(goals_of(context)); // 0x0002C130
    switch (state)
    {
    case 0x3EA:
        if (!requests_pending(context))
        {
            load_goal(context.goal_dizzy, context);
        }
        return;
    case 0x401:
        cue_sound(context); // 0x000ADB90
        recover_pace(context); // 0x000ADFB0
        tick_pace(context); // 0x000ADDC0
        update_chase(context); // 0x000ACD40
        return;
    case 0x409:
        tick_pace(context);
        update_knocked_down(context); // 0x000ACEA0
        return;
    case 0x40C:
        recover_pace(context);
        cue_sound(context);
        tick_pace(context);
        update_swing(context); // 0x000ACF70
        return;
    case 0x40D:
        cue_sound(context);
        tick_pace(context);
        update_idle(context); // 0x000AD330
        return;
    default:
        return;
    }
}

// Xbox 0x000AD7D0
void dispatch_runtime_grim_reaper_message(std::span<const u32> message, RuntimeGrimReaperStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const u32 self = strategy_base(strategy).actor;
    switch (message[0])
    {
    case 0x27:
        if (strategy_base(strategy).state == 0x3F2)
        {
            return;
        }
        break;
    case 0x31:
    {
        // A death: his target's, or one whose body (+0x95C, its +0x270) he holds, feeds him.
        RuntimeMindTarget* target = mind_target(context);
        if (target != nullptr && get(target->bytes, mind_target_field::actor) == message[1])
        {
            fields_of(strategy).fed = 1u;
        }
        else
        {
            Actor* dead = context.views.actor_at(message[1]);
            if (dead == nullptr)
            {
                throw Error("0x000AD7D0 reads the body of a dead actor outside the scene");
            }
            const RuntimeStrategyBodyView body = context.views.body_of(*dead);
            if (read_u32(body.bytes, 0x270) != self) // The dead actor's body (+0x95C) +0x270.
            {
                return;
            }
        }
        request_actor_sound(actor_of(context), 6, "XACT_SOUNDBANK_GGRIMREAPER_SUCKLIFE", context.sounds); // 0x00041D00
        change_state(0x3F2, context);
        return;
    }
    case 0x36:
        notice_runtime_strategy_hostile_target(10, context.checks); // 0x0013A4F0
        return;
    case 0x6E:
    {
        if (!runtime_decay_enabled(goals.clock)) // 0x00117560 with EAX 1
        {
            return;
        }
        if (message[1] == self)
        {
            if (fields_of(strategy).slowed == 0)
            {
                fields_of(strategy).slowed = 1u;
                fields_of(strategy).held_speed = fields_of(strategy).speed;
            }
            fields_of(strategy).speed = kEpsilon;
            const float slow = block(context, 0x30) * fields_of(strategy).held_speed;
            fields_of(strategy).slow = slow;
            fields_of(strategy).slow_length = slow;
            if (strategy_base(strategy).pause_count == 0 && kind_running(context)
                && (goals.entry.ring == nullptr || first_goal_kind(context) == 0))
            {
                change_state(0x401, context);
            }
        }
        if (runtime_message_lands_on_actor(message, self)) // 0x001408A0
        {
            change_state(0x409, context);
            return;
        }
        if (runtime_message_lands_from_mind_target(message, mind_of(context), goals.target_at)) // 0x00140910
        {
            change_state(0x40D, context);
        }
        return;
    }
    case 0x82:
        react_runtime_strategy_cower(context.checks); // 0x001398A0
        return;
    case 0x84:
        react_runtime_strategy_startle(context.checks); // 0x001399A0
        return;
    case 0x86:
        if (message[2] == 2)
        {
            change_state(0x3EA, context);
        }
        return;
    case 0x87:
        if (message[2] == 2)
        {
            change_state(0x401, context);
        }
        return;
    default:
        break;
    }
    dispatch_runtime_base_strategy_message(message, context.base); // 0x0002BC10
}

// Xbox 0x000ADA70
void enter_runtime_grim_reaper_state(u16 state, u16 previous, RuntimeGrimReaperStrategy& context)
{
    switch (state)
    {
    case 0x3EA:
        load_goal(context.goal_dizzy, context);
        return;
    case 0x401:
        // 0x000ACD00, EAX strategy: appearing, he waits 5 s first.
        if (fields_of(strategy_of(context)).appearing != 0)
        {
            put_float(request(0x27, context).bytes, 0x10, 5.f); // DAT_0032B184
        }
        return;
    case 0x409:
        // 0x000ACE80, EAX strategy.
        clear_requests(context);
        load_goal(context.goal_knocked_down, context);
        return;
    case 0x40C:
        start_swing(context); // 0x000ACF10
        return;
    case 0x40D:
        return;
    default:
        enter_runtime_base_strategy_state(goals_of(context).entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000ADB10
void leave_runtime_grim_reaper_state(u16 state, u16 next, RuntimeGrimReaperStrategy& context)
{
    (void)next;
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3EA:
    case 0x401:
    case 0x409:
    case 0x40D:
        return;
    case 0x40C:
        end_swing(context); // 0x000ACF40
        return;
    default:
        if (state == 0x3FC || state == 0x3FF || state == 0x415)
        {
            --strategy_base(strategy).pause_count;
        }
        return;
    }
}
}
