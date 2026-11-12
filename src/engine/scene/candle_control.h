#pragma once
#include "scene_control_fields.h"
#include "../entities/candle_weapon.h"
#include "gbtg/object_factory.h"

#include <functional>

namespace gbtg
{
// entitySceneControlCandle (factory type 0x9C, getter 0x00080EC0, vtable 0x00505560; the factory table's row 0x9B
// carries the name) over the scene control (type 3): a relic candle's flame on a cutscene actor. The walk-in script
// action 0x001274B0 makes it from aid_objparams_ghoulies_scenecontrol_candle when its cutscene has a reliccandle
// actor, the actor's address in the parameters' +8. It lights the candle weapon's flame at the actor's model binding
// 0, keeps it burning while it ticks, and puts it out when it goes. Its +0xC is the scene control's 0x00115EE0.
// Retail never makes one: the only cutscene with a reliccandle is chapter 1 scene 10's walk, which the relics store
// control (type 0x94) plays, and no script's walk-in names it.
struct CandleControlState
{
    Actor* candle = nullptr; // +0x24, the parameters' +8
    // +0x28: the flame block. Its pointer words are the four members after it.
    CandleFlameBlock flame_block{};
    const u8* binding = nullptr; // +0x2C
    std::unique_ptr<std::array<u8, 0x98>> light; // The +0x30 slot's record.
    std::span<const u8> flame; // +0x38
    std::span<const u8> glow; // +0x3C
};
struct CandleControlServices
{
    RuntimeCandleServices& candle;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    RuntimeActorSoundQueues& sounds;
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
    std::function<const float&(Actor&)> elapsed; // Actor +0x924.
    std::function<Actor*(u32)> actor_at;
};
namespace candle_control_parameter
{
constexpr RetailField<u32, 0x08> actor{}; // the candle actor's address
}
void candle_control_object_sizes(ObjectSizes& output); // Xbox 0x00080EA0
bool construct_runtime_candle_control(std::span<const u8> parameters, CandleControlState& control,
    CandleControlServices& services); // Xbox 0x00080F10
void destroy_runtime_candle_control(CandleControlState& control, CandleControlServices& services); // Xbox 0x00080F60
void tick_runtime_candle_control(SceneControlBase& base, CandleControlState& control,
    CandleControlServices& services); // Xbox 0x00080F80
void draw_runtime_candle_control(CandleControlState& control, CandleControlServices& services); // Xbox 0x00080FD0
}
