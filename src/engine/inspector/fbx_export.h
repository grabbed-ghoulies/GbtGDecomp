#pragma once
// Binary FBX from a decoded model: the skeleton as LimbNode models, the body and the blend-shape branch (head) as
// meshes with a Skin deformer (one cluster per joint) and a bind pose, the head's MORPH_TARGET_* shapes as a
// BlendShape, and the colour textures as PNG files beside the FBX (in <name>.fbm) and embedded in it. Plugin builds
// only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "serialized_model.h"

#include <filesystem>
#include <vector>

namespace gbtg
{
// Writes `out` and its texture folder.
void write_fbx_model(const DecodedModel& model, const std::filesystem::path& out);
}
#endif
