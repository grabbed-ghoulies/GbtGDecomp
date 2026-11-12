#include "candle_control.h"
#include "../entities/actor_model_anchors.h"

namespace gbtg
{
namespace
{
namespace parameter = candle_control_parameter;

RuntimeCandleFlame candle_flame(CandleControlState& control, CandleControlServices& services)
{
    return RuntimeCandleFlame{control.flame_block, *control.candle, control.binding, control.light, control.flame,
        control.glow, services.elapsed(*control.candle), services.candle, services.registry, services.error,
        services.sounds};
}
}

// Xbox 0x00080EA0
void candle_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x40;
    output.parameters = 0xC;
}

// Xbox 0x00080F10
bool construct_runtime_candle_control(std::span<const u8> parameters, CandleControlState& control,
    CandleControlServices& services)
{
    control.candle = services.actor_at(get(parameters, parameter::actor));
    control.binding = find_actor_model_binding(*control.candle, 0); // 0x000408D0, EAX the actor, EDI 0
    CandleFlameBlock& block = control.flame_block;
    block.lit = 0;
    control.flame = {};
    control.glow = {};
    block.light = -1;
    block.point_light = -1;
    RuntimeCandleFlame flame = candle_flame(control, services);
    light_runtime_candle(flame); // 0x000C6FB0, EAX +0x28, ECX +0x24
    return true;
}

// Xbox 0x00080F60
void destroy_runtime_candle_control(CandleControlState& control, CandleControlServices& services)
{
    RuntimeCandleFlame flame = candle_flame(control, services);
    put_out_runtime_candle(flame); // 0x000C7260, EDI +0x28, stack +0x24
}

// Xbox 0x00080F80
void tick_runtime_candle_control(SceneControlBase& base, CandleControlState& control,
    CandleControlServices& services)
{
    float step = 0.f;
    if (!(services.freeze > 0.f))
    {
        step = services.dt * services.scene_scale;
    }
    base.elapsed = base.elapsed + step;
    RuntimeCandleFlame flame = candle_flame(control, services);
    update_runtime_candle_flame(1.f, 1, flame); // 0x000C7060, EDI +0x28, ECX +0x24, XMM0 DAT_0032B0F8, EAX 1
}

// Xbox 0x00080FD0
void draw_runtime_candle_control(CandleControlState& control, CandleControlServices& services)
{
    RuntimeCandleFlame flame = candle_flame(control, services);
    queue_runtime_candle_flame(flame); // 0x000C6C80, EBX +0x24, EDI +0x28
}
}
