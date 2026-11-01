#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "scene_build_tool.h"

#include "../scene/executable_resources.h"
#include "gbtg/aid.h"
#include "gbtg/bundle_builder.h"
#include "gbtg/log.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace gbtg
{
namespace
{
std::string lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// "aid_script_x_y" -> "aid_marker_x_y", as 0x0011CB80 names a scene's markers.
std::string marker_name(const std::string& scene)
{
    const size_t underscore = scene.find('_', 4);
    return "aid_marker" + scene.substr(underscore);
}

std::string scene_bundle(const std::string& scene)
{
    const auto relative = aid_to_bundle_rel(scene);
    if (!relative || !scene.starts_with("aid_script_"))
    {
        throw Error(scene + " is not an aid_script_ name");
    }
    return *relative;
}
}

std::string scene_marker_name(const std::string& scene)
{
    return marker_name(scene);
}

std::vector<std::string> embedded_aids(std::span<const u8> bytes)
{
    std::vector<std::string> out;
    const auto name_char = [](u8 c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    };
    for (size_t at = 0; at + 4 < bytes.size(); ++at)
    {
        if (std::memcmp(bytes.data() + at, "aid_", 4) != 0 || (at > 0 && name_char(bytes[at - 1])))
        {
            continue;
        }
        size_t end = at;
        while (end < bytes.size() && name_char(bytes[end]))
        {
            ++end;
        }
        std::string name = lower(std::string_view(reinterpret_cast<const char*>(bytes.data() + at), end - at));
        if (std::count(name.begin(), name.end(), '_') >= 2 && std::find(out.begin(), out.end(), name) == out.end())
        {
            out.push_back(std::move(name));
        }
        at = end;
    }
    return out;
}

void AidIndex::load(const std::filesystem::path& game_dir, const std::filesystem::path& cache)
{
    locations_.clear();
    names_.clear();
    if (std::filesystem::exists(cache))
    {
        const std::vector<u8> bytes = read_file(cache);
        std::string text(bytes.begin(), bytes.end());
        size_t at = 0;
        while (at < text.size())
        {
            const size_t end = std::min(text.find('\n', at), text.size());
            const std::string line = text.substr(at, end - at);
            at = end + 1;
            const size_t first = line.find('\t');
            const size_t second = first == std::string::npos ? first : line.find('\t', first + 1);
            if (second == std::string::npos)
            {
                continue;
            }
            locations_[line.substr(0, first)].push_back(
                {line.substr(first + 1, second - first - 1), ResourceType(std::stoul(line.substr(second + 1)))});
        }
    }
    else
    {
        const std::filesystem::path root = game_dir / "bundles";
        for (const auto& file : std::filesystem::recursive_directory_iterator(root))
        {
            if (!file.is_regular_file() || lower(file.path().extension().string()) != ".bnl")
            {
                continue;
            }
            const std::string relative = file.path().lexically_relative(root).generic_string();
            const Bundle bundle = load_bundle(file.path());
            for (const BundleEntry& entry : bundle.entries)
            {
                locations_[lower(entry.name)].push_back({relative, entry.type});
            }
        }
        // The executable's objparams, which recipes copy from "default.xbe".
        const SceneExecutableResources executable(game_dir);
        for (const BundleEntry& entry : executable_objparams_bundle(executable).entries)
        {
            locations_[lower(entry.name)].push_back({kExecutableSource, entry.type});
        }
        std::string text;
        for (const auto& [aid, places] : locations_)
        {
            for (const Location& place : places)
            {
                text += aid + '\t' + place.bundle + '\t' + std::to_string(u32(place.type)) + '\n';
            }
        }
        std::filesystem::create_directories(cache.parent_path());
        write_file(cache, std::span<const u8>(reinterpret_cast<const u8*>(text.data()), text.size()));
    }
    for (const auto& [aid, places] : locations_)
    {
        names_.push_back(aid);
    }
}

const std::vector<AidIndex::Location>* AidIndex::find(std::string_view aid) const
{
    const auto found = locations_.find(lower(aid));
    return found == locations_.end() ? nullptr : &found->second;
}

const Bundle& RecipeBundleCache::open(const std::string& relative)
{
    const std::string key = lower(relative);
    auto found = bundles_.find(key);
    if (found == bundles_.end())
    {
        if (key == kExecutableSource)
        {
            executable_ = std::make_shared<SceneExecutableResources>(game_dir_);
            found = bundles_.emplace(key, executable_objparams_bundle(*executable_)).first;
        }
        else
        {
            found = bundles_.emplace(key, load_bundle(game_dir_ / "bundles" / std::filesystem::path(relative))).first;
        }
    }
    return found->second;
}

SceneRecipe recipe_from_scene(const std::string& scene, const std::string& as, RecipeBundleCache& bundles)
{
    const std::string source = scene_bundle(scene);
    const Bundle& bundle = bundles.open(source);
    const std::string markers = marker_name(scene);
    SceneRecipe recipe;
    recipe.bundle = scene_bundle(as);
    recipe.transition = scene;
    RecipeResource script;
    script.kind = RecipeResource::Kind::Script;
    script.aid = as;
    RecipeResource rows;
    rows.kind = RecipeResource::Kind::Markers;
    rows.aid = marker_name(as);
    for (const BundleEntry& entry : bundle.entries)
    {
        if (entry.type == ResourceType::Script && lower(entry.name) == lower(scene))
        {
            const auto nodes = script_nodes(entry.cpu);
            for (u32 index = 0; index < nodes.size(); ++index)
            {
                RecipeItem& item = script.items.emplace_back();
                item.source = RecipeSource{source, entry.name};
                item.index = index;
            }
        }
        else if (entry.type == ResourceType::Marker && lower(entry.name) == lower(markers))
        {
            for (const auto row : marker_rows(entry.cpu))
            {
                RecipeItem& item = rows.items.emplace_back();
                item.source = RecipeSource{source, entry.name};
                item.index = read_u16(row, 6);
            }
        }
        else
        {
            recipe.parts.push_back({source, entry.name});
        }
    }
    if (script.items.empty())
    {
        throw Error(source + " has no script " + scene);
    }
    recipe.resources.push_back(std::move(script));
    if (!rows.items.empty())
    {
        recipe.resources.push_back(std::move(rows));
    }
    return recipe;
}

int check_bundle_builder(const std::filesystem::path& bundles)
{
    size_t checked = 0;
    size_t failed = 0;
    for (const auto& file : std::filesystem::recursive_directory_iterator(bundles))
    {
        if (!file.is_regular_file() || lower(file.path().extension().string()) != ".bnl")
        {
            continue;
        }
        const Bundle original = load_bundle(file.path());
        BundleBuilder builder;
        for (const BundleEntry& entry : original.entries)
        {
            builder.add(entry);
        }
        const Bundle rebuilt = builder.build();
        const char* differs = rebuilt.file_count != original.file_count ? "file count"
            : rebuilt.file_table != original.file_table ? "file table"
            : rebuilt.res_meta != original.res_meta ? "GPU tables"
            : rebuilt.gpu_heap != original.gpu_heap ? "GPU heap"
            : rebuilt.cpu_heap != original.cpu_heap ? "CPU heap" : nullptr;
        ++checked;
        if (differs != nullptr)
        {
            ++failed;
            log::error("bundle builder: %s differs in its %s", file.path().string().c_str(), differs);
        }
    }
    log::info("bundle builder: %zu of %zu bundles rebuilt byte for byte", checked - failed, checked);
    return failed == 0 && checked != 0 ? 0 : 1;
}

int build_recipe_file(const std::filesystem::path& recipe, const std::filesystem::path& game_dir,
    const std::filesystem::path& out)
{
    RecipeBundleCache bundles(game_dir);
    const Bundle bundle = build_scene_recipe(read_scene_recipe(recipe), bundles.source());
    std::filesystem::create_directories(out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path());
    save_bundle(bundle, out);
    log::info("built %s: %u resources", out.string().c_str(), unsigned(bundle.file_count));
    return 0;
}

int write_recipe_from_scene(const std::string& scene, const std::string& as, const std::filesystem::path& game_dir,
    const std::filesystem::path& out)
{
    RecipeBundleCache bundles(game_dir);
    const SceneRecipe recipe = recipe_from_scene(lower(scene), lower(as), bundles);
    std::filesystem::create_directories(out.parent_path().empty() ? std::filesystem::path(".") : out.parent_path());
    write_scene_recipe(recipe, out);
    log::info("wrote %s: %zu parts, %zu resources", out.string().c_str(), recipe.parts.size(),
        recipe.resources.size());
    return 0;
}

} // namespace gbtg
#endif
