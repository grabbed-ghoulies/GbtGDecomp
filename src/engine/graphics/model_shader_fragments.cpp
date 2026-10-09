#include "model_shader_fragments.h"

namespace gbtg
{
// Xbox 0x00159390
u32 runtime_scene_lighting_flags(const RuntimeModelRenderContext& renderer)
{
    return renderer.lighting_flags;
}
// Xbox 0x00158EB0
void find_runtime_shader_fragment(u32& program, u8 use_lighting,
    RuntimeShaderFragmentContext& c)
{
    u32 flags = c.renderer.lighting_flags;
    if (use_lighting == 0)
    {
        flags = 0;
    }
    RuntimeRenderLight* link = next_runtime_light(c.buckets[flags % 239]);
    if (link != nullptr)
    {
        while (link->type != flags)
        {
            link = next_runtime_light(*link);
            if (link == nullptr)
            {
                return;
            }
        }
        program = c.programs.at(link);
    }
}
}
