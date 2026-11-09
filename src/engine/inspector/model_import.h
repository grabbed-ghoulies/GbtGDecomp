#pragma once
// Model import (docs/INSPECTOR.md, "Model export and import"): a glTF skinned to a disc model's skeleton becomes a new
// serialized model resource built on that model as a template. Every template node keeps its offset; the new vertex
// streams join the first GPU chunk, and the index lists, skin palettes, extra palette groups and matrix rows are appended
// after the CPU block with tree-relative pointers, so the loader (0x00052910) relocates the result as it does the disc
// model. Materials, shaders, the skeleton and the blend shape's layout are the template's. Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gltf_import.h"
#include "gbtg/bundle.h"

#include <string>
#include <vector>

namespace gbtg
{
struct ModelImportResult
{
    std::vector<u8> resource; // A loose resource file (docs/FORMATS.md), for a plugin's aid/ folder.
    std::vector<std::string> notes; // What was mapped, collapsed or left out.
};

ModelImportResult import_model(const BundleEntry& template_entry, const GltfScene& scene);
}
#endif
