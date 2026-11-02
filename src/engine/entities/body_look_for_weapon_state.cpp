#include "body_look_for_weapon_state.h"
#include "mind_fields.h"
#include "actor_spawn_eligibility.h"
#include "actor_spawn_query.h"
#include "body_orientation.h"
#include "player_mind_requests.h"
#include "../graphics/billboard_queue.h"
#include "body_fields.h"

#include <array>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = look_for_weapon_state_field;
namespace parameter = look_for_weapon_state_parameter;

std::function<RuntimeLookForWeaponStateContext&(u32)> look_for_weapon_context_of;

struct LookForWeaponViews
{
    const AnimationRecord* found = nullptr; // +0x24
    std::span<const u8> puzzled; // +0x28
    std::span<const u8> bulb; // +0x2C
    std::span<const u8> glow; // +0x30
};
std::unordered_map<const u8*, LookForWeaponViews> look_for_weapon_views;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// A sprite AID (0x00035E90); a missing one raises resource error 2 (0x00032700).
std::span<const u8> acquire_sprite(const char* name, RuntimeLookForWeaponStateContext& context)
{
    u32 prefix = 0;
    std::memcpy(&prefix, name, 4);
    if (prefix != 0x5F646961) // "aid_"
    {
        return {};
    }
    const auto sprite = acquire_animation_resource(context.registry, name); // 0x00035E90
    if (sprite.data() == nullptr)
    {
        resource_error(context.error, 2); // 0x00032700
    }
    return sprite;
}

void release_sprite(std::span<const u8>& sprite, std::span<u8> state, u32 offset,
    RuntimeLookForWeaponStateContext& context)
{
    if (read_u32(state, offset) == 0)
    {
        return;
    }
    release_animation_resource(context.registry, sprite); // 0x00035C00
    sprite = {};
    const u32 zero = 0;
    std::memcpy(state.data() + offset, &zero, 4);
}

bool construct_look_for_weapon_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_look_for_weapon_state(state, parameters,
        look_for_weapon_context_of(body_address_of(state)));
}

void destroy_look_for_weapon_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_look_for_weapon_state(state, look_for_weapon_context_of(body_address_of(state)));
    look_for_weapon_views.erase(state.data());
}

const ObjectFactoryVtable* look_for_weapon_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505A40, look_for_weapon_state_object_sizes,
        construct_look_for_weapon_state_view, destroy_look_for_weapon_state_view};
    return &table;
}
}

// Xbox 0x0009D8A0
void look_for_weapon_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0009D920
bool initialize_runtime_look_for_weapon_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeLookForWeaponStateContext& context)
{
    const auto body = context.body;
    const Actor& actor = context.actor;
    set(state, field::found, 0u);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const float shortest = get(parameters, parameter::shortest_wait);
    set(state, field::wait, (get(parameters, parameter::longest_wait) - shortest) * fraction + shortest);
    auto& views = look_for_weapon_views[state.data()];
    // The inlined 0x0003AAA0 pair: the attached actor's +0xDB0 variant first, then the plain name.
    views.found = find_runtime_body_animation(context.image_string(get(parameters, parameter::found_animation)),
        context.actions);
    set(state, field::found_animation, views.found != nullptr ? 1u : 0u);
    u32 joint = 0xFFFFFFFFu;
    if (actor.model != nullptr && actor.model->cpu.data() != nullptr)
    {
        const u32 index = actor_lookup_joint_index(actor.model->cpu, 4) & 0xFF; // 0x00052FC0
        if (index != 0xFF)
        {
            joint = index;
        }
    }
    set(state, field::joint, joint);
    views.puzzled = acquire_sprite(field_data(parameters, parameter::puzzled), context);
    set(state, field::puzzled, views.puzzled.data() != nullptr ? 1u : 0u);
    views.bulb = acquire_sprite(field_data(parameters, parameter::bulb), context);
    set(state, field::bulb, views.bulb.data() != nullptr ? 1u : 0u);
    views.glow = acquire_sprite(field_data(parameters, parameter::glow), context);
    set(state, field::glow, views.glow.data() != nullptr ? 1u : 0u);
    body[0x38] = 1;
    body[0x39] = 1;
    body[0xA7] = 1;
    return true;
}

// Xbox 0x0009DAE0
void destroy_runtime_look_for_weapon_state(std::span<u8> state, RuntimeLookForWeaponStateContext& context)
{
    context.body[0xA7] = 0;
    auto& views = look_for_weapon_views[state.data()];
    release_sprite(views.puzzled, state, field::puzzled.offset, context);
    release_sprite(views.bulb, state, field::bulb.offset, context);
    release_sprite(views.glow, state, field::glow.offset, context);
}

// Xbox 0x0009DB30
void tick_runtime_look_for_weapon_state(std::span<u8> state, RuntimeLookForWeaponStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed, runtime_actor_step(actor, context.clock)
        + get(state, body_state_field::elapsed));
    const u32 phase = get(state, field::found);
    if (phase == 0)
    {
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
        const bool found = find_runtime_mind_weapon(context); // 0x0009DC70
        if (get(state, body_state_field::elapsed) < get(state, field::wait))
        {
            draw_runtime_look_for_weapon_state(state, context); // 0x0009DCD0
            return;
        }
        if (found)
        {
            const AnimationRecord* record = look_for_weapon_views.at(state.data()).found;
            if (record == nullptr)
            {
                throw Error("look-for-weapon animation record is null; 0x0001AFA0 reads its +0x117");
            }
            start_runtime_body_animation_record(*record, context.actions); // 0x0001AFA0
            set(state, field::found, 1u);
            request_actor_sound(actor, 2, "XACT_SOUNDBANK_GMISC_LIGHTBULBON", context.sounds); // 0x00041D00, 0x00302940
            draw_runtime_look_for_weapon_state(state, context); // 0x0009DCD0
            return;
        }
        set(state, body_state_field::next_key, context.next_key()); // Body vtable +0x20.
        draw_runtime_look_for_weapon_state(state, context);
        return;
    }
    if (phase == 1)
    {
        if (context.mind.data() != nullptr && get(context.mind, mind_field::action_actor) != 0)
        {
            const Actor* weapon = context.actor_at(get(context.mind, mind_field::action_actor));
            if (weapon == nullptr)
            {
                throw Error("0x0009DB30 turns toward a mind weapon (mind +0x18) that is not an actor");
            }
            face_runtime_body_actor(body, actor, *weapon); // 0x001345B0, ESI the weapon
        }
        if (runtime_actor_animation_slot_complete(context.actions.animation, 0)) // The inlined 0x0003B290.
        {
            set(state, body_state_field::next_key, context.next_key()); // Body vtable +0x20.
        }
    }
    draw_runtime_look_for_weapon_state(state, context); // 0x0009DCD0
}

// Xbox 0x0009DC70, EAX actor
bool find_runtime_mind_weapon(RuntimeLookForWeaponStateContext& context)
{
    const auto mind = context.mind;
    if (mind.data() == nullptr)
    {
        return false;
    }
    const u32 chosen = get(mind, mind_field::action_actor);
    if (chosen != 0)
    {
        const Actor* weapon = context.actor_at(chosen);
        if (weapon == nullptr)
        {
            throw Error("0x0009DC70 checks a mind weapon (mind +0x18) that is not an actor");
        }
        if (actor_can_supply_ghoulybox(*weapon) && context.bytes_at(chosen)[0x93C] != 0) // 0x00146150, ECX the weapon
        {
            return true;
        }
    }
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Actor* nearest = find_nearest_spawn_actor(position, &context.actor, nullptr, 0x1F); // 0x00129D70
    const u32 address = nearest == nullptr ? 0u : context.address_of(*nearest);
    set(mind, mind_field::action_actor, address);
    return address != 0;
}

// Xbox 0x0009DCD0, EAX actor, ESI state
void draw_runtime_look_for_weapon_state(std::span<const u8> state, RuntimeLookForWeaponStateContext& context)
{
    const Actor& actor = context.actor;
    const u32 joint = get(state, field::joint);
    if (joint == 0xFFFFFFFFu)
    {
        throw Error("0x0009DCD0 places the question mark at a missing kind 4 locator (joint -1)");
    }
    Vec3 point;
    actor_transform_joint_point(actor, u16(joint), actor_transform_origin, point); // 0x0003AEB0
    point.y = actor.radius_xz * 7.5f + point.y; // DAT_0032B2B0.
    const auto& views = look_for_weapon_views.at(state.data());
    constexpr u32 kColor = 0xFFFFFFFFu; // DAT_00314F48.
    if (get(state, field::found) == 0)
    {
        const std::array<float, 2> size{2.75f, 2.75f}; // DAT_004286E0.
        queue_billboard(g_billboard_queue, views.puzzled.data(), kColor, 0, point, size.data(), 0.f); // 0x0005C1A0
        return;
    }
    const std::array<float, 2> size{3.5f, 3.5f}; // DAT_004286E8.
    queue_billboard(g_billboard_queue, views.glow.data(), kColor, 0, point, size.data(), 0.f); // 0x0005C1A0
    queue_billboard(g_billboard_queue, views.bulb.data(), kColor, 0, point, size.data(), 0.f);
}

// Xbox 0x001345B0, EBX body, ESI target
void face_runtime_body_actor(std::span<u8> body, const Actor& actor, const Actor& target)
{
    if (get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f)
    {
        face_runtime_body_point(body, actor, actor_world_position(target)); // 0x000412A0, 0x00134510
    }
}

void install_runtime_look_for_weapon_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeLookForWeaponStateContext&(u32 body_address)> context_of)
{
    look_for_weapon_context_of = std::move(context_of);
    types[0x7B] = ObjectFactoryType{9, look_for_weapon_state_lifecycle}; // Getter 0x0009D8C0.
}
}
