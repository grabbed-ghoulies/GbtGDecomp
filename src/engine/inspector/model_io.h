#pragma once
// Model export and import for the inspector (docs/INSPECTOR.md, "Model export and import"): a disc model's skinned mesh
// to glTF 2.0 and back, written as a plugin's loose model resource the native loader reads in place of the disc one.
// Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/bundle.h"

#include <filesystem>
#include <string>
#include <vector>

namespace gbtg
{
// A disc model from its disc bundle, without any plugin's replacement: the template an import builds on.
struct DiscModel
{
    Bundle bundle;
    const BundleEntry* entry = nullptr;
};
DiscModel load_disc_model(const std::string& aid, const std::filesystem::path& game_dir);
// The inspector's own path and game folder, recorded in export manifests for the 3ds Max tools.
void set_model_io_host(const std::filesystem::path& executable, const std::filesystem::path& game_dir);
// Writes the model as binary FBX (.fbx) or glTF 2.0 binary (.glb), by the file's extension; returns a summary.
std::string export_model_file(const BundleEntry& entry, const std::filesystem::path& out);
// Reads an .fbx or .glb (by its extension) and builds it on the disc model <aid> and writes it to <plugin>/aid/<aid> (and a plugin.ini when the folder has
// none). Returns the file; `notes` say what was mapped, collapsed or left out.
std::filesystem::path import_model_file(const std::filesystem::path& glb, const std::string& aid,
    const std::filesystem::path& game_dir, const std::filesystem::path& plugin, std::vector<std::string>& notes);

// The serialized model's directory, node tree and GPU chunks as text: what the loader (0x00052910) relocates.
std::string describe_serialized_model(const BundleEntry& entry);

// --describe-model <aid> --game-dir <dir>: prints describe_serialized_model. Returns the exit code.
int describe_model_command(const std::string& aid, const std::filesystem::path& game_dir);
// --export-model <aid> --out <file.glb> --game-dir <dir>: the model as glTF 2.0. Returns the exit code.
// --export-textures <aid> --out <dir> --game-dir <dir>: every texture of the model as PNG, named by the materials and
// roles (colour, celmap, lightmap) that use it. Returns the exit code.
int export_textures_command(const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& out);
int export_model_command(const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& out);
// --import-model <file.glb> --as <aid> --plugin <dir> --game-dir <dir>: the glTF built on the disc model <aid> as a
// template, written to <dir>/aid/<aid> (with a plugin.ini when the folder has none). Returns the exit code.
int import_model_command(const std::filesystem::path& glb, const std::string& aid, const std::filesystem::path& game_dir,
    const std::filesystem::path& plugin);
}
#endif
