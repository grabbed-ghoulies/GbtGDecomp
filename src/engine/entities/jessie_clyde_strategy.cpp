#include "jessie_clyde_strategy.h"
#include "gbtg/retail_field.h"
#include <cstddef>
#include "body_fields.h"
#include "strategy_fields.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_action_ids.h"
#include "body_animation_commands.h"
#include "body_message_helpers.h"
#include "body_orientation.h"
#include "player_children.h"
#include "player_goal_cleanup.h"
#include "player_messages.h"
#include "player_mind_events.h"
#include "player_mind_goals.h"
#include "player_strategy.h"
#include "player_strategy_entry.h"
#include "player_strategy_goals.h"
#include "../graphics/actor_effect_stream.h"
#include "../navigation/navigation.h"
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include "player_fields.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
JessieClydeStrategyState& fields_of(std::span<u8> strategy)
{
    return strategy_view<JessieClydeStrategyState>(strategy);
}

const JessieClydeStrategyState& fields_of(std::span<const u8> strategy)
{
    return strategy_view<JessieClydeStrategyState>(strategy);
}


constexpr float kHalfPi = 1.5707964f; // DAT_0032B140.

RuntimePlayerStrategyGoals& goals_of(RuntimeJessieClydeStrategy& context)
{
    return context.shared.base.goals;
}

std::span<u8> strategy_of(RuntimeJessieClydeStrategy& context)
{
    return goals_of(context).entry.strategy;
}

Actor& actor_of(RuntimeJessieClydeStrategy& context)
{
    return goals_of(context).entry.actor;
}

// Actor +0x958.
std::span<u8> mind_of(RuntimeJessieClydeStrategy& context)
{
    return goals_of(context).entry.events.mind;
}

// Actor +0x95C.
std::span<u8> body_of(RuntimeJessieClydeStrategy& context)
{
    return goals_of(context).entry.body;
}

u32 actor_address(RuntimeJessieClydeStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    return goals.address_of(goals.entry.actor_bytes.data());
}

EntityBitBlock& body_bits(RuntimeJessieClydeStrategy& context, const char* caller)
{
    EntityBitBlock* bits = goals_of(context).body_bits; // Body +0x15C.
    if (bits == nullptr)
    {
        throw Error(std::string(caller) + " reads the body +0x15C bit block of a body with none");
    }
    return *bits;
}

const JessieClydeParameters& parameters_of(RuntimeJessieClydeStrategy& context)
{
    return retail_view<JessieClydeParameters>(context.parameters);
}

// Mind +0x58: the first pending mind request.
bool requests_pending(RuntimeJessieClydeStrategy& context)
{
    return goals_of(context).setup.mind->first != nullptr;
}

void clear_requests(RuntimeJessieClydeStrategy& context)
{
    clear_runtime_player_mind_requests(*goals_of(context).setup.mind); // 0x00029E80, EBX mind
}

// The inlined 0x0002B8D0 sequence: vtable +0x18 with the old state, the state, 0x00029E80 and vtable +0x14.
void change_state(u16 state, RuntimeJessieClydeStrategy& context)
{
    change_runtime_player_strategy_state(goals_of(context).strategy, state, goals_of(context).setup);
}

void load_goal(const char* name, RuntimeJessieClydeStrategy& context)
{
    load_runtime_mind_goal(name, goals_of(context).entry.goals); // 0x0002A8E0
}

// The mind's target record (+0x10), or null.
RuntimeMindTarget* mind_target(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> mind = mind_of(context);
    if (mind.data() == nullptr)
    {
        return nullptr;
    }
    const u32 target = mind_record(mind).target_record;
    return target == 0 ? nullptr : goals_of(context).target_at(target);
}

float roll(RuntimeJessieClydeStrategy& context)
{
    u32& random = goals_of(context).random; // DAT_0054DD7C.
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// Actor +0x964 +offset, or 0 without the attribute resource.
float attribute(RuntimeJessieClydeStrategy& context, size_t offset)
{
    const std::span<const u8> attributes = context.shared.checks.attributes;
    return attributes.data() != nullptr ? read_f32(attributes, offset) : 0.f;
}

// Actor +0x978 above zero, or else actor +0x969.
u32 provoked(RuntimeJessieClydeStrategy& context)
{
    const std::span<const u8> actor = goals_of(context).entry.actor_bytes;
    return player_record(actor).countdown_56 > 0.f ? 1u : u32(player_record(actor).provoked); // DAT_0032B100.
}

// The current effect (+0x13C) ends (message 5).
void end_effect(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const std::array<u32, 2> message{5, 0};
    context.object_message(fields_of(strategy).effect, message); // Effect vtable +0xC.
    fields_of(strategy).effect = 0u;
}

// The effect named at +0x4C, made with the actor at +8 (0x00107720, 0x00107520, 0x000663E0), kept at +0x13C.
void make_effect(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const char* name = fields_of(strategy).effect_name.data();
    fields_of(strategy).effect = context.create_effect(name, actor_address(context));
}

// Xbox 0x000B8080, AX state: an attack state (0x3ED..0x3EF).
bool attacking(u16 state)
{
    return state > 0x3EC && state < 0x3F0;
}

// Xbox 0x000B83A0: the transformation's steps.
void step_transform(u32 step, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    const std::span<u8> body = body_of(context);
    if (step == 1)
    {
        clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).events), context.lists); // 0x0013B810
        clear_requests(context);
        load_goal(context.goal_transform, context);
        make_effect(context);
        increment_runtime_body_latches(body, actor, context.latches); // The inlined 0x0001FC40.
        fields_of(strategy).transform_step = 1u;
        return;
    }
    if (step != 2)
    {
        fields_of(strategy).transform_step = step;
        return;
    }
    fields_of(strategy).calm = 0u;
    EntityBitBlock& bits = body_bits(context, "0x000B83A0");
    bits.bytes[1] = u8(bits.bytes[1] | 0x80);
    if (fields_of(strategy).effect != 0)
    {
        end_effect(context);
    }
    decrement_runtime_body_latches(body, actor, context.latches); // The inlined 0x0001FD20.
    const char* flower = "noflower"; // 0x00324304.
    std::memcpy(body.data() + offsetof(BodyRecord, variant), flower, std::strlen(flower) + 1);
    if (actor.model == nullptr)
    {
        throw Error("0x00052F80 reads Jessie Clyde's model data through address zero");
    }
    if (actor_model_record_bit(actor.model->cpu, 0x10)) // 0x00052F80, EDX the model data
    {
        context.model_nodes(0x10, 0); // 0x000F96F0 on +0x204 and +0x310
    }
    fields_of(strategy).transform_step = 2u;
}

// Xbox 0x000B8270, EAX actor: her target is in reach of the eye: a clear path to it, within a quarter turn of her
// facing and nearer than the block's +0x10.
bool target_in_view(RuntimeJessieClydeStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        return false;
    }
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const Vec3 point{mind_target_record(target->bytes).shape.position.x,
        mind_target_record(target->bytes).shape.position.y, mind_target_record(target->bytes).shape.position.z};
    const Vec3 position = actor_world_position(actor_of(context)); // 0x000412A0
    i32 last_cell = 0;
    if (!navigation_path_clear(position, point, 1, 0, 0, 0, nullptr, &last_cell,
            i16(read_u16(goals.entry.actor_bytes, 6)))) // 0x0012CC50
    {
        return false;
    }
    const float facing = read_f32(goals.entry.actor_bytes, 0x18);
    const Vec3 from = actor_world_position(actor_of(context)); // 0x000412A0
    const Vec3 direction{point.x - from.x, point.y - from.y, point.z - from.z};
    float angle = 0.f;
    runtime_direction_angle(direction, angle); // 0x00105F90
    if (engine_absolute(facing - angle) > kHalfPi) // 0x00152BB0
    {
        return false;
    }
    return runtime_actor_point_within(actor_of(context), target->bytes, parameters_of(context).eye_reach); // 0x000416C0
}

// Xbox 0x000B8100, EAX actor, EDI strategy: calm, she walks about until struck, provoked or seeing her target near,
// then transforms.
void tick_calm(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (fields_of(strategy).transform_step)
    {
    case 0:
        if (provoked(context) == 0 && fields_of(strategy).struck == 0 && !target_in_view(context))
        {
            if (!requests_pending(context))
            {
                load_goal(context.goal_random_walk, context);
            }
            return;
        }
        step_transform(1, context);
        return;
    case 1:
    {
        if (!requests_pending(context))
        {
            step_transform(2, context);
            return;
        }
        const RuntimeMindTarget* target = mind_target(context);
        if (target != nullptr)
        {
            face_runtime_body_target(body_of(context), actor_of(context), target->bytes); // 0x001345F0, ECX body
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000B87F0, EAX actor: the cue is out and her target stands, fights, is not down and is within the
// block's +4.
bool ready_to_attack(RuntimeJessieClydeStrategy& context)
{
    const RuntimeMindTarget* target = mind_target(context);
    if (!(fields_of(strategy_of(context)).cue <= 0.001f) || target == nullptr) // DAT_0032B1A4.
    {
        return false;
    }
    if (target->body.energy == 0) // Target body +0x94.
    {
        return false;
    }
    const EntityBitBlock* bits = target->body.bits; // Target body +0x15C.
    if (bits == nullptr)
    {
        throw Error("0x000B87F0 reads the bit block of a target with no body");
    }
    if ((bits->bytes[3] & 8) != 0 || (bits->bytes[4] & 0x10) != 0)
    {
        return false;
    }
    if (runtime_body_bit_set(*bits, 0x25) || runtime_body_bit_set(*bits, 0x1F)) // 0x0001EF10
    {
        return false;
    }
    if (!runtime_body_timer_is_idle(context.shared.views.body_of(target->actor).bytes)) // 0x00024C20
    {
        return false;
    }
    return runtime_actor_point_within(actor_of(context), target->bytes, parameters_of(context).attack_reach); // 0x000416C0
}

// Xbox 0x000B86E0, ECX strategy: the attack: the special (0x3EF) when open or by the attributes' +8 chance, else the
// knockdown (0x3EE) by the +0xC chance or the shout (0x3ED).
void choose_attack(RuntimeJessieClydeStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    if (mind_target(context) == nullptr)
    {
        return;
    }
    goals.room = nullptr; // Actor +0x954 -> +8 -> +0xCA0.
    if (fields_of(strategy_of(context)).open == 0)
    {
        const float chance = attribute(context, 8);
        if (chance < roll(context))
        {
            change_state(actor_event_chance(goals.random, attribute(context, 0xC)) ? 0x3EE : 0x3ED,
                context); // 0x00019200
            return;
        }
    }
    change_state(0x3EF, context);
}

// Xbox 0x000B81B0, EBX actor, EDI strategy: angry, she notices her target, counts the cue down and attacks once
// ready, else walks about angrily.
void tick_angry(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (mind_target(context) != nullptr)
    {
        notice_runtime_strategy_target(0x14, context.shared.checks); // 0x0013A430
    }
    float cue = fields_of(strategy).cue;
    count_down_runtime_strategy_timer(cue, goals_of(context)); // Inlined 0x0002B9F0.
    fields_of(strategy).cue = cue;
    if (ready_to_attack(context)) // 0x000B87F0
    {
        choose_attack(context); // The tail jump to 0x000B86E0.
        return;
    }
    if (!requests_pending(context))
    {
        load_goal(context.goal_angry_random_walk, context);
    }
}

// Xbox 0x000B80A0, EAX actor, ECX strategy: state 0x401.
void tick_idle(RuntimeJessieClydeStrategy& context)
{
    RuntimeMindTarget* target = nullptr;
    if (select_runtime_strategy_hostile_target(target, 0, goals_of(context)) // 0x00139C70, EDX 0
        && runtime_strategy_abandons_target(*target, context.shared.checks) != 0 // 0x00138410, EDX target
        && retreat_runtime_strategy_from_reached_target(context.shared.checks) != 0) // 0x001383A0
    {
        return;
    }
    if (fields_of(strategy_of(context)).calm != 0)
    {
        tick_calm(context); // 0x000B8100
        return;
    }
    tick_angry(context); // 0x000B81B0
}

// Xbox 0x000B8A70, EAX step, ECX strategy: the shout's steps; step 2 scares the room from the shout's joint, pushed
// out along her facing.
void step_shout(u32 step, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).shout_step == step)
    {
        return;
    }
    fields_of(strategy).shout_step = step;
    if (step == 1)
    {
        load_goal(context.goal_shout, context);
        return;
    }
    if (step != 2)
    {
        return;
    }
    const Actor& actor = actor_of(context);
    Vec3 point{};
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 joint = actor_lookup_joint_index(actor.model->cpu, 5) & 0xFFu; // 0x00052FC0
        if (joint != 0xFF)
        {
            actor_transform_joint_point(actor, u16(joint), actor_transform_origin, point); // 0x0003AEB0, DAT_0050DD80
        }
    }
    const float facing = read_f32(goals_of(context).entry.actor_bytes, 0x18);
    const float forward = parameters_of(context).scare_forward;
    point.x = float(retail_sin(facing) * forward + point.x); // 0x00153320
    const long double cosine = retail_cos(facing); // 0x00153230
    EntityBitBlock& bits = body_bits(context, "0x000B8A70");
    point.y = parameters_of(context).scare_rise + point.y;
    bits.bytes[2] = u8(bits.bytes[2] & 0x7F);
    point.z = float(cosine * forward + point.z);
    context.scare_at(point, actor_address(context), parameters_of(context).scare_intensity, parameters_of(context).scare_scale, 1,
        1); // 0x0013B270
}

// Xbox 0x000B8970, EAX strategy: the shout (0x3ED): back to 0x401 once its target is down, the scare on the
// shout's animation event (body bit block byte 2 bit 7), else she faces the target.
void tick_shout(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    const u32 step = fields_of(strategy).shout_step;
    if (step == 2)
    {
        if (!requests_pending(context))
        {
            change_state(0x401, context);
        }
        return;
    }
    if (step != 1)
    {
        return;
    }
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr)
    {
        // 0x401, then the record at address zero's +0x10 actor.
        throw Error("0x000B8970 reads the body of a missing target (address zero)");
    }
    const EntityBitBlock* target_bits = target->body.bits; // Target body +0x15C.
    if (target_bits == nullptr)
    {
        throw Error("0x000B8970 reads the bit block of a target with no body");
    }
    if ((target_bits->bytes[3] & 8) != 0 || target->body.energy == 0)
    {
        change_state(0x401, context);
    }
    if ((body_bits(context, "0x000B8970").bytes[2] & 0x80) != 0)
    {
        step_shout(2, context); // The tail jump to 0x000B8A70.
        return;
    }
    const RuntimeMindTarget* current = mind_target(context);
    if (current != nullptr)
    {
        face_runtime_body_target(body_of(context), actor_of(context), current->bytes); // The tail jump to 0x001345F0.
    }
}

// Xbox 0x000B8E30, EAX actor, ESI strategy: the knockdown (0x3EE): its effect ends on the body's bit block byte 0
// bit 4; done, she goes to 0x3F2 after message 0x57, else 0x401.
void tick_knockdown(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (!requests_pending(context))
    {
        change_state(fields_of(strategy).follow_up != 0 ? 0x3F2 : 0x401, context);
        return;
    }
    if (fields_of(strategy).knockdown_effect == 0)
    {
        return;
    }
    EntityBitBlock& bits = body_bits(context, "0x000B8E30");
    if ((bits.bytes[0] & 0x10) == 0)
    {
        return;
    }
    bits.bytes[0] = u8(bits.bytes[0] & 0xEF);
    end_effect(context);
    decrement_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F2F0
    fields_of(strategy).knockdown_effect = 0u;
}

// Xbox 0x000B9160, EAX strategy: the special's time runs out this tick, or message 0x6E asked it to end.
bool special_over(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    float time = fields_of(strategy).special_time;
    const bool done = count_down_runtime_strategy_timer(time, goals_of(context)); // Inlined 0x0002B9F0.
    fields_of(strategy).special_time = time;
    if (done)
    {
        return true;
    }
    return fields_of(strategy).unk_D4 != 0;
}

// Xbox 0x000B90D0, EAX strategy: the special (0x3EF) ends in 0x3F2 (asked to) or 0x401.
void tick_special(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).unk_D8 != 0)
    {
        RuntimeMindTarget* target = nullptr;
        if (!select_runtime_strategy_target(target, 0, goals_of(context))) // 0x00139AA0
        {
            return;
        }
    }
    if (fields_of(strategy).special_step != 2 || !special_over(context)) // 0x000B9160
    {
        return;
    }
    change_state(fields_of(strategy).unk_D4 != 0 ? 0x3F2 : 0x401, context);
}

// Xbox 0x000B9300, EAX step, ESI strategy: the special's steps; step 2 times it, makes its effect and raises every
// body latch.
void step_special(u32 step, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).special_step == step)
    {
        return;
    }
    fields_of(strategy).special_step = step;
    if (step == 1)
    {
        load_goal(context.goal_special_prepare, context);
        fields_of(strategy).unk_D8 = 1u;
        fields_of(strategy).unk_D0 = 1u;
        fields_of(strategy).unk_D4 = 0u;
        fields_of(strategy).special_effect = 0u;
        body_record(body_of(context)).move_class = 6;
        strategy_base(strategy).attacking = 1;
        return;
    }
    if (step != 2)
    {
        return;
    }
    const float shortest = parameters_of(context).special_time_low;
    fields_of(strategy).special_time = (parameters_of(context).special_time_high - shortest) * roll(context) + shortest;
    make_effect(context);
    increment_runtime_body_latch_mask(body_of(context), 0x3F, actor_of(context), context.latches); // 0x0001F3F0
    fields_of(strategy).special_effect = 1u;
}

// Xbox 0x000B9260, EAX strategy: message 0x6E from her target during the special's second step: ended at once when
// it began here (+0xD0), else her target goes and she turns on a hostile one ("special") or back to 0x401.
void special_struck(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).special_step != 2 || strategy_base(strategy).state != 0x3EF)
    {
        return;
    }
    if (fields_of(strategy).unk_D0 != 0)
    {
        fields_of(strategy).unk_D4 = 1u;
        return;
    }
    const u32 none = 0;
    std::memcpy(mind_of(context).data() + 0x10, &none, 4); // Written directly, not through 0x0002A030.
    RuntimeMindTarget* target = nullptr;
    select_runtime_strategy_hostile_target(target, 0, goals_of(context)); // 0x00139C70, EDX 0
    fields_of(strategy).unk_D8 = 1u;
    fields_of(strategy).unk_D0 = 1u;
    fields_of(strategy).unk_D4 = 0u;
    if (target != nullptr)
    {
        load_goal(context.goal_special, context);
        return;
    }
    change_state(0x401, context);
}

// Xbox 0x000B91F0, EDI strategy: message 0x2D during the special's second step turns her on the named actor's
// target record (+0x8F8) when it is free (+0x18 clear).
bool special_retarget(u32 actor, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).special_step != 2 || strategy_base(strategy).state != 0x3EF)
    {
        return false;
    }
    const u32 record = context.shared.views.first_live_attachment_at(actor); // Actor +0x8F8.
    if (record == 0 || goals_of(context).target_at(record)->bytes[0x18] != 0)
    {
        return false;
    }
    set_runtime_mind_target(goals_of(context).entry.events, record); // 0x0002A030, ESI mind, EAX record
    fields_of(strategy).unk_D8 = 0u;
    fields_of(strategy).unk_D0 = 0u;
    clear_requests(context);
    load_goal(context.goal_special, context);
    return true;
}

// Xbox 0x000B9B50, EAX strategy, ECX actor: message 0x77 from her target's actor within the block's +0xC, while
// she is angry, sends her to the knockdown (0x3EE).
void target_called(u32 actor, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    if (fields_of(strategy).calm != 0)
    {
        return;
    }
    const RuntimeMindTarget* target = mind_target(context);
    if (target == nullptr || mind_target_record(target->bytes).actor != actor)
    {
        return;
    }
    if (!runtime_actor_point_within(actor_of(context), target->bytes, parameters_of(context).call_reach)) // 0x000416C0
    {
        return;
    }
    const u16 state = strategy_base(strategy).state;
    if (state == 0x3ED || state == 0x3EF)
    {
        if (requests_pending(context))
        {
            return;
        }
    }
    else if (state == 0x401)
    {
        clear_requests(context);
    }
    else
    {
        return;
    }
    change_state(0x3EE, context);
    goals_of(context).room = nullptr; // Actor +0x954 -> +8 -> +0xCA0.
}
}

// Xbox 0x000B9420
void jessie_clyde_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(JessieClydeStrategyState);
    output.parameters = 0x98;
}

// Xbox 0x000B9490
bool initialize_runtime_jessie_clyde_strategy(std::span<const u8> parameters, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    Actor& actor = actor_of(context);
    fields_of(strategy).parameters = context.parameters_address;
    fields_of(strategy).calm = 1u;
    fields_of(strategy).struck = 0u;
    fields_of(strategy).unk_D8 = 1u;
    fields_of(strategy).effect = 0u;
    fields_of(strategy).open = provoked(context);
    const char* name = reinterpret_cast<const char*>(parameters.data() + 0x18);
    std::memcpy(fields_of(strategy).effect_name.data(), name, std::strlen(name) + 1);
    EntityBitBlock& bits = body_bits(context, "0x000B9490");
    bits.bytes[1] = u8(bits.bytes[1] & 0x7F);
    if (actor.model == nullptr)
    {
        throw Error("0x000B9490 reads Jessie Clyde's model data through address zero");
    }
    if (actor_model_record_bit(actor.model->cpu, 0x10)) // The inlined 0x00052F80.
    {
        context.model_nodes(0x10, 1); // 0x000F96F0 on +0x204 and +0x310
    }
    dispatch_actor_attachment_records(context.resource.resource.data(), 0x11, record_runtime_body_attachment_event,
        nullptr, context.callouts,
        goals_of(context).address_of(strategy.data()) + offsetof(JessieClydeStrategyState, events)); // 0x0003BE20
    return true;
}

// Xbox 0x000B95A0
void destroy_runtime_jessie_clyde_strategy(RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    // Both blocks as the inlined 0x0013B810.
    clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).other_events), context.lists);
    clear_runtime_player_attachment_events(retail_bytes(fields_of(strategy).events), context.lists);
}

// Xbox 0x000B9630
void update_runtime_jessie_clyde_strategy(RuntimeJessieClydeStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const u16 state = strategy_base(strategy).state;
    const RuntimeMindTarget* target = mind_target(context);
    u32 open = 0;
    if (target == nullptr)
    {
        open = provoked(context);
    }
    else
    {
        open = provoked(context) != 0 || !runtime_strategy_target_watching_player(*target) ? 1u : 0u; // 0x001414F0
    }
    fields_of(strategy).open = open;
    update_runtime_base_strategy(goals); // 0x0002C130
    switch (state)
    {
    case 0x3ED:
        tick_shout(context); // 0x000B8970
        break;
    case 0x3EE:
        tick_knockdown(context); // 0x000B8E30
        break;
    case 0x3EF:
        tick_special(context); // 0x000B90D0
        break;
    case 0x401:
        tick_idle(context); // 0x000B80A0
        break;
    default:
        break;
    }
    alert_runtime_goal_listener(goals); // 0x0013D400
}

// Xbox 0x000B9750
void dispatch_runtime_jessie_clyde_message(std::span<const u32> message, RuntimeJessieClydeStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    RuntimeSpiderStrategy& shared = context.shared;
    const std::span<u8> strategy = strategy_of(context);
    const u32 actor = strategy_base(strategy).actor;
    react_runtime_strategy_message(message, shared.checks); // 0x001384F0; its result is not read.
    dispatch_runtime_base_strategy_message(message, shared.base); // 0x0002BC10
    const u16 state = strategy_base(strategy).state;
    const bool calm = fields_of(strategy).calm != 0;
    switch (message[0])
    {
    case 0x14:
        if (fields_of(strategy).effect == message[1])
        {
            fields_of(strategy).effect = 0u;
        }
        return;
    case 0x2C:
        if (!calm && !attacking(state)) // 0x000B8080
        {
            change_state(0x3F2, context);
        }
        return;
    case 0x2D:
        special_retarget(message[2], context); // 0x000B91F0
        return;
    case 0x31:
        if (!calm && fields_of(strategy).unk_D0 != 0)
        {
            react_runtime_strategy_watched_actor(message, shared.checks, shared.views); // 0x001397B0
        }
        return;
    case 0x36:
        notice_runtime_strategy_hostile_target(0x14, shared.checks); // 0x0013A4F0
        return;
    case 0x3B:
        // The tail jump to 0x000B9130: the special's second step, when it is not already on.
        if (message[2] == 2 && fields_of(strategy).special_step != 2 && state == 0x3EF)
        {
            step_special(2, context); // 0x000B9300
        }
        return;
    case 0x57:
        fields_of(strategy).follow_up = 1u;
        return;
    case 0x6E:
    {
        if (check_runtime_player_knockout(goals) != 0) // 0x0013A490
        {
            return;
        }
        if (runtime_message_from_mind_target(message, actor, mind_of(context), goals.target_at)) // 0x001408E0
        {
            special_struck(context); // The tail jump to 0x000B9260.
            return;
        }
        if (message[1] != actor)
        {
            return;
        }
        if (calm)
        {
            fields_of(strategy).struck = 1u;
        }
        retaliate_runtime_strategy_contact(message, -1, shared.checks, shared.views); // 0x00139390
        if (strategy_base(strategy).pause_count != 0)
        {
            return;
        }
        const std::span<const u8> actor_bytes = goals.entry.actor_bytes;
        const u32 kind =
            player_record(actor_bytes).mode == 2 || player_record(actor_bytes).case_link != 0 ? 1u : 0u;
        if (goals.resume.references[kind] != 0 || goals.resume.decay_enabled[goals.resume.decay_index] == 0)
        {
            return;
        }
        // The first goal kind of the actor +0x980 ring; an empty ring reads the word at address zero.
        const RuntimePlayerGoalRing* ring = goals.entry.ring;
        if (ring != nullptr
            && (ring->tail == ring->capacity ? read_u16(goals.entry.address_zero, 0)
                                             : read_u16(ring->rows, size_t(ring->stride) * ring->head))
                != 0)
        {
            return;
        }
        change_state(0x401, context);
        return;
    }
    case 0x77:
        target_called(message[1], context); // 0x000B9B50, EBP actor
        return;
    case 0x82:
        if (!calm && !attacking(state)) // 0x000B8080
        {
            react_runtime_strategy_cower(shared.checks); // The tail jump to 0x001398A0.
        }
        return;
    case 0x84:
        if (!calm && !attacking(state)) // 0x000B8080
        {
            react_runtime_strategy_startle(shared.checks); // The tail jump to 0x001399A0.
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000B9A20
void enter_runtime_jessie_clyde_state(u16 state, u16 previous, RuntimeJessieClydeStrategy& context)
{
    RuntimePlayerStrategyGoals& goals = goals_of(context);
    const std::span<u8> strategy = strategy_of(context);
    const std::span<u8> body = body_of(context);
    body_record(body).move_class = 2;
    switch (state)
    {
    case 0x3ED:
        // 0x000B88F0.
        if (fields_of(strategy).shout_step != 1)
        {
            fields_of(strategy).shout_step = 1u;
            load_goal(context.goal_shout, context);
        }
        body_record(body).move_class = 4;
        strategy_base(strategy).attacking = 1;
        return;
    case 0x3EE:
    {
        // 0x000B8B90, EDI strategy.
        load_goal(context.goal_knockdown, context);
        fields_of(strategy).follow_up = 0u;
        fields_of(strategy).knockdown_effect = 0u;
        body_record(body).move_class = 5;
        strategy_base(strategy).attacking = 1;
        const EntityBitBlock& bits = body_bits(context, "0x000B8B90");
        if ((bits.bytes[3] & 0x80) != 0 || (bits.bytes[4] & 0x10) != 0)
        {
            fields_of(strategy).knockdown_effect = 1u;
            make_effect(context);
            increment_runtime_body_latches(body, actor_of(context), context.latches); // The inlined 0x0001FC40.
            fields_of(strategy).knockdown_effect = 1u;
        }
        return;
    }
    case 0x3EF:
        step_special(1, context); // 0x000B8F30, the same first step as 0x000B9300.
        return;
    case 0x401:
        fields_of(strategy).cue = parameters_of(context).cue_time;
        return;
    case 0x403:
        // 0x000B88A0.
        fields_of(strategy).struck = 1u;
        change_state(0x401, context);
        return;
    default:
        enter_runtime_base_strategy_state(goals.entry, state); // 0x0002BAF0
        (void)previous; // Forwarded to 0x0002BAF0, which does not read it.
        return;
    }
}

// Xbox 0x000B9AC0
void leave_runtime_jessie_clyde_state(u16 state, RuntimeJessieClydeStrategy& context)
{
    const std::span<u8> strategy = strategy_of(context);
    switch (state)
    {
    case 0x3ED:
    {
        // The tail jump to 0x000B8930.
        if (fields_of(strategy).shout_step != 0)
        {
            fields_of(strategy).shout_step = 0u;
        }
        EntityBitBlock& bits = body_bits(context, "0x000B8930");
        bits.bytes[2] = u8(bits.bytes[2] & 0x7F);
        strategy_base(strategy).attacking = 0;
        return;
    }
    case 0x3EE:
    {
        // 0x000B8D00.
        strategy_base(strategy).attacking = 0;
        if (fields_of(strategy).knockdown_effect != 0)
        {
            end_effect(context);
            decrement_runtime_body_latches(body_of(context), actor_of(context), context.latches); // Inlined 0x0001FD20.
            fields_of(strategy).knockdown_effect = 0u;
        }
        EntityBitBlock& bits = body_bits(context, "0x000B8D00");
        bits.bytes[0] = u8(bits.bytes[0] & 0xEF);
        return;
    }
    case 0x3EF:
        // 0x000B8F90, EDI strategy.
        fields_of(strategy).unk_D8 = 1u;
        fields_of(strategy).unk_D0 = 1u;
        fields_of(strategy).unk_D4 = 0u;
        if (fields_of(strategy).special_effect != 0)
        {
            end_effect(context);
            decrement_runtime_body_latches(body_of(context), actor_of(context), context.latches); // Inlined 0x0001FD20.
            fields_of(strategy).special_effect = 0u;
        }
        strategy_base(strategy).attacking = 0;
        if (fields_of(strategy).special_step != 0)
        {
            fields_of(strategy).special_step = 0u;
        }
        return;
    case 0x401:
    case 0x403:
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
