#include "body_scoop_whack_state.h"
#include <cstddef>
#include "mind_fields.h"
#include "actor_collision_culling.h"
#include "actor_live_attachments.h"
#include "actor_runtime_queries.h"
#include "actor_shape_bounds.h"
#include "shape_transform.h"
#include "gbtg/entity.h"
#include "body_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace parameter = scoop_whack_state_parameter;

std::function<RuntimeScoopWhackStateContext&(u32)> scoop_whack_context_of;

constexpr float kStrikeStart = 0.22f; // DAT_0032B500.
constexpr float kStrikeEnd = 0.32f; // DAT_0032B4FC.

// The body's latches and the actor's live attachment holds (+0x8F8 records, +0x15) all move by `step`.
void hold_body(i32 step, RuntimeScoopWhackStateContext& context)
{
    for (auto* live = static_cast<ActorLiveAttachment*>(context.live_attachments.next); live != nullptr;
         live = static_cast<ActorLiveAttachment*>(live->owner_link.next))
    {
        live->hold = u8(live->hold + step);
    }
    body_record(context.body).unk_6D = u8(body_record(context.body).unk_6D + step);
    body_record(context.body).unk_6A = u8(body_record(context.body).unk_6A + step);
}

// Slot 0's player of the actor's animation controller (+0x50 -> +0x58), or null.
MinaPlayer* slot_player(RuntimeScoopWhackStateContext& context)
{
    AnimationController* controller = context.animation.controller.get();
    if (controller == nullptr || controller->slots.empty())
    {
        return nullptr;
    }
    return controller->slots[0].player.get();
}

// The resource's own length (player +0 -> +8).
float resource_duration(const MinaPlayer& player)
{
    if (player.mina.data() == nullptr)
    {
        throw Error("scoop and whack state reads +8 of slot 0's animation through address zero");
    }
    return read_f32(player.mina, 8);
}

// The shape pose 0x00056330 reads: the model instance's +0x168, else +0x164.
const EntityBonePose* shape_pose(const Actor& actor)
{
    if (actor.model == nullptr)
    {
        return nullptr;
    }
    const EntityBonePose* pose = actor.model->alternate_shape_pose;
    return pose != nullptr ? pose : actor.model->primary_shape_pose;
}

bool construct_scoop_whack_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    ScoopWhackState& state = body_state_view<ScoopWhackState>({object.bytes.get(), object.size});
    return initialize_runtime_scoop_whack_state(state, parameters, scoop_whack_context_of(state.base.body));
}

void destroy_scoop_whack_state_view(FactoryObject& object)
{
    ScoopWhackState& state = body_state_view<ScoopWhackState>({object.bytes.get(), object.size});
    destroy_runtime_scoop_whack_state(scoop_whack_context_of(state.base.body));
}

const ObjectFactoryVtable* scoop_whack_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505AA0, scoop_whack_state_object_sizes,
        construct_scoop_whack_state_view, destroy_scoop_whack_state_view};
    return &table;
}
}

// Xbox 0x0009EB20
void scoop_whack_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(ScoopWhackState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0009EBA0
bool initialize_runtime_scoop_whack_state(ScoopWhackState& state, std::span<const u8> parameters,
    RuntimeScoopWhackStateContext& context)
{
    const auto body = context.body;
    state.callout = get(parameters, parameter::callout);
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    hold_body(1, context);
    ++body_record(body).unk_309;
    ++body_record(body).unk_30A;
    ++body_record(body).unk_30B;
    ++body_record(body).unk_30C;
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        MinaPlayer* player = slot_player(context);
        const float length = player != nullptr ? resource_duration(*player) : 0.f;
        retime_mina_playback(player, length * 0.5f, controller->dirty); // 0x00030DB0, EAX 0, DAT_0032B0F4
    }
    return true;
}

// Xbox 0x0009EC40
void destroy_runtime_scoop_whack_state(RuntimeScoopWhackStateContext& context)
{
    const auto body = context.body;
    --body_record(body).unk_309;
    --body_record(body).unk_30A;
    --body_record(body).unk_30B;
    --body_record(body).unk_30C;
    hold_body(-1, context);
}

// Xbox 0x0009EC90
void tick_runtime_scoop_whack_state(ScoopWhackState& state, RuntimeScoopWhackStateContext& context)
{
    const auto body = context.body;
    const u32 target = context.mind.data() == nullptr ? 0u : mind_record(context.mind).target_record;
    state.base.elapsed = runtime_actor_rate(context.actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    level_body_euler(body);
    if (state.struck == 0 && target != 0)
    {
        const MinaPlayer* player = slot_player(context);
        if (player != nullptr && player->position >= kStrikeStart && !(kStrikeEnd < player->position))
        {
            const u32 target_actor = read_u32(context.target_at(target), 0x10);
            const Actor* struck = context.actor_at(target_actor);
            if (struck == nullptr)
            {
                throw Error("0x0009EF30 reads the bound of a target actor outside the scene");
            }
            Vec3 point;
            if (find_runtime_scoop_whack_contact(context.actor, *struck, point)) // 0x0009EF30
            {
                const std::array<u32, 5> message{0x4C, context.actor_address, std::bit_cast<u32>(point.x),
                    std::bit_cast<u32>(point.y), std::bit_cast<u32>(point.z)};
                context.message(target_actor, message); // Vtable +0xC.
                state.struck = 1u;
            }
        }
    }
    AnimationController* controller = context.animation.controller.get();
    MinaPlayer* player = slot_player(context);
    if (controller != nullptr && player != nullptr)
    {
        // Passing 0.32 this frame (either way round, or across the wrap) restores the animation's own speed.
        const float now = player->position; // +0x14.
        const float before = player->previous; // +0x10C.
        bool crossed = false;
        if (now != before)
        {
            float low = before;
            float high = now;
            if (0.f > player->speed) // +0x1C.
            {
                low = now;
                high = before;
            }
            if (high > low)
            {
                crossed = !(kStrikeEnd < low) && high > kStrikeEnd;
            }
            else
            {
                crossed = kStrikeEnd >= low || high > kStrikeEnd;
            }
        }
        if (crossed)
        {
            retime_mina_playback(player, resource_duration(*player), controller->dirty); // 0x00030DB0, EAX 0
        }
    }
    if (controller != nullptr && player != nullptr)
    {
        bool done = false;
        if (player->mode == 1) // +0x2A, played once.
        {
            done = 0.f > player->speed ? !(0.f < player->position) : !(player->position < 1.f); // DAT_0032B0F8.
        }
        if (!done)
        {
            return;
        }
    }
    state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
}

// Xbox 0x0009EED0
void message_runtime_scoop_whack_state(const ScoopWhackState& state, std::span<const u32> message,
    RuntimeScoopWhackStateContext& context)
{
    if (message[0] == 0x41)
    {
        const Vec3 point{std::bit_cast<float>(message[7]), std::bit_cast<float>(message[8]),
            std::bit_cast<float>(message[9])};
        call_out_runtime_hit_point(point, message[5], state.callout, context.actor_callout); // 0x0013AD60
        return;
    }
    if (message[0] == 0x70 && message[2] == context.actor_address && message[1] + 0x8Cu != 0)
    {
        context.actor_callout(message[1], state.callout, nullptr); // 0x0003BE20
    }
}

// Xbox 0x0013AD60, EAX point, ECX actor
void call_out_runtime_hit_point(const Vec3& point, u32 actor, u32 id, const RuntimeActorCallout& callout)
{
    RuntimeActorCreationOptions& options = g_actor_creation_options; // DAT_004E1C5C.
    options.flags = 0;
    options.position = point;
    options.flags = 1;
    if (actor + 0x8Cu == 0)
    {
        return;
    }
    callout(actor, id, &options); // 0x0003BE20
}

// Xbox 0x0009EF30, EDX the target record
bool find_runtime_scoop_whack_contact(const Actor& actor, const Actor& target, Vec3& point)
{
    Vec3 bound;
    float radius = 0.f;
    actor_model_world_bound(target, bound, radius); // 0x0003DA00, EAX the target actor
    // The loop also scans the model's node table for kind 4 and keeps nothing from it.
    for (const u8* shape = find_runtime_actor_shape(actor, 0x20002, 0, -1); shape != nullptr;
         shape = next_runtime_actor_shape(actor, shape, 0x20002, 0, -1)) // 0x00042560, BL 0, DI -1
    {
        Vec3 center;
        const float shape_radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(shape),
            actor.xform, shape_pose(actor), center); // 0x00056330, EAX actor +0x2AC
        if (runtime_collision_spheres_overlap(bound, radius, center, shape_radius)) // 0x0015A270
        {
            point = center;
            return true;
        }
    }
    return false;
}

void install_runtime_scoop_whack_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeScoopWhackStateContext&(u32 body_address)> context_of)
{
    scoop_whack_context_of = std::move(context_of);
    types[0x7E] = ObjectFactoryType{9, scoop_whack_state_lifecycle}; // Getter 0x0009EB40.
}
}
