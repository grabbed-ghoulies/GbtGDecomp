#include "homing_projectile.h"
#include "weapon_actor_detail.h"
#include "weapon_breakage.h"
#include "weapon_motion.h"
#include "actor_attachment_events.h"
#include "actor_live_attachments.h"
#include "actor_render_registration.h"
#include "actor_spawn_eligibility.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "body_animation_commands.h"
#include "body_contacts.h"
#include "body_timers.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "fx_anim_emitter.h"
#include "player_mind_targets.h"
#include "segment_records.h"
#include "strategy_target_selection.h"
#include "thrown_projectile.h"
#include "../audio/bank_lookup.h"
#include "../navigation/cell_registration.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/sound_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = homing_projectile_field;
namespace parameter = homing_projectile_parameter;

// 0x0003C040 / 0x0003BE20 with the actor's +0x8C resource: its callout records for an id.
void send_callout(RuntimeWeaponActor& weapon, i32 id)
{
    dispatch_actor_resource_callout(weapon.view.startup.resource, id, nullptr, weapon.base().callouts);
}

// 0x0003C750: actor +0x118 gains the bits and the shape probe is registered again.
void raise_model_mode(RuntimeWeaponActor& weapon, i32 bits)
{
    weapon.actor.model_mode |= bits;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}

// Actor +0x118 loses bit 4, after the navigation cells (+0xCFC) are released.
void lower_model_mode(RuntimeWeaponActor& weapon)
{
    if (weapon.cells != nullptr)
    {
        entity_detail::release_navigation_cell_registration(weapon.cells, read_u32(weapon.bytes, 0xD00)); // 0x001108B0
    }
    weapon.actor.model_mode &= ~4;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}

// The flight animation (+0xE64) when there is one (0x0003A9E0), else the animation cleared (0x0003AC20).
void start_flight_animation(RuntimeWeaponActor& weapon)
{
    if (weapon.flight_animation != nullptr)
    {
        start_actor_animation(weapon.view.animation, *weapon.flight_animation, weapon.registry, weapon.error,
            weapon.random); // 0x0003A9E0, EAX actor
        return;
    }
    clear_actor_animation(weapon.view.animation, weapon.actor.model != nullptr ? &weapon.view.pose : nullptr,
        weapon.registry); // 0x0003AC20, EAX actor
}

// The actor of the weapon's follow record (+0xE50 -> +0x10), by its retail address; zero without a record.
u32 follow_target_address(RuntimeWeaponActor& weapon)
{
    const u32 record = read_u32(weapon.bytes, 0xE50);
    if (record == 0)
    {
        return 0;
    }
    RuntimeMindTarget* target = weapon.services.target_at(record);
    if (target == nullptr)
    {
        throw Error("homing projectile reads a follow record (+0xE50) with no actor");
    }
    return weapon.services.actor_address(target->actor);
}

// Xbox 0x000CD2A0, ESI actor: the flight ends at once, shattering (state 4) with +0xE94, else breaking (state 9).
void end_runtime_homing_projectile_flight(RuntimeWeaponActor& weapon)
{
    stop_runtime_weapon_hold_sound(weapon);
    change_runtime_weapon_state(weapon, get(weapon.bytes, field::shatters) != 0 ? 4 : 9); // The inlined 0x0014A290.
}

// Xbox 0x000CD120, EAX actor: state 10's flight takes the projectile's follow words (+0xCDC..+0xCF8), stops, and
// runs its flight animation.
void start_runtime_homing_projectile_flight(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0xCDC, 1);
    weapon.actor.mask_mode = 1; // Native alias of the +0xCDC word.
    const float slack = get(bytes, field::follow_slack);
    const float speed = get(bytes, field::follow_speed);
    const float rise = get(bytes, field::follow_rise);
    std::memcpy(bytes.data() + 0xCEC, &slack, 4);
    std::memcpy(bytes.data() + 0xCF0, &speed, 4);
    std::memcpy(bytes.data() + 0xCF4, &rise, 4);
    store32(bytes, 0xCF8, get(bytes, field::ghost));
    weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80.
    start_flight_animation(weapon); // The tail jumps to 0x0003A9E0 / 0x0003AC20.
}

// Xbox 0x000CD230, EBX actor: back to the pose message 0x9D recorded.
void restore_runtime_homing_projectile_root(RuntimeWeaponActor& weapon)
{
    const Vec3 position = get(weapon.bytes, field::rest_position);
    actor_set_world_position(weapon.actor, position); // 0x00041730, EDI actor, ESI +0xE7C
    const Vec3 euler = get(weapon.bytes, field::rest_euler);
    normalize_euler_angles(weapon.actor.euler, euler); // 0x00105AA0, EDI actor +0x14, ESI +0xE70
    weapon.actor.xform_valid = 0; // +0x2A8.
}

// Xbox 0x000CD340, ESI actor
void start_runtime_homing_projectile_shatter(RuntimeWeaponActor& weapon)
{
    weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80.
    if (Actor* holder = weapon.actor.spawn_owner; holder != nullptr) // +0xD8C.
    {
        insert_runtime_body_contact_override(weapon.collision_overrides,
            weapon.contact_actor_at(weapon.services.actor_address(*holder)), 0, weapon.players); // 0x0013EBE0
    }
    set(weapon.bytes, field::shatter_timer, 0.5f); // DAT_004288F4.
}

// Xbox 0x000CD3A0, EAX actor: the shatter's reach (DAT_004288FC to DAT_004288F8 as the timer runs) reports every
// actor in it; true when the timer runs out.
bool update_runtime_homing_projectile_shatter(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    constexpr float duration = 0.5f; // DAT_004288F4.
    constexpr float last = 8.f; // DAT_004288F8.
    constexpr float first = 20.f; // DAT_004288FC.
    float reach = last;
    if (duration != 0.f)
    {
        reach = get(bytes, field::shatter_timer) / duration * (last - first) + first;
        if (last <= first)
        {
            if (reach < last)
            {
                reach = last;
            }
            else if (first < reach)
            {
                reach = first;
            }
        }
        else if (last < reach)
        {
            reach = last;
        }
        else if (reach < first)
        {
            reach = first;
        }
    }
    if (get(bytes, field::shatter_shown) == 0 && duration - get(bytes, field::shatter_timer) > 0.f) // DAT_00428900.
    {
        send_callout(weapon, 10); // 0x0003BE20
        set(bytes, field::shatter_shown, 1u);
    }
    Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    if (weapon.collision_overrides != nullptr)
    {
        suspend_runtime_body_contact_overrides(*weapon.collision_overrides); // 0x0013EE10
    }
    const i32 count = entity_collect_contact_probes(position, reach, 0x00220142u); // 0x00122B00
    if (weapon.collision_overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
    }
    std::vector<std::pair<entity_detail::SegmentProbe*, GhoulySpawnPool*>> found;
    for (i32 index = 0; index < count; ++index)
    {
        auto& slot = entity_detail::g_segment_slots[index];
        found.emplace_back(slot.object, copy_segment_records(slot.pool)); // 0x000FCC90
    }
    const Vec3 still{0.f, 0.f, 0.f}; // DAT_0050DD80.
    for (auto& [probe, records] : found)
    {
        if (report_runtime_projectile_contact(weapon, weapon.actor.spawn_owner, weapon.contact_actor(*probe), position,
                records, still, 0, bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], get(bytes, field::hits) == 0,
                get(bytes, field::contact_tail)) != 0) // 0x00141A30
        {
            set(bytes, field::hits, get(bytes, field::hits) + 1);
        }
        engine_heap_free(records); // 0x0014D360 on the block's heap.
    }
    const float timer = get(bytes, field::shatter_timer);
    if (timer > 0.f)
    {
        const float left = timer - frame_rate(weapon.actor) * frame_step(weapon.frame);
        set(bytes, field::shatter_timer, left);
        if (left <= 0.f) // COMISS 0, left; JC keeps it.
        {
            set(bytes, field::shatter_timer, 0.f);
            return true;
        }
    }
    return false;
}

// States 9 and 0xB once the pieces settle: a half-length burst (0x00069590) where the projectile was, then it goes.
void finish_runtime_homing_projectile(RuntimeWeaponActor& weapon)
{
    if (!actor_model_pieces_settled(weapon.actor, weapon.pieces)) // 0x00040040
    {
        return;
    }
    std::array<u8, 0x354> parameters{};
    build_runtime_emitter_burst_parameters(parameters, weapon.actor, weapon.self_address(), 0.5f,
        weapon.services.objects.default_name); // 0x00069590, ESI actor
    parameters[0x33C] = weapon.bytes[0x19B];
    store32(parameters, 0x350, 0);
    store32(parameters, 0x34C, 0);
    weapon.services.keep(create_factory_object(weapon.services.objects.factory, parameters)); // 0x00107520
    activate_actor_bind_tree(weapon.actor); // 0x00112CC0, ESI this
}

// State 10's beep: with +0xE98, each +0xEA0 seconds the cue at +0xEA8 plays while the holder's target is nearer than
// +0xEA4.
void beep_runtime_homing_projectile(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (get(bytes, field::beeps) == 0)
    {
        return;
    }
    float timer = get(bytes, field::beep_timer);
    tick_runtime_body_frame_timer(timer, weapon.clock); // 0x0012B2A0, EAX +0xE9C
    set(bytes, field::beep_timer, timer);
    const RuntimeMindTarget* target = runtime_weapon_holder_target(weapon); // 0x00149C30
    if (target == nullptr)
    {
        return;
    }
    const long double distance = runtime_target_distance(weapon.actor, target->bytes); // 0x00041560, EBX target
    if (!(static_cast<long double>(get(bytes, field::beep_range)) > distance)) // FCOMIP; JBE.
    {
        return;
    }
    if (!(0.001f > get(bytes, field::beep_timer))) // DAT_0032B1A4; JBE.
    {
        return;
    }
    const char* cue = reinterpret_cast<const char*>(record(bytes, field::beep_sound).data());
    if (cue[0] == 0 || !resolve_audio_bank_cue(cue, nullptr, nullptr)) // 0x000380B0, EAX +0xEA8
    {
        return;
    }
    request_actor_sound_at(weapon.actor, 1, cue, nullptr, nullptr, 1.f, weapon.services.sounds); // 0x00041D40, ECX 1
    set(bytes, field::beep_timer, get(bytes, field::beep_interval));
}
}

// Xbox 0x000CC770
void homing_projectile_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = 0x6A4;
}

// Xbox 0x000CC800
bool initialize_runtime_homing_projectile(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0x99C, 0x000CD1B0); // The class's contact test, called through +0x99C.
    // 0x0003AAA0 with EAX 0 (no variant), ECX the name and the actor's +0x48 table.
    const u32 name = get(parameters, parameter::flight_animation);
    const char* text = nullptr;
    if (name != 0)
    {
        const auto view = weapon.executable(name, 0);
        if (std::memchr(view.data(), 0, view.size()) == nullptr)
        {
            throw Error("homing projectile parameter +0x5F8 name has no terminator");
        }
        text = reinterpret_cast<const char*>(view.data());
    }
    weapon.flight_animation = weapon.view.animation.table == nullptr || text == nullptr ? nullptr
        : find_actor_animation(weapon.view.animation.table, text, nullptr, weapon.random); // +0xE64
    set(bytes, field::trail_scale, get(parameters, parameter::trail_scale));
    set(bytes, field::shatters, get(parameters, parameter::shatters));
    set(bytes, field::follow_slack, get(parameters, parameter::follow_slack));
    set(bytes, field::follow_speed, get(parameters, parameter::follow_speed));
    set(bytes, field::follow_rise, get(parameters, parameter::follow_rise));
    set(bytes, field::ghost, get(parameters, parameter::ghost));
    set(bytes, field::contact_tail, get(parameters, parameter::contact_tail));
    set(bytes, field::beeps, get(parameters, parameter::beeps));
    if (get(bytes, field::beeps) != 0)
    {
        const auto source = record(parameters, parameter::beep_sound);
        auto target = record(bytes, field::beep_sound);
        size_t index = 0;
        do
        {
            target[index] = source[index];
        }
        while (source[index++] != 0);
        set(bytes, field::beep_range, get(parameters, parameter::beep_range));
        set(bytes, field::beep_interval, get(parameters, parameter::beep_interval));
        set(bytes, field::beep_timer, 0.f);
    }
    return true;
}

// Xbox 0x000CC8E0
void destroy_runtime_homing_projectile(RuntimeWeaponActor& weapon)
{
    if (weapon.collision_overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
        engine_heap_free(weapon.collision_overrides); // 0x000663E0
        weapon.collision_overrides = nullptr;
    }
}

// Xbox 0x000CC970
void update_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state)
{
    switch (state)
    {
    case 3:
        return;
    case 4:
        if (update_runtime_homing_projectile_shatter(weapon)) // 0x000CD3A0
        {
            store16(weapon.bytes, 0xC0E, 0xB);
        }
        return;
    case 9:
    case 0xB:
        finish_runtime_homing_projectile(weapon);
        return;
    case 10:
        beep_runtime_homing_projectile(weapon);
        trail_runtime_thrown_projectile(weapon); // 0x000CC910, ECX actor: the same +0xE68 / +0xE6C words.
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}

// Xbox 0x000CCB70
void dispatch_runtime_homing_projectile_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x13:
        change_runtime_weapon_state(weapon, 9); // 0x0014A290
        break;
    case 0x18:
        message[4] = 1;
        send_callout(weapon, 0x19); // 0x0003C040
        store16(bytes, 0xC0E, 9);
        break;
    case 0x31:
        // The holder or the follow target noticed in flight ends it.
        if (read_u16(bytes, 0xC0C) == 10 && read_u32(bytes, 0xE50) != 0)
        {
            const Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
            const u32 holder_address = holder == nullptr ? 0 : weapon.services.actor_address(*holder);
            if (message[1] == holder_address || message[1] == follow_target_address(weapon))
            {
                end_runtime_homing_projectile_flight(weapon); // 0x000CD2A0
            }
        }
        break;
    case 0x49:
        if (message[0xB] != 0 && read_u32(bytes, 0xE50) != 0 && message[6] == follow_target_address(weapon))
        {
            end_runtime_homing_projectile_flight(weapon); // 0x000CD2A0
        }
        break;
    case 0x7D:
        if (read_u16(bytes, 0xC0C) == 0)
        {
            change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        break;
    case 0x93:
        // 0x000CD260: to the message's point (+4), and it always succeeds.
        actor_set_world_position(weapon.actor, Vec3{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
            std::bit_cast<float>(message[3])}); // 0x00041730, ESI the point, EDI actor
        activate_actor_bind_tree(weapon.actor); // 0x00112CC0
        return;
    case 0x97:
        change_runtime_weapon_state(weapon, 9); // 0x0014A290
        return;
    case 0x9C:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    case 0x9D:
    {
        Vec3 euler{};
        Vec3 rest{};
        record_runtime_weapon_root(weapon.actor, euler, rest); // 0x000CC010, EAX +0xE70, the stack +0xE7C
        set(bytes, field::rest_euler, euler);
        set(bytes, field::rest_position, rest);
        break;
    }
    case 0xA5:
        change_runtime_weapon_state(weapon, 10); // 0x0014A290
        send_callout(weapon, 0x4F); // 0x0003C040
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000CCE00
void enter_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    switch (state)
    {
    case 3:
        if (holder == nullptr)
        {
            throw Error("0x000CCE00 state 3 reads the body of a projectile with no holder (+0xD8C)");
        }
        // Holder +0x95C -> +0x4C and +0x14: this weapon.
        weapon.services.hold_secondary(*holder, weapon.self_address());
        return;
    case 4:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            break_runtime_weapon(weapon); // 0x001454F0
        }
        start_runtime_homing_projectile_shatter(weapon); // 0x000CD340
        return;
    case 7:
        restore_runtime_homing_projectile_root(weapon); // 0x000CD230, EBX actor
        start_flight_animation(weapon); // The tail jumps to 0x0003A9E0 / 0x0003AC20.
        return;
    case 9:
        send_callout(weapon, 0x35); // 0x0003BE20
        weapon.actor.model_flags &= ~0x800u;
        if (!actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            activate_actor_bind_tree(weapon.actor); // 0x00112CC0
            return;
        }
        break_runtime_weapon(weapon); // 0x001454F0, the tail jump
        return;
    case 10:
    {
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        start_runtime_homing_projectile_flight(weapon); // 0x000CD120
        // 0x0013B7C0: 0x0003BE20 over actor +0x8C, id 0x4A, with 0x0013B8B0 recording the attachments at +0xF34.
        ActorAttachmentEventContext events = weapon.base().callouts;
        events.command_attachments = record(weapon.bytes, field::command_attachments);
        dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), 0x4A,
            record_runtime_body_attachment_event, nullptr, events,
            weapon.self_address() + field::command_attachments.offset);
        return;
    }
    case 0x10:
        register_runtime_weapon_cells(weapon); // 0x00145BC0
        raise_model_mode(weapon, 4); // 0x0003C750 inlined
        weapon.actor.model_flags |= 0x100u;
        start_runtime_weapon_idle(weapon); // 0x00144D00
        if (read_u32(weapon.bytes, 0xDA8) == 0)
        {
            attach_runtime_weapon_volume(weapon); // 0x00146310
        }
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        return;
    }
}

// Xbox 0x000CCF60
void leave_runtime_homing_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    switch (state)
    {
    case 0:
        lower_model_mode(weapon);
        return;
    case 3:
    {
        if (holder == nullptr)
        {
            activate_actor_bind_tree(weapon.actor); // 0x00112CC0
            return;
        }
        // Holder +0x95C -> +0x4C and +0x14 cleared.
        weapon.services.hold_secondary(*holder, 0);
        const auto body = weapon.services.body_of(weapon.services.actor_address(*holder));
        if (get(body, body_field::attached) == weapon.self_address())
        {
            weapon.services.empty_hand(*holder); // The inlined 0x0001B680 on the holder's body.
        }
        return;
    }
    case 10:
        if (weapon.attachment_lists == nullptr)
        {
            throw Error("homing projectile leaving its flight has no attachment lists for 0x0013B810");
        }
        clear_runtime_player_attachment_events(record(weapon.bytes, field::command_attachments),
            *weapon.attachment_lists); // 0x0013B810, EBX +0xF34
        leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
        return;
    case 0x10:
        lower_model_mode(weapon);
        weapon.actor.model_flags &= ~0x100u;
        clear_actor_live_attachments(weapon.view.live_attachments); // The inlined 0x00042040.
        return;
    default:
        leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
        return;
    }
}

// Xbox 0x000CD1B0
u32 test_runtime_homing_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    if (report_runtime_projectile_contact(weapon, weapon.actor.spawn_owner, object, point, contacts, velocity, word,
            bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], get(bytes, field::hits) == 0,
            get(bytes, field::contact_tail)) != 0) // 0x00141A30
    {
        set(bytes, field::hits, get(bytes, field::hits) + 1);
    }
    return bytes[0x93C] == 0 ? 1u : 0u;
}

// Xbox 0x000CD090
u32 draw_runtime_homing_projectile(RuntimeWeaponActor& weapon, ActorDrawState& state, i32 kind,
    const ActorFrameDrawContext& attachments, SceneModelDrawContext& draw, RuntimeGlowPass& glow)
{
    if (get(weapon.bytes, field::ghost) == 0)
    {
        draw_runtime_actor(state, kind, attachments, draw); // 0x0003C9E0
        return 1;
    }
    draw_actor_frame_attachments(state.frame, 3, attachments); // 0x0003DE00, EBX 3, EDI actor
    if (kind == 2)
    {
        begin_runtime_glow_capture(nullptr, nullptr, glow); // 0x00062220, EAX and EDI 0
        draw_runtime_actor(state, 1, attachments, draw); // 0x0003C9E0
        stamp_runtime_glow(0x80FFFFFFu, 1, glow); // 0x00062490, EAX 1
    }
    return 1;
}
}
