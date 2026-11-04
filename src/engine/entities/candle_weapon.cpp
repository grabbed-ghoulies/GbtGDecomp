#include "candle_weapon.h"
#include "powerup_flags.h"
#include "weapon_actor_detail.h"
#include "actor_attachment_events.h"
#include "actor_model_anchors.h"
#include "actor_transform.h"
#include "body_damage_notices.h"
#include "body_damage_tables.h"
#include "fx_custom_electric.h"
#include "../audio/listener_settings.h"
#include "../frontend/scoreboard_update.h"
#include "../scene/scene_shadow_casters.h"
#include "gbtg/sound_runtime.h"
#include <bit>
#include <cstring>

namespace gbtg
{
using namespace weapon_detail;

namespace
{
CandleWeaponState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<CandleWeaponState>(bytes);
}

const CandleWeaponState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<CandleWeaponState>(bytes);
}

namespace flame_field = candle_flame_field;
constexpr u32 kColor = 0xC8CE93FFu; // DAT_00303588.
constexpr u32 kSecondColor = 0xC800BFBFu; // DAT_0030358C.
constexpr float kNear = 78.f; // DAT_0032B394.
constexpr float kFar = 268.f; // DAT_0032B390.
constexpr float kPointNear = 0.917f; // DAT_0032B38C.
constexpr float kPointFar = std::bit_cast<float>(0x3F3BE76Du); // DAT_0032B388, 0.734.

RuntimeCandleServices& candle_of(RuntimeWeaponActor& weapon)
{
    return weapon.services.candle();
}

void put_float(u8* bytes, size_t offset, float value)
{
    std::memcpy(bytes + offset, &value, 4);
}

void put_word(u8* bytes, size_t offset, u32 value)
{
    std::memcpy(bytes + offset, &value, 4);
}

// The light record's +0x66 flags with bit 0 (its values changed).
void mark_light(u8* record)
{
    u16 flags;
    std::memcpy(&flags, record + 0x66, 2);
    flags |= 1;
    std::memcpy(record + 0x66, &flags, 2);
}

// 0x00145FD0, EAX actor: the holder whose link actor +0x918 names, while that link is set.
Actor* holder_of(const RuntimeWeaponActor& weapon)
{
    const ActorLink* link = weapon.actor.link;
    return link != nullptr && link->head != nullptr ? link->owner : nullptr;
}

// The model's selected pose (+0x204's +0x168, else +0x164) for the binding's joint.
const ModelPoseBuffer& binding_pose(const Actor& actor)
{
    const RuntimeModelInstance* model = actor.model == nullptr ? nullptr : actor.model->runtime;
    const ModelPoseBuffer* pose = model == nullptr ? nullptr
        : model->alternate != nullptr ? model->alternate : model->primary;
    if (pose == nullptr)
    {
        throw Error("0x00031530 reads the candle's flame joint without a model pose");
    }
    return *pose;
}

// The binding's position (+0x10, +0x20, +0x30) and joint (+2).
Vec3 binding_local(const u8* binding, i16& joint)
{
    if (binding == nullptr)
    {
        throw Error("the candle reads its flame binding, which its model does not have");
    }
    Vec3 local{};
    std::memcpy(&local.x, binding + 0x10, 4);
    std::memcpy(&local.y, binding + 0x20, 4);
    std::memcpy(&local.z, binding + 0x30, 4);
    std::memcpy(&joint, binding + 2, 2);
    return local;
}

// 0x000C6560's inline 3x4 products: x ((z, y), x), y ((x, z), y), z ((x, z), y).
Vec3 construct_transform(const float* m, const Vec3& p)
{
    return Vec3{((m[2] * p.z + m[1] * p.y) + m[0] * p.x) + m[3], ((m[4] * p.x + m[6] * p.z) + m[5] * p.y) + m[7],
        ((m[8] * p.x + m[10] * p.z) + m[9] * p.y) + m[11]};
}

// 0x000C7060's: x ((y, z), x), y ((x, y), z), z ((x, y), z).
Vec3 update_transform(const float* m, const Vec3& p)
{
    return Vec3{((m[1] * p.y + m[2] * p.z) + m[0] * p.x) + m[3], ((m[4] * p.x + m[5] * p.y) + m[6] * p.z) + m[7],
        ((m[8] * p.x + m[9] * p.y) + m[10] * p.z) + m[11]};
}

// 0x000C6C80's: x ((z, y), x), y ((z, y), x), z ((z, y), x).
Vec3 draw_transform(const float* m, const Vec3& p)
{
    return Vec3{((m[2] * p.z + m[1] * p.y) + m[0] * p.x) + m[3], ((m[6] * p.z + m[5] * p.y) + m[4] * p.x) + m[7],
        ((m[10] * p.z + m[9] * p.y) + m[8] * p.x) + m[11]};
}

// The flame point: the binding through its joint (when not negative), then through the actor transform at +0x2AC.
Vec3 flame_point(const Actor& actor, const u8* binding, Vec3 (*transform)(const float*, const Vec3&))
{
    i16 joint = 0;
    Vec3 point = binding_local(binding, joint);
    if (joint >= 0)
    {
        point = transform(model_pose_matrix(binding_pose(actor), joint).data(), point); // 0x00031530
    }
    return transform(actor.xform, point); // Actor +0x2AC.
}

// The inline release of a DAT_00511B70 record.
void release_point_light(RuntimeCandleFlame& flame)
{
    release_runtime_scene_point_light(get(flame.block, flame_field::point_light), flame.services.point_lights);
}

// Xbox 0x000C6C10: a kind-2 light at the origin (DAT_0050DD80), 78..268 with the candle's colour.
std::int8_t start_candle_light(RuntimeCandleFlame& flame)
{
    flame.light = std::make_unique<std::array<u8, 0x98>>(); // 0x000F72D0(0x98) in 0x000FE080.
    u8* record = flame.light->data();
    const std::int8_t slot = allocate_runtime_light(2, Vec3{}, record, flame.services.lights); // 0x000FE080, ESI DAT_0050DD80
    record[0x66] |= 0x48;
    put_float(record, 0x6C, kNear);
    mark_light(record);
    put_float(record, 0x70, kFar);
    mark_light(record);
    put_word(record, 0x78, kColor);
    mark_light(record);
    put_word(record, 0x80, kColor);
    put_word(record, 0x84, kColor);
    put_float(record, 0x88, 2.f); // DAT_0032B110.
    return slot;
}

// 0x0005C1A0 for one of the flame's sprites: its frame from the actor's run time (+0x924) when it animates.
void queue_flame_sprite(RuntimeCandleFlame& flame, std::span<const u8> sprite, u32 color, float size,
    const Vec3& position)
{
    const std::array<float, 2> extent{size, size};
    u8 frame = 0;
    const float rate = float(sprite[0x10]);
    if (rate != 0.f) // UCOMISS DAT_0032B100.
    {
        const auto frames = u32(truncate_runtime_float(static_cast<long double>(rate) * flame.elapsed)); // 0x001673D8
        frame = u8(frames % sprite[0x11]);
    }
    queue_billboard(flame.services.billboards, sprite.data(), color, frame, position, extent.data(), 0.f);
}
}

RuntimeCandleFlame runtime_candle_weapon_flame(RuntimeWeaponActor& weapon)
{
    return RuntimeCandleFlame{retail_bytes(fields_of(weapon.bytes).flame), weapon.actor,
        weapon.candle_binding, weapon.candle_light, weapon.candle_flame, weapon.candle_glow, weapon.view.frame.elapsed,
        candle_of(weapon), weapon.registry, weapon.error, weapon.services.sounds};
}

// Xbox 0x000C64D0
void candle_weapon_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(CandleWeaponState);
    sizes.parameters = 0x5F8;
}

// Xbox 0x000C6560
bool initialize_runtime_candle_weapon(RuntimeWeaponActor& weapon)
{
    RuntimeCandleFlame flame = runtime_candle_weapon_flame(weapon);
    weapon_base(weapon.bytes).contact_test = 0x000C6F10;
    flame.binding = find_actor_model_binding(weapon.actor, 0); // 0x000408D0, EDI 0
    set(flame.block, flame_field::lit, 0u);
    flame.flame = {};
    flame.glow = {};
    set(flame.block, flame_field::light, std::int8_t(-1));
    set(flame.block, flame_field::point_light, -1);
    fields_of(weapon.bytes).fuel = 1.f; // DAT_0032B0F8.
    if (get(flame.block, flame_field::point_light) < 0 && flame.services.point_lights_on != 0)
    {
        const Vec3 position = flame_point(weapon.actor, flame.binding, construct_transform);
        set(flame.block, flame_field::point_light, add_runtime_scene_point_light(position, kColor, kSecondColor,
            kPointNear, kPointFar, kNear, kFar, 0, flame.services.point_lights)); // 0x00123720, ECX position
    }
    return true;
}

// Xbox 0x000C67A0
void destroy_runtime_candle_weapon(RuntimeWeaponActor& weapon)
{
    RuntimeCandleFlame flame = runtime_candle_weapon_flame(weapon);
    put_out_runtime_candle(flame); // 0x000C7260
    release_point_light(flame); // The inlined release.
    set(flame.block, flame_field::point_light, -1);
}

// Xbox 0x000C7500, XMM0 fuel
i32 runtime_candle_strength(float fuel)
{
    if (fuel == 0.f) // DAT_0032B100.
    {
        return 0;
    }
    if (fuel > 0.999f) // DAT_0032B2A8.
    {
        fuel = std::bit_cast<float>(0x3F7FBE77u);
    }
    return 1 - i32(fuel * -4.f); // DAT_0032B2A4; CVTTSS2SI.
}

// Xbox 0x000C6FB0, EAX block, ECX actor
void light_runtime_candle(RuntimeCandleFlame& flame)
{
    if (get(flame.block, flame_field::lit) != 0)
    {
        return;
    }
    Actor& actor = flame.actor;
    queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f, "XACT_SOUNDBANK_GMISC_TORCHBURN",
        flame.sounds); // 0x0012A260, EBX actor +0x614
    set(flame.block, flame_field::light, start_candle_light(flame)); // 0x000C6C10
    flame.flame = acquire_animation_resource(flame.registry, "aid_texture_particles_candlefinalflame"); // 0x00035E90
    if (flame.flame.data() == nullptr)
    {
        resource_error(flame.error, 2); // 0x00032700
    }
    flame.glow = acquire_animation_resource(flame.registry, "aid_texture_particles_candlefinalglow");
    if (flame.glow.data() == nullptr)
    {
        resource_error(flame.error, 2);
    }
    set(flame.block, flame_field::lit, 1u);
    (void)actor_world_position(actor); // 0x000412A0 into a stack vector nothing reads.
    flame.services.script_lights = 0;
}

// Xbox 0x000C7260, EDI block, stack actor
void put_out_runtime_candle(RuntimeCandleFlame& flame)
{
    if (get(flame.block, flame_field::lit) == 0)
    {
        return;
    }
    RuntimeCandleServices& services = flame.services;
    const std::int8_t slot = get(flame.block, flame_field::light);
    if (slot != -1)
    {
        u8* record = services.lights.records[size_t(slot)];
        if ((record[0x66] & 0x10) == 0)
        {
            flame.light.reset(); // 0x000663E0
        }
        services.lights.records[size_t(slot)] = nullptr;
        --services.lights.count;
        set(flame.block, flame_field::light, std::int8_t(-1));
    }
    if (flame.flame.data() != nullptr)
    {
        release_animation_resource(flame.registry, flame.flame); // 0x00035C00
        flame.flame = {};
    }
    if (flame.glow.data() != nullptr)
    {
        release_animation_resource(flame.registry, flame.glow);
        flame.glow = {};
    }
    release_actor_sound(flame.actor.sound_slots[4], flame.sounds); // 0x0012A3D0, ESI actor +0x614
    set(flame.block, flame_field::lit, 0u);
    if (get(flame.block, flame_field::point_light) >= 0)
    {
        release_point_light(flame); // The inlined release.
        services.script_lights = 1;
    }
    set(flame.block, flame_field::point_light, -1);
}

// Xbox 0x000C7540, EAX block, ECX actor
i32 tend_runtime_candle(float fuel, u32 burning, RuntimeCandleFlame& flame)
{
    const i32 strength = runtime_candle_strength(fuel); // The inlined 0x000C7500.
    if (strength > 0 && burning != 0)
    {
        if (get(flame.block, flame_field::lit) == 0)
        {
            light_runtime_candle(flame); // 0x000C6FB0
        }
        return strength;
    }
    if (get(flame.block, flame_field::lit) != 0)
    {
        put_out_runtime_candle(flame); // 0x000C7260
    }
    return strength;
}

// Xbox 0x000C7310, EAX block, EBX position
void place_runtime_candle_lights(RuntimeCandleFlame& flame, const Vec3& position)
{
    RuntimeCandleServices& services = flame.services;
    u8* record = services.lights.records[size_t(get(flame.block, flame_field::light))];
    std::memcpy(record + 0x40, &position, 12);
    if (record[0x64] == 2)
    {
        Vec3 direction{0.f - position.x, 0.f - position.y, 0.f - position.z};
        Vec3 output{};
        runtime_direction_or_axis(direction, output, 0x79); // 0x00106B90, 'y'
        std::memcpy(record + 0x4C, &output, 12);
    }
    put_float(record, 0x6C, kNear);
    mark_light(record);
    put_float(record, 0x70, kFar);
    mark_light(record);
    put_word(record, 0x78, kColor);
    mark_light(record);
    put_word(record, 0x80, kColor);
    put_word(record, 0x84, kColor);
    if (get(flame.block, flame_field::point_light) < 0 && services.point_lights_on != 0)
    {
        set(flame.block, flame_field::point_light, add_runtime_scene_point_light(position, kColor, kSecondColor,
            kPointNear, kPointFar, kNear, kFar, 0, services.point_lights)); // 0x00123720, ECX position
    }
    const i32 index = get(flame.block, flame_field::point_light);
    if (index >= 0)
    {
        SceneAmbientPoint& point = services.point_lights.points.at(size_t(index));
        if (point.active == 0)
        {
            set(flame.block, flame_field::point_light, -1);
        }
        else
        {
            point.position = position;
            point.color = kColor;
            point.unk_14 = kPointNear;
            point.unk_1C = kNear;
            point.second_color = kSecondColor;
            point.unk_18 = kPointFar;
            point.unk_20 = kFar;
        }
    }
    services.spot.active = 1;
    services.spot.position = Vec3{position.x, position.y + 40.f, position.z}; // DAT_0032B1B8.
}

// Xbox 0x000C7060, EDI block, ECX actor, XMM0 fuel, EAX burning
void update_runtime_candle_flame(float fuel, u32 burning, RuntimeCandleFlame& flame)
{
    const i32 strength = tend_runtime_candle(fuel, burning, flame); // 0x000C7540
    const Vec3 position = flame_point(flame.actor, flame.binding, update_transform);
    if (strength > 0)
    {
        if (get(flame.block, flame_field::light) != -1)
        {
            place_runtime_candle_lights(flame, position); // 0x000C7310
        }
        return;
    }
    if (get(flame.block, flame_field::point_light) >= 0)
    {
        release_point_light(flame); // The inlined release.
    }
    set(flame.block, flame_field::point_light, -1);
    flame.services.script_lights = 1;
}

// Xbox 0x000C6C80, EBX actor, EDI block
void queue_runtime_candle_flame(RuntimeCandleFlame& flame)
{
    if (get(flame.block, flame_field::lit) == 0)
    {
        return;
    }
    const Vec3 position = flame_point(flame.actor, flame.binding, draw_transform);
    if (flame.flame.data() != nullptr)
    {
        queue_flame_sprite(flame, flame.flame, 0xFFFFFFFFu, 5.f, position); // DAT_0032B184.
    }
    if (flame.glow.data() != nullptr)
    {
        queue_flame_sprite(flame, flame.glow, 0x37FFFFFFu, 7.f, position); // DAT_0032B284.
    }
}

// Xbox 0x000C6F10
u32 test_runtime_candle_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word)
{
    auto bytes = weapon.bytes;
    const bool lit = get(std::span<const u8>(fields_of(bytes).flame), flame_field::lit) != 0;
    report_runtime_weapon_contact(weapon, holder_of(weapon), object, point, contacts, velocity, word,
                                  lit ? weapon_base(bytes).hit_kind : u8(7),
                                  lit ? weapon_base(bytes).hit_strength : u8(0x3F),
                                  weapon_base(bytes).hit_style, fields_of(bytes).spent == 0); // 0x00141890
    return weapon_base(bytes).mask == 0 ? 1u : 0u;
}

// Xbox 0x000C6990
void dispatch_runtime_candle_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message)
{
    dispatch_runtime_weapon_message(weapon, message); // 0x001463D0
    auto bytes = weapon.bytes;
    switch (message[0])
    {
    case 0x11:
        fields_of(bytes).burning = 1u;
        return;
    case 0x12:
        fields_of(bytes).burning = 0u;
        return;
    case 0x5A:
    {
        if (message[0xB] == 0)
        {
            return;
        }
        Actor* holder = holder_of(weapon); // 0x00145FD0
        const i32 strength = runtime_candle_strength(fields_of(bytes).fuel); // 0x000C7500
        if (strength > 0)
        {
            RuntimeActorCreationOptions& hit = candle_of(weapon).hit;
            hit.flags = 0;
            hit.position = Vec3{std::bit_cast<float>(message[8]), std::bit_cast<float>(message[9]),
                std::bit_cast<float>(message[10])};
            hit.flags = 1;
            dispatch_actor_resource_callout(weapon.view.startup.resource, 0x54, &hit,
                weapon.base().callouts); // 0x0003C040, ECX actor, EDX 0x54
        }
        if (fields_of(bytes).spent != 0)
        {
            return;
        }
        fields_of(bytes).spent = 1u;
        if (strength == 0)
        {
            return;
        }
        if (holder == nullptr)
        {
            throw Error("candle message 0x5A burns fuel without a holder for 0x00028D00");
        }
        if (runtime_actor_is_player(*holder) && runtime_weapon_boost_active() != 0) // 0x00028D00, DAT_00512278
        {
            return;
        }
        const float left = fields_of(bytes).fuel - 0.225f; // DAT_0032B684.
        fields_of(bytes).fuel = left;
        if (0.f > left)
        {
            fields_of(bytes).fuel = 0.f;
            fields_of(bytes).relight = 5.f; // DAT_0032B184.
        }
        return;
    }
    case 0x7D:
    {
        RuntimeCandleFlame flame = runtime_candle_weapon_flame(weapon);
        light_runtime_candle(flame); // The tail jump to 0x000C6FB0, EAX +0xE5C.
        return;
    }
    case 0x91:
        change_runtime_weapon_state(weapon, 3); // 0x0014A290, EDI 3
        return;
    case 0x92:
        if (message[1] != 0)
        {
            fields_of(bytes).spent = 0u;
        }
        return;
    case 0x9D:
        change_runtime_weapon_state(weapon, 7); // 0x0014A290, EDI 7
        return;
    default:
        return;
    }
}

// Xbox 0x000C6800
void update_runtime_candle_weapon_state(RuntimeWeaponActor& weapon, u16 state)
{
    RuntimeCandleServices& candle = candle_of(weapon);
    auto bytes = weapon.bytes;
    Actor* holder = holder_of(weapon);
    RuntimeCandleFlame flame = runtime_candle_weapon_flame(weapon);
    update_runtime_candle_flame(fields_of(bytes).fuel, fields_of(bytes).burning,
        flame); // 0x000C7060, EAX +0xE74, XMM0 fuel
    const RuntimeActorClock& clock = weapon.clock;
    bool tick = true;
    if (holder != nullptr)
    {
        if (runtime_player_damage_enabled(*holder, candle.blocked, candle.modes, candle.mode) == 0) // 0x00028D50
        {
            tick = false;
        }
        else if ((holder->mask_mode == 2 || holder->mask_flag != 0) && runtime_weapon_boost_active() != 0) // DAT_00512278
        {
            fields_of(bytes).relight = 0.f;
            fields_of(bytes).fuel = 1.f;
        }
    }
    if (tick)
    {
        const float step = clock.freeze > 0.f ? 0.f : clock.dt * clock.scale;
        const float relight = fields_of(bytes).relight;
        if (relight > 0.f)
        {
            const float left = relight - step;
            fields_of(bytes).relight = left;
            if (!(0.f < left)) // COMISS; JC keeps a positive delay.
            {
                fields_of(bytes).relight = 0.f;
            }
        }
        if (fields_of(bytes).relight == 0.f) // UCOMISS DAT_0032B100.
        {
            const float refill = clock.freeze > 0.f ? 0.f : clock.dt * clock.scale;
            const float fuel = refill * 0.075f + fields_of(bytes).fuel; // DAT_0032B44C.
            fields_of(bytes).fuel = fuel;
            if (fuel > 1.f)
            {
                fields_of(bytes).fuel = 1.f;
            }
        }
    }
    if (holder != nullptr
        && read_u32(weapon.bytes_at(weapon.services.actor_address(*holder)), 0xCE0) != 0
        && (holder->mask_mode == 2 || holder->mask_flag != 0)
        && runtime_player_damage_enabled(*holder, candle.blocked, candle.modes, candle.mode) != 0) // 0x00028D50
    {
        show_runtime_meter_level(fields_of(bytes).fuel, weapon.services.scoreboard, weapon.registry,
            weapon.error); // 0x0012EC70
    }
    update_runtime_weapon_state(weapon, state); // 0x0014A220
}

// Xbox 0x000C6BC0
void leave_runtime_candle_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next)
{
    if (state == 0)
    {
        return;
    }
    leave_runtime_weapon_state(weapon, state, next); // The tail jump to 0x0014A180.
}
}
