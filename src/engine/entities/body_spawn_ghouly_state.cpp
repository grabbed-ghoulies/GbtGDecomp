#include "body_spawn_ghouly_state.h"
#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "body_death_drops.h"
#include "body_scoop_whack_state.h"
#include "ghouly_spawn_scheduler.h"
#include "player_mind_events.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"
#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = spawn_ghouly_state_field;
namespace parameter = spawn_ghouly_state_parameter;

std::function<RuntimeSpawnGhoulyStateContext&(u32)> spawn_ghouly_context_of;
// The host half of state +0x1C: the spawn animation's record, by the state's bytes.
std::unordered_map<const u8*, const AnimationRecord*> spawn_animations;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

// The four input counters (+0x309 .. +0x30C) move together.
void step_inputs(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// Each of the actor's live attachment records (+0x8F8) holds `step` more.
void step_live_holds(i32 step, RuntimeSpawnGhoulyStateContext& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
}

// Xbox 0x000A1FD0, EAX message: the ghouly's {0x4D} placement. A free point within the actor's radius (+0x1C4 times
// +0x38) of the point twice that far ahead (kind 3, facing back toward the spawn point), else one between one and two
// radii of the spawn point (kind 3), else the spawn point itself with the actor's heading (kind 2).
bool place_runtime_spawned_ghouly(std::span<u32> message, const Vec3& spawn, RuntimeSpawnGhoulyStateContext& context)
{
    const Actor& actor = context.actor;
    const float radius = context.actor.model_size_x * context.actor.radius_xz; // +0x1C4, +0x38
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float heading = actor.euler.y; // Actor +0x18.
    const float reach = radius * 2.f; // DAT_0032B110.
    const Vec3 ahead{float(retail_sin(heading) * static_cast<long double>(reach) + position.x), position.y,
        float(retail_cos(heading) * static_cast<long double>(reach) + position.z)}; // 0x00153320, 0x00153230
    const i32 floor = i32(read_u32(context.actor_bytes, 0xCEC));
    Vec3 found{};
    bool placed = entity_detail::select_navigation_spawn_position(floor, nullptr, ahead, 0.f, radius, nullptr,
        nullptr, 0, 0, found, context.random, context.occupancy_initial); // 0x00111420
    if (!placed)
    {
        placed = entity_detail::select_navigation_spawn_position(floor, nullptr, spawn, radius, reach, nullptr,
            nullptr, 0, 0, found, context.random, context.occupancy_initial); // 0x00111420
    }
    if (!placed)
    {
        message[1] = 2;
        message[2] = std::bit_cast<u32>(spawn.x);
        message[3] = std::bit_cast<u32>(spawn.y);
        message[4] = std::bit_cast<u32>(spawn.z);
        message[7] = 0;
        message[6] = 0;
        message[5] = 0;
        message[6] = std::bit_cast<u32>(heading);
        return false;
    }
    message[1] = 3;
    message[2] = std::bit_cast<u32>(found.x);
    message[3] = std::bit_cast<u32>(found.y);
    message[4] = std::bit_cast<u32>(found.z);
    message[7] = 0;
    message[6] = 0;
    message[5] = 0;
    const Vec3 direction{found.x - spawn.x, found.y - spawn.y, found.z - spawn.z};
    float yaw = 0.f;
    runtime_direction_angle(direction, yaw); // 0x00105F90, EBX message +0x18
    message[6] = std::bit_cast<u32>(yaw);
    return true;
}

bool construct_spawn_ghouly_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_spawn_ghouly_state(state, parameters, spawn_ghouly_context_of(body_address_of(state)));
}

void destroy_spawn_ghouly_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_spawn_ghouly_state(state, spawn_ghouly_context_of(body_address_of(state)));
}

// Getter 0x000A1A10: sizes are 0x0009EB20; +0xC / +0x14 / +0x18 are the empty 0x001628B0 and +0x1C is 0x0002B620.
const ObjectFactoryVtable* spawn_ghouly_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505B80, scoop_whack_state_object_sizes,
        construct_spawn_ghouly_state_view, destroy_spawn_ghouly_state_view};
    return &table;
}
}

// Xbox 0x000A1A70
bool initialize_runtime_spawn_ghouly_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSpawnGhoulyStateContext& context)
{
    const std::span<u8> body = context.body;
    step_live_holds(1, context);
    body[0x6D] = u8(body[0x6D] + 1);
    step_inputs(body, 1);
    increment_runtime_body_latches(body, context.actor, context.latches); // 0x0001FC40, ECX body
    put_half(body, 0x30, u16(read_u16(body, 0x30) + 1));
    body[0x38] = 1;
    body[0x39] = 1;
    RuntimeBodyStateContext shape = context.state();
    set(state, field::shape_mode, set_runtime_body_shape_mode(2, shape)); // 0x0001FEB0, BL 2, EDI body
    // The inlined 0x0003AAA0 pair over the body's held actor (+0x48) +0xDB0 variant.
    const AnimationRecord* record = find_runtime_body_animation(context.image_string(get(parameters,
        parameter::animation)), context.actions);
    spawn_animations[state.data()] = record;
    set(state, field::animation, record != nullptr ? 1u : 0u);
    if (read_u32(context.actor_bytes, 0xB98) == 0)
    {
        finish_runtime_body_death(1, context.scene_flags, context.restart_requested, context.death); // 0x00134AE0
    }
    return true;
}

// Xbox 0x000A1B60
void destroy_runtime_spawn_ghouly_state(std::span<u8> state, RuntimeSpawnGhoulyStateContext& context)
{
    const std::span<u8> body = context.body;
    step_inputs(body, -1);
    step_live_holds(-1, context);
    body[0x6D] = u8(body[0x6D] - 1);
    decrement_runtime_body_latches(body, context.actor, context.latches); // 0x0001FD20, ECX body
    put_half(body, 0x30, u16(read_u16(body, 0x30) - 1));
    RuntimeBodyStateContext shape = context.state();
    set_runtime_body_shape_mode(get(state, field::shape_mode), shape); // 0x0001FEB0, BL state +0x18
    spawn_animations.erase(state.data());
}

// Xbox 0x000A1BE0
void tick_runtime_spawn_ghouly_state(std::span<u8> state, RuntimeSpawnGhoulyStateContext& context)
{
    const std::span<u8> body = context.body;
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed, runtime_actor_rate(actor) * runtime_frame_step(context.clock)
        + get(state, body_state_field::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if ((context.bits.bytes[0] & 4) != 0) // Body +0x15C -> +8, bit 2: the animation's spawn moment.
    {
        std::array<char, 0x80> name{};
        std::array<char, 0x80> variant{};
        bool spawn = take_runtime_body_drop_names(context.actor_bytes, name.data(), variant.data()); // 0x000286B0
        if (!spawn)
        {
            GhoulySpawnRecord* record = context.spawn_record; // Actor +0xB98.
            if (record == nullptr)
            {
                throw Error("0x000A1BE0 asks actor +0xB98's spawn record (0x000F40A0) through address zero");
            }
            const i32 result = dispatch_next_ghouly_spawn(*record, 1, context.scene.spawns()); // 0x000F40A0
            if (result != -1 && result == 1 && record->remaining_c != -1)
            {
                --record->remaining_c;
            }
            take_runtime_body_drop_names(context.actor_bytes, name.data(), variant.data()); // 0x000286B0
            u32 prefix;
            std::memcpy(&prefix, name.data(), 4);
            spawn = prefix == 0x5F646961; // "aid_"
        }
        if (spawn)
        {
            RuntimeBodyStateContext shape = context.state();
            set_runtime_body_shape_mode(0, shape); // 0x0001FEB0, BL 0, EDI body
            Vec3 point{};
            bool joint_found = false;
            if (actor.model != nullptr && actor.model->cpu.data() != nullptr) // Actor +0x204 and its first word.
            {
                const u32 joint = actor_lookup_joint_index(actor.model->cpu, 0) & 0xFFu; // 0x00052FC0
                if (joint != 0xFFu && joint != 0xFFFFFFFFu)
                {
                    actor_transform_joint_point(actor, u16(joint), context.default_position, point); // 0x0003AEB0
                    joint_found = true;
                }
            }
            if (!joint_found)
            {
                point = actor_world_position(actor); // 0x000412A0
            }
            std::array<u32, 9> placement{};
            placement[0] = 0x4D;
            const bool placed = place_runtime_spawned_ghouly(placement, point, context); // 0x000A1FD0
            const i32 kind = std::bit_cast<i16>(read_u16(context.actor_bytes, 0xB9C));
            const Vec3 euler{std::bit_cast<float>(placement[5]), std::bit_cast<float>(placement[6]),
                std::bit_cast<float>(placement[7])};
            if (kind == 0)
            {
                u32 variant_prefix;
                std::memcpy(&variant_prefix, variant.data(), 4);
                const char* variant_name = variant_prefix == 0x5F646961 ? variant.data() : nullptr;
                u32 prefix;
                std::memcpy(&prefix, name.data(), 4);
                if (prefix == 0x5F646961)
                {
                    auto clone = clone_executable_resource(context.resources, name.data()); // 0x00107720, EAX name
                    if (clone.bytes != nullptr)
                    {
                        std::memcpy(clone.bytes.get() + 8, &point, 12);
                        std::memcpy(clone.bytes.get() + 0x14, &euler, 12);
                        const float scale = 1.f; // DAT_0032B0F8.
                        const u16 zero = 0;
                        std::memcpy(clone.bytes.get() + 0x20, &scale, 4);
                        std::memcpy(clone.bytes.get() + 0x2AA, &zero, 2);
                        std::memcpy(clone.bytes.get() + 0x2AC, &zero, 2);
                        if (variant_name != nullptr)
                        {
                            char* output = reinterpret_cast<char*>(clone.bytes.get() + 0x5BA);
                            const char* source = variant_name;
                            char character;
                            do
                            {
                                character = *source++;
                                *output++ = character;
                            } while (character != '\0');
                        }
                        FactoryObject object = create_factory_object(context.factory,
                            {clone.bytes.get(), clone.size}); // 0x00107520
                        clone.bytes.reset(); // 0x000663E0
                        const u32 created = object.bytes == nullptr ? 0u : context.scene.keep(std::move(object));
                        if (created != 0)
                        {
                            context.message(created, placement); // Vtable +0xC with the {0x4D} placement.
                            // {0x51}; words 1..3 are the spawn point's stack slots.
                            const std::array<u32, 4> emerge{0x51, std::bit_cast<u32>(point.x),
                                std::bit_cast<u32>(point.y), std::bit_cast<u32>(point.z)};
                            context.message(created, emerge);
                        }
                    }
                }
            }
            else if (kind > 0 && kind <= 2)
            {
                FactoryObject object = create_runtime_actor_at(name.data(), &point, &euler, 1.f, context.resources,
                    context.factory, context.default_position, context.default_euler); // 0x00112C50, EAX name
                if (object.bytes != nullptr)
                {
                    const u32 created = context.scene.keep(std::move(object));
                    if (placed)
                    {
                        const std::array<u32, 7> arrive{0x96, placement[2], placement[3], placement[4], placement[5],
                            placement[6], placement[7]};
                        context.message(created, arrive); // Vtable +0xC.
                    }
                }
            }
        }
    }
    // LAB 0x000A1F2E: while slot 0's animation runs (mode 1, not at its end) the state waits.
    const AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        const MinaPlayer* player = controller->slots.at(0).player.get(); // +0x58 [0].
        if (player != nullptr)
        {
            bool finished = false;
            if (player->mode == 1)
            {
                if (0.f > player->speed)
                {
                    finished = 0.f >= player->position;
                }
                else
                {
                    finished = player->position >= 1.f; // DAT_0032B0F8.
                }
            }
            if (!finished)
            {
                return;
            }
        }
    }
    const GhoulySpawnRecord* record = context.spawn_record; // Actor +0xB98.
    if (read_u32(context.actor_bytes, 0xBA0) == 0x5F646961 || (record != nullptr && record->remaining_c != 0))
    {
        const AnimationRecord* animation = spawn_animations.at(state.data());
        if (animation == nullptr)
        {
            throw Error("0x000A1BE0 replays the spawn animation (0x0001AFA0) that 0x0003AAA0 did not find");
        }
        start_runtime_body_animation_record(*animation, context.actions); // 0x0001AFA0, ECX state +0x1C, ESI body
        return;
    }
    finish_runtime_body_death(1, context.scene_flags, context.restart_requested, context.death); // 0x00134AE0
}

void install_runtime_spawn_ghouly_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSpawnGhoulyStateContext&(u32 body_address)> context_of)
{
    spawn_ghouly_context_of = std::move(context_of);
    types[0x85] = ObjectFactoryType{9, spawn_ghouly_state_lifecycle}; // Getter 0x000A1A10.
}
}
