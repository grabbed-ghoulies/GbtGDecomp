#pragma once

#include "weapon_actor.h"
#include "../graphics/billboard_queue.h"
#include "../scene/scene_point_lights.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/model_runtime.h"

namespace gbtg
{
// entityWeaponCandle, factory type 0xA1 (getter 0x000C64F0, vtable 0x00506108) over the weapon (type 0x0B): scene 9's
// special candle. It keeps the weapon update 0x0014A2E0 (+0x10) and the empty 0x001628B0 (+0x48); its enter and leave
// (+0x3C 0x000C6BB0, +0x40 0x000C6BC0) jump to the weapon's. While held and lit it burns a flame at model binding 0: a
// kind-2 scene light (DAT_00509B7C), a point light (DAT_00511B70), the torch sound on sound slot 4 and two billboards.
// Its fuel (+0xE54) gives the flame's strength; a lit swing that lands costs 0.225 of it once, an empty candle
// relights after 5 s and refills at 0.075 a second, and the scoreboard meter shows it while a live player holds it.
namespace candle_weapon_field
{
constexpr RetailField<float, 0xE54> fuel{}; // 1 at construction
constexpr RetailField<float, 0xE58> relight{}; // seconds before the fuel refills, 5 once it runs out
constexpr u32 kFlame = 0xE5C; // The flame block (candle_flame_field), 0x18 bytes.
constexpr RetailField<u32, 0xE74> burning{}; // messages 0x11 / 0x12: the flame may burn
constexpr RetailField<u32, 0xE78> spent{}; // a landed swing took its fuel; message 0x92 (a new swing) clears it
constexpr u32 kSize = 0xE7C;
static_assert(retail_fits(kSize, spent));
} // namespace candle_weapon_field
// The flame block 0x000C6C80..0x000C7540 work on (EDI): the candle's +0xE5C and the scene 10 control's +0x40. +4 is
// the model binding and +0x10 / +0x14 the flame and glow sprites (host: RuntimeCandleFlame's references).
namespace candle_flame_field
{
constexpr RetailField<u32, 0x00> lit{};
constexpr RetailField<std::int8_t, 0x08> light{}; // its DAT_00509B7C slot, -1 for none (host record: light)
constexpr RetailField<i32, 0x0C> point_light{}; // its DAT_00511B70 record, -1 for none
constexpr u32 kSize = 0x18;
static_assert(retail_fits(kSize, point_light));
} // namespace candle_flame_field
// DAT_0050D740..DAT_0050D74C: set while the candle's flame burns, the flame point raised 40.
struct RuntimeCandleFlameSpot
{
    u32 active = 0;
    Vec3 position{};
};
struct RuntimeCandleServices
{
    RuntimeShaderRegistrationState& lights; // DAT_00509B7C / DAT_00509B78.
    RuntimeScenePointLights& point_lights; // DAT_00511B70.
    const u32& point_lights_on; // DAT_00512138.
    u32& script_lights; // DAT_00512104.
    BillboardQueue& billboards;
    std::span<const u32, 2> blocked; // DAT_0050E764 / DAT_0050E768, for 0x00028D50.
    std::span<const u8> modes; // DAT_0030F144.
    const u32& mode; // DAT_0051248C.
    RuntimeActorCreationOptions& hit; // DAT_004E1C5C, the burst callout's position.
    RuntimeCandleFlameSpot& spot;
};
// One flame block and the actor whose model binding carries it (ECX / EBX in the flame functions).
struct RuntimeCandleFlame
{
    std::span<u8> block; // candle_flame_field.
    Actor& actor;
    const u8*& binding; // +4, 0x000408D0's binding 0 of the actor.
    std::unique_ptr<std::array<u8, 0x98>>& light; // The +8 slot's record.
    std::span<const u8>& flame; // +0x10.
    std::span<const u8>& glow; // +0x14.
    const float& elapsed; // Actor +0x924, the sprites' clock.
    RuntimeCandleServices& services;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    RuntimeActorSoundQueues& sounds;
};
// The candle weapon's own flame block (+0xE5C).
RuntimeCandleFlame runtime_candle_weapon_flame(RuntimeWeaponActor& weapon);
void candle_weapon_object_sizes(ObjectSizes& sizes); // Xbox 0x000C64D0
bool initialize_runtime_candle_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C6560
void destroy_runtime_candle_weapon(RuntimeWeaponActor& weapon); // Xbox 0x000C67A0
void dispatch_runtime_candle_weapon_message(RuntimeWeaponActor& weapon, std::span<u32> message); // Xbox 0x000C6990
// The candle's contact test (+0x99C): the weapon contact report, a burning hit while lit.
u32 test_runtime_candle_contact(RuntimeWeaponActor& weapon, RuntimeSegmentContactActor& object, const Vec3& point,
    GhoulySpawnPool* contacts, const Vec3& velocity, u16 word); // Xbox 0x000C6F10
// The flame's strength from its fuel: 0 when empty, else 1 + 4 * fuel (fuel kept below 0.999).
i32 runtime_candle_strength(float fuel); // Xbox 0x000C7500, XMM0 fuel
// The flame lights (the torch sound, the scene light, the sprites) the first time.
void light_runtime_candle(RuntimeCandleFlame& flame); // Xbox 0x000C6FB0, EAX block, ECX actor
// The flame goes out: its light, sprites, sound and point light released.
void put_out_runtime_candle(RuntimeCandleFlame& flame); // Xbox 0x000C7260, EDI block, stack actor
// The flame's strength, lighting or putting it out as `burning` and the fuel allow.
i32 tend_runtime_candle(float fuel, u32 burning, RuntimeCandleFlame& flame); // Xbox 0x000C7540, EAX block, ECX actor
// The flame follows the binding: its lights at the flame point, or out once the strength is gone.
void update_runtime_candle_flame(float fuel, u32 burning,
    RuntimeCandleFlame& flame); // Xbox 0x000C7060, EDI block, ECX actor, XMM0 fuel, EAX burning
void place_runtime_candle_lights(RuntimeCandleFlame& flame, const Vec3& position); // Xbox 0x000C7310, EAX block, EBX position
// The flame and glow billboards at the flame point.
void queue_runtime_candle_flame(RuntimeCandleFlame& flame); // Xbox 0x000C6C80, EBX actor, EDI block
void update_runtime_candle_weapon_state(RuntimeWeaponActor& weapon, u16 state); // Xbox 0x000C6800, vtable +0x44
void leave_runtime_candle_weapon_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Xbox 0x000C6BC0, vtable +0x40
}
