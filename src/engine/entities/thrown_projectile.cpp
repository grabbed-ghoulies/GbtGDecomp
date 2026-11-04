#include "thrown_projectile.h"
#include "weapon_actor_detail.h"
#include "weapon_breakage.h"
#include "actor_attachment_events.h"
#include "actor_live_attachments.h"
#include "actor_render_registration.h"
#include "actor_sound_pose.h"
#include "actor_spawn_eligibility.h"
#include "actor_template_creation.h"
#include "actor_transform.h"
#include "actor_trail_requests.h"
#include "actor_world_lists.h"
#include "body_animation_commands.h"
#include "body_contacts.h"
#include "entity_collision.h"
#include "entity_heap.h"
#include "fx_anim_emitter.h"
#include "projectile_weapon.h"
#include "segment_records.h"
#include "../camera/entity_camera.h"
#include "../navigation/cell_registration.h"
#include "gbtg/actor_runtime.h"
#include "body_fields.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
ThrownProjectileState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<ThrownProjectileState>(bytes);
}

const ThrownProjectileState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<ThrownProjectileState>(bytes);
}

namespace parameter = thrown_projectile_parameter;

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
        entity_detail::release_navigation_cell_registration(weapon.cells,
                                                            weapon_base(weapon.bytes).floor); // 0x001108B0
    }
    weapon.actor.model_mode &= ~4;
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770
}

// The frame's time step: DAT_005096C0 x DAT_004312C4 unless DAT_004312DC is above zero, times the actor's rate.
float step_of(RuntimeWeaponActor& weapon)
{
    return frame_rate(weapon.actor) * frame_step(weapon.frame);
}

// States 9 and 0xB once the pieces settle: a half-length burst (0x00069590) where the projectile was, then it goes.
void finish_runtime_thrown_projectile(RuntimeWeaponActor& weapon)
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
}

// Xbox 0x000CB550
void thrown_projectile_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xF70;
    sizes.parameters = 0x6A0;
}

// Xbox 0x000CB5E0
bool initialize_runtime_thrown_projectile(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    weapon_base(bytes).contact_test = 0x000CBF80; // The class's contact test, called through +0x99C.
    // 0x0003AAA0 with EAX 0 (no variant), ECX the name and the actor's +0x48 table.
    const u32 name = get(parameters, parameter::flight_animation);
    const char* text = nullptr;
    if (name != 0)
    {
        const auto view = weapon.executable(name, 0);
        if (std::memchr(view.data(), 0, view.size()) == nullptr)
        {
            throw Error("projectile parameter +0x5F8 name has no terminator");
        }
        text = reinterpret_cast<const char*>(view.data());
    }
    weapon.flight_animation = weapon.view.animation.table == nullptr || text == nullptr ? nullptr
        : find_actor_animation(weapon.view.animation.table, text, nullptr, weapon.random); // +0xE64
    fields_of(bytes).trail_scale = get(parameters, parameter::trail_scale);
    fields_of(bytes).launch_mode = get(parameters, parameter::launch_mode);
    fields_of(bytes).flight_slide = get(parameters, parameter::flight_slide);
    fields_of(bytes).flight_quiet = get(parameters, parameter::flight_quiet);
    fields_of(bytes).flight_mask = get(parameters, parameter::flight_mask);
    fields_of(bytes).spin_rate = get(parameters, parameter::spin_rate);
    const auto source = record(parameters, parameter::spawn_name);
    auto target = retail_bytes(fields_of(bytes).spawn_name);
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
    fields_of(bytes).shatters = get(parameters, parameter::shatters);
    return true;
}

// Xbox 0x000CB6A0
void destroy_runtime_thrown_projectile(RuntimeWeaponActor& weapon)
{
    if (weapon.collision_overrides != nullptr)
    {
        restore_runtime_body_collision_overrides(*weapon.collision_overrides); // 0x0013EE80
        engine_heap_free(weapon.collision_overrides); // 0x000663E0
        weapon.collision_overrides = nullptr;
    }
}

// Xbox 0x000CB6D0
void update_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state)
{
    switch (state)
    {
    case 2:
        trail_runtime_thrown_projectile(weapon); // 0x000CC910, ECX actor
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        spin_runtime_thrown_projectile(weapon); // 0x000CC210, EBX actor
        return;
    case 3:
        return;
    case 4:
        if (update_runtime_thrown_projectile_shatter(weapon)) // 0x000CC530
        {
            weapon_base(weapon.bytes).next_state = 0xB;
        }
        return;
    case 9:
    case 0xB:
        finish_runtime_thrown_projectile(weapon);
        return;
    case 0x10:
        notify_runtime_weapon_grab_points(weapon); // 0x00147430
        return;
    default:
        update_runtime_weapon_state(weapon, state); // 0x0014A220
        return;
    }
}

// Xbox 0x000CB860
void dispatch_runtime_thrown_projectile_message(RuntimeWeaponActor& weapon, std::span<u32> message)
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
        weapon_base(bytes).next_state = 9;
        break;
    case 0x49:
        if (message[0xB] != 0)
        {
            end_runtime_thrown_projectile_flight(weapon); // 0x000CC430, ESI actor
        }
        break;
    case 0x7C:
        if (weapon.collision_overrides != nullptr)
        {
            remove_runtime_body_contact_override(weapon.collision_overrides,
                *weapon.actor_at(message[1])); // 0x0013EDD0
        }
        break;
    case 0x7D:
        if (weapon_base(bytes).state == 0)
        {
            change_runtime_weapon_state(weapon, 0x10); // 0x0014A290
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290
        break;
    case 0x93:
        if (land_runtime_thrown_projectile(weapon, message)) // 0x000CC340
        {
            activate_actor_bind_tree(weapon.actor); // 0x00112CC0, ESI this
            return;
        }
        if (fields_of(bytes).shatters != 0)
        {
            change_runtime_weapon_state(weapon, 4); // 0x0014A290
            return;
        }
        break;
    case 0x97:
        change_runtime_weapon_state(weapon, 9); // 0x0014A290
        return;
    case 0x9C:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290
        break;
    case 0x9D:
        record_runtime_thrown_projectile_root(weapon); // 0x000CC010
        break;
    case 0xA5:
        fields_of(bytes).throw_target = Vec3{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
            std::bit_cast<float>(message[3])};
        change_runtime_weapon_state(weapon, 2); // 0x0014A290
        send_callout(weapon, 0x4F); // 0x0003C040
        break;
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000CBAF0
void enter_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 previous)
{
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    switch (state)
    {
    case 2:
    {
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        launch_runtime_thrown_projectile(weapon, holder); // 0x000CBD90, EBX actor
        // 0x0013B7C0: 0x0003BE20 over actor +0x8C, id 0x4A, with 0x0013B8B0 recording the attachments at +0xF4C.
        ActorAttachmentEventContext events = weapon.base().callouts;
        events.command_attachments = retail_bytes(fields_of(weapon.bytes).command_attachments);
        dispatch_actor_attachment_records(weapon.view.startup.resource.resource.data(), 0x4A,
            record_runtime_body_attachment_event, nullptr, events,
            weapon.self_address() + offsetof(ThrownProjectileState, command_attachments));
        return;
    }
    case 3:
        if (holder == nullptr)
        {
            throw Error("0x000CBAF0 state 3 reads the body of a projectile with no holder (+0xD8C)");
        }
        // Holder +0x95C -> +0x4C and +0x14: this weapon.
        weapon.services.hold_secondary(*holder, weapon.self_address());
        return;
    case 4:
        if (actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            mark_runtime_weapon_player_category(weapon); // 0x001454B0
            break_runtime_weapon(weapon); // 0x001454F0
        }
        start_runtime_thrown_projectile_shatter(weapon); // 0x000CC4D0
        return;
    case 7:
        restore_runtime_thrown_projectile_root(weapon); // 0x000CC1E0
        return;
    case 9:
        send_callout(weapon, 0x35); // 0x0003BE20
        weapon.actor.model_flags &= ~0x800u;
        if (!actor_has_spawn_model(weapon.actor)) // 0x00145470
        {
            activate_actor_bind_tree(weapon.actor); // 0x00112CC0
            return;
        }
        mark_runtime_weapon_player_category(weapon); // 0x001454B0
        break_runtime_weapon(weapon); // 0x001454F0, the tail jump
        return;
    case 0x10:
        register_runtime_weapon_cells(weapon); // 0x00145BC0
        raise_model_mode(weapon, 4); // 0x0003C750 inlined
        weapon.actor.model_flags |= 0x100u;
        start_runtime_weapon_idle(weapon); // 0x00144D00
        if (weapon_base(weapon.bytes).unk_DA8 == 0)
        {
            attach_runtime_weapon_volume(weapon); // 0x00146310
        }
        return;
    default:
        enter_runtime_weapon_state(weapon, state, previous); // 0x0014A120
        return;
    }
}

// Xbox 0x000CBC60
void leave_runtime_thrown_projectile_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    switch (state)
    {
    case 0:
        lower_model_mode(weapon);
        return;
    case 2:
        if (weapon.attachment_lists == nullptr)
        {
            throw Error("projectile leaving its flight has no attachment lists for 0x0013B810");
        }
        clear_runtime_player_attachment_events(retail_bytes(fields_of(weapon.bytes).command_attachments),
            *weapon.attachment_lists); // 0x0013B810
        leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
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
            weapon.services.empty_hand(*holder); // 0x0001B680, ECX the holder's body
        }
        return;
    }
    case 0x10:
        lower_model_mode(weapon);
        weapon.actor.model_flags &= ~0x100u;
        clear_actor_live_attachments(weapon.view.live_attachments); // 0x00042040
        return;
    default:
        leave_runtime_weapon_state(weapon, state, next); // 0x0014A180
        return;
    }
}

// Xbox 0x000CBD90, EBX actor
void launch_runtime_thrown_projectile(RuntimeWeaponActor& weapon, Actor* holder)
{
    auto bytes = weapon.bytes;
    const Vec3 position = actor_world_position(weapon.actor); // 0x000412A0
    const Vec3 target = fields_of(bytes).throw_target;
    const u32 mode = fields_of(bytes).launch_mode;
    float gravity = 0.f;
    Vec3 velocity{};
    if (mode > 1)
    {
        throw Error("0x000CBD90 launch mode " + std::to_string(mode) + " leaves its velocity unset");
    }
    if (holder == nullptr)
    {
        throw Error("0x000CBD90 reads the body of a projectile with no holder (+0xD8C)");
    }
    const auto body = weapon.services.body_of(weapon.services.actor_address(*holder)); // Holder +0x95C.
    if (mode == 0)
    {
        // A timed arc: holder body +0x7C seconds, shortened inside DAT_004288D8, under DAT_0032B638.
        gravity = -250.f;
        float time = read_f32(body, 0x7C);
        const long double dx = static_cast<long double>(position.x) - target.x;
        const long double dy = static_cast<long double>(position.y) - target.y;
        const long double dz = static_cast<long double>(position.z) - target.z;
        const float distance = float(std::sqrt(dz * dz + dy * dy + dx * dx));
        if (static_cast<long double>(50.f) > distance)
        {
            time = distance / 50.f * time;
        }
        const float inverse = 1.f / time;
        velocity.x = (target.x - position.x) * inverse;
        velocity.z = (target.z - position.z) * inverse;
        velocity.y = ((target.y - position.y) - time * time * gravity * 0.5f) * inverse; // DAT_0032B0F4.
    }
    else
    {
        // The holder's throw speed (body +0x80) and no gravity.
        velocity = runtime_ballistic_velocity(target, position, read_f32(body, 0x80), 0.f); // 0x00140D80
    }
    velocity.y = step_of(weapon) * gravity + velocity.y;
    start_runtime_weapon_flight(weapon, &velocity, &weapon.default_euler, gravity, fields_of(bytes).flight_quiet,
        fields_of(bytes).flight_slide, fields_of(bytes).flight_mask); // 0x00148E60
    if (weapon.flight_animation != nullptr) // +0xE64.
    {
        start_actor_animation(weapon.view.animation, *weapon.flight_animation, weapon.registry, weapon.error,
            weapon.random); // 0x0003A9E0, EAX actor
        return;
    }
    clear_actor_animation(weapon.view.animation, weapon.actor.model != nullptr ? &weapon.view.pose : nullptr,
        weapon.registry); // 0x0003AC20, EAX actor
}

// Xbox 0x000CBF80
u32 test_runtime_thrown_projectile_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object,
    const Vec3& point, GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    if (report_runtime_projectile_contact(weapon, weapon.actor.spawn_owner, object, point, contacts, velocity, word,
                                          weapon_base(bytes).hit_kind, weapon_base(bytes).hit_strength,
                                          weapon_base(bytes).hit_style, fields_of(bytes).hits == 0,
                                          0) != 0) // 0x00141A30
    {
        fields_of(bytes).hits = fields_of(bytes).hits + 1;
    }
    if (fields_of(bytes).shatters != 0)
    {
        insert_runtime_body_contact_override(weapon.collision_overrides, object, 0, weapon.players); // 0x0013EBE0
    }
    return weapon_base(bytes).mask == 0 ? 1u : 0u;
}

// Xbox 0x000CC010, ECX actor, EAX the euler out, the stack the position out
void record_runtime_weapon_root(const Actor& actor, Vec3& euler, Vec3& rest)
{
    // Model +0x1BC with a joint count (0x00156A20 reads its +0x20).
    const RuntimeModelInstance* model = actor.model != nullptr ? actor.model->runtime : nullptr;
    if (model == nullptr || model->skeleton_node == nullptr || model->skeleton_node->joints.empty())
    {
        rest = actor_world_position(actor); // 0x000412A0, EDI the position out
        euler = actor_matrix_euler(actor.xform); // 0x00104040, ESI actor +0x2AC
        return;
    }
    Vec3 root{};
    actor_joint_world_euler(actor, 0, root); // 0x0003B050, EDX actor
    const Vec3 position = actor_world_position(actor); // 0x000412A0, ESI actor
    euler = root;
    const ModelPoseBuffer* pose = model->alternate; // Model +0x168, else +0x164.
    if (pose == nullptr)
    {
        pose = model->primary;
    }
    if (pose == nullptr)
    {
        throw Error("0x000CC010 reads joint 0 of a projectile without a model pose");
    }
    const AnimationMatrix& m = model_pose_matrix(*pose, 0); // 0x00031530, EAX 0
    const Vec3& o = actor_transform_origin; // DAT_0050DD80.
    const Vec3 joint{((m[1] * o.y + m[2] * o.z) + m[0] * o.x) + m[3],
        ((m[5] * o.y + m[6] * o.z) + m[4] * o.x) + m[7],
        ((m[9] * o.y + m[10] * o.z) + m[8] * o.x) + m[11]};
    Vec3 turned = joint;
    rotate_euler_vector(root, joint, turned); // 0x00106770, EAX euler, ESI output
    rest = Vec3{position.x - turned.x, position.y - turned.y, position.z - turned.z};
}

// 0x000CC010 with EAX +0xF24 and the stack +0xF30.
void record_runtime_thrown_projectile_root(RuntimeWeaponActor& weapon)
{
    Vec3 euler{};
    Vec3 rest{};
    record_runtime_weapon_root(weapon.actor, euler, rest);
    fields_of(weapon.bytes).rest_euler = euler;
    fields_of(weapon.bytes).rest_position = rest;
}

// Xbox 0x000CC1E0, EBX actor
void restore_runtime_thrown_projectile_root(RuntimeWeaponActor& weapon)
{
    Vec3 position = fields_of(weapon.bytes).rest_position;
    actor_set_world_position(weapon.actor, position); // 0x00041730, EDI actor, ESI +0xF30
    const Vec3 euler = fields_of(weapon.bytes).rest_euler;
    normalize_euler_angles(weapon.actor.euler, euler); // 0x00105AA0, EDI actor +0x14, ESI +0xF24
    weapon.actor.xform_valid = 0; // +0x2A8.
}

// Xbox 0x000CC210, EBX actor
void spin_runtime_thrown_projectile(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    const float step = step_of(weapon);
    actor_world_euler(weapon.actor); // 0x00041820 into the euler that the spin then overwrites.
    const Vec3 velocity = weapon.actor.anchor; // +0x3C.
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(velocity.x, velocity.y, velocity.z, yaw, pitch); // 0x00105E20, ECX yaw, stack pitch
    const Vec3 rate = fields_of(bytes).spin_rate;
    Vec3 spin = fields_of(bytes).spin;
    spin.y = rate.y * step + spin.y;
    spin.z = rate.z * step + spin.z;
    spin.x = spin.x + rate.x * step;
    fields_of(bytes).spin = spin;
    const Vec3 euler{spin.x + pitch, spin.y + yaw, spin.z};
    normalize_euler_angles(weapon.actor.euler, euler); // 0x00105AA0, EDI actor +0x14
    weapon.actor.xform_valid = 0; // +0x2A8.
}

// Xbox 0x000CC340, EAX actor
bool land_runtime_thrown_projectile(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    Vec3 position{std::bit_cast<float>(message[1]), std::bit_cast<float>(message[2]),
        std::bit_cast<float>(message[3])};
    actor_set_world_position(weapon.actor, position); // 0x00041730, EDI actor
    Actor* holder = weapon.actor.spawn_owner; // +0xD8C.
    if (read_u32(weapon.bytes, offsetof(ThrownProjectileState, spawn_name)) != kAidPrefix)
    {
        return false;
    }
    auto& objects = weapon.services.objects;
    const Vec3 euler{0.f, 0.f, 0.f};
    auto created = create_runtime_actor_at(fields_of(weapon.bytes).spawn_name.data(),
        &position, &euler, 1.f, objects.resources, objects.factory, actor_transform_origin,
        weapon.default_euler); // 0x00112C50, EAX name, ECX position, EBX euler
    if (created.bytes == nullptr || holder == nullptr)
    {
        if (created.bytes != nullptr)
        {
            weapon.services.keep(std::move(created));
        }
        return true;
    }
    FactoryObject* kept = weapon.services.keep(std::move(created));
    const u32 spawned = weapon.object_address(*kept);
    const u32 holder_address = weapon.services.actor_address(*holder);
    // Word 1 is the address of message 0x93's position words.
    const u32 where = weapon.services.bind_address(message.data() + 1, 12);
    std::array<u32, 0x42> notice{};
    notice[0] = 0x7A;
    notice[1] = where;
    notice[2] = message[0x16];
    notice[3] = spawned;
    notice[4] = weapon.self_address();
    notice[5] = holder_address;
    weapon.send(holder_address, notice); // Holder vtable +0xC.
    weapon.send(spawned, notice); // Spawned actor vtable +0xC.
    weapon.services.unbind_address(message.data() + 1);
    return true;
}

// Xbox 0x000CC430, ESI actor
void end_runtime_thrown_projectile_flight(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    weapon_base(bytes).mask = 0;
    weapon.actor.mask_flag = i32(u32(weapon.actor.mask_flag) & ~0xFFu); // Native alias of the +0x93C byte.
    if (weapon_base(bytes).voice_name.data()[0] != 0)
    {
        weapon_base(bytes).voice_name.data()[0] = 0;
        if (weapon_base(bytes).voice != 0xFFFF)
        {
            stop_actor_sound_voice(i16(weapon_base(bytes).voice), weapon.services.sounds); // 0x00039690
            weapon_base(bytes).voice = 0xFFFF;
        }
    }
    change_runtime_weapon_state(weapon, fields_of(bytes).shatters != 0 ? 4 : 9); // Inlined as 0x0014A290.
}

// Xbox 0x000CC4D0, ESI actor
void start_runtime_thrown_projectile_shatter(RuntimeWeaponActor& weapon)
{
    weapon.actor.anchor = actor_transform_origin; // DAT_0050DD80.
    if (Actor* holder = weapon.actor.spawn_owner; holder != nullptr) // +0xD8C.
    {
        insert_runtime_body_contact_override(weapon.collision_overrides,
            weapon.contact_actor_at(weapon.services.actor_address(*holder)), 0, weapon.players); // 0x0013EBE0
    }
    fields_of(weapon.bytes).shatter_timer = 0.5f; // DAT_004288DC.
}

// Xbox 0x000CC530, EAX actor
bool update_runtime_thrown_projectile_shatter(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    // DAT_004288DC 0.5 s, the reach from DAT_004288E4 (20) at the start to DAT_004288E0 (8) at the end.
    constexpr float duration = 0.5f;
    constexpr float last = 8.f;
    constexpr float first = 20.f;
    float reach = last;
    if (duration != 0.f)
    {
        reach = fields_of(bytes).shatter_timer / duration * (last - first) + first;
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
    if (fields_of(bytes).shatter_shown == 0 && duration - fields_of(bytes).shatter_timer > 0.f) // DAT_004288E8.
    {
        send_callout(weapon, 10); // 0x0003BE20
        fields_of(bytes).shatter_shown = 1u;
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
        test_runtime_thrown_projectile_contact(weapon, weapon.contact_actor(*probe), position, records, still,
            0); // 0x000CBF80
        engine_heap_free(records); // 0x0014D360 on the block's heap.
    }
    const float timer = fields_of(bytes).shatter_timer;
    if (timer > 0.f)
    {
        const float left = timer - step_of(weapon);
        fields_of(bytes).shatter_timer = left;
        if (left <= 0.f) // COMISS 0, left; JC keeps it.
        {
            fields_of(bytes).shatter_timer = 0.f;
            return true;
        }
    }
    return false;
}

// Xbox 0x000CC910, ECX actor
void trail_runtime_thrown_projectile(RuntimeWeaponActor& weapon)
{
    const float scale = fields_of(weapon.bytes).trail_scale;
    if (scale == 0.f) // DAT_0032B100.
    {
        return;
    }
    if (weapon.services.pal == nullptr || weapon.services.pal_override == nullptr || !weapon.services.trail)
    {
        throw Error("projectile flight has no trail or PAL views");
    }
    const i32 rate = *weapon.services.pal == 0 || *weapon.services.pal_override != 0 ? 0x3C : 0x32;
    auto trails = weapon.services.trail(weapon.self_address());
    float timer = fields_of(weapon.bytes).trail_timer;
    request_actor_trail(weapon.actor, scale, timer, rate / 2, trails); // 0x0013B030
    fields_of(weapon.bytes).trail_timer = timer;
}

// Xbox 0x00148E60, EAX velocity, EDX euler, ECX quiet, ESI actor
void start_runtime_weapon_flight(RuntimeWeaponActor& weapon, const Vec3* velocity, const Vec3* euler, float gravity,
    u32 quiet, u32 slide, u32 mask)
{
    auto bytes = weapon.bytes;
    if (velocity != nullptr)
    {
        weapon.actor.anchor = *velocity; // +0x3C.
    }
    if (euler != nullptr)
    {
        std::memcpy(&weapon_base(bytes).motion_euler, euler, 12);
    }
    weapon_base(bytes).gravity = gravity;
    weapon_base(bytes).probes_contacts = 1;
    weapon_base(bytes).slides = slide;
    weapon_base(bytes).quiet = quiet;
    weapon_base(bytes).contact_mask = mask;
    weapon_base(bytes).hit = 0;
    if (quiet == 0 && weapon.markers != nullptr)
    {
        activate_all_runtime_body_contact_shapes(weapon.actor, *weapon.markers, 0xFF); // 0x0013F5B0
        std::array<u32, 0x42> shapes{};
        shapes[0] = 0x92;
        shapes[1] = 1;
        weapon.message(shapes); // Vtable +0xC.
    }
}

// Xbox 0x00140D80, ECX target, EDX origin, ESI the output
Vec3 runtime_ballistic_velocity(const Vec3& target, const Vec3& origin, float speed, float gravity)
{
    const float dx = target.x - origin.x;
    const long double dy = static_cast<long double>(target.y) - origin.y;
    const long double dz = static_cast<long double>(target.z) - origin.z;
    const long double x = dx;
    const float time = float(std::sqrt(x * x + dy * dy + dz * dz) / speed);
    float divisor = 1.f; // DAT_0032B0F8.
    if (time != 0.f) // DAT_0032B100.
    {
        divisor = time;
    }
    const float inverse = 1.f / divisor;
    Vec3 output;
    output.x = dx * inverse;
    output.z = (target.z - origin.z) * inverse;
    output.y = ((target.y - origin.y) - divisor * divisor * gravity * 0.5f) * inverse; // DAT_0032B0F4.
    return output;
}
}
