#pragma once
// Scene recipes: a plugin's custom bundle as text, assembled from the player's own disc when the game opens it
// (docs/PLUGINS.md, "Scene recipes"). A recipe names retail resources to copy (parts), and builds the scene's own
// script, markers and other resources from copied script nodes, marker rows and resources plus edits, or from new
// bytes. A recipe holds references and edits, not retail bytes. Non-retail builds only.
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/bundle.h"
#include "gbtg/types.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gbtg
{

// A resource in a disc bundle: `bundle` relative to bundles/ ("aid_script/ghoulies_chapter1_scene1_2playcam.bnl").
struct RecipeSource
{
    std::string bundle;
    std::string aid;
};

// `set <offset> <kind> <value>`: bytes written over an item, `offset` from the item's first byte. Kinds: u8, u16,
// u32, i32, f32, hex (bytes) and str (a NUL-terminated string).
struct RecipePatch
{
    u32 offset = 0;
    std::string kind;
    std::string value;
};

// A script node, a marker row, or a whole resource.
struct RecipeItem
{
    std::optional<RecipeSource> source; // copy: the resource it comes from
    u32 index = 0; // copy: the script node's index, or the marker row's id
    u32 type = 0; // new: the node type, marker kind or resource type
    std::vector<u8> data; // new: the bytes after the node's or row's 8-byte head, or the resource's bytes
    std::optional<u16> id; // markers: the row's id
    std::optional<Vec3> position; // markers: +0xC
    std::optional<Vec3> rotation; // markers: +0x18, Euler radians
    std::vector<RecipePatch> patches;
};

struct RecipeResource
{
    enum class Kind
    {
        Script, // type 24: nodes, then the {8, 0} terminator
        Markers, // type 12: rows, then the kind-0 terminator row
        Data, // any other: one item
    };
    Kind kind = Kind::Data;
    std::string aid;
    std::vector<RecipeItem> items;
};

struct SceneRecipe
{
    std::string bundle; // The bundle this recipe makes, relative to bundles/.
    // A disc scene whose transition and book clip the scene uses (the executable's scene directory, 0x00116220,
    // has no row for a plugin's scene); empty uses the directory's first row, as retail does for an unknown name.
    std::string transition;
    // Disc bundles the game opens with this one ([Bundle] Fallback): the resources gameplay asks for later than the
    // load are found there, as in the scene's own bundle.
    std::vector<std::string> fallbacks;
    std::vector<RecipeSource> parts; // Copied as they are.
    std::vector<RecipeResource> resources;
};

SceneRecipe read_scene_recipe(const std::filesystem::path& path);
SceneRecipe parse_scene_recipe(std::string_view text, const std::string& where);
void write_scene_recipe(const SceneRecipe& recipe, const std::filesystem::path& path);
std::string format_scene_recipe(const SceneRecipe& recipe);

// The source name "default.xbe" stands for the executable's objparams records (pools 3 and 5): a [Resource] named
// aid_objparams_... copied from it, with edits, joins the executable's objparams instead of the bundle.
inline constexpr const char* kExecutableSource = "default.xbe";
class SceneExecutableResources;
// The executable's objparams records as a bundle of CPU-only entries, for recipes to copy from.
Bundle executable_objparams_bundle(const SceneExecutableResources& executable);
bool recipe_objparams(const RecipeResource& resource);

// Opens a disc bundle by its path relative to bundles/. The bundle must stay valid while the recipe is built.
using RecipeBundles = std::function<const Bundle&(const std::string& relative)>;

// The resource's bytes (CPU only) and its type.
std::vector<u8> resolve_recipe_resource(const RecipeResource& resource, const RecipeBundles& bundles,
    ResourceType& type);
// One item's bytes: a script node or marker row with its head, or a data resource.
std::vector<u8> resolve_recipe_item(const RecipeResource& resource, const RecipeItem& item,
    const RecipeBundles& bundles);
Bundle build_scene_recipe(const SceneRecipe& recipe, const RecipeBundles& bundles);

// The nodes of a script blob and the rows of a marker blob, each with its head, without the terminator.
std::vector<std::span<const u8>> script_nodes(std::span<const u8> script);
std::vector<std::span<const u8>> marker_rows(std::span<const u8> markers);

} // namespace gbtg
#endif
