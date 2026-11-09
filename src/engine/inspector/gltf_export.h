#pragma once
// glTF 2.0 binary (.glb) from a decoded model: the skeleton as joint nodes, the body and the blend-shape branch (head)
// as skinned meshes, one primitive per material, morph targets on the head, and the colour textures embedded as PNG.
// Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "serialized_model.h"

#include <vector>

namespace gbtg
{
std::vector<u8> write_gltf(const DecodedModel& model);
}
#endif
