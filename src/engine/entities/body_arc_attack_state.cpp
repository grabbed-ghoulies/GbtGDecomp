#include "body_arc_attack_state.h"
#include "mind_fields.h"
#include "actor_pose_updates.h"
#include "actor_transform.h"
#include "body_action_selection.h"
#include "body_damage_motion.h"
#include "body_damage_notices.h"
#include "body_message_helpers.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "player_mind_feedback.h"
#include "player_mind_requests.h"
#include "thrown_projectile.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = arc_attack_state_field;
namespace parameter = arc_attack_state_parameter;

constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kDegrees = std::bit_cast<float>(0x3C8EFA35u); // DAT_0032B0FC, pi / 180.

// The host halves of the five animation words (+0x248..+0x258): the records 0x00088A10 found.
struct ArcAttackRecords
{
    std::array<const AnimationRecord*, 5> animations{};
};
std::unordered_map<const u8*, ArcAttackRecords> arc_attack_records;

constexpr std::array<size_t, 5> kAnimationOffsets{0x248, 0x24C, 0x250, 0x254, 0x258};

size_t animation_index(size_t offset)
{
    for (size_t index = 0; index < kAnimationOffsets.size(); ++index)
    {
        if (kAnimationOffsets[index] == offset)
        {
            return index;
        }
    }
    throw Error("arc attack state animation word outside +0x248..+0x258");
}

std::function<RuntimeArcAttackStateContext&(u32)> arc_attack_context_of;

u32 body_address_of(std::span<const u8> state)
{
    u32 address;
    std::memcpy(&address, state.data() + 8, 4);
    return address;
}

bool construct_arc_attack_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_arc_attack_state(state, parameters, arc_attack_context_of(body_address_of(state)));
}

void destroy_arc_attack_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_arc_attack_state(state, arc_attack_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* arc_attack_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005056C0, arc_attack_state_object_sizes,
        construct_arc_attack_state_view, destroy_arc_attack_state_view};
    return &table;
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void step_byte(std::span<u8> bytes, size_t offset, i32 step)
{
    bytes[offset] = u8(bytes[offset] + step);
}

// Body +0x30 and the four +0x309..+0x30C counters the constructor raises and the destructor drops.
void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + step));
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        step_byte(body, offset, step);
    }
}

// The target record's retail image (state +0x18), read as 0x00089DB0, 0x00089D20 and 0x00089760 do.
std::span<const u8> target_record(std::span<const u8> state, RuntimeArcAttackStateContext& context)
{
    const auto record = context.target_at(get(state, field::target));
    if (record.empty())
    {
        throw Error("arc attack state target record has no retail image");
    }
    return record;
}

Vec3 record_position(std::span<const u8> record)
{
    return Vec3{read_f32(record, 0x20), read_f32(record, 0x24), read_f32(record, 0x28)};
}

// The first slot's play position (actor +0x50 -> +0x58 -> [0] -> +0x14), or 0 without a player.
float slot_position(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    const MinaPlayer* player = controller == nullptr ? nullptr : controller->slots.at(0).player.get();
    return player == nullptr ? 0.f : player->position;
}

void set_velocity(RuntimeArcAttackStateContext& context, const Vec3& velocity)
{
    context.actor.anchor = velocity; // Actor +0x3C.
    set(context.body, body_field::velocity, velocity);
}

void send_callout(RuntimeArcAttackStateContext& context, i32 id, const RuntimeActorCreationOptions* options)
{
    if (context.actor_address != 0xFFFFFF74u)
    {
        dispatch_actor_attachment_records(context.resource.resource.data(), id, nullptr, options, context.callouts,
            0); // 0x0003BE20, EAX actor +0x8C
    }
}

// The inlined 0x0003AAA0 pairs (the attached actor's +0xDB0 variant first, then the plain name), only for a name.
void find_animation(std::span<u8> state, size_t offset, u32 name, RuntimeArcAttackStateContext& context)
{
    if (name == 0)
    {
        return;
    }
    const AnimationRecord* record = find_runtime_body_animation(context.image_string(name), context.actions);
    arc_attack_records[state.data()].animations[animation_index(offset)] = record;
    put_word(state, offset, record != nullptr ? 1u : 0u);
}

template <typename To, typename From>
void copy_name(std::span<u8> state, To to, std::span<const u8> parameters, From from)
{
    char* target = field_data(state, to);
    const char* source = field_data(parameters, from);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

// Xbox 0x00089700, EAX state
void start_runtime_arc_attack_animation(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    size_t offset;
    switch (get(state, field::phase))
    {
    case 1:
        offset = field::take_off_animation.offset;
        break;
    case 2:
        offset = field::lift_animation.offset;
        break;
    case 3:
        offset = field::flight_animation.offset;
        break;
    case 4:
        offset = field::leap_animation.offset;
        break;
    case 5:
        offset = field::retreat_animation.offset;
        break;
    default:
        return;
    }
    if (read_u32(state, offset) == 0)
    {
        return;
    }
    const auto found = arc_attack_records.find(state.data());
    const AnimationRecord* record = found == arc_attack_records.end() ? nullptr
        : found->second.animations[animation_index(offset)];
    if (record == nullptr)
    {
        throw Error("arc attack state starts an animation record it has no host view of");
    }
    start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0
}

// Xbox 0x0001F830, EAX body: the leap's contact shapes off again.
void release_runtime_arc_attack_shapes(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    if (get(state, field::shapes_active) == 0)
    {
        return;
    }
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, get(state, field::shape_tag)); // 0x0013F4D0
    }
    set(state, field::shapes_active, 0);
}

// Xbox 0x0008A1A0, ESI state: the state ends on its own key.
void finish_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    u16 key;
    if (get(state, field::retreated) == 0 && get(state, field::damage_end) != 0)
    {
        // The inlined 0x00133BD0 for key 0x11 and the actor's class.
        const float value = runtime_class_damage_value(0x11, context.actor.damage_class, context.damage_tables);
        put_float(context.body, 0x23C, value * 0.5f); // DAT_0032B0F4.
        key = context.body_slot_key(0x34);
    }
    else
    {
        key = get(state, field::end_key);
        if (key == 0)
        {
            key = context.body_slot_key(0x20);
        }
    }
    set(state, body_state_field::next_key, key);
    set(state, field::phase, 0);
    start_runtime_arc_attack_animation(state, context); // 0x00089700
    release_runtime_arc_attack_shapes(state, context); // 0x0001F830
}

// Xbox 0x00089DB0, EDI state, EAX normal: the leap meets something. Without a bounce key the target is hurt and the
// body takes its vtable +0x4C key; with one the actor's velocity reflects off `normal`.
void hit_runtime_arc_attack(std::span<u8> state, const Vec3& normal, const Vec3& position,
    RuntimeArcAttackStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    u16 key;
    if (get(state, field::bounce_key) == 0)
    {
        u32 amount = 0;
        u32 victim = 0;
        if (get(state, field::target) != 0)
        {
            victim = read_u32(target_record(state, context), 0x10);
            const Actor* target = context.actor_at(victim);
            if (target == nullptr)
            {
                throw Error("0x00089DB0 reads the target record's +0x10 actor +0xCF4 through no actor view");
            }
            if (get(state, field::unk_260) != 0)
            {
                if ((actor.mask_mode == 2 || actor.mask_flag != 0) && target->damage_class != 0)
                {
                    amount = u32(i32(runtime_class_damage_value(0x34, target->damage_class,
                        context.damage_tables))); // 0x00133BD0, CVTTSS2SI
                }
                else
                {
                    amount = u32(runtime_actor_damage_value(0x15, actor.damage_class,
                        context.damage_tables)); // 0x00028DA0, ECX actor
                    const bool live = runtime_actor_is_live_player(*target,
                        read_u32(context.bytes_at(victim), 0xCE0)) && context.player_damage != 0; // 0x00028D20
                    if (amount != 0 && live)
                    {
                        amount = get(body, body_field::energy);
                    }
                }
            }
        }
        put_word(body, 0x26C, victim);
        put_word(body, 0x270, victim);
        put_word(body, 0x274, victim);
        put_word(body, 0x27C, 3);
        put_word(body, 0x280, amount);
        put_word(body, 0x278, 0);
        put_half(body, 0x162, u16(read_u16(body, 0x162) + 1));
        put_float(body, 0x220, 15.f); // DAT_0032B214.
        put_float(body, 0x224, 15.f);
        put_float(body, 0x228, 0.25f); // DAT_0032B104.
        put_float(body, 0x22C, 0.5f); // DAT_0032B0F4.
        put_half(body, 0x160, u16(read_u16(body, 0x160) + 1));
        put_half(body, 0x162, u16(read_u16(body, 0x162) + 1));
        if (amount != 0)
        {
            subtract_runtime_body_damage_health(body, actor, context.bits, amount, 1, context.player_blocked,
                context.player_modes, context.player_mode); // 0x00021120, EAX amount, ESI body
        }
        key = context.body_slot_key(0x4C);
    }
    else
    {
        const Vec3 velocity = actor.anchor;
        const float x = velocity.x * -1.f; // DAT_0032B120.
        const float y = velocity.y * -1.f;
        const float z = velocity.z * -1.f;
        const float scale = ((normal.z * z + normal.y * y) + normal.x * x) * 2.f; // DAT_0032B110.
        set_velocity(context, Vec3{normal.x * scale - x, normal.y * scale - y, normal.z * scale - z});
        key = get(state, field::bounce_key);
    }
    set(state, body_state_field::next_key, key);
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags = 1;
    send_callout(context, 0x37, &g_actor_creation_options);
    set(state, field::phase, 0);
    start_runtime_arc_attack_animation(state, context); // 0x00089700
    release_runtime_arc_attack_shapes(state, context); // 0x0001F830
}

// Xbox 0x00089C00, EAX actor: the spin about the actor's x axis; true once it passes a whole turn either way.
bool spin_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    const float rate = get(state, field::spin_rate);
    if (rate == 0.f)
    {
        return true;
    }
    Actor& actor = context.actor;
    Vec3 euler = actor_world_euler(actor); // 0x00041820
    const long double remainder = animation_remainder(euler.x, kTurn); // 0x00152BE0
    float before = float(remainder);
    if (0.L > remainder)
    {
        before += kTurn;
    }
    const float after = runtime_actor_step(actor, context.clock) * rate + euler.x;
    euler.x = after;
    wrap_runtime_euler(euler, actor.euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // Actor +0x2A8.
    set(context.body, body_field::euler, actor_world_euler(actor)); // 0x00041820 into body +0x72C
    if (after <= 0.f && before > 0.f)
    {
        return true;
    }
    if (after >= kTurn && kTurn > before)
    {
        return true;
    }
    return false;
}

// Xbox 0x00089D20, EAX state, ECX actor: the body faces the target record's position.
void face_runtime_arc_attack_target(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    if (get(state, field::target) == 0)
    {
        return;
    }
    const Vec3 target = record_position(target_record(state, context));
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    float yaw = 0.f;
    runtime_direction_angle(Vec3{target.x - position.x, target.y - position.y, target.z - position.z},
        yaw); // 0x00105F90
    set(context.body, body_field::euler, Vec3{0.f, yaw, 0.f});
}

// Xbox 0x00089AB0, EAX state, ECX body, ESI actor: the body rises by the lift height through the lift animation;
// true when that animation ends.
bool rise_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    const Vec3 origin = get(state, field::lift_origin);
    const float lifted = get(state, field::lift_height) + origin.y;
    const float start = get(state, field::lift_start);
    const float fraction = (slot_position(context.animation) - start) / (1.f - start); // DAT_0032B0F8.
    set(context.body, body_field::position, Vec3{(origin.x - origin.x) * fraction + origin.x,
        (lifted - origin.y) * fraction + origin.y, (origin.z - origin.z) * fraction + origin.z});
    // The rest is 0x0003B290's test of slot 0, inlined.
    return runtime_actor_animation_slot_complete(context.animation, 0);
}

// Xbox 0x00089BA0, EAX actor, ECX state
bool hold_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    const float hold = get(state, field::hold) - runtime_actor_step(context.actor, context.clock);
    set(state, field::hold, hold);
    return hold <= 0.f;
}

// Xbox 0x00089760, EAX state: the leap's velocity toward the target record (raised by its +0x30 by the height
// scale), or 40 ahead and 15 up without one.
void launch_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    Actor& actor = context.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    Vec3 target;
    if (get(state, field::target) != 0)
    {
        const auto record = target_record(state, context);
        target = Vec3{read_f32(record, 0x20), read_f32(record, 0x30) * get(state, field::height_scale)
            + read_f32(record, 0x24), read_f32(record, 0x28)};
    }
    else
    {
        const float yaw = actor.euler.y; // Actor +0x18.
        target.x = float(retail_sin(yaw) * 40.0L + position.x); // 0x00153320, DAT_0032B1B8
        target.y = position.y + 15.f; // DAT_0032B214.
        target.z = float(retail_cos(yaw) * 40.0L + position.z); // 0x00153230
    }
    const float gravity = get(state, field::gravity);
    const Vec3 velocity = runtime_ballistic_velocity(target, position, get(state, field::speed),
        gravity); // 0x00140D80
    const float rise = get(state, field::flat) == 0 ? velocity.y : 0.f;
    set_velocity(context, Vec3{velocity.x, rise, velocity.z});
    set(context.body, body_field::gravity, gravity);
    float yaw = get(state, field::launch_yaw);
    runtime_direction_angle(Vec3{target.x - position.x, target.y - position.y, target.z - position.z},
        yaw); // 0x00105F90, EBX state +0x24
    set(state, field::launch_yaw, yaw);
}

// Xbox 0x000898C0: message 0x41 hops the body back toward where the state started, half the way, between 3 and 6
// reaches of the actor.
void retreat_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    Actor& actor = context.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 start = get(state, field::start);
    const float dz = start.z - position.z;
    const float dx = start.x - position.x;
    float distance = float(retail_square_root(dz * dz + dx * dx) * 0.5L); // 0x00152B30, DAT_0032B0F4
    const float reach = actor.radius_xz * get(context.body, body_field::radius_scale);
    const float nearest = reach * 3.f; // DAT_0032B260.
    const float farthest = reach * 6.f; // DAT_0032B218.
    if (nearest > distance)
    {
        distance = nearest;
    }
    else if (distance > farthest)
    {
        distance = farthest;
    }
    const float angle = float(static_cast<long double>(get(state, field::launch_yaw)) - kPi);
    const Vec3 target{float(retail_sin(angle) * distance + position.x), start.y,
        float(retail_cos(angle) * distance + position.z)}; // 0x00153320, 0x00153230
    const float gravity = get(state, field::retreat_gravity);
    const Vec3 velocity = runtime_ballistic_velocity(target, position, get(state, field::retreat_speed),
        gravity); // 0x00140D80
    set(context.body, body_field::gravity, gravity);
    set_velocity(context, velocity);
    set(state, field::phase, 5);
    start_runtime_arc_attack_animation(state, context); // 0x00089700
    set(state, field::spin_rate, get(state, field::retreat_spin_degrees) * kDegrees);
    release_runtime_arc_attack_shapes(state, context); // 0x0001F830
    set_runtime_body_shape_mode(0, context.shape); // 0x0001FEB0, BL 0
    send_callout(context, 0x2F, nullptr);
    set(state, field::retreated, 1);
    set(state, field::settled, 1);
}

// Xbox 0x00089300, EBX state, argument the actor: the leap spins on until a contact bit ends it, then the trail and
// the landing gravity.
void fly_runtime_arc_attack(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    Actor& actor = context.actor;
    const float elapsed = get(state, body_state_field::elapsed);
    const auto& bits = context.bits.bytes; // Body +0x15C -> +8.
    const bool wall = ((bits[5] >> 1) & 1) != 0;
    const bool contact = ((bits[1] >> 4) & 1) != 0;
    const bool ground = ((bits[2] >> 5) & 1) != 0;
    spin_runtime_arc_attack(state, context); // 0x00089C00
    if (elapsed > 0.f && (wall || contact || ground))
    {
        if (get(state, field::land_on_contact) != 0)
        {
            if (get(state, field::keep_pitch) == 0)
            {
                Vec3 euler = actor_world_euler(actor); // 0x00041820
                euler.x = 0.f;
                set_runtime_player_actor_rotation(actor, actor, context.body, euler); // 0x0002B560
            }
            send_callout(context, 0x12, nullptr);
            send_callout(context, 0x2F, nullptr);
            finish_runtime_arc_attack(state, context); // 0x0008A1A0
        }
        else if (contact && get(state, field::bounce) != 0 && get(state, field::retreated) == 0)
        {
            const Vec3 position = actor_world_position(actor); // 0x000412A0
            Vec3 normal{};
            runtime_horizontal_direction(normal, actor.euler.y + kPi, 1.f); // 0x001060A0
            hit_runtime_arc_attack(state, normal, position, context); // 0x00089DB0
        }
    }
    const float trail = get(state, field::trail_scale);
    if (trail != 0.f)
    {
        const i32 rate = context.trails.pal == 0 || context.trails.pal_override != 0 ? 0x3C : 0x32;
        float timer = get(state, field::trail_timer);
        request_actor_trail(actor, trail, timer, rate / 2, context.trails); // 0x0013B030, state +0x2C
        set(state, field::trail_timer, timer);
    }
    if (get(state, field::settled) != 0)
    {
        return;
    }
    const float step = runtime_actor_step(actor, context.clock);
    const float timer = get(state, field::settle_timer);
    if (timer > 0.f)
    {
        const float left = timer - step;
        set(state, field::settle_timer, left);
        if (left <= 0.f)
        {
            set(state, field::settle_timer, 0.f);
        }
    }
    if (0.001f > get(state, field::settle_timer)) // DAT_0032B1A4.
    {
        set(context.body, body_field::gravity, get(state, field::landing_gravity));
        set(state, field::settled, 1);
    }
}

// Xbox 0x0008A030, EAX velocity, ESI contact: true when the leap meets the contact's surface head-on (the flat
// directions' dot below -0.7).
bool runtime_arc_attack_meets_surface(const Vec3& velocity, std::span<const u8> contact)
{
    float vx = velocity.x;
    float vy = 0.f;
    float vz = velocity.z;
    if (vx == 0.f && vz == 0.f)
    {
        return false;
    }
    const float square = vz * vz + vx * vx;
    if (square != 0.f)
    {
        const float scale = engine_reciprocal_sqrt(square); // 0x001534C0
        vx = float(static_cast<long double>(vx) * scale);
        vy = float(0.0L * scale); // DAT_0032B100.
        vz = float(static_cast<long double>(vz) * scale);
    }
    float nx = read_f32(contact, 0xC);
    float ny = 0.f;
    float nz = read_f32(contact, 0x14);
    if (nx == 0.f && nz == 0.f)
    {
        return false;
    }
    const float length = nz * nz + nx * nx;
    if (length != 0.f)
    {
        const float scale = engine_reciprocal_sqrt(length);
        nx = float(static_cast<long double>(nx) * scale);
        ny = float(0.0L * scale);
        nz = float(static_cast<long double>(nz) * scale);
    }
    return 0.f - ((nz * vz + ny * vy) + nx * vx) > 0.7f; // DAT_0032B264.
}

// Xbox 0x00140940, ECX message, EDX actor: message 0x6E is about the actor's mind's +0x10 record.
bool runtime_arc_attack_reflects(std::span<const u32> message, RuntimeArcAttackStateContext& context)
{
    if (context.mind.empty())
    {
        return false;
    }
    const u32 record = get(context.mind, mind_field::target_record);
    if (record == 0)
    {
        return false;
    }
    return read_u32(context.target_at(record), 0x10) == message[1] && message[4] != 0
        && message[2] == context.actor_address;
}
}

// Xbox 0x00088990
void arc_attack_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x00088A10
bool initialize_runtime_arc_attack_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeArcAttackStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    arc_attack_records[state.data()] = ArcAttackRecords{};
    set(state, field::target, context.mind.empty() ? 0u : get(context.mind, mind_field::action_record));
    set(state, field::saved_gravity, get(body, body_field::gravity));
    set(state, field::start, actor_world_position(actor)); // 0x000412A0, EDI state +0x64
    set(state, field::take_off_time, get(parameters, parameter::take_off_time));
    set(state, field::lift_start, get(parameters, parameter::lift_start));
    set(state, field::hold, get(parameters, parameter::hold));
    set(state, field::settle, get(parameters, parameter::settle));
    set(state, field::flat, get(parameters, parameter::flat));
    set(state, field::height_scale, get(parameters, parameter::height_scale));
    if (get(state, field::target) == 0 || get(state, field::flat) == 0)
    {
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
        const float lowest = get(parameters, parameter::lift_minimum);
        set(state, field::lift_height, (get(parameters, parameter::lift_maximum) - lowest) * roll + lowest);
    }
    else
    {
        set(state, field::lift_height, read_f32(target_record(state, context), 0x30) * get(state, field::height_scale));
    }
    copy_name(state, field::take_off_sound, parameters, parameter::take_off_sound);
    copy_name(state, field::launch_sound, parameters, parameter::launch_sound);
    copy_name(state, field::flight_sound, parameters, parameter::flight_sound);
    set(state, field::trail_timer, 0.f);
    set(state, field::trail_scale, get(parameters, parameter::trail_scale));
    set(state, field::speed, get(parameters, parameter::speed));
    set(state, field::spin_degrees, get(parameters, parameter::spin_degrees));
    set(state, field::gravity, get(parameters, parameter::gravity));
    set(state, field::landing_gravity, get(parameters, parameter::landing_gravity));
    set(state, field::settled, 0);
    // {0x58, reply, value}: the actor may override the leap's speed.
    std::array<u32, 0x42> query{};
    query[0] = 0x58;
    context.actor_message(query); // Actor vtable +0xC.
    if (query[1] != 0)
    {
        set(state, field::speed, std::bit_cast<float>(query[2]));
    }
    set(state, field::zero_gravity, get(parameters, parameter::zero_gravity));
    if (get(state, field::zero_gravity) != 0)
    {
        set(body, body_field::gravity, 0.f);
    }
    set(state, field::retreat, get(parameters, parameter::retreat));
    set(state, field::retreat_speed, get(parameters, parameter::retreat_speed));
    set(state, field::retreat_spin_degrees, get(parameters, parameter::retreat_spin_degrees));
    set(state, field::retreat_gravity, get(parameters, parameter::retreat_gravity));
    set(state, field::damage_end, get(parameters, parameter::damage_end));
    set(state, field::end_key, get(parameters, parameter::end_key));
    set(state, field::reflect, get(parameters, parameter::reflect));
    adjust_body_counters(body, 1);
    step_byte(body, 0x6A, 1);
    set(state, field::shape_tag, get(parameters, parameter::shape_tag));
    set(state, field::shape_scale, get(parameters, parameter::shape_scale));
    set(state, field::shape_mode, set_runtime_body_shape_mode(4, context.shape)); // 0x0001FEB0, BL 4
    set(state, field::bounce, get(parameters, parameter::bounce));
    set(state, field::keep_pitch, get(parameters, parameter::keep_pitch));
    set(state, field::land_on_contact, get(parameters, parameter::land_on_contact));
    set(state, field::bounce_key, get(parameters, parameter::bounce_key));
    set(state, field::latch, get(parameters, parameter::latch));
    set(state, field::latched, 0);
    find_animation(state, field::take_off_animation.offset, get(parameters, parameter::take_off_animation), context);
    find_animation(state, field::flight_animation.offset, get(parameters, parameter::flight_animation), context);
    find_animation(state, field::lift_animation.offset, get(parameters, parameter::lift_animation), context);
    find_animation(state, field::retreat_animation.offset, get(parameters, parameter::retreat_animation), context);
    find_animation(state, field::leap_animation.offset, get(parameters, parameter::leap_animation), context);
    set(state, field::phase, 1);
    start_runtime_arc_attack_animation(state, context); // 0x00089700
    set_velocity(context, Vec3{0.f, 0.f, 0.f});
    if (get(state, field::zero_gravity) != 0)
    {
        set(body, body_field::gravity, 0.f);
    }
    return true;
}

// Xbox 0x00088ED0
void destroy_runtime_arc_attack_state(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    const auto body = context.body;
    adjust_body_counters(body, -1);
    set(body, body_field::gravity, get(state, field::saved_gravity));
    set_runtime_body_shape_mode(u8(get(state, field::shape_mode)), context.shape); // 0x0001FEB0, BL state +0x60
    step_byte(body, 0x6A, -1);
    if (get(state, field::shapes_active) != 0)
    {
        if (context.collision != nullptr)
        {
            deactivate_runtime_body_contact_shapes(*context.collision, get(state, field::shape_tag)); // 0x0013F4D0
        }
        set(state, field::shapes_active, 0);
    }
    if (get(state, field::latched) != 0)
    {
        decrement_runtime_body_latches(body, context.actor, context.latches); // The inlined 0x0001FD20.
        set(state, field::latched, 0);
    }
    if (field_data(state, field::flight_sound)[0] != 0)
    {
        release_actor_sound(context.actor.sound_slots[7], context.sounds); // 0x0012A3D0, ESI actor +0x83C
    }
    arc_attack_records.erase(state.data());
}

// Xbox 0x00089530
void message_runtime_arc_attack_state(std::span<u8> state, std::span<u32> message, std::span<const u8> contact,
    RuntimeArcAttackStateContext& context)
{
    switch (message[0])
    {
    case 0x41:
        if (get(state, field::retreat) != 0)
        {
            retreat_runtime_arc_attack(state, context); // 0x000898C0
        }
        return;
    case 0x62:
    {
        if (get(state, field::bounce) == 0 || get(state, field::retreated) != 0)
        {
            return;
        }
        if (contact.empty())
        {
            throw Error("arc attack state message 0x62 without its +0x1C contact record");
        }
        const Vec3 velocity = context.actor.anchor; // Actor +0x3C.
        message[1] = 1;
        if (runtime_arc_attack_meets_surface(velocity, contact)
            || runtime_body_bit_set(context.bits, 0xD)) // 0x0008A030, 0x0001EF10
        {
            const Vec3 normal{read_f32(contact, 0xC), read_f32(contact, 0x10), read_f32(contact, 0x14)};
            const Vec3 position{read_f32(contact, 0), read_f32(contact, 4), read_f32(contact, 8)};
            hit_runtime_arc_attack(state, normal, position, context); // 0x00089DB0
            message[2] = 1;
        }
        return;
    }
    case 0x6E:
        if (get(state, field::reflect) != 0 && runtime_arc_attack_reflects(message, context)) // 0x00140940
        {
            const Vec3 velocity = context.actor.anchor;
            set_velocity(context, Vec3{velocity.x * -1.f, velocity.y, velocity.z * -1.f}); // DAT_0032B120
            set(state, body_state_field::next_key, get(state, field::end_key));
        }
        return;
    case 0x8D:
        if (message[1] == get(state, field::target))
        {
            set(state, field::target, 0);
            finish_runtime_arc_attack(state, context); // 0x0008A1A0
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00089060
void tick_runtime_arc_attack_state(std::span<u8> state, RuntimeArcAttackStateContext& context)
{
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed,
        runtime_actor_step(actor, context.clock) + get(state, body_state_field::elapsed));
    switch (get(state, field::phase))
    {
    case 1:
        if (slot_position(context.animation) >= get(state, field::take_off_time))
        {
            set(state, field::phase, 2);
            start_runtime_arc_attack_animation(state, context); // 0x00089700
            set(context.body, body_field::gravity, 0.f);
            set_velocity(context, actor_transform_origin); // DAT_0050DD80.
            set(state, field::lift_origin, actor_world_position(actor)); // 0x000412A0, EDI state +0x1AC
            request_actor_sound(actor, 1, field_data(state, field::take_off_sound), context.sounds); // 0x00041D00
        }
        return;
    case 2:
        if (rise_runtime_arc_attack(state, context)) // 0x00089AB0
        {
            set(state, field::phase, 3);
            start_runtime_arc_attack_animation(state, context); // 0x00089700
            set(state, field::spin_rate, get(state, field::spin_degrees) * kDegrees);
        }
        face_runtime_arc_attack_target(state, context); // 0x00089D20
        return;
    case 3:
        spin_runtime_arc_attack(state, context); // 0x00089C00
        if (hold_runtime_arc_attack(state, context)) // 0x00089BA0
        {
            if (get(state, field::latched) != 0)
            {
                decrement_runtime_body_latch_mask(context.body, 0x3F, actor, context.latches); // 0x0001F2F0
                set(state, field::latched, 0);
            }
            set(state, field::phase, 4);
            start_runtime_arc_attack_animation(state, context); // 0x00089700
            set(state, field::settle_timer, get(state, field::settle));
            request_actor_sound(actor, 1, field_data(state, field::launch_sound), context.sounds); // 0x00041D00
            request_actor_sound(actor, 7, field_data(state, field::flight_sound), context.sounds);
            if (i16(get(state, field::shape_tag)) >= 0)
            {
                // 0x0001F800, ECX 0xFF, EDX the tag, XMM4 the scale, ESI body.
                if (context.collision != nullptr)
                {
                    activate_runtime_body_contact_shapes(*context.collision, actor, get(state, field::shape_tag),
                        0xFF, get(state, field::shape_scale)); // 0x0013F420
                    put_word(context.body, 0x2CC, 0);
                }
                set(state, field::shapes_active, 1);
            }
            if (get(state, field::latch) != 0 && get(state, field::latched) == 0)
            {
                increment_runtime_body_latch_mask(context.body, 0x3F, actor, context.latches); // 0x0001F3F0
                set(state, field::latched, 1);
            }
            launch_runtime_arc_attack(state, context); // 0x00089760
            set_runtime_body_shape_mode(0, context.shape); // 0x0001FEB0, BL 0
        }
        if (get(state, field::trail_scale) != 0.f)
        {
            float timer = get(state, field::trail_timer);
            request_actor_video_rate_trail(actor, get(state, field::trail_scale), timer,
                context.trails); // 0x0013B1B0, state +0x2C
            set(state, field::trail_timer, timer);
        }
        face_runtime_arc_attack_target(state, context); // 0x00089D20
        return;
    case 4:
    case 5:
        fly_runtime_arc_attack(state, context); // 0x00089300
        return;
    default:
        return;
    }
}

void install_runtime_arc_attack_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeArcAttackStateContext&(u32 body_address)> context_of)
{
    arc_attack_context_of = std::move(context_of);
    types[0x5F] = ObjectFactoryType{9, arc_attack_state_lifecycle}; // Getter 0x000889B0.
}
}
