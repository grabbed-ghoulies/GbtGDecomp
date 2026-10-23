#include "body_flamethrower_state.h"
#include "actor_shape_bounds.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "body_gaze.h"
#include "direction_pitch.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "segment_records.h"
#include "../graphics/actor_effect_nodes.h"
#include "gbtg/actor_runtime.h"

#include <array>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = flamethrower_state_field;
namespace parameter = flamethrower_state_parameter;

std::function<RuntimeFlamethrowerStateContext&(u32)> flamethrower_context_of;
// The +0x130 override pools, by the state's bytes; the field holds whether there is one.
std::unordered_map<const u8*, GhoulySpawnPool*> flamethrower_overrides;

constexpr float kAimOffset = 1.0471975803375244f; // DAT_0032B484, 60 degrees.
constexpr float kQuarterTurn = 1.5707963705062866f; // DAT_0032B140.

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vec3(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

GhoulySpawnPool*& overrides_of(std::span<const u8> state)
{
    return flamethrower_overrides[state.data()];
}

void sync_overrides(std::span<u8> state)
{
    set(state, field::overrides, overrides_of(state) != nullptr ? 1u : 0u);
}

// Body +0x72C..+0x734: the euler the body turns its actor to.
void set_body_turn(std::span<u8> body, float pitch, float yaw, float roll)
{
    put_float(body, 0x72C, pitch);
    put_float(body, 0x730, yaw);
    put_float(body, 0x734, roll);
}

// The inlined 0x00042360 over a state timer.
bool count_down(float& timer, RuntimeFlamethrowerStateContext& context)
{
    if (timer > 0.f)
    {
        const float remaining = timer - runtime_actor_step(context.actor, context.clock);
        timer = remaining;
        if (!(0.f < remaining))
        {
            timer = 0.f;
            return true;
        }
    }
    return false;
}

bool count_down_timer(std::span<u8> state, RuntimeFlamethrowerStateContext& context)
{
    float timer = get(state, field::timer);
    const bool elapsed = count_down(timer, context);
    set(state, field::timer, timer);
    return elapsed;
}

ActorEffectNode& effect_node(i32 slot)
{
    ActorEffectNode* effect = g_actor_effect_nodes.at(size_t(slot));
    if (effect == nullptr)
    {
        throw Error("flamethrower state effect slot is empty in DAT_00506930");
    }
    return *effect;
}

// The actor's euler with its pitch raised 60 degrees, and the kind 5 locator's world point.
Vec3 flame_euler(const Actor& actor)
{
    Vec3 euler = actor_world_euler(actor); // 0x00041820
    euler.x += kAimOffset;
    return euler;
}

// The inlined string copy.
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

bool construct_flamethrower_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_flamethrower_state(state, parameters, flamethrower_context_of(body_address_of(state)));
}

void destroy_flamethrower_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_flamethrower_state(state, flamethrower_context_of(body_address_of(state)));
    flamethrower_overrides.erase(state.data());
}

const ObjectFactoryVtable* flamethrower_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505820, flamethrower_state_object_sizes,
        construct_flamethrower_state_view, destroy_flamethrower_state_view};
    return &table;
}
}

// Xbox 0x0008F6D0
void flamethrower_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008F750
bool initialize_runtime_flamethrower_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeFlamethrowerStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, field::effect_slot, -1);
    set(state, field::effect_joint, 0xFFFFFFFFu);
    copy_name(state, field::sound, parameters, parameter::sound);
    copy_name(state, field::effect, parameters, parameter::effect);
    body[0x38] = 1;
    body[0x39] = 1;
    ++body[0x309];
    ++body[0x30A];
    ++body[0x30B];
    ++body[0x30C];
    ++body[0x6A];
    set(state, field::timer, 1.25f); // DAT_0032B114.
    set(state, field::yaw, actor.euler.y);
    set(state, field::phase, 0u);
    set(state, field::reach, 0.01f); // DAT_0032B124.
    overrides_of(state) = nullptr;
    sync_overrides(state);
    const char* sound = field_data(state, field::sound);
    if (sound[0] != 0)
    {
        queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f, sound,
            context.sounds); // 0x0012A260, EBX actor +0x614
    }
    start_runtime_flamethrower_effect(state, context); // 0x0008FCD0
    return true;
}

// Xbox 0x0008F850
void destroy_runtime_flamethrower_state(std::span<u8> state, RuntimeFlamethrowerStateContext& context)
{
    const auto body = context.body;
    --body[0x309];
    --body[0x30A];
    --body[0x30B];
    --body[0x30C];
    --body[0x6A];
    if (field_data(state, field::sound)[0] != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    GhoulySpawnPool*& overrides = overrides_of(state);
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides); // 0x0013EE80
        engine_heap_free(overrides); // 0x000663E0
        overrides = nullptr;
        sync_overrides(state);
    }
    const i32 slot = get(state, field::effect_slot);
    if (slot != -1)
    {
        ActorEffectNode& effect = effect_node(slot);
        effect.bytes[0x18] |= 8;
        put_float(effect.bytes, 8, -1.f); // DAT_0032B120.
        set(state, field::effect_slot, -1);
        set(state, field::effect_joint, 0xFFFFFFFFu);
    }
}

// Xbox 0x0008F8F0
void tick_runtime_flamethrower_state(std::span<u8> state, RuntimeFlamethrowerStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed, runtime_actor_step(actor, context.clock)
        + get(state, body_state_field::elapsed));
    constexpr float kSwayRate = 2.6179938316345215f; // DAT_0032B510, 150 degrees a second.
    bool next = false;
    switch (get(state, field::phase))
    {
    case 0:
    {
        const float yaw = actor.euler.y;
        set_body_turn(body, runtime_flamethrower_aim_pitch(context), yaw, 0.f); // 0x0008FC20
        if (count_down_timer(state, context)) // 0x00042360
        {
            set(state, field::phase, 1u);
            set(state, field::timer, 3.7f); // DAT_0032B514.
        }
        break;
    }
    case 1:
    {
        const float sway = runtime_actor_step(actor, context.clock) * kSwayRate + get(state, field::sway);
        set(state, field::sway, sway);
        const float pitch = runtime_flamethrower_aim_pitch(context); // 0x0008FC20
        const float yaw = float(retail_sin(get(state, field::sway)) * kQuarterTurn + get(state, field::yaw));
        set_body_turn(body, pitch, yaw, 0.f); // 0x00153320
        next = true;
        break;
    }
    case 2:
    {
        set(state, field::sway, runtime_actor_step(actor, context.clock) * kSwayRate + get(state, field::sway));
        const Vec3 euler = actor_world_euler(actor); // 0x00041820
        // The aim's pitch is stored for both the pitch and the yaw, and the turn tested is the pitch's.
        const float pitch = runtime_flamethrower_aim_pitch(context); // 0x0008FC20
        set_body_turn(body, pitch, pitch, euler.z);
        if (runtime_relative_angle(pitch, get(state, field::yaw)) > kQuarterTurn) // 0x00100E40
        {
            set(state, field::phase, 3u);
        }
        next = true;
        break;
    }
    case 3:
    {
        const Vec3 euler = actor_world_euler(actor); // 0x00041820
        const float yaw = euler.y - runtime_actor_step(actor, context.clock) * kSwayRate;
        set_body_turn(body, runtime_flamethrower_aim_pitch(context), yaw, euler.z); // 0x0008FC20
        if (-kQuarterTurn > runtime_relative_angle(yaw, get(state, field::yaw))) // 0x00100E40, DAT_0032B13C
        {
            set(state, field::phase, 2u);
        }
        next = true;
        break;
    }
    default:
        break;
    }
    if (next && count_down_timer(state, context)) // 0x00042360
    {
        set(state, body_state_field::next_key, context.next_key()); // Body vtable +0x20.
    }
    if (overrides_of(state) != nullptr)
    {
        age_runtime_body_contact_overrides(*overrides_of(state), context.clock); // 0x0013ED20
    }
    sweep_runtime_flamethrower(state, context); // 0x0008FEC0
    move_runtime_flamethrower_effect(state, context); // 0x0008FE10
}

// Xbox 0x0008FBF0
void message_runtime_flamethrower_state(std::span<u8> state, std::span<const u32> message,
    RuntimeFlamethrowerStateContext& context)
{
    if (message[0] != 0x7C)
    {
        return;
    }
    GhoulySpawnPool* overrides = overrides_of(state);
    if (overrides == nullptr)
    {
        return;
    }
    Actor* object = context.actor_at(message[1]);
    if (object == nullptr)
    {
        throw Error("0x0008FBF0 lets an actor back in through address zero");
    }
    remove_runtime_body_contact_override(overrides, *object); // 0x0013EDD0, EDX message +4
}

// Xbox 0x0008FC20
float runtime_flamethrower_aim_pitch(RuntimeFlamethrowerStateContext& context)
{
    const auto mind = context.mind;
    if (mind.data() == nullptr)
    {
        return 0.f;
    }
    const u32 address = read_u32(mind, 0x14);
    if (address == 0)
    {
        return 0.f;
    }
    const auto target = context.target_at(address);
    const Actor& actor = context.actor;
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float eye = read_f32(context.body, 0x884) * actor.radius_y + position.y;
    const Vec3 direction{read_f32(target, 0x20) - position.x,
        (read_f32(target, 0x30) * 0.25f + read_f32(target, 0x24)) - eye, // DAT_0032B104.
        read_f32(target, 0x28) - position.z};
    float pitch = 0.f;
    runtime_direction_pitch(direction, pitch); // 0x00106000
    return pitch - kAimOffset;
}

// Xbox 0x0008FCD0
void start_runtime_flamethrower_effect(std::span<u8> state, RuntimeFlamethrowerStateContext& context)
{
    const char* name = field_data(state, field::effect);
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return;
    }
    const Actor& actor = context.actor;
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return;
    }
    const u32 joint = actor_lookup_joint_index(actor.model->cpu, 5) & 0xFF; // 0x00052FC0
    if (joint == 0xFF || joint == 0xFFFFFFFFu)
    {
        return;
    }
    g_actor_creation_options.flags = 0;
    const Vec3 euler = flame_euler(actor);
    Vec3 point;
    actor_transform_joint_point(actor, u16(joint), actor_transform_origin, point); // 0x0003AEB0
    g_actor_creation_options.position = point;
    g_actor_creation_options.flags |= 1;
    g_actor_creation_options.flags |= 4;
    g_actor_creation_options.euler = euler;
    if (load_actor_effect_stream(name, &g_actor_creation_options, context.effects) == 0) // 0x00053170
    {
        return;
    }
    const i32 slot = g_last_actor_effect;
    g_last_actor_effect = -1;
    if (slot == -1)
    {
        return;
    }
    set(state, field::effect_slot, slot);
    set(state, field::effect_joint, joint);
}

// Xbox 0x0008FE10
void move_runtime_flamethrower_effect(std::span<const u8> state, RuntimeFlamethrowerStateContext& context)
{
    const i32 slot = get(state, field::effect_slot);
    if (slot == -1)
    {
        return;
    }
    const Actor& actor = context.actor;
    const Vec3 euler = flame_euler(actor);
    Vec3 point;
    actor_transform_joint_point(actor, u16(get(state, field::effect_joint)), actor_transform_origin,
        point); // 0x0003AEB0
    ActorEffectNode& effect = effect_node(slot);
    put_vec3(effect.bytes, 0x1C, point);
    put_vec3(effect.bytes, 0x50, euler);
}

// Xbox 0x0008FEC0
void sweep_runtime_flamethrower(std::span<u8> state, RuntimeFlamethrowerStateContext& context)
{
    if (get(state, field::effect_slot) == -1)
    {
        return;
    }
    Actor& actor = context.actor;
    const Vec3 euler = flame_euler(actor);
    if (get(state, field::aimed) == 0)
    {
        set(state, field::flame_pitch, euler.x);
        set(state, field::flame_yaw, euler.y);
        set(state, field::aimed, 1u);
    }
    const float step = runtime_actor_step(actor, context.clock);
    const float turn = step * 1.7453292608261108f; // DAT_0032B50C, 100 degrees a second.
    set(state, field::flame_pitch, step_runtime_damage_angle(get(state, field::flame_pitch), euler.x,
        turn)); // 0x00100FE0
    set(state, field::flame_yaw, step_runtime_damage_angle(get(state, field::flame_yaw), euler.y, turn));
    const float reach = step * 1.9800000190734863f + get(state, field::reach); // DAT_0032B460.
    set(state, field::reach, reach);
    if (reach > 1.f) // DAT_0032B0F8.
    {
        set(state, field::reach, 1.f);
    }
    Vec3 direction{};
    add_sound_direction(direction, get(state, field::flame_pitch), get(state, field::flame_yaw), 1.f); // 0x00106220
    Vec3 point;
    actor_transform_joint_point(actor, u16(get(state, field::effect_joint)), actor_transform_origin,
        point); // 0x0003AEB0
    GhoulySpawnPool*& overrides = overrides_of(state);
    // Each segment probe's actor (DAT_00511AF4, probe - 0x1E0) whose shapes carry 0x800 stays out of the sweep.
    for (entity_detail::SegmentProbe* probe = entity_detail::g_world.segment; probe != nullptr; probe = probe->next)
    {
        RuntimeSegmentContactActor& object = context.contact_actor(*probe);
        if ((runtime_actor_physical_shape_flags(object.actor) & 0x800) != 0) // 0x00042500
        {
            insert_runtime_body_contact_override(overrides, object, 0, context.players); // 0x0013EBE0
            sync_overrides(state);
        }
    }
    if (overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*overrides); // 0x0013EE10, EDI overrides
    }
    const u32 filter = actor.model_flags;
    actor.model_flags = 0;
    float previous = 3.f; // DAT_0032B260.
    for (i32 index = 0; index < 5; ++index)
    {
        // DAT_0032B1AC, DAT_0032B20C; clamped to [1.5, 4] (DAT_0032B210, DAT_0032B1B0).
        const float scale = float(index) * 0.2f * 2.5f + 1.5f;
        float factor = 4.f;
        if (!(scale > 4.f))
        {
            factor = 1.5f;
            if (!(1.5f > scale))
            {
                factor = scale;
            }
        }
        const float radius = get(state, field::reach) * factor;
        const float distance = radius + previous;
        Vec3 center{direction.x * distance + point.x, direction.y * distance + point.y,
            direction.z * distance + point.z};
        const i32 count = entity_collect_contact_probes(center, radius, 0x20002); // 0x00122B00
        struct Contact
        {
            entity_detail::SegmentProbe* object;
            GhoulySpawnPool* records;
        };
        std::array<Contact, entity_detail::kSegmentSlots> kept;
        for (i32 slot = 0; slot < count; ++slot)
        {
            auto& found = entity_detail::g_segment_slots[slot]; // DAT_00511A1C.
            kept[slot] = {found.object, copy_segment_records(found.pool)}; // 0x000FCC90
        }
        for (i32 slot = 0; slot < count; ++slot)
        {
            report_runtime_flamethrower_contact(state, context.contact_actor(*kept[slot].object), direction, center,
                kept[slot].records, context); // 0x000902A0
            engine_heap_free(kept[slot].records); // HeapFree on the block's heap (0x000F7100).
        }
        previous = radius;
        point = center;
    }
    actor.model_flags = filter;
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides); // The inlined 0x0013EE80.
    }
}

// Xbox 0x000902A0, EAX state, ECX direction, ESI point, EDI object
void report_runtime_flamethrower_contact(std::span<u8> state, RuntimeSegmentContactActor& object,
    const Vec3& direction, const Vec3& point, GhoulySpawnPool* contacts, RuntimeFlamethrowerStateContext& context)
{
    insert_runtime_body_contact_override(overrides_of(state), object, 1, context.players); // 0x0013EBE0
    sync_overrides(state);
    const Vec3 velocity{direction.x * 50.f, direction.y * 50.f, direction.z * 50.f}; // DAT_0032B1E0.
    RuntimeContactNotice notice{0x15, 0, &context.actor, velocity, contacts, point};
    object.message(&notice); // Vtable +0xC.
    RuntimeContactReport report{0x41, -1, velocity, &object, contacts, point, notice.reply};
    context.report(report); // The actor's vtable +0xC.
}

void install_runtime_flamethrower_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeFlamethrowerStateContext&(u32 body_address)> context_of)
{
    flamethrower_context_of = std::move(context_of);
    types[0x6A] = ObjectFactoryType{9, flamethrower_state_lifecycle}; // Getter 0x0008F6F0.
}
}
