#pragma once
// Inspector tools for plugin scenes (docs/PLUGINS.md, "Scene recipes"): plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/scene_recipe.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace gbtg
{

// Opens disc bundles for recipes from a game folder, each once.
class RecipeBundleCache
{
public:
    explicit RecipeBundleCache(std::filesystem::path game_dir) : game_dir_(std::move(game_dir))
    {
    }
    const Bundle& open(const std::string& relative);
    RecipeBundles source()
    {
        return [this](const std::string& relative) -> const Bundle&
        {
            return open(relative);
        };
    }
    const std::filesystem::path& game_dir() const
    {
        return game_dir_;
    }

private:
    std::filesystem::path game_dir_;
    std::map<std::string, Bundle> bundles_;
    std::shared_ptr<SceneExecutableResources> executable_; // For the "default.xbe" objparams source.
};

// Every AID in the disc's bundles and the bundles that hold it, for adding parts by name.
class AidIndex
{
public:
    struct Location
    {
        std::string bundle; // Relative to bundles/.
        ResourceType type{};
    };
    // Reads `cache` when it exists, else scans <game>/bundles and writes it. Slow on a scan (seconds).
    void load(const std::filesystem::path& game_dir, const std::filesystem::path& cache);
    const std::vector<Location>* find(std::string_view aid) const;
    // Every AID, sorted.
    const std::vector<std::string>& names() const
    {
        return names_;
    }
    bool empty() const
    {
        return names_.empty();
    }

private:
    std::map<std::string, std::vector<Location>> locations_;
    std::vector<std::string> names_;
};

// The "aid_..." names written into a resource's bytes (script and marker strings, objparams references).
std::vector<std::string> embedded_aids(std::span<const u8> bytes);
// The resource this resource's bytes resolve through 0x0011CB80: "aid_script_x" -> "aid_marker_x".
std::string scene_marker_name(const std::string& scene);

// A recipe that remakes the disc scene `scene` (an aid_script_ name) under the name `as`: its resources as parts,
// its script nodes and marker rows copied one by one.
SceneRecipe recipe_from_scene(const std::string& scene, const std::string& as, RecipeBundleCache& bundles);

// --check-bundle-builder <bundles dir>: rebuilds every .bnl under the folder with BundleBuilder from its own entries
// and compares the four chunks byte for byte. Returns the process exit code.
int check_bundle_builder(const std::filesystem::path& bundles);
// --build-recipe <recipe> --game-dir <dir> --out <file.bnl>: assembles the recipe's bundle. Returns the exit code.
int build_recipe_file(const std::filesystem::path& recipe, const std::filesystem::path& game_dir,
    const std::filesystem::path& out);
// --recipe-from-scene <aid_script_...> --as <aid_script_...> --game-dir <dir> --out <file.scene>.
int write_recipe_from_scene(const std::string& scene, const std::string& as, const std::filesystem::path& game_dir,
    const std::filesystem::path& out);

} // namespace gbtg
#endif
