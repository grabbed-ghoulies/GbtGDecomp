#include "body_explode_state.h"
#include "actor_render_registration.h"
#include "actor_runtime_queries.h"
#include "actor_transform.h"
#include "body_death_effects.h"
#include "body_resources.h"
#include "entity.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "giant_hand_strategy.h"
#include "player_goal_cleanup.h"
#include "player_mind_requests.h"
#include "segment_records.h"
#include "body_fields.h"

#include <array>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{

std::function<RuntimeExplodeStateContext&(u32)> explode_context_of;
// The +0x20 override pools, by the state's bytes; the field holds whether there is one.
std::unordered_map<const u8*, GhoulySpawnPool*> explode_overrides;

constexpr float kBlastTime = 0.3f; // DAT_0032B268.
constexpr float kBlastReach = 25.f; // DAT_0032B170.
constexpr float kBlastShrink = 3.3333333f; // DAT_0032B3B4.
constexpr float kBlastShrinkScale = 17.f; // DAT_0032B518.
constexpr float kBlastNearest = 8.f; // DAT_0032B26C.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

GhoulySpawnPool*& overrides_of(const ExplodeState& state)
{
    return explode_overrides[body_state_key(state)];
}

void sync_overrides(ExplodeState& state)
{
    state.overrides = overrides_of(state) != nullptr ? 1u : 0u;
}

// The body's +0x309..+0x30C counts and +0x6A move by `step`.
void step_counters(std::span<u8> body, i32 step)
{
    body[0x6A] = u8(body[0x6A] + step);
    body[0x309] = u8(body[0x309] + step);
    body[0x30B] = u8(body[0x30B] + step);
    body[0x30A] = u8(body[0x30A] + step);
    body[0x30C] = u8(body[0x30C] + step);
}

bool construct_explode_state_view(FactoryObject& object, std::span<const u8>)
{
    ExplodeState& state = body_state_view<ExplodeState>({object.bytes.get(), object.size});
    return initialize_runtime_explode_state(state, explode_context_of(state.base.body));
}

void destroy_explode_state_view(FactoryObject& object)
{
    ExplodeState& state = body_state_view<ExplodeState>({object.bytes.get(), object.size});
    destroy_runtime_explode_state(state, explode_context_of(state.base.body));
    explode_overrides.erase(body_state_key(state));
}

const ObjectFactoryVtable* explode_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005057E0, explode_state_object_sizes, construct_explode_state_view,
        destroy_explode_state_view};
    return &table;
}
}

// Xbox 0x0008E740
void explode_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(ExplodeState);
    output.parameters = 0xC;
}

// Xbox 0x0008E7C0
bool initialize_runtime_explode_state(ExplodeState& state, RuntimeExplodeStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    step_counters(body, 1);
    increment_runtime_body_latches(body, knockdown.actor, context.latches); // The inlined 0x0001FC40.
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) + 1);
    put_word(body, 0x330, read_u32(body, 0x334));
    set(body, body_field::ease_rate, 0.f);
    unregister_actor_segment_probes(context.render); // 0x0003E850
    body[0x38] = 1;
    body[0x39] = 1;
    state.phase = 0u;
    return true;
}

// Xbox 0x0008E930
void destroy_runtime_explode_state(ExplodeState& state, RuntimeExplodeStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    step_counters(body, -1);
    decrement_runtime_body_latches(body, knockdown.actor, context.latches); // The inlined 0x0001FD20.
    put_word(body, 0x334, read_u32(body, 0x330));
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) - 1);
    GhoulySpawnPool*& overrides = overrides_of(state);
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides); // 0x0013EE80
        engine_heap_free(overrides); // 0x000663E0
        overrides = nullptr;
        sync_overrides(state);
    }
}

// Xbox 0x0008EA90
void tick_runtime_explode_state(ExplodeState& state, RuntimeExplodeStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    Actor& actor = knockdown.actor;
    state.base.elapsed = runtime_actor_rate(actor) * runtime_frame_step(knockdown.clock) + state.base.elapsed;
    put_vector(knockdown.body, 0x318, knockdown.default_position); // DAT_0050DD80.
    switch (state.phase)
    {
    case 0:
        if (runtime_actor_animation_slot_complete(knockdown.animation, 0)) // 0x0003B290, ECX 0, EAX actor
        {
            start_runtime_explode_blast(state, context); // 0x0008EB80, EAX actor
            state.phase = 1u;
        }
        return;
    case 1:
        if (blast_runtime_explode_contacts(state, actor, context)) // 0x0008EBD0, EAX body
        {
            state.phase = 2u;
        }
        return;
    case 2:
        if (!runtime_actor_sound_playing(actor, context.sounds)) // 0x00041CB0, EAX actor
        {
            state.phase = 2u;
            finish_runtime_body_death(0, knockdown.scene_flags, knockdown.restart_requested,
                knockdown.death); // 0x00134AE0, EAX actor
        }
        return;
    default:
        return;
    }
}

// Xbox 0x0008EB80
void start_runtime_explode_blast(ExplodeState& state, RuntimeExplodeStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    hide_runtime_actor(context.actor_view, knockdown.bits); // 0x001364B0, EBX actor
    remove_runtime_actor_collision(context.actor_view, context.state); // 0x00136540, ESI actor
    state.blast_time = kBlastTime;
    knockdown.callouts(0x17, nullptr); // 0x0003BE20 over actor +0x8C, options 0
}

// Xbox 0x0008EBD0
bool blast_runtime_explode_contacts(ExplodeState& state, Actor& actor, RuntimeExplodeStateContext& context)
{
    RuntimeKnockdownContext& knockdown = context.knockdown;
    const std::span<const u8> body = knockdown.body;
    const float shrunk = kBlastReach - state.blast_time * kBlastShrink * kBlastShrinkScale;
    float radius;
    if (kBlastNearest > shrunk)
    {
        radius = kBlastNearest;
    }
    else
    {
        radius = kBlastReach;
        if (!(shrunk > kBlastReach))
        {
            radius = shrunk;
        }
    }
    Vec3 center = actor_world_position(actor); // 0x000412A0, ESI actor
    center.y = knockdown.actor.radius_y * get(body, body_field::height_scale) * kHalf + center.y;
    GhoulySpawnPool*& overrides = overrides_of(state);
    if (overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*overrides); // 0x0013EE10, EDI overrides
    }
    const i32 count = entity_collect_contact_probes(center, radius, 0x20142); // 0x00122B00
    if (overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*overrides); // 0x0013EE80
    }
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
        RuntimeSegmentContactActor& object = context.contact_actor(*kept[slot].object);
        const Actor& hit = object.actor;
        Vec3 point = actor_world_position(hit); // 0x000412A0, EDI point
        // x87: the half height difference stays on the stack; the extent's absolute value is discarded.
        const float spread = (hit.model_size_y - hit.model_size_z) * hit.radius_y;
        point.y = hit.radius_y * hit.model_size_z + point.y;
        const float half = engine_absolute(spread); // 0x00152BB0
        engine_absolute(hit.model_size_x * hit.radius_xz); // 0x00152BB0
        point.y = half * kHalf + point.y;
        RuntimeBlastNotice notice{0x21, 0, &actor, point};
        object.message(&notice); // Vtable +0xC.
        if (notice.reply == 0)
        {
            context.report(actor, object, point, kept[slot].records, knockdown.default_position, 0); // 0x0001F8C0
        }
        engine_heap_free(kept[slot].records); // HeapFree on the block's heap (0x000F7100).
        insert_runtime_body_contact_override(overrides, object, 0, context.players); // 0x0013EBE0
        sync_overrides(state);
    }
    const float step = runtime_frame_step(knockdown.clock);
    const float rate = runtime_actor_rate(knockdown.actor);
    const float left = state.blast_time;
    if (!(left > 0.f))
    {
        return false;
    }
    const float next = left - rate * step;
    state.blast_time = next;
    if (0.f < next) // COMISS; JC keeps a positive time.
    {
        return false;
    }
    state.blast_time = 0.f;
    return true;
}

void install_runtime_explode_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeExplodeStateContext&(u32 body_address)> context_of)
{
    explode_context_of = std::move(context_of);
    types[0x68] = ObjectFactoryType{9, explode_state_lifecycle}; // Getter 0x0008E760.
}
}
