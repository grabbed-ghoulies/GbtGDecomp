#include "fx_anim_emitter.h"
#include "actor_attachment_requests.h"
#include "actor_model_anchors.h"
#include "actor_render_registration.h"
#include "actor_scare_requests.h"
#include "actor_shape_bounds.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "avatar_fields.h"
#include "body_animation_commands.h"
#include "shape_transform.h"
#include "../camera/view_frustum.h"
#include "gbtg/anim.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
constexpr std::array<u32, 3> random_powerup_kinds{0x11, 0xE, 4}; // DAT_0030E220.

float actor_step(const RuntimeFxAnimEmitter& context)
{
    return runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
}

// The emitter's own callout context; record callbacks write tokens into +0x950.
ActorAttachmentEventContext emitter_callouts(RuntimeFxAnimEmitter& context)
{
    const ActorAttachmentEventContext& callouts = context.base.callouts;
    return ActorAttachmentEventContext{callouts.actor, callouts.attachments, callouts.effects, callouts.factory,
        callouts.resources, callouts.objects, callouts.actor_address, callouts.linked_records,
        record(context.bytes, anim_emitter_field::callout_tokens)};
}

// The inlined 0x0003AAA0/0x0003A9E0 pair that restarts the "wait" animation (0x0031CB4C).
void start_wait_animation(RuntimeFxAnimEmitter& context)
{
    if (context.animation.table == nullptr)
    {
        return;
    }
    const AnimationRecord* record = find_actor_animation(context.animation.table, "wait", nullptr, context.random);
    if (record != nullptr)
    {
        start_actor_animation(context.animation, *record, context.registry, context.error, context.random);
    }
}

float pulse_size(float phase)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    float angle;
    if (phase > tau)
    {
        angle = tau;
    }
    else if (0.f > phase)
    {
        angle = 0.f;
    }
    else
    {
        angle = phase;
    }
    const long double sine = retail_sin(angle);
    return float((sine + 1.0f) * 0.5f * std::bit_cast<float>(0x3E4CCCD0u) // DAT_0032B0F4, DAT_0032B5D4.
        + std::bit_cast<float>(0x3F666666u)); // DAT_0032B174.
}
}

// Xbox 0x00109DB0
void fx_anim_emitter_object_sizes(ObjectSizes& output)
{
    output.instance = anim_emitter_field::kSize;
    output.parameters = anim_emitter_parameter::kSize;
}

// Xbox 0x00109FC0
bool initialize_runtime_fx_anim_emitter(std::span<const u8> parameters, RuntimeFxAnimEmitter& context)
{
    if (parameters.data() == nullptr)
    {
        return false;
    }
    const u32 kind = get(parameters, anim_emitter_parameter::kind);
    if (kind == 1)
    {
        const u32 challenge = get(parameters, anim_emitter_parameter::id);
        if ((context.challenge_bits[challenge >> 3] & u8(1u << (challenge & 7))) != 0)
        {
            return false;
        }
    }
    if ((context.scene_flags & 8) != 0 || (context.runtime_flags & 0x200000) != 0)
    {
        return false;
    }
    std::span<u8> bytes = context.bytes;
    set(bytes, anim_emitter_field::kind, kind);
    set(bytes, anim_emitter_field::amount, get(parameters, anim_emitter_parameter::amount));
    set(bytes, anim_emitter_field::extra, get(parameters, anim_emitter_parameter::extra));
    set(bytes, anim_emitter_field::id, get(parameters, anim_emitter_parameter::id));
    set(bytes, anim_emitter_field::sprite_alpha, 0xC0);
    if (kind == 0x10)
    {
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float fraction = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        const i32 index = _mm_cvttss_si32(_mm_set_ss(fraction * -3.f)); // DAT_0032B1A8.
        set(bytes, anim_emitter_field::kind, random_powerup_kinds[size_t(-index)]);
    }
    if (std::memcmp(field_data(parameters, anim_emitter_parameter::sprite), "aid_", 4) == 0)
    {
        context.sprite = acquire_named_animation_resource(context.registry,
            field_data(parameters, anim_emitter_parameter::sprite), context.error);
        set(bytes, anim_emitter_field::sprite_width, 1.f);
        set(bytes, anim_emitter_field::sprite_height, 1.f);
    }
    set(bytes, anim_emitter_field::state, 1);
    start_wait_animation(context);
    return true;
}

// Xbox 0x00109980
void release_runtime_fx_emitter_attachments(RuntimeFxAnimEmitter& context)
{
    std::span<u8> bytes = context.bytes;
    for (i32 index = 0; index < get(bytes, anim_emitter_field::callout_count); ++index)
    {
        remove_runtime_body_attachment(u16(get(bytes, anim_emitter_field::callout_ids).at(size_t(index))),
            context.attachment_lists);
    }
    set(bytes, anim_emitter_field::callout_count, 0);
}

// Xbox 0x0010A120
void destroy_runtime_fx_anim_emitter(RuntimeFxAnimEmitter& context)
{
    release_runtime_fx_emitter_attachments(context);
    if (context.sprite.data() != nullptr)
    {
        release_animation_resource(context.registry, context.sprite);
        context.sprite = {};
    }
    entity_detail::ListHead& link = context.link;
    if (link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(link.prev)->next = link.next;
    }
    if (link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(link.next)->prev = link.prev;
    }
    link.prev = nullptr;
    link.next = nullptr;
}

// Xbox 0x00109D10
void launch_runtime_fx_anim_emitter(std::span<const u32, 3> landing, RuntimeFxAnimEmitter& context)
{
    std::span<u8> bytes = context.bytes;
    const Vec3 origin = actor_world_position(context.actor);
    set(bytes, anim_emitter_field::origin, origin);
    set(bytes, anim_emitter_field::landing, Vec3{std::bit_cast<float>(landing[0]), std::bit_cast<float>(landing[1]),
        std::bit_cast<float>(landing[2])});
    if (get(bytes, anim_emitter_field::kind) == 1)
    {
        field_data(bytes, anim_emitter_field::landing)->y += 2.f; // DAT_0032B110.
    }
    set(bytes, anim_emitter_field::bounces, 0);
    set(bytes, anim_emitter_field::flight_timer, 0.75f); // DAT_0032B2AC.
    if (context.address + 0x8Cu != 0)
    {
        ActorAttachmentEventContext callouts = emitter_callouts(context);
        dispatch_actor_attachment_records(context.resource.resource.data(), 0x4A, record_runtime_body_attachment_event,
            nullptr, callouts, context.address + anim_emitter_field::callout_tokens.offset);
    }
    set(bytes, anim_emitter_field::state, 0);
    if (context.animation.controller != nullptr)
    {
        release_animation_controller(context.animation.controller, false, context.registry);
        context.animation.controller = nullptr;
    }
}

// Xbox 0x0010A180
void dispatch_runtime_fx_anim_emitter_message(std::span<u32> message, RuntimeFxAnimEmitter& context)
{
    switch (message[0])
    {
    case 7:
    {
        switch (read_u16(context.memory_at(message[3]), 0xC))
        {
        case 0x1C:
        case 0x1E:
        case 0x20:
        case 0x21:
        case 0x2F:
        {
            const Vec3 camera = context.view->position;
            const Vec3 position = actor_world_position(context.actor);
            const float dz = camera.z - position.z;
            const float dy = camera.y - position.y;
            const float dx = camera.x - position.x;
            if (!((dz * dz + dy * dy) + dx * dx > 10000.f)) // DAT_0032B4D0.
            {
                context.base_message(context.base, message);
            }
            return;
        }
        default:
            return;
        }
    }
    case 0x23:
    {
        context.base_message(context.base, message);
        entity_detail::ListHead* tail = &context.emitters;
        while (tail->next != nullptr)
        {
            tail = static_cast<entity_detail::ListHead*>(tail->next);
        }
        tail->next = &context.link;
        context.link.prev = tail;
        context.link.next = nullptr;
        return;
    }
    case 0x96:
        launch_runtime_fx_anim_emitter(std::span<const u32, 3>(message.data() + 1, 3), context);
        return;
    default:
        context.base_message(context.base, message);
        return;
    }
}

// Xbox 0x001099C0
void update_runtime_fx_emitter_flight(RuntimeFxAnimEmitter& context)
{
    std::span<u8> bytes = context.bytes;
    Actor& actor = context.actor;
    if (get(bytes, anim_emitter_field::bounces) == 0)
    {
        const float step = runtime_frame_step(context.clock);
        const float rate = runtime_actor_rate(actor);
        bool landed = false;
        const float timer = get(bytes, anim_emitter_field::flight_timer);
        if (timer > 0.f)
        {
            const float remaining = timer - rate * step;
            set(bytes, anim_emitter_field::flight_timer, remaining);
            if (0.f >= remaining)
            {
                set(bytes, anim_emitter_field::flight_timer, 0.f);
                landed = true;
            }
        }
        const float progress =
            1.f - get(bytes, anim_emitter_field::flight_timer) * std::bit_cast<float>(0x3FAAAAABu); // DAT_0032B3C0.
        const Vec3 origin = get(bytes, anim_emitter_field::origin);
        const Vec3 landing = get(bytes, anim_emitter_field::landing);
        Vec3 position{(landing.x - origin.x) * progress + origin.x, (landing.y - origin.y) * progress + origin.y,
            (landing.z - origin.z) * progress + origin.z};
        const float angle = progress * std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
        const float lift = float(retail_sin(angle));
        (void)retail_cos(angle); // Retail evaluates and discards the cosine.
        position.y = lift * 8.f + position.y; // DAT_0032B26C.
        actor_set_world_position(actor, position);
        if (landed)
        {
            ActorAttachmentEventContext& callouts = context.base.callouts;
            dispatch_actor_resource_callout(context.resource, 0xC, nullptr, callouts);
            actor.anchor = Vec3{0.f, 15.f, 0.f}; // DAT_0032B214.
            set(bytes, anim_emitter_field::bounces, get(bytes, anim_emitter_field::bounces) + 1);
        }
        return;
    }
    bool settled = false;
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(actor);
    const float delta = rate * step;
    Vec3 velocity = actor.anchor;
    velocity.y = velocity.y - delta * 50.f; // DAT_0032B1E0.
    const Vec3 position = actor_world_position(actor);
    Vec3 next{velocity.x * delta + position.x, velocity.y * delta + position.y, velocity.z * delta + position.z};
    const float floor = get(bytes, anim_emitter_field::landing).y;
    if (floor > next.y && 0.f > velocity.y)
    {
        const u32 bounces = get(bytes, anim_emitter_field::bounces) + 1;
        next.y = floor;
        set(bytes, anim_emitter_field::bounces, bounces);
        if (i32(bounces) >= 3)
        {
            velocity = Vec3{0.f, 0.f, 0.f};
            settled = true;
        }
        else
        {
            velocity.y = velocity.y * -0.4f; // DAT_0032B658.
        }
    }
    actor.anchor = velocity;
    actor_set_world_position(actor, next);
    if (settled)
    {
        clear_runtime_player_attachment_events(record(bytes, anim_emitter_field::callout_tokens),
            context.attachment_lists);
        actor.anchor = context.base.callouts.effects.default_position; // DAT_0050DD80.
        set(bytes, anim_emitter_field::state, 1);
        start_wait_animation(context);
    }
}

// Xbox 0x00069590
void build_runtime_emitter_burst_parameters(std::span<u8, 0x354> parameters, const Actor& actor, u32 address,
    float scale, const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name);
    const u16 size = 0x354;
    const u16 version = 0x43;
    const u16 type = 0x10;
    std::memcpy(parameters.data(), &size, 2);
    std::memcpy(parameters.data() + 2, &version, 2);
    std::memcpy(parameters.data() + 4, &type, 2);
    set(parameters, actor_parameter_field::model_scale, actor.scale_200);
    set(parameters, avatar_afterimage_parameter::source, address);
    set(parameters, avatar_afterimage_parameter::life, scale);
    set(parameters, avatar_afterimage_parameter::alpha, 0x80);
    set(parameters, avatar_afterimage_parameter::texture_record, 0);
    set(parameters, avatar_afterimage_parameter::glow, 0);
    set(parameters, avatar_afterimage_parameter::end_scale,
        Vec3{actor.scale_x, actor.radius_y, actor.radius_xz}); // +0x30
}

// Xbox 0x0010A6D0
u32 collect_runtime_fx_anim_emitter(const EntityCylinderBound& bounds, RuntimeFxEmitterPlayer& player,
    RuntimeFxAnimEmitter& context)
{
    const Vec3 position = actor_world_position(player.body_actor);
    EntityCylinderBound reach{};
    reach.kind = 4;
    reach.size = 0x20;
    reach.flag = 1;
    reach.x = position.x;
    reach.y = position.y;
    reach.z = position.z;
    reach.pad0 = 0.f;
    reach.height = player.body_actor.radius_y * read_f32(player.body, 0x884);
    reach.pad1 = 0.f;
    reach.radius = player.body_actor.radius_xz * read_f32(player.body, 0x888);
    const bool overlapping = !runtime_cylinders_apart(bounds, reach);
    bool touching = false;
    const Actor* carried = player.case_actor;
    if (carried != nullptr)
    {
        const u8* record = find_runtime_actor_shape(*carried, 0x10, 0, -1);
        while (record != nullptr)
        {
            const EntityBonePose* pose = nullptr;
            if (carried->model != nullptr)
            {
                pose = carried->model->alternate_shape_pose != nullptr ? carried->model->alternate_shape_pose
                                                                       : carried->model->primary_shape_pose;
            }
            Vec3 center;
            const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(record),
                carried->xform, pose, center);
            const float diameter = float(static_cast<long double>(radius) + radius);
            const float bottom = center.y - radius;
            if (!(bottom > bounds.height + bounds.y) && !(bounds.y > diameter + bottom))
            {
                const float limit = bounds.radius + radius;
                const float dz = center.z - bounds.z;
                const float dx = center.x - bounds.x;
                const long double root = retail_square_root(dz * dz + dx * dx);
                float distance = float(root);
                if (static_cast<long double>(std::bit_cast<float>(0x3727C5ACu)) > root) // DAT_0032B230.
                {
                    distance = limit - 0.01f; // DAT_0032B124.
                }
                if (!(distance >= limit))
                {
                    touching = true;
                    break;
                }
            }
            record = next_runtime_actor_shape(*carried, record, 0x10, 0, -1);
        }
    }
    if (!overlapping && !touching)
    {
        return 0;
    }
    std::span<u8> bytes = context.bytes;
    dispatch_actor_resource_callout(context.resource, 0x16, nullptr, context.base.callouts);
    dispatch_actor_resource_callout(context.resource, 0x48, nullptr, context.base.callouts);
    dispatch_actor_resource_callout(player.resource, 0x49, nullptr, player.callouts);
    context.award(get(bytes, anim_emitter_field::id), player, get(bytes, anim_emitter_field::kind),
        get(bytes, anim_emitter_field::amount), get(bytes, anim_emitter_field::extra));
    std::array<u8, 0x354> parameters{};
    build_runtime_emitter_burst_parameters(parameters, context.actor, context.address, 1.f, context.default_name);
    set(parameters, avatar_afterimage_parameter::alpha, bytes[0x19B]);
    set(parameters, avatar_afterimage_parameter::texture_record, 0);
    set(parameters, avatar_afterimage_parameter::glow, 1);
    context.keep(create_factory_object(context.factory, parameters));
    unregister_actor_segment_probes(context.base.render);
    clear_actor_shape_probe(context.actor, context.base.render.shape);
    clear_actor_attachment_effects(context.attachments, context.pending);
    auto* anchor = static_cast<ActorModelAnchor*>(context.anchors.next);
    while (anchor != nullptr)
    {
        auto* next = static_cast<ActorModelAnchor*>(anchor->owner_link.next);
        release_actor_model_anchor(*anchor);
        anchor = next;
    }
    if (context.animation.controller != nullptr)
    {
        release_animation_controller(context.animation.controller, false, context.registry);
        context.animation.controller = nullptr;
    }
    set(bytes, anim_emitter_field::fade_timer, 1.f);
    set(bytes, anim_emitter_field::state, 2);
    return 1;
}

// Xbox 0x0010A590
void scan_runtime_fx_emitter_players(RuntimeFxAnimEmitter& context)
{
    const Vec3 position = actor_world_position(context.actor);
    EntityCylinderBound bounds{};
    bounds.x = position.x;
    bounds.y = position.y;
    bounds.z = position.z;
    bounds.height = float(actor_model_height(context.actor));
    bounds.pad0 = 0.f;
    bounds.pad1 = 0.f;
    const long double radius = actor_model_bound_radius(context.actor);
    bounds.radius = float(radius + radius);
    for (RuntimeFxEmitterPlayer* player : context.players)
    {
        std::span<const u8> actor = player->actor_bytes;
        if (get(actor, player_field::live) == 0)
        {
            continue;
        }
        if (get(actor, player_field::mode) != 2 && get(actor, player_field::case_link) == 0)
        {
            continue;
        }
        // Both remaining cases select reference group 1.
        if (context.resume.references[1] != 0 || context.resume.decay_enabled[context.resume.decay_index] == 0
            || read_u16(player->body, 0x94) == 0)
        {
            continue;
        }
        if (get(context.bytes, anim_emitter_field::kind) == 1 || (context.story_byte & 0x80) != 0)
        {
            if (collect_runtime_fx_anim_emitter(bounds, *player, context) != 0)
            {
                return;
            }
            continue;
        }
        const float distance = runtime_actor_squared_distance(context.actor, player->actor);
        if (!(900.f >= distance)) // DAT_0032B4EC.
        {
            return;
        }
        if (queue_runtime_intro_notice("intros_powerup_first", context.intro) != 0) // 0x0032A380.
        {
            context.story_byte |= 0x80;
        }
        return;
    }
}

// Xbox 0x0010A2E0
void update_runtime_fx_anim_emitter(RuntimeFxAnimEmitter& context)
{
    std::span<u8> bytes = context.bytes;
    update_actor_frame(context.frame, context.frame_context);
    const float first = actor_step(context) + get(bytes, anim_emitter_field::pulse_phase);
    set(bytes, anim_emitter_field::pulse_phase, first);
    const float second = actor_step(context) + get(bytes, anim_emitter_field::second_pulse_phase);
    set(bytes, anim_emitter_field::second_pulse_phase, second);
    if (first > 1.f)
    {
        float phase = get(bytes, anim_emitter_field::pulse_phase);
        do
        {
            phase = phase - 1.f;
        } while (phase > 1.f);
        set(bytes, anim_emitter_field::pulse_phase, phase);
    }
    if (second > 1.5f) // DAT_0032B210.
    {
        float phase = get(bytes, anim_emitter_field::second_pulse_phase);
        do
        {
            phase = phase - 1.5f;
        } while (phase > 1.5f);
        set(bytes, anim_emitter_field::second_pulse_phase, phase);
    }
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    set(bytes, anim_emitter_field::sprite_width, pulse_size(get(bytes, anim_emitter_field::pulse_phase) * tau));
    const float two_thirds = std::bit_cast<float>(0x3F2AAAABu); // DAT_0032B204.
    set(bytes, anim_emitter_field::sprite_height,
        pulse_size(get(bytes, anim_emitter_field::second_pulse_phase) * two_thirds * tau));
    switch (get(bytes, anim_emitter_field::state))
    {
    case 0:
        update_runtime_fx_emitter_flight(context);
        return;
    case 1:
        scan_runtime_fx_emitter_players(context);
        return;
    case 2:
    {
        const float timer = get(bytes, anim_emitter_field::fade_timer);
        if (timer > 0.f)
        {
            const float remaining = timer - actor_step(context);
            set(bytes, anim_emitter_field::fade_timer, remaining);
            if (0.f >= remaining)
            {
                set(bytes, anim_emitter_field::fade_timer, 0.f);
            }
        }
        float alpha = get(bytes, anim_emitter_field::fade_timer) * 192.f; // DAT_0032B454.
        if (alpha > 192.f)
        {
            alpha = 192.f;
        }
        else if (0.f > alpha)
        {
            alpha = 0.f;
        }
        const i32 value = _mm_cvttss_si32(_mm_set_ss(alpha));
        set(bytes, anim_emitter_field::sprite_alpha, u32(value));
        if (!runtime_actor_sound_playing(context.actor, context.sounds) && value == 0)
        {
            activate_actor_bind_tree(context.actor);
        }
        return;
    }
    default:
        return;
    }
}

// Xbox 0x00109E30
bool runtime_fx_anim_emitter_visible(u32 pass, u32 slot, RuntimeFxAnimEmitter& context)
{
    const Vec3 position = actor_world_position(context.actor);
    float radius = runtime_actor_view_radius(context.actor, context.bytes);
    if (context.matrices.matrices.count > 0)
    {
        const float reach = runtime_actor_matrix_reach(context.actor, context.matrices);
        if (reach > radius)
        {
            radius = reach;
        }
    }
    radius = radius * 2.f; // DAT_0032B110.
    if (radius == 0.f)
    {
        return false;
    }
    u8& plane = context.bytes[0x184 + size_t(pass) * 4 + slot];
    return classify_runtime_sphere_frustum(context.view->frustum, position, radius, plane) != 0;
}

// Xbox 0x00109EE0
u32 draw_runtime_fx_anim_emitter(i32 kind, RuntimeFxEmitterDraw& draw, RuntimeFxAnimEmitter& context)
{
    std::span<u8> bytes = context.bytes;
    u32 result = 0;
    if (get(bytes, anim_emitter_field::state) != 2)
    {
        result = draw_runtime_actor(draw.state, kind, draw.attachments, draw.scene);
    }
    if (context.sprite.data() != nullptr && kind == 2)
    {
        const std::array<float, 2> size{get(bytes, anim_emitter_field::sprite_width) * 5.f,
            get(bytes, anim_emitter_field::sprite_height) * 5.f}; // DAT_0032B184.
        // DAT_00314F48 with the alpha byte.
        const u32 color =
            (0xFFFFFFFFu & 0x00FFFFFFu) | (u32(field_byte(bytes, anim_emitter_field::sprite_alpha, 0)) << 24);
        Vec3 point;
        actor_transform_joint_point(context.actor, 0, context.base.callouts.effects.default_position, point); // DAT_0050DD80.
        point.y = float(static_cast<long double>(context.actor.radius_y) * 2.5f // DAT_0032B20C.
            + (actor_model_height(context.actor) + point.y));
        queue_billboard(draw.billboards, context.sprite.data(), color, 0, point, size.data(), 0.f);
        result = 1;
    }
    return result;
}
}
