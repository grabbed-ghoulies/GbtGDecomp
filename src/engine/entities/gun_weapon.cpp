#include "gun_weapon.h"
#include "weapon_actor_detail.h"
#include "actor_live_attachments.h"
#include "actor_model_anchors.h"
#include "actor_world_lists.h"
#include "body_damage_notices.h"
#include "candle_weapon.h"
#include "extinguisher_weapon.h"
#include "weapon_motion.h"
#include "../frontend/scoreboard_update.h"
#include "gbtg/entity.h"
#include "gbtg/sound_runtime.h"
#include <cstring>
#include <unordered_map>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
namespace field = gun_weapon_field;
namespace parameter = gun_weapon_parameter;
constexpr u32 kScriptHold = 0; // DAT_00512278, BSS that no retail code writes.

// The two locator records (+0xEEC muzzle, +0xEF0 aim) of each gun, by the weapon's bytes.
struct GunLocators
{
    const u8* muzzle = nullptr;
    const u8* aim = nullptr;
};
std::unordered_map<const u8*, GunLocators> gun_locators;

// 0x00145FD0, EAX actor: the holder whose link actor +0x918 names, while that link is set.
Actor* holder_of(const RuntimeWeaponActor& weapon)
{
    const ActorLink* link = weapon.actor.link;
    return link != nullptr && link->head != nullptr ? link->owner : nullptr;
}

// The inline frame step: zero while frozen, otherwise the frame's dt by the scene scale.
float frame_step(const RuntimeWeaponActor& weapon)
{
    return runtime_frame_step(weapon.clock);
}

// 0x00039690 with stack 0 on the voice at +0xD88 once the sound flag (+0xD08) drops.
void stop_weapon_sound(RuntimeWeaponActor& weapon)
{
    auto bytes = weapon.bytes;
    if (bytes[0xD08] == 0)
    {
        return;
    }
    bytes[0xD08] = 0;
    const u16 voice = read_u16(bytes, 0xD88);
    if (voice != 0xFFFF)
    {
        stop_actor_sound_voice(i16(voice), weapon.services.sounds); // 0x00039690
        store16(bytes, 0xD88, 0xFFFF);
    }
}

// Vtable +0x4C: firing starts (message 0xA2, with the message's +8 words).
void fire_start_hook(RuntimeWeaponActor& weapon, const Vec3&, const Vec3&, std::span<const u32>)
{
    if (weapon.type == 0xA3)
    {
        start_runtime_extinguisher_spray(weapon); // 0x000C7D10
        return;
    }
    throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x4C is not translated");
}

// Vtable +0x50: firing ends.
void fire_end_hook(RuntimeWeaponActor& weapon, const Vec3&, const Vec3&)
{
    if (weapon.type == 0xA3)
    {
        end_runtime_extinguisher_spray(weapon); // 0x000C7D50
        return;
    }
    throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x50 is not translated");
}

// Vtable +0x54 with the actor's frame step.
void update_hook(RuntimeWeaponActor& weapon, float step)
{
    if (weapon.type == 0xA3)
    {
        update_runtime_extinguisher_spray(weapon, step); // 0x000C7D60
        return;
    }
    throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x54 is not translated");
}

// Vtable +0x5C: the fire tick while held.
void fire_tick_hook(RuntimeWeaponActor& weapon, u32 firing, const Vec3& point, const Vec3& direction)
{
    if (weapon.type == 0xA3)
    {
        emit_runtime_extinguisher_spray(weapon, firing, point, direction); // 0x000C8030
        return;
    }
    throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x5C is not translated");
}

// Vtable +0x60: the hit query, a pool of RuntimeSprayHit records or null.
GhoulySpawnPool* hit_query_hook(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3& direction)
{
    if (weapon.type == 0xA3)
    {
        return query_runtime_extinguisher_spray(weapon, point, direction); // 0x000C8590
    }
    throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x60 is not translated");
}
}

// Xbox 0x000CA960
void gun_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = field::kSize;
    sizes.parameters = parameter::kSize;
}

// Xbox 0x000CA9F0
bool initialize_runtime_gun_weapon(RuntimeWeaponActor& weapon, std::span<const u8> parameters)
{
    auto bytes = weapon.bytes;
    store32(bytes, 0x99C, 0x000CB040);
    std::strcpy(field_data(bytes, field::name), field_data(parameters, parameter::name));
    set(bytes, field::charge, 1.f); // DAT_0032B0F8.
    GunLocators& locators = gun_locators[bytes.data()];
    locators.muzzle = find_actor_model_binding(weapon.actor, 3); // 0x000408D0, EDI 3
    locators.aim = find_actor_model_binding(weapon.actor, 4); // 0x000408D0, EDI 4
    if (locators.muzzle == nullptr || locators.aim == nullptr)
    {
        locators = GunLocators{};
    }
    set(bytes, field::muzzle, locators.muzzle != nullptr ? 1u : 0u);
    set(bytes, field::aim, locators.aim != nullptr ? 1u : 0u);
    return true;
}

// Xbox 0x000CAB40
void dispatch_runtime_gun_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x5A:
        if (message[0xB] != 0)
        {
            set(bytes, field::unk_EF4, 1u);
        }
        break;
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290, EDI 3
        break;
    case 0x92:
        if (message[1] != 0)
        {
            set(bytes, field::unk_EF4, 0u);
        }
        break;
    case 0x9D:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290, EDI 7
        break;
    case 0x9F:
    {
        // The muzzle into the message: +8 the point, +0x14 the direction, +4 whether the locators exist.
        Vec3 point{};
        Vec3 direction{};
        const bool found = runtime_gun_muzzle(weapon, point, direction); // 0x000CB330, EAX message +8
        if (found)
        {
            std::memcpy(message.data() + 2, &point, 12);
            std::memcpy(message.data() + 5, &direction, 12);
        }
        message[1] = found ? 1u : 0u;
        break;
    }
    case 0xA1:
        set(bytes, field::unk_EE8, message[1]);
        break;
    case 0xA2:
    {
        request_actor_sound(weapon.actor, 4, field_data(bytes, field::name), weapon.services.sounds); // 0x00041D00
        set(bytes, field::firing, 1u);
        Vec3 point{};
        Vec3 direction{};
        if (runtime_gun_muzzle(weapon, point, direction)) // 0x000CB330
        {
            fire_start_hook(weapon, point, direction, std::span<const u32>(message).subspan(2)); // Vtable +0x4C
            set(bytes, field::hits, 0u);
        }
        break;
    }
    case 0xA3:
    {
        release_actor_sound(weapon.actor.sound_slots[4], weapon.services.sounds); // 0x0012A3D0, ESI actor +0x614
        set_runtime_weapon_mask(weapon, get(bytes, field::charge) > 0.f ? 1 : 0); // 0x00146230
        set(bytes, field::firing, 0u);
        Vec3 point{};
        Vec3 direction{};
        if (runtime_gun_muzzle(weapon, point, direction)) // 0x000CB330
        {
            fire_end_hook(weapon, point, direction); // Vtable +0x50
            set(bytes, field::hits, 0u);
        }
        break;
    }
    default:
        break;
    }
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
}

// Xbox 0x000CAA80
void update_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    const Actor* holder = holder_of(weapon);
    if (holder != nullptr && read_u32(weapon.bytes_at(weapon.services.actor_address(*holder)), 0xCE0) != 0
        && (holder->mask_mode == 2 || holder->mask_flag != 0))
    {
        RuntimeCandleServices& candle = weapon.services.candle(); // DAT_0050E764 / DAT_0030F144 / DAT_0051248C.
        if (runtime_player_damage_enabled(*holder, candle.blocked, candle.modes, candle.mode) != 0) // 0x00028D50
        {
            show_runtime_meter_level(get(weapon.bytes, field::charge), weapon.services.scoreboard, weapon.registry,
                weapon.error); // 0x0012EC70
        }
    }
    update_hook(weapon, runtime_actor_rate(weapon.actor) * frame_step(weapon)); // Vtable +0x54
    update_runtime_weapon_state(weapon, state); // 0x0014A220
}

// Xbox 0x000CAD90
void draw_runtime_gun_weapon_effects(RuntimeWeaponActor& weapon, u32 pass)
{
    if ((pass & 2) == 0)
    {
        return;
    }
    if (weapon.type != 0xA3) // Vtable +0x58 is set for the extinguisher.
    {
        throw Error("gun factory type " + std::to_string(weapon.type) + " vtable +0x58 is not translated");
    }
    Vec3 point{};
    Vec3 direction{};
    if (!runtime_gun_muzzle(weapon, point, direction)) // 0x000CB330
    {
        return;
    }
    draw_runtime_extinguisher_spray(weapon); // Vtable +0x58, 0x000C8C70
}

// Xbox 0x000CAE90
void enter_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    switch (state)
    {
    case 2:
        enter_runtime_weapon_flight(weapon); // 0x001487C0
        return;
    case 5:
    case 8:
        weapon.actor.model_flags &= ~0x100u; // Actor +0x1F0.
        return;
    case 10:
        enter_runtime_weapon_follow(weapon); // 0x001495C0
        return;
    default:
        return;
    }
}

// Xbox 0x000CAEB0
void leave_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    auto bytes = weapon.bytes;
    if (state != 0)
    {
        switch (state)
        {
        case 0x10:
            if (read_u32(bytes, 0xDD0) == kAidPrefix)
            {
                auto* controller = weapon.view.animation.controller.get(); // Actor +0x50.
                if (controller != nullptr)
                {
                    if (!controller->slots.empty() && controller->slots[0].player != nullptr)
                    {
                        --controller->slots[0].player->paused; // Player +0x125.
                    }
                    controller->dirty = true;
                }
            }
            clear_actor_live_attachments(weapon.view.live_attachments); // 0x00042040
            break;
        case 2:
            leave_runtime_weapon_flight(weapon); // 0x00148850
            return;
        case 5:
            weapon.actor.model_flags |= 0x100u; // Actor +0x1F0.
            weapon.actor.anchor = actor_transform_origin; // Actor +0x3C, DAT_0050DD80.
            return;
        case 8:
            weapon.actor.model_flags |= 0x100u;
            return;
        case 10:
            leave_runtime_weapon_follow(weapon); // 0x001496D0
            return;
        default:
            return;
        }
    }
    weapon.actor.model_mode &= ~4; // Actor +0x118.
    register_actor_shape_probe(weapon.actor, weapon.view.shape); // 0x0003C770, inlined
}

// Xbox 0x000CAF10
void late_update_runtime_gun_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    if (state != 3)
    {
        return;
    }
    auto bytes = weapon.bytes;
    Actor* holder = holder_of(weapon);
    Vec3 point{};
    Vec3 direction{};
    if (!runtime_gun_muzzle(weapon, point, direction)) // 0x000CB330
    {
        return;
    }
    if (holder == nullptr)
    {
        throw Error("0x000CAF10 reads +0xCDC of a held gun's holder through address zero");
    }
    if ((holder->mask_mode == 2 || holder->mask_flag != 0) && kScriptHold != 0) // DAT_00512278
    {
        set(bytes, field::charge, 1.f); // DAT_0032B0F8.
    }
    if (get(bytes, field::firing) == 0)
    {
        recharge_runtime_gun_weapon(weapon, holder); // 0x000CB200, EAX holder
    }
    else
    {
        const float charge = get(bytes, field::charge) - get(bytes, field::drain) * frame_step(weapon);
        set(bytes, field::charge, charge);
        if (0.f > charge)
        {
            exhaust_runtime_gun_weapon(weapon, point, direction); // 0x000CAE00
        }
    }
    fire_tick_hook(weapon, get(bytes, field::firing), point, direction); // Vtable +0x5C
    report_runtime_gun_weapon_hits(weapon, *holder, point, direction); // 0x000CB0B0, EDX point
}

// Xbox 0x000CB330, ECX actor, EAX point
bool runtime_gun_muzzle(RuntimeWeaponActor& weapon, Vec3& point, Vec3& direction)
{
    if (get(weapon.bytes, field::muzzle) == 0)
    {
        return false;
    }
    const GunLocators& locators = gun_locators.at(weapon.bytes.data());
    const Actor& actor = weapon.actor;
    const ModelPoseBuffer* pose = weapon_shape_pose(actor);
    AnimationMatrix world;
    std::memcpy(world.data(), actor.xform, 48); // Actor +0x2AC.
    const auto point_of = [&](const u8* record, Vec3& output)
    {
        const auto binding = locator_binding(record);
        if (binding.joint >= 0 && pose == nullptr)
        {
            throw Error("gun locator names a joint but the model instance has no pose");
        }
        model_bone_world_point(binding, pose, &world, output); // 0x00051CE0 (the aim locator inlined)
    };
    point_of(locators.muzzle, point);
    Vec3 aim;
    point_of(locators.aim, aim);
    direction = Vec3{aim.x - point.x, aim.y - point.y, aim.z - point.z};
    const float length = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (length != 0.f) // DAT_0032B100.
    {
        const long double scale = retail_reciprocal_sqrt(length); // 0x001534C0
        direction.x = float(scale * direction.x);
        direction.y = float(scale * direction.y);
        direction.z = float(scale * direction.z);
    }
    return true;
}

// Xbox 0x000CB200, EAX holder, ESI actor
void recharge_runtime_gun_weapon(RuntimeWeaponActor& weapon, const Actor* holder)
{
    auto bytes = weapon.bytes;
    RuntimeCandleServices& candle = weapon.services.candle(); // 0x00028D50's views.
    if (holder == nullptr || runtime_player_damage_enabled(*holder, candle.blocked, candle.modes, candle.mode) != 0)
    {
        const float step = frame_step(weapon);
        const float delay = get(bytes, field::empty_delay);
        if (0.f < delay)
        {
            const float left = delay - step;
            set(bytes, field::empty_delay, left);
            if (left <= 0.f)
            {
                set(bytes, field::empty_delay, 0.f);
            }
        }
        if (get(bytes, field::empty_delay) == 0.f)
        {
            const float charge = get(bytes, field::recharge) * frame_step(weapon) + get(bytes, field::charge);
            set(bytes, field::charge, charge);
            if (1.f < charge) // DAT_0032B0F8.
            {
                set(bytes, field::charge, 1.f);
            }
        }
    }
    const bool charged = 0.f < get(bytes, field::charge);
    bytes[0x93C] = charged ? 1 : 0;
    if (!charged)
    {
        stop_weapon_sound(weapon);
    }
}

// Xbox 0x000CAE00, ESI actor
void exhaust_runtime_gun_weapon(RuntimeWeaponActor& weapon, const Vec3& point, const Vec3& direction)
{
    auto bytes = weapon.bytes;
    bytes[0x93C] = 0;
    stop_weapon_sound(weapon);
    set(bytes, field::charge, 0.f);
    set(bytes, field::firing, 0u);
    set(bytes, field::empty_delay, 5.f); // DAT_0032B184.
    fire_end_hook(weapon, point, direction); // Vtable +0x50
}

// Xbox 0x000CB0B0, EDX point
void report_runtime_gun_weapon_hits(RuntimeWeaponActor& weapon, Actor& holder, const Vec3& point,
    const Vec3& direction)
{
    auto bytes = weapon.bytes;
    // The holder's own shapes are out of the query.
    const u32 filter = holder.model_flags;
    holder.model_flags = 0;
    GhoulySpawnPool* hits = hit_query_hook(weapon, point, direction); // Vtable +0x60
    holder.model_flags = filter;
    if (hits == nullptr)
    {
        return;
    }
    for (u8* at = hits->begin; at < hits->cursor; at += sizeof(RuntimeSprayHit))
    {
        auto& hit = *reinterpret_cast<RuntimeSprayHit*>(at);
        RuntimeSegmentContactActor& object = weapon.contact_actor(*hit.object);
        if (report_runtime_weapon_spray_contact(weapon, &holder, object, hit.point, hit.contacts, hit.velocity,
                bytes[0xDAC], bytes[0xDAD], bytes[0xDAE], get(bytes, field::hits) == 0) != 0) // 0x00141C00
        {
            set(bytes, field::hits, get(bytes, field::hits) + 1);
        }
        engine_heap_free(hit.contacts); // HeapFree on the block's heap (0x000F7100).
    }
    engine_heap_free(hits);
}

// Xbox 0x000CB040
u32 test_runtime_gun_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    report_runtime_weapon_contact(weapon, holder_of(weapon), object, point, contacts, velocity, word, 7, 0x3A, 0x68,
        get(weapon.bytes, field::unk_EF4) == 0); // 0x00141890, EDX 0x68
    return weapon.bytes[0x93C] == 0 ? 1u : 0u;
}
}
