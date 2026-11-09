#pragma once
// Reads a binary FBX for the inspector's model import into the same scene the glTF reader makes: skinned meshes
// (Skin clusters), their BlendShape channels, the bones' bind points and the materials' diffuse images. Positions come
// out in the file's world space; the import fits that to the template skeleton. Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gltf_import.h"

namespace gbtg
{
GltfScene read_fbx_scene(const std::filesystem::path& path);
}
#endif
