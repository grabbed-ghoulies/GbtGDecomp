#include "body_swing_state.h"
#include "mind_fields.h"
#include "actor_runtime_queries.h"
#include "actor_spawn_query.h"
#include "actor_transform.h"
#include "body_action_selection.h"
#include "player_goal_cleanup.h"
#include "player_mind_events.h"
#include "player_mind_requests.h"
#include "shape_transform.h"
#include "../camera/camera_body_slots.h"
#include "../frontend/dialogue_resources.h"
#include "ghouly_spawn_record.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace parameter = swing_state_parameter;

constexpr float kTurn = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
constexpr float kPi = std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
constexpr float kNegativePi = std::bit_cast<float>(0xC0490FDBu); // DAT_0032B29C.

// The host halves of the state's pointer words: the seven animation records (+0x104..+0x11C), the four effect
// shapes (+0x68 + 8n) and the cutscene instance (+0x1A8).
struct SwingRecords
{
    std::array<const AnimationRecord*, 7> animations{};
    std::array<const u8*, 4> shapes{};
    RuntimeCutsceneInstance* cutscene = nullptr;
};
std::unordered_map<const u8*, SwingRecords> swing_records;

std::function<RuntimeSwingStateContext&(u32)> swing_context_of;

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// Their order in SwingRecords, the words' order in the state.
enum SwingAnimation : size_t
{
    kGrabAnimation,
    kLiftPositiveAnimation,
    kLiftNegativeAnimation,
    kSwingPositiveAnimation,
    kSwingNegativeAnimation,
    kDropPositiveAnimation,
    kDropNegativeAnimation
};

// The record 0x0001AFA0 starts (ECX the state's word); retail reads a missing one through address zero.
const AnimationRecord& record_at(const SwingState& state, SwingAnimation animation)
{
    const auto found = swing_records.find(body_state_key(state));
    const AnimationRecord* record = found == swing_records.end() ? nullptr
        : found->second.animations[animation];
    if (record == nullptr)
    {
        throw Error("swing state starts a missing animation record through address zero");
    }
    return *record;
}

bool construct_swing_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    SwingState& state = body_state_view<SwingState>({object.bytes.get(), object.size});
    return initialize_runtime_swing_state(state, parameters, swing_context_of(state.base.body));
}

void destroy_swing_state_view(FactoryObject& object)
{
    SwingState& state = body_state_view<SwingState>({object.bytes.get(), object.size});
    destroy_runtime_swing_state(state, swing_context_of(state.base.body));
}

const ObjectFactoryVtable* swing_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005059E0, swing_state_object_sizes, construct_swing_state_view,
        destroy_swing_state_view};
    return &table;
}

void put_body_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, 12);
}

void send_words(const std::function<void(std::span<const u32>)>& send, std::initializer_list<u32> words)
{
    std::array<u32, 0x42> message{};
    std::copy(words.begin(), words.end(), message.begin());
    send(message);
}

void send_weapon(RuntimeSwingStateContext& context, u32 weapon, std::initializer_list<u32> words)
{
    if (!context.pickup.weapon_message)
    {
        throw Error("swing state has no weapon message view");
    }
    std::array<u32, 0x42> message{};
    std::copy(words.begin(), words.end(), message.begin());
    context.pickup.weapon_message(weapon, message);
}

// Actor +0x958 -> +0x18: the weapon the mind chose, or 0.
u32 mind_weapon(RuntimeSwingStateContext& context)
{
    return context.pickup.mind.empty() ? 0 : mind_record(context.pickup.mind).action_actor;
}

// Body +0x48: the attached actor's address, or 0.
u32 held_weapon(RuntimeSwingStateContext& context)
{
    return body_record(context.pickup.body).attached;
}

const EntityBonePose* shape_pose(const Actor& actor)
{
    if (actor.model == nullptr)
    {
        return nullptr;
    }
    const EntityBonePose* pose = actor.model->alternate_shape_pose;
    return pose != nullptr ? pose : actor.model->primary_shape_pose;
}

i32 effect_slot(const SwingState& state, size_t index)
{
    return state.effects[index].slot;
}

void set_effect(SwingState& state, size_t index, i32 slot, const u8* shape)
{
    state.effects[index].slot = slot;
    state.effects[index].shape = shape != nullptr ? 1u : 0u;
    swing_records[body_state_key(state)].shapes[index] = shape;
}

// The inlined 0x00152BE0 wrap into [-pi, pi): the x87 remainder is compared, its float kept.
float wrap_signed(float value)
{
    const long double remainder = animation_remainder(value, kTurn);
    float result = float(remainder);
    if (static_cast<long double>(kNegativePi) > remainder)
    {
        result += kTurn;
    }
    if (result >= kPi)
    {
        result -= kTurn;
    }
    return result;
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

// The spin curve at DAT_0030291C: four (time, turn) rows, sampled by binary search and clamped to the span.
float sample_spin_curve(float time)
{
    struct Row
    {
        float time;
        float value;
    };
    static constexpr std::array<Row, 4> rows{{{0.f, 0.f}, {0.3f, 0.2f}, {0.85f, 0.9f}, {1.f, 1.f}}};
    i32 low = 0;
    i32 high = 3;
    float low_time = 1.f; // DAT_0032B0F8, kept in XMM4 when the time lies outside [0, 1].
    float high_time = 0.f;
    if (0.f > time)
    {
        low = 3;
        high = 0;
    }
    else if (time > 1.f)
    {
        low = 3;
        high = 0;
    }
    else
    {
        do
        {
            const i32 middle = (high - low) / 2 + low;
            if (time >= rows[size_t(middle)].time) // COMISS / CMOVNC: unordered keeps the low row.
            {
                low = middle;
            }
            else
            {
                high = middle;
            }
        }
        while (low + 1 < high);
        low_time = rows[size_t(low)].time;
        high_time = rows[size_t(high)].time;
    }
    const float high_value = rows[size_t(high)].value;
    const float low_value = rows[size_t(low)].value;
    if (high_time == low_time) // UCOMISS / LAHF: an ordered equal takes the high row.
    {
        return high_value;
    }
    const float value = (time - low_time) / (high_time - low_time) * (high_value - low_value) + low_value;
    if (high_value > low_value)
    {
        if (value > high_value)
        {
            return high_value;
        }
        if (low_value > value)
        {
            return low_value;
        }
        return value;
    }
    if (high_value > value)
    {
        return high_value;
    }
    if (value > low_value)
    {
        return low_value;
    }
    return value;
}

// Xbox 0x00099120, EAX state
void release_runtime_swing_effects(SwingState& state)
{
    for (size_t index = 0; index < 4; ++index)
    {
        const i32 slot = effect_slot(state, index);
        if (slot == -1)
        {
            continue;
        }
        ActorEffectNode* effect = g_actor_effect_nodes.at(size_t(slot));
        if (effect == nullptr)
        {
            throw Error("swing state effect slot is empty in DAT_00506930");
        }
        put_half(effect->bytes, 0x18, u16(read_u16(effect->bytes, 0x18) | 8));
        put_float(effect->bytes, 8, -1.f); // DAT_0032B120.
        set_effect(state, index, -1, nullptr);
    }
}

// Xbox 0x00098F40, EAX body
void start_runtime_swing_effects(SwingState& state, RuntimeSwingStateContext& context)
{
    const u32 held = held_weapon(context);
    if (held == 0)
    {
        return;
    }
    const Actor& weapon = context.pickup.weapon_at(held).actor;
    g_actor_creation_options.flags = 0;
    g_actor_creation_options.euler = actor_world_euler(weapon); // 0x00041820
    g_actor_creation_options.flags |= 4;
    size_t count = 0;
    for (const u8* shape = find_runtime_actor_shape(weapon, 0xFFFFFFFFu, 0, 0x12); shape != nullptr;
        shape = next_runtime_actor_shape(weapon, shape, 0xFFFFFFFFu, 0, 0x12)) // 0x00042560 / 0x000425D0
    {
        Vec3 center;
        shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape), weapon.xform, shape_pose(weapon),
            center); // 0x00056330
        g_actor_creation_options.position = center;
        g_actor_creation_options.flags |= 1;
        const char* name = state.trail_effect.data();
        if (std::memcmp(name, "aid_", 4) != 0)
        {
            continue;
        }
        // The inlined 0x00053170.
        if (load_actor_effect_stream(name, &g_actor_creation_options, context.effects) == 0)
        {
            continue;
        }
        const i32 slot = g_last_actor_effect;
        g_last_actor_effect = -1;
        if (slot == -1)
        {
            continue;
        }
        set_effect(state, count, slot, shape);
        ++count;
        if (count == 4)
        {
            return;
        }
    }
}

// Xbox 0x000991B0, ECX state, EAX body
void update_runtime_swing_effects(SwingState& state, RuntimeSwingStateContext& context)
{
    const u32 held = held_weapon(context);
    if (held == 0)
    {
        release_runtime_swing_effects(state); // 0x00099120
        return;
    }
    const Actor& weapon = context.pickup.weapon_at(held).actor;
    const Vec3 euler = actor_world_euler(weapon); // 0x00041820
    const auto& records = swing_records[body_state_key(state)];
    for (size_t index = 0; index < 4; ++index)
    {
        const i32 slot = effect_slot(state, index);
        if (slot == -1)
        {
            continue;
        }
        ActorEffectNode* effect = g_actor_effect_nodes.at(size_t(slot));
        if (effect == nullptr || records.shapes[index] == nullptr)
        {
            throw Error("swing state effect slot is empty in DAT_00506930");
        }
        Vec3 center;
        shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(records.shapes[index]), weapon.xform,
            shape_pose(weapon), center); // 0x00056330
        put_body_vec3(effect->bytes, 0x1C, center);
        put_body_vec3(effect->bytes, 0x50, euler);
    }
}

// Xbox 0x00098780
void approach_runtime_swing_weapon(SwingState& state, RuntimeSwingStateContext& context)
{
    const u32 weapon_address = mind_weapon(context);
    if (weapon_address == 0)
    {
        return;
    }
    Actor& actor = context.pickup.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    Vec3 point;
    float radius = 0.f;
    i32 index = 0;
    // EBX the weapon, EDI the position, ESI the point; the result is not tested.
    const Actor& weapon = context.pickup.weapon_at(weapon_address).actor;
    if (!select_actor_spawn_point(weapon, position, point, radius, index)) // 0x001476D0
    {
        throw Error("0x00098780 reads the pick-up point of a weapon without pick-up spheres");
    }
    const Vec3 at = actor_world_position(weapon); // 0x000412A0
    float yaw = 0.f;
    if (!runtime_direction_angle(Vec3{at.x - point.x, at.y - point.y, at.z - point.z}, yaw)) // 0x00105F90
    {
        runtime_direction_angle(Vec3{at.x - position.x, at.y - position.y, at.z - position.z}, yaw);
    }
    const MinaPlayer* player = first_player(context.pickup.animation);
    const float fraction = player == nullptr ? 0.f : player->position;
    const Vec3 from = state.start_euler;
    const Vec3 euler{step_runtime_angle_toward(0.f, from.x, fraction), step_runtime_angle_toward(yaw, from.y, fraction),
        step_runtime_angle_toward(0.f, from.z, fraction)}; // 0x001013A0
    const Vec3 start = state.start_position;
    const Vec3 moved{(point.x - start.x) * fraction + start.x, (point.y - start.y) * fraction + start.y,
        (point.z - start.z) * fraction + start.z};
    actor_set_world_position(actor, moved); // 0x00041730
   body_record(context.pickup.body).position = actor_world_position(actor); // 0x000412A0 into body +0x18
    wrap_runtime_euler(euler, actor.euler); // 0x00105AA0
    actor.xform_valid = 0; // Actor +0x2A8.
   body_record(context.pickup.body).euler = actor_world_euler(actor); // 0x00041820 into body +0x72C
}

// Xbox 0x00098E00, EAX state
bool attach_runtime_swing_weapon(SwingState& state, RuntimeSwingStateContext& context)
{
    const u32 weapon_address = mind_weapon(context);
    if (weapon_address == 0)
    {
        return false;
    }
    Actor& actor = context.pickup.actor;
    actor_refresh_transform(actor); // 0x00040DE0
    const Vec3 actor_euler = actor_world_euler(actor); // 0x00041820 (0x000412A0's position is overwritten)
    auto& weapon = context.pickup.weapon_at(weapon_address);
    const Vec3 at = actor_world_position(weapon.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(weapon.actor); // 0x00041820
    state.weapon_position = at;
    state.weapon_euler = euler;
    if (!attach_runtime_player_weapon(weapon, 0, context.pickup.attachment, context.pickup.default_position,
        context.pickup.default_euler)) // 0x0001B190, EAX the weapon
    {
        return false;
    }
    Vec3 turn;
    wrap_runtime_euler_difference(actor_euler, euler, turn); // 0x001057F0
    actor_set_world_position(weapon.actor, at); // 0x00041730
    wrap_runtime_euler(turn, weapon.actor.euler); // 0x00105AA0
    weapon.actor.xform_valid = 0;
    actor_refresh_transform(weapon.actor); // 0x00040DE0
    send_weapon(context, weapon_address, {0x95, read_u32(context.pickup.body, 8)});
    send_words(context.pickup.actor_message, {0x48});
    return true;
}

// Xbox 0x00098980, EAX state
u32 pick_runtime_swing_side(RuntimeSwingStateContext& context)
{
    const Actor& actor = context.pickup.actor;
    i32 best = 0;
    i32 sum = 0;
    float nearest = 0.f;
    const auto marks = context.marks();
    for (const RuntimeDirectorActor& mark : marks)
    {
        if (mark.actor == &actor)
        {
            continue;
        }
        if (mark.actor == nullptr)
        {
            throw Error("0x00098980 reads a DAT_00511B38 node without its actor view");
        }
        if (mark.body.empty() || mark.body_bits == nullptr)
        {
            throw Error("0x00098980 reads a DAT_00511B38 actor without its body views");
        }
        if (body_record(mark.body).unk_69 != 0)
        {
            continue;
        }
        const u8 bits = mark.body_bits->bytes[3]; // Body +0x15C -> +8, byte 3.
        if ((bits & 8) != 0 || (bits & 0x20) == 0)
        {
            continue;
        }
        const i32 side = runtime_actor_side(*mark.actor, actor, nullptr); // 0x00026720
        sum += side;
        const Vec3 from = actor_world_position(actor); // 0x000412A0
        const Vec3 to = actor_world_position(*mark.actor);
        const float dx = from.x - to.x;
        const float dy = from.y - to.y;
        const float dz = from.z - to.z;
        const float distance = dz * dz + dx * dx + dy * dy;
        if (best == 0 || nearest > distance)
        {
            best = side;
            nearest = distance;
        }
    }
    if (sum == 0) // An empty list leaves the sum zero too.
    {
        sum = best;
    }
    if (sum > 0)
    {
        return 2;
    }
    if (sum < 0)
    {
        return 3;
    }
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    return 0.5f < roll ? 2u : 3u; // DAT_0032B0F4: JC keeps 2.
}

// Xbox 0x00098B30, EBX state, XMM0 direction
void swing_runtime_weapon(SwingState& state, const AnimationRecord& record, float direction,
    RuntimeSwingStateContext& context)
{
    state.direction = direction;
    state.spin_euler = actor_world_euler(context.pickup.actor); // 0x00041820, EDI state +0x40
    const u32 weapon = mind_weapon(context);
    if (weapon == 0)
    {
        throw Error("0x00098B30 reads a missing weapon's +0x998 through address zero");
    }
    RuntimeBodyCollisionState* markers = context.weapon_collision(weapon);
    if (markers != nullptr)
    {
        activate_all_runtime_body_contact_shapes(context.pickup.weapon_at(weapon).actor, *markers, 1); // 0x0013F5B0
        send_weapon(context, weapon, {0x92, 1});
    }
    start_runtime_body_animation_record(record, context.pickup.attachment.actions); // 0x0001AFA0, ESI body
    body_record(context.pickup.body).unk_6A = u8(body_record(context.pickup.body).unk_6A + 1);
    context.pickup.weapon_callouts(weapon, 0x4D); // 0x0003BE20 over the weapon's +0x8C
    start_runtime_swing_effects(state, context); // 0x00098F40, EAX body
    send_words(context.pickup.actor_message, {0x2B});
}

// Xbox 0x00098C10, EAX state
bool spin_runtime_swing_weapon(SwingState& state, RuntimeSwingStateContext& context)
{
    Actor& actor = context.pickup.actor;
    const u32 weapon = mind_weapon(context);
    if (weapon == 0)
    {
        throw Error("0x00098C10 trails a missing weapon through address zero");
    }
    float trail = state.trail_timer;
    ActorTrailContext trails = context.trails(weapon);
    request_actor_video_rate_trail(context.pickup.weapon_at(weapon).actor, std::bit_cast<float>(0x3E19999Au), trail,
        trails); // The inlined 0x0013B1B0: 0x0013B030 at half the video rate.
    state.trail_timer = trail;
    bool done = false;
    const float timer = state.spin_timer;
    if (timer > 0.f)
    {
        const float left = timer - runtime_actor_step(actor, context.pickup.clock);
        state.spin_timer = left;
        if (!(0.f < left))
        {
            state.spin_timer = 0.f;
            done = true;
        }
    }
    const float time = 1.f - state.spin_timer * std::bit_cast<float>(0x3F2AAAABu); // DAT_0032B204.
    const float yaw = sample_spin_curve(time) * kTurn * state.direction + state.spin_euler.y;
    wrap_runtime_euler(Vec3{0.f, yaw, 0.f}, actor.euler); // 0x00105AA0
    actor.xform_valid = 0;
   body_record(context.pickup.body).euler = actor_world_euler(actor); // 0x00041820 into body +0x72C
    return done;
}

// Xbox 0x00099280
void release_runtime_swing_weapon(SwingState& state, RuntimeSwingStateContext& context)
{
    const auto body = context.pickup.body;
    const u32 weapon = mind_weapon(context);
    const u32 held = held_weapon(context);
    if (held != 0)
    {
        Actor& released = context.pickup.weapon_at(held).actor;
        detach_runtime_body_actor(body, context.pickup.attachment.ownership); // 0x0001B4C0
        actor_set_world_position(released, state.weapon_position); // 0x00041730
        wrap_runtime_euler(state.weapon_euler, released.euler); // 0x00105AA0
        released.xform_valid = 0;
    }
    if (state.counted != 0)
    {
        const u32 count = body_record(body).holds - 1;
        body_record(body).holds = count;
        if (count == 0)
        {
            send_words(context.body_message, {0x4A});
        }
        state.counted = 0;
       body_record(body).position.y = state.start_position.y; // Body +0x18..+0x20 keeps x and z.
    }
    if (weapon == 0)
    {
        throw Error("0x00099280 reads a missing weapon's +0x614 sound through address zero");
    }
    Actor& taken = context.pickup.weapon_at(weapon).actor;
    ActorSoundSlot& sound = taken.sound_slots[4]; // Weapon +0x614.
    if (actor_sound_is_active(sound, context.sounds) && i16(sound.previous_bone) != -1) // 0x0012A970
    {
        release_actor_sound(sound, context.sounds); // 0x0012A3D0
    }
    RuntimeBodyCollisionState* markers = context.weapon_collision(weapon);
    if (markers != nullptr)
    {
        deactivate_all_runtime_body_contact_shapes(*markers); // 0x0013F660
        send_weapon(context, weapon, {0x92, 0});
    }
    body_record(body).unk_6A = u8(body_record(body).unk_6A - 1);
    release_runtime_swing_effects(state); // 0x00099120
}

// Xbox 0x000993A0
void start_runtime_swing_cutscene(SwingState& state, RuntimeSwingStateContext& context)
{
    const char* suffix = runtime_door_room_suffix(context.room, context.aliases); // 0x001410F0
    if (suffix == nullptr)
    {
        return;
    }
    const Vec3 position = actor_world_position(context.pickup.actor); // 0x000412A0
    const u32 weapon = mind_weapon(context);
    if (weapon == 0)
    {
        throw Error("0x000993A0 reads a missing weapon's pick-up spheres through address zero");
    }
    Vec3 point;
    float radius = 0.f;
    i32 index = 0;
    if (!select_actor_spawn_point(context.pickup.weapon_at(weapon).actor, position, point, radius, index)) // 0x001476D0
    {
        return;
    }
    char name[0x80];
    format_dialogue_resource_name(name, "aid_cutscene_ghoulies_bob_%s%d", suffix, index); // 0x00035830
    if (find_animation_resource(context.resources.records, name) == nullptr
        && !cutscene_resource_exists(name, context.resources)) // DAT_004DF250's open records, then 0x000356D0
    {
        return;
    }
    RuntimeCutsceneInstance* instance = start_scene_cutscene(name, 0, 0, context.cutscenes()); // 0x0004EDA0
    swing_records[body_state_key(state)].cutscene = instance;
    state.cutscene = instance == nullptr ? 0u : context.heap_address(instance);
    if (instance == nullptr)
    {
        return;
    }
    const u8 cameras = instance->resource[2];
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    instance->camera = i32(roll * float(i32(cameras))); // CVTTSS2SI.
}

// Xbox 0x00098680, ECX state, EAX phase
void start_runtime_swing_phase(SwingState& state, u32 phase, RuntimeSwingStateContext& context)
{
    state.phase = phase;
    auto& actions = context.pickup.attachment.actions;
    switch (phase)
    {
    case 1:
        start_runtime_body_animation_record(record_at(state, kGrabAnimation), actions); // 0x0001AFA0
        state.start_position = actor_world_position(context.pickup.actor); // 0x000412A0, EDI +0x4C
        state.start_euler = actor_world_euler(context.pickup.actor); // 0x00041820, EDI +0x58
        return;
    case 2:
        start_runtime_body_animation_record(record_at(state, kLiftPositiveAnimation), actions);
        return;
    case 3:
        start_runtime_body_animation_record(record_at(state, kLiftNegativeAnimation), actions);
        return;
    case 4:
        swing_runtime_weapon(state, record_at(state, kSwingPositiveAnimation), 1.f, context); // DAT_0032B0F8
        return;
    case 5:
        swing_runtime_weapon(state, record_at(state, kSwingNegativeAnimation), -1.f, context); // DAT_0032B120
        return;
    case 6:
        release_runtime_swing_weapon(state, context); // 0x00099280
        start_runtime_body_animation_record(record_at(state, kDropPositiveAnimation), actions);
        return;
    case 7:
        release_runtime_swing_weapon(state, context); // 0x00099280
        start_runtime_body_animation_record(record_at(state, kDropNegativeAnimation), actions);
        return;
    case 8:
        select_runtime_body_next_action(state.base, 0, context.next); // 0x00134180, EAX body
        return;
    default:
        return;
    }
}

// The inlined 0x0003AAA0 pairs (the attached actor's +0xDB0 variant first, then the plain name).
void find_animation(SwingState& state, u32& found_word, SwingAnimation animation, u32 name,
    RuntimeSwingStateContext& context)
{
    const AnimationRecord* record = find_runtime_body_animation(context.image_string(name),
        context.pickup.attachment.actions);
    swing_records[body_state_key(state)].animations[animation] = record;
    found_word = record != nullptr ? 1u : 0u;
}

template <typename From>
void copy_name(char* target, std::span<const u8> parameters, From from)
{
    const char* source = field_data(parameters, from);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}
}

// Xbox 0x00026720, EAX body, ECX other
i32 runtime_actor_side(const Actor& other, const Actor& actor, u32* ahead)
{
    const Vec3 to = actor_world_position(other); // 0x000412A0
    const Vec3 from = actor_world_position(actor);
    float yaw = 0.f;
    runtime_direction_angle(Vec3{to.x - from.x, to.y - from.y, to.z - from.z}, yaw); // 0x00105F90
    const float turn = wrap_signed(yaw - actor.euler.y); // Actor +0x18.
    if (ahead != nullptr)
    {
        *ahead = engine_absolute(turn) < std::bit_cast<float>(0x43652EE1u) ? 1u : 0u; // 0x00152BB0, DAT_0032B4A8
    }
    return turn > 0.f ? 1 : -1; // DAT_0032B100.
}

// Xbox 0x001057F0, ESI from, EDI to, EBX output
void wrap_runtime_euler_difference(const Vec3& from, const Vec3& to, Vec3& output)
{
    output.x = wrap_signed(from.x - to.x);
    output.y = wrap_signed(from.y - to.y);
    output.z = wrap_signed(from.z - to.z);
}

// Xbox 0x00097F90
void swing_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(SwingState);
    output.parameters = parameter::kSize;
}

// Xbox 0x00098010
bool initialize_runtime_swing_state(SwingState& state, std::span<const u8> parameters,
    RuntimeSwingStateContext& context)
{
    const auto body = context.pickup.body;
    swing_records[body_state_key(state)] = SwingRecords{};
    for (size_t index = 0; index < 4; ++index)
    {
        state.effects[index].slot = -1;
    }
    find_animation(state, state.grab, kGrabAnimation, get(parameters, parameter::grab), context);
    find_animation(state, state.swing_positive, kSwingPositiveAnimation,
        get(parameters, parameter::swing_positive), context);
    find_animation(state, state.swing_negative, kSwingNegativeAnimation,
        get(parameters, parameter::swing_negative), context);
    find_animation(state, state.lift_positive, kLiftPositiveAnimation,
        get(parameters, parameter::lift_positive), context);
    find_animation(state, state.lift_negative, kLiftNegativeAnimation,
        get(parameters, parameter::lift_negative), context);
    find_animation(state, state.drop_positive, kDropPositiveAnimation,
        get(parameters, parameter::drop_positive), context);
    find_animation(state, state.drop_negative, kDropNegativeAnimation,
        get(parameters, parameter::drop_negative), context);
    state.shape_mode = body_record(body).push_mode;
    copy_name(state.hit_sound.data(), parameters, parameter::hit_sound);
    copy_name(state.trail_effect.data(), parameters, parameter::trail_effect);
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    state.phase = 1;
    start_runtime_body_animation_record(record_at(state, kGrabAnimation),
        context.pickup.attachment.actions); // 0x0001AFA0, ESI body
    state.start_position = actor_world_position(context.pickup.actor); // 0x000412A0, EDI +0x4C
    state.start_euler = actor_world_euler(context.pickup.actor); // 0x00041820, EDI +0x58
    adjust_body_counters(body, 1);
    increment_runtime_body_latches(body, context.pickup.actor, *context.pickup.latches); // 0x0001FC40, ECX body
    body_record(body).move_holds = u16(body_record(body).move_holds + 1);
    set_runtime_body_shape_mode(0, context.shape); // 0x0001FEB0, BL 0
    body_record(body).holds = body_record(body).holds + 1;
    context.pickup.bits.bytes[7] |= 2; // Body +0x15C -> +8, byte 7.
    state.trail_timer = 0.f;
    state.counted = 1;
    state.spin_timer = 1.5f; // DAT_0032B210.
    start_runtime_swing_cutscene(state, context); // 0x000993A0
    return true;
}

// Xbox 0x00098370
void destroy_runtime_swing_state(SwingState& state, RuntimeSwingStateContext& context)
{
    const auto body = context.pickup.body;
    adjust_body_counters(body, -1);
    decrement_runtime_body_latches(body, context.pickup.actor, *context.pickup.latches); // 0x0001FD20, ECX body
    body_record(body).move_holds = u16(body_record(body).move_holds - 1);
    set_runtime_body_shape_mode(state.shape_mode, context.shape); // 0x0001FEB0, BL state +0x120
    context.pickup.bits.bytes[7] &= 0xFD;
    const u32 weapon = mind_weapon(context);
    if (weapon != 0)
    {
        release_actor_sound(context.pickup.weapon_at(weapon).actor.sound_slots[4], context.sounds); // 0x0012A3D0
    }
    if (state.counted != 0)
    {
        const u32 count = body_record(body).holds - 1;
        body_record(body).holds = count;
        if (count == 0)
        {
            send_words(context.body_message, {0x4A});
        }
        state.counted = 0;
    }
    release_runtime_swing_effects(state); // 0x00099120
    if (state.cutscene != 0)
    {
        stop_scene_cutscene(swing_records[body_state_key(state)].cutscene, context.cutscenes()); // 0x0004EEA0
        state.cutscene = 0;
    }
    swing_records.erase(body_state_key(state));
}

// Xbox 0x00098460
void tick_runtime_swing_state(SwingState& state, RuntimeSwingStateContext& context)
{
    Actor& actor = context.pickup.actor;
    state.base.elapsed = runtime_actor_step(actor, context.pickup.clock) + state.base.elapsed;
    const auto& animation = context.pickup.animation;
    switch (state.phase)
    {
    case 1:
        approach_runtime_swing_weapon(state, context); // 0x00098780
        if (!runtime_actor_animation_slot_complete(animation, 0)) // 0x0003B290, ECX 0
        {
            return;
        }
        if (!attach_runtime_swing_weapon(state, context)) // 0x00098E00
        {
            select_runtime_body_next_action(state.base, 0, context.next); // 0x00134180, EAX body
            return;
        }
        start_runtime_swing_phase(state, pick_runtime_swing_side(context), context); // 0x00098980, 0x00098680
        return;
    case 2:
        if (runtime_actor_animation_slot_complete(animation, 0))
        {
            state.phase = 4;
            swing_runtime_weapon(state, record_at(state, kSwingPositiveAnimation), 1.f, context); // 0x00098B30
        }
        return;
    case 3:
        if (runtime_actor_animation_slot_complete(animation, 0))
        {
            state.phase = 5;
            swing_runtime_weapon(state, record_at(state, kSwingNegativeAnimation), -1.f, context);
        }
        return;
    case 4:
        update_runtime_swing_effects(state, context); // 0x000991B0
        if (spin_runtime_swing_weapon(state, context)) // 0x00098C10
        {
            state.phase = 6;
            release_runtime_swing_weapon(state, context); // 0x00099280
            start_runtime_body_animation_record(record_at(state, kDropPositiveAnimation),
                context.pickup.attachment.actions); // 0x0001AFA0
        }
        return;
    case 5:
        update_runtime_swing_effects(state, context);
        if (spin_runtime_swing_weapon(state, context))
        {
            state.phase = 7;
            release_runtime_swing_weapon(state, context);
            start_runtime_body_animation_record(record_at(state, kDropNegativeAnimation),
                context.pickup.attachment.actions);
        }
        return;
    case 6:
    case 7:
        if (runtime_actor_animation_slot_complete(animation, 0))
        {
            state.phase = 8;
            select_runtime_body_next_action(state.base, 0, context.next); // 0x00134180, EAX body
        }
        return;
    default:
        return;
    }
}

// Xbox 0x00098620
void message_runtime_swing_state(SwingState& state, std::span<const u32> message, RuntimeSwingStateContext& context)
{
    if (message[0] == 0x5A)
    {
        const char* sound = state.hit_sound.data();
        if (sound[0] == 0)
        {
            return;
        }
        // Word +0x18 the actor hit, +0x1C its contact pool; the pool's +4 (the first record) is the position.
        Actor* hit = context.actor_at(message[6]);
        if (hit == nullptr || message[7] == 0)
        {
            throw Error("0x00098620 reads a hit report's actor or contact pool through address zero");
        }
        const auto pool = context.record_at(message[7], sizeof(GhoulySpawnPool));
        const auto* contacts = reinterpret_cast<const GhoulySpawnPool*>(pool.data());
        queue_actor_sound(hit->sound_slots[2], 0, hit, 2, reinterpret_cast<const Vec3*>(contacts->begin), nullptr, 1.f,
            sound, context.sounds); // 0x0012A260, EBX the actor's +0x4A4
        return;
    }
    if (message[0] == 0x7F && message[1] == state.cutscene)
    {
        state.cutscene = 0;
        swing_records[body_state_key(state)].cutscene = nullptr;
    }
}

void install_runtime_swing_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSwingStateContext&(u32 body_address)> context_of)
{
    swing_context_of = std::move(context_of);
    types[0x78] = ObjectFactoryType{9, swing_state_lifecycle}; // Getter 0x00097FB0.
}
}
