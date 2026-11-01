#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "scene_builder.h"

#include "../plugins/plugins.h"
#include "gbtg/aid.h"
#include "gbtg/log.h"
#include "imgui.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr int kMaxResolveRounds = 400;

std::string lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

void copy_text(char* out, size_t size, const std::string& text)
{
    std::snprintf(out, size, "%s", text.c_str());
}

const char* marker_kind_name(u16 kind)
{
    switch (kind)
    {
    case 2: return "player start";
    case 3: return "prop";
    case 4: return "spawned actor";
    case 5: return "modified template";
    case 6: return "template object";
    case 7: return "chain name";
    case 8: return "sphere";
    case 0xA: return "camera";
    case 0xB: return "trigger";
    case 0xD: return "effect";
    case 0xE: return "named object";
    case 0xF: return "switch";
    case 0x10: return "area";
    case 0x11: return "named trigger";
    case 0x12: return "unused";
    case 0x13: return "camera start";
    case 0x14: return "box";
    case 0x15: return "multi name";
    case 0x16: return "scare";
    case 0x17: return "dropper";
    case 0x18: return "template object";
    case 0x19: return "actor services";
    default: return "kind";
    }
}

const char* node_type_name(u32 type)
{
    switch (type)
    {
    case 0x01: return "model";
    case 0x02: return "objparams";
    case 0x0A: return "region name";
    case 0x0E: return "event title";
    case 0x13: return "cutscene";
    case 0x18: return "body";
    case 0x29: return "body";
    case 0x2A: return "ghouly box";
    case 0x3C: return "end of block";
    case 0x3D: return "timeout";
    case 0x9F: return "actor tag";
    default: return "";
    }
}

// The first printable string in a node or row, for its label.
std::string first_text(std::span<const u8> bytes, size_t from)
{
    for (size_t at = from; at < bytes.size(); ++at)
    {
        size_t end = at;
        while (end < bytes.size() && bytes[end] >= 0x20 && bytes[end] < 0x7F)
        {
            ++end;
        }
        if (end - at >= 4 && end < bytes.size() && bytes[end] == 0)
        {
            return std::string(reinterpret_cast<const char*>(bytes.data() + at), end - at);
        }
        at = end;
    }
    return {};
}

float read_float(std::span<const u8> bytes, size_t at)
{
    return std::bit_cast<float>(read_u32(bytes, at));
}

void hex_view(std::span<const u8> bytes)
{
    ImGui::BeginChild("hex", {0.f, 160.f}, ImGuiChildFlags_Borders);
    for (size_t row = 0; row < bytes.size(); row += 16)
    {
        char line[128];
        int used = std::snprintf(line, sizeof(line), "%04zx ", row);
        for (size_t column = row; column < row + 16 && column < bytes.size(); ++column)
        {
            used += std::snprintf(line + used, sizeof(line) - size_t(used), " %02x", bytes[column]);
        }
        ImGui::TextUnformatted(line);
    }
    ImGui::EndChild();
}

template <class T>
void move_item(std::vector<T>& items, int& selected, int by)
{
    const int target = selected + by;
    if (selected < 0 || target < 0 || target >= int(items.size()))
    {
        return;
    }
    std::swap(items[size_t(selected)], items[size_t(target)]);
    selected = target;
}
}

SceneBuilder::SceneBuilder(std::filesystem::path game_dir, std::filesystem::path plugin_dir)
    : game_dir_(std::move(game_dir)), bundles_(game_dir_)
{
    copy_text(plugin_dir_, sizeof(plugin_dir_), plugin_dir.string());
}

std::string SceneBuilder::scene_name() const
{
    for (const RecipeResource& resource : recipe_.resources)
    {
        if (resource.kind == RecipeResource::Kind::Script)
        {
            return resource.aid;
        }
    }
    return {};
}

RecipeResource& SceneBuilder::resource_of(RecipeResource::Kind kind, const std::string& aid)
{
    for (RecipeResource& resource : recipe_.resources)
    {
        if (resource.kind == kind && (kind != RecipeResource::Kind::Data || lower(resource.aid) == lower(aid)))
        {
            return resource;
        }
    }
    RecipeResource& added = recipe_.resources.emplace_back();
    added.kind = kind;
    added.aid = aid;
    return added;
}

bool SceneBuilder::in_recipe(const std::string& aid) const
{
    const std::string key = lower(aid);
    for (const RecipeSource& part : recipe_.parts)
    {
        if (lower(part.aid) == key)
        {
            return true;
        }
    }
    for (const RecipeResource& resource : recipe_.resources)
    {
        if (lower(resource.aid) == key)
        {
            return true;
        }
    }
    return false;
}

bool SceneBuilder::add_part(const std::string& aid)
{
    if (in_recipe(aid) || index_.empty())
    {
        return false;
    }
    const auto* places = index_.find(aid);
    if (places == nullptr || places->empty())
    {
        return false;
    }
    // common.bnl is always loaded and the executable's objparams always registered; neither needs a part.
    for (const auto& place : *places)
    {
        if (lower(place.bundle) == "common.bnl" || lower(place.bundle) == kExecutableSource)
        {
            return false;
        }
    }
    const AidIndex::Location* chosen = &places->front();
    for (const auto& place : *places)
    {
        if (std::any_of(recipe_.parts.begin(), recipe_.parts.end(), [&](const RecipeSource& part)
        {
            return lower(part.bundle) == lower(place.bundle);
        }))
        {
            chosen = &place;
            break;
        }
    }
    const BundleEntry* entry = find_entry(bundles_.open(chosen->bundle), aid);
    recipe_.parts.push_back({chosen->bundle, entry != nullptr ? entry->name : aid});
    dirty_ = true;
    return true;
}

size_t SceneBuilder::add_embedded_parts()
{
    size_t added = 0;
    // Each added part may name more; repeat until nothing new appears.
    for (bool more = true; more;)
    {
        more = false;
        std::vector<std::string> named;
        for (const RecipeSource& part : recipe_.parts)
        {
            const BundleEntry* entry = find_entry(bundles_.open(part.bundle), part.aid);
            if (entry != nullptr)
            {
                const auto aids = embedded_aids(entry->cpu);
                named.insert(named.end(), aids.begin(), aids.end());
            }
        }
        for (const RecipeResource& resource : recipe_.resources)
        {
            ResourceType type{};
            try
            {
                const auto aids = embedded_aids(resolve_recipe_resource(resource, bundles_.source(), type));
                named.insert(named.end(), aids.begin(), aids.end());
            }
            catch (const std::exception&)
            {
            }
        }
        for (const std::string& aid : named)
        {
            if (add_part(aid))
            {
                ++added;
                more = true;
            }
        }
    }
    return added;
}

void SceneBuilder::update_fallbacks()
{
    // Gameplay asks for resources later than the load (an attack's animation, an event's cutscene): the game opens
    // the parts' bundles with the scene so it finds them there.
    std::vector<std::string> bundles;
    for (const RecipeSource& part : recipe_.parts)
    {
        const std::string bundle = lower(part.bundle);
        if (bundle != kExecutableSource && std::find(bundles.begin(), bundles.end(), bundle) == bundles.end())
        {
            bundles.push_back(bundle);
        }
    }
    if (bundles != recipe_.fallbacks)
    {
        recipe_.fallbacks = std::move(bundles);
        dirty_ = true;
    }
}

void SceneBuilder::new_scene(const std::string& scene, bool from_scene, const std::string& source)
{
    const std::string name = lower(scene);
    if (!name.starts_with("aid_script_") || name.size() >= 0x80)
    {
        status_ = "A scene name is aid_script_<name>, under 128 characters.";
        return;
    }
    try
    {
        if (from_scene)
        {
            recipe_ = recipe_from_scene(lower(source), name, bundles_);
        }
        else
        {
            recipe_ = {};
            recipe_.bundle = *aid_to_bundle_rel(name);
            resource_of(RecipeResource::Kind::Script, name);
            resource_of(RecipeResource::Kind::Markers, scene_marker_name(name));
        }
        has_recipe_ = true;
        dirty_ = true;
        const std::string file = name.substr(std::strlen("aid_script_")) + ".scene";
        copy_text(recipe_file_, sizeof(recipe_file_),
            (std::filesystem::path(plugin_dir_) / "scenes" / file).string());
        status_ = from_scene ? "Remade " + source + " by reference as " + name : "New empty scene " + name;
    }
    catch (const std::exception& error)
    {
        status_ = error.what();
    }
}

void SceneBuilder::open_recipe(const std::filesystem::path& path)
{
    try
    {
        recipe_ = read_scene_recipe(path);
        has_recipe_ = true;
        dirty_ = false;
        copy_text(recipe_file_, sizeof(recipe_file_), path.string());
        status_ = "Opened " + path.string();
    }
    catch (const std::exception& error)
    {
        status_ = error.what();
    }
}

bool SceneBuilder::save()
{
    try
    {
        // While resolving, no fallbacks: a resource the load misses must show, to become a part.
        if (use_fallbacks_ && resolve_rounds_ == 0)
        {
            update_fallbacks();
        }
        else if (!recipe_.fallbacks.empty())
        {
            recipe_.fallbacks.clear();
        }
        const std::filesystem::path path = recipe_file_;
        std::filesystem::create_directories(path.parent_path());
        write_scene_recipe(recipe_, path);
        dirty_ = false;
        status_ = "Saved " + path.string();
        return true;
    }
    catch (const std::exception& error)
    {
        status_ = error.what();
        return false;
    }
}

void SceneBuilder::resolve()
{
    if (index_.empty())
    {
        index_.load(game_dir_, "cache/aid_index.tsv");
    }
    resolve_rounds_ = kMaxResolveRounds;
    log::info("scene builder: %zu parts the recipe names", add_embedded_parts());
    build_and_load();
}

bool SceneBuilder::build_and_load()
{
    const std::string scene = scene_name();
    if (scene.empty())
    {
        status_ = "The recipe has no [Script] resource, so it is not a scene.";
        return false;
    }
    if (!save())
    {
        return false;
    }
    try
    {
        // Check that it assembles before restarting the runtime on it.
        const Bundle bundle = build_scene_recipe(recipe_, bundles_.source());
        plugin_register_recipe(recipe_file_);
        restart_scene = scene;
        requested_ = scene;
        waiting_ = true;
        status_ = "Built " + std::to_string(bundle.file_count) + " resources; loading " + scene;
        return true;
    }
    catch (const std::exception& error)
    {
        status_ = error.what();
        return false;
    }
}

void SceneBuilder::export_bundle()
{
    try
    {
        const Bundle bundle = build_scene_recipe(recipe_, bundles_.source());
        const std::filesystem::path out = std::filesystem::path(plugin_dir_) / "bundles" / recipe_.bundle;
        std::filesystem::create_directories(out.parent_path());
        save_bundle(bundle, out);
        status_ = "Exported " + out.string() + ". It holds disc resources: keep it local, do not share or commit it.";
    }
    catch (const std::exception& error)
    {
        status_ = error.what();
    }
}

void SceneBuilder::after_load(InspectorSession& session)
{
    // While resolving, a marker's missing resource fails the load so the next build can add it.
    session.fail_on_marker_errors = resolve_rounds_ > 0;
    if (!waiting_ || session.state == InspectionState::Loading || !restart_scene.empty())
    {
        return;
    }
    waiting_ = false;
    if (session.state == InspectionState::Ready)
    {
        status_ = "Loaded " + requested_ + ".";
        const bool resolved = resolve_rounds_ > 0;
        resolve_rounds_ = 0;
        if (dirty_ || resolved)
        {
            save();
            status_ += resolved ? " Saved the parts it needed and its fallbacks." : " Saved.";
        }
        log::info("scene builder: %s", status_.c_str());
        resolve_rounds_ = 0;
        return;
    }
    // "... last missing resource: <aid>" from the scene's resource error.
    static constexpr std::string_view kMissing = "last missing resource: ";
    const size_t at = session.error.find(kMissing);
    std::string missing;
    if (at != std::string::npos)
    {
        const size_t from = at + kMissing.size();
        size_t end = from;
        while (end < session.error.size() && (std::isalnum(static_cast<unsigned char>(session.error[end]))
            || session.error[end] == '_'))
        {
            ++end;
        }
        missing = session.error.substr(from, end - from);
    }
    if (resolve_rounds_ > 0 && !missing.empty() && add_part(missing))
    {
        --resolve_rounds_;
        status_ = "Added the missing " + missing + "; loading again.";
        log::info("scene builder: added %s", missing.c_str());
        add_embedded_parts();
        build_and_load();
        return;
    }
    if (resolve_rounds_ > 0 && !missing.empty())
    {
        log::warn("scene builder: %s is not in the AID index outside common.bnl and the recipe", missing.c_str());
    }
    status_ = missing.empty() ? "The scene failed to load: " + session.error
        : "The scene needs " + missing + ", which the AID index " + (index_.find(missing) ? "has in common.bnl or "
            "the recipe already" : "does not have") + ".";
    resolve_rounds_ = 0;
}

void SceneBuilder::draw(InspectorSession& session, bool& open)
{
    after_load(session);
    if (indexing_.valid() && indexing_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try
        {
            index_ = indexing_.get();
            status_ = "AID index: " + std::to_string(index_.names().size()) + " resources.";
        }
        catch (const std::exception& error)
        {
            status_ = std::string("AID index: ") + error.what();
        }
    }
    if (index_.empty() && !indexing_.valid())
    {
        const std::filesystem::path game = game_dir_;
        indexing_ = std::async(std::launch::async, [game]()
        {
            AidIndex index;
            index.load(game, "cache/aid_index.tsv");
            return index;
        });
        status_ = "Indexing the disc's bundles (cache/aid_index.tsv)...";
    }
    ImGui::SetNextWindowSize({760.f, 640.f}, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scene builder", &open))
    {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped("%s", status_.c_str());
    if (has_recipe_)
    {
        ImGui::Text("%s%s | %zu parts | bundle %s", scene_name().c_str(), dirty_ ? " (unsaved)" : "",
            recipe_.parts.size(), recipe_.bundle.c_str());
    }
    if (ImGui::BeginTabBar("builder-tabs"))
    {
        if (ImGui::BeginTabItem("Scene"))
        {
            draw_scene_tab(session);
            ImGui::EndTabItem();
        }
        if (has_recipe_)
        {
            if (ImGui::BeginTabItem("Parts"))
            {
                draw_parts_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Markers"))
            {
                draw_markers_tab(session);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Script"))
            {
                draw_script_tab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Resources"))
            {
                draw_resources_tab();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void SceneBuilder::draw_scene_tab(InspectorSession&)
{
    ImGui::InputText("Plugin folder", plugin_dir_, sizeof(plugin_dir_));
    ImGui::SeparatorText("New scene");
    ImGui::InputText("Scene name", new_name_, sizeof(new_name_));
    if (ImGui::Button("New empty scene"))
    {
        new_scene(new_name_, false, {});
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(320.f);
    ImGui::InputText("##from", from_scene_, sizeof(from_scene_));
    ImGui::SameLine();
    if (ImGui::Button("New from disc scene"))
    {
        new_scene(new_name_, true, from_scene_);
    }
    ImGui::TextWrapped("An empty scene starts with no script nodes, markers or parts: add a level model and actors "
        "from any disc scene in the other tabs. A scene from the disc copies every part, node and marker of that "
        "scene by reference, to edit.");
    ImGui::SeparatorText("Recipe file");
    ImGui::InputText("Recipe", recipe_file_, sizeof(recipe_file_));
    if (ImGui::Button("Open"))
    {
        open_recipe(recipe_file_);
    }
    if (!has_recipe_)
    {
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Build & load"))
    {
        resolve_rounds_ = 0;
        build_and_load();
    }
    ImGui::SameLine();
    if (ImGui::Button("Build, load and resolve parts"))
    {
        resolve();
    }
    if (ImGui::Button("Add parts the recipe names"))
    {
        status_ = "Added " + std::to_string(add_embedded_parts()) + " parts named in the recipe's resources.";
    }
    ImGui::SameLine();
    if (ImGui::Button("Export built .bnl"))
    {
        export_bundle();
    }
    ImGui::Checkbox("Open the parts' bundles with the scene (Fallback)", &use_fallbacks_);
    ImGui::TextWrapped("Gameplay asks for some resources only later (an attack's animation, an event's cutscene). "
        "With Fallback the game opens the bundles the parts come from with the scene and finds them there; without "
        "it, every resource the scene ever uses must be a part.");
    ImGui::TextWrapped("Build, load and resolve parts loads the scene in the inspector; each resource the native "
        "loader misses is added from the disc and the scene loads again, up to %d times. The recipe holds only "
        "references and edits; an exported .bnl holds disc resources, so keep it local.", kMaxResolveRounds);
}

void SceneBuilder::draw_source_browser(ResourceType type, char* filter, size_t filter_size, std::string& chosen)
{
    ImGui::InputText("Find", filter, filter_size);
    ImGui::BeginChild("sources", {0.f, 110.f}, ImGuiChildFlags_Borders);
    const std::string wanted = lower(filter);
    int shown = 0;
    for (const std::string& aid : index_.names())
    {
        if (aid.find(wanted) == std::string::npos)
        {
            continue;
        }
        const auto* places = index_.find(aid);
        if (places == nullptr || places->front().type != type)
        {
            continue;
        }
        if (++shown > 400)
        {
            ImGui::TextUnformatted("... narrow the search");
            break;
        }
        if (ImGui::Selectable(aid.c_str(), chosen == aid))
        {
            chosen = aid;
        }
    }
    ImGui::EndChild();
}

void SceneBuilder::draw_parts_tab()
{
    ImGui::TextWrapped("Parts are disc resources copied into the scene as they are: its level model, actors' "
        "models, animations, goals and attributes, effects, cutscenes. The native loader finds each by name.");
    ImGui::BeginChild("parts", {0.f, 200.f}, ImGuiChildFlags_Borders);
    for (int index = 0; index < int(recipe_.parts.size()); ++index)
    {
        const RecipeSource& part = recipe_.parts[size_t(index)];
        const std::string label = part.aid + "  (" + part.bundle + ")##" + std::to_string(index);
        if (ImGui::Selectable(label.c_str(), selected_part_ == index))
        {
            selected_part_ = index;
        }
    }
    ImGui::EndChild();
    if (selected_part_ >= 0 && selected_part_ < int(recipe_.parts.size()) && ImGui::Button("Remove part"))
    {
        recipe_.parts.erase(recipe_.parts.begin() + selected_part_);
        selected_part_ = -1;
        dirty_ = true;
    }
    ImGui::SeparatorText("Add from the disc");
    static const char* kTypes[] = {"any", "model", "anim", "texture", "actor goals", "actor attribs", "callout",
        "cutscene", "ghouly box", "emitter", "particle", "rumble", "aidlist", "objparams (misc)"};
    static const u32 kTypeIds[] = {0, 4, 2, 1, 11, 25, 13, 7, 22, 26, 27, 28, 14, 10};
    ImGui::SetNextItemWidth(160.f);
    ImGui::Combo("Type", &part_type_, kTypes, IM_ARRAYSIZE(kTypes));
    ImGui::SameLine();
    ImGui::InputText("Find##parts", part_filter_, sizeof(part_filter_));
    ImGui::BeginChild("index", {0.f, 200.f}, ImGuiChildFlags_Borders);
    if (index_.empty())
    {
        ImGui::TextUnformatted("Indexing...");
    }
    const std::string wanted = lower(part_filter_);
    int shown = 0;
    for (const std::string& aid : index_.names())
    {
        if (wanted.size() < 3 || aid.find(wanted) == std::string::npos)
        {
            continue;
        }
        const auto* places = index_.find(aid);
        if (part_type_ != 0 && u32(places->front().type) != kTypeIds[part_type_])
        {
            continue;
        }
        if (++shown > 400)
        {
            ImGui::TextUnformatted("... narrow the search");
            break;
        }
        ImGui::PushID(aid.c_str());
        const bool present = in_recipe(aid);
        ImGui::BeginDisabled(present);
        if (ImGui::SmallButton(present ? "added" : "add"))
        {
            add_part(aid);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("%s  (%s%s)", aid.c_str(), places->front().bundle.c_str(), places->size() > 1 ? ", ..." : "");
        ImGui::PopID();
    }
    if (wanted.size() < 3)
    {
        ImGui::TextUnformatted("Type at least three letters, e.g. background, impstandard, butler.");
    }
    ImGui::EndChild();
}

void SceneBuilder::draw_item_editor(RecipeResource& resource, RecipeItem& item, bool marker)
{
    std::vector<u8> bytes;
    try
    {
        bytes = resolve_recipe_item(resource, item, bundles_.source());
    }
    catch (const std::exception& error)
    {
        ImGui::TextWrapped("%s", error.what());
    }
    if (item.source)
    {
        ImGui::TextWrapped("Copied from %s %s %u", item.source->bundle.c_str(), item.source->aid.c_str(), item.index);
    }
    else
    {
        ImGui::Text("New, type 0x%x, %zu bytes", item.type, item.data.size());
    }
    if (marker && bytes.size() >= 0x24)
    {
        int id = read_u16(bytes, 6);
        if (ImGui::InputInt("Id", &id))
        {
            item.id = u16(std::clamp(id, 1, 0xFFFF));
            dirty_ = true;
        }
        float position[3] = {read_float(bytes, 0xC), read_float(bytes, 0x10), read_float(bytes, 0x14)};
        if (ImGui::DragFloat3("Position", position, 0.05f))
        {
            item.position = Vec3{position[0], position[1], position[2]};
            dirty_ = true;
        }
        float rotation[3] = {read_float(bytes, 0x18), read_float(bytes, 0x1C), read_float(bytes, 0x20)};
        if (ImGui::DragFloat3("Rotation (radians)", rotation, 0.01f))
        {
            item.rotation = Vec3{rotation[0], rotation[1], rotation[2]};
            dirty_ = true;
        }
    }
    ImGui::SeparatorText("Field edits (set)");
    for (size_t index = 0; index < item.patches.size(); ++index)
    {
        RecipePatch& patch = item.patches[index];
        ImGui::PushID(int(index));
        int offset = int(patch.offset);
        ImGui::SetNextItemWidth(90.f);
        if (ImGui::InputInt("##offset", &offset, 1, 4, ImGuiInputTextFlags_CharsHexadecimal))
        {
            patch.offset = u32(std::max(0, offset));
            dirty_ = true;
        }
        ImGui::SameLine();
        static const char* kKinds[] = {"u8", "u16", "u32", "i32", "f32", "hex", "str"};
        int kind = int(std::find(std::begin(kKinds), std::end(kKinds), patch.kind) - std::begin(kKinds));
        ImGui::SetNextItemWidth(70.f);
        if (ImGui::Combo("##kind", &kind, kKinds, IM_ARRAYSIZE(kKinds)))
        {
            patch.kind = kKinds[kind];
            dirty_ = true;
        }
        ImGui::SameLine();
        char value[256];
        copy_text(value, sizeof(value), patch.value);
        ImGui::SetNextItemWidth(260.f);
        if (ImGui::InputText("##value", value, sizeof(value)))
        {
            patch.value = value;
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("x"))
        {
            item.patches.erase(item.patches.begin() + std::ptrdiff_t(index));
            dirty_ = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("Add field edit"))
    {
        item.patches.push_back({marker ? 0x24u : 8u, "u32", "0"});
        dirty_ = true;
    }
    ImGui::TextWrapped("Offsets count from the item's first byte (its size word). A str edit writes the text and a "
        "zero; the item keeps its size.");
    if (!bytes.empty())
    {
        hex_view(bytes);
    }
}

void SceneBuilder::draw_markers_tab(InspectorSession& session)
{
    RecipeResource& markers = resource_of(RecipeResource::Kind::Markers, scene_marker_name(scene_name()));
    ImGui::Text("%s: %zu rows", markers.aid.c_str(), markers.items.size());
    ImGui::BeginChild("rows", {0.f, 170.f}, ImGuiChildFlags_Borders);
    for (int index = 0; index < int(markers.items.size()); ++index)
    {
        std::vector<u8> bytes;
        try
        {
            bytes = resolve_recipe_item(markers, markers.items[size_t(index)], bundles_.source());
        }
        catch (const std::exception&)
        {
        }
        char label[200];
        if (bytes.size() >= 0x18)
        {
            const u16 kind = read_u16(bytes, 4);
            std::snprintf(label, sizeof(label), "#%u  %s (0x%x)  at %.1f %.1f %.1f  %s##%d", read_u16(bytes, 6),
                marker_kind_name(kind), kind, read_float(bytes, 0xC), read_float(bytes, 0x10), read_float(bytes, 0x14),
                first_text(bytes, 0x24).c_str(), index);
        }
        else
        {
            std::snprintf(label, sizeof(label), "(unresolved)##%d", index);
        }
        if (ImGui::Selectable(label, selected_row_ == index))
        {
            selected_row_ = index;
        }
    }
    ImGui::EndChild();
    if (selected_row_ >= 0 && selected_row_ < int(markers.items.size()))
    {
        if (ImGui::Button("Up"))
        {
            move_item(markers.items, selected_row_, -1);
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Down"))
        {
            move_item(markers.items, selected_row_, 1);
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate"))
        {
            RecipeItem copy = markers.items[size_t(selected_row_)];
            u16 next = 1;
            for (const RecipeItem& item : markers.items)
            {
                try
                {
                    next = std::max<u16>(next, u16(read_u16(resolve_recipe_item(markers, item, bundles_.source()), 6) + 1));
                }
                catch (const std::exception&)
                {
                }
            }
            copy.id = next;
            markers.items.insert(markers.items.begin() + selected_row_ + 1, std::move(copy));
            ++selected_row_;
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Place at inspector camera"))
        {
            markers.items[size_t(selected_row_)].position = session.camera.position;
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            markers.items.erase(markers.items.begin() + selected_row_);
            selected_row_ = -1;
            dirty_ = true;
        }
        if (selected_row_ >= 0)
        {
            ImGui::PushID("row");
            draw_item_editor(markers, markers.items[size_t(selected_row_)], true);
            ImGui::PopID();
        }
    }
    ImGui::SeparatorText("Add a marker row from a disc scene");
    draw_source_browser(ResourceType::Marker, marker_source_filter_, sizeof(marker_source_filter_), marker_source_);
    if (marker_source_.empty() || index_.find(marker_source_) == nullptr)
    {
        return;
    }
    const std::string bundle = index_.find(marker_source_)->front().bundle;
    const BundleEntry* entry = find_entry(bundles_.open(bundle), marker_source_);
    if (entry == nullptr)
    {
        return;
    }
    ImGui::BeginChild("source-rows", {0.f, 150.f}, ImGuiChildFlags_Borders);
    for (const auto row : marker_rows(entry->cpu))
    {
        const u16 id = read_u16(row, 6);
        const u16 kind = read_u16(row, 4);
        ImGui::PushID(id);
        if (ImGui::SmallButton("add"))
        {
            RecipeItem& item = markers.items.emplace_back();
            item.source = RecipeSource{bundle, entry->name};
            item.index = id;
            selected_row_ = int(markers.items.size()) - 1;
            dirty_ = true;
        }
        ImGui::SameLine();
        if (row.size() >= 0x18)
        {
            ImGui::Text("#%u %s (0x%x) at %.1f %.1f %.1f %s", id, marker_kind_name(kind), kind, read_float(row, 0xC),
                read_float(row, 0x10), read_float(row, 0x14), first_text(row, 0x24).c_str());
        }
        else
        {
            ImGui::Text("#%u %s (0x%x)", id, marker_kind_name(kind), kind);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::TextWrapped("A copied row keeps its id; give it a free one when the scene already has that id. Then add "
        "the parts it names (Scene tab) or let Build, load and resolve find them.");
}

void SceneBuilder::draw_script_tab()
{
    RecipeResource& script = resource_of(RecipeResource::Kind::Script, scene_name());
    ImGui::Text("%s: %zu nodes", script.aid.c_str(), script.items.size());
    ImGui::BeginChild("nodes", {0.f, 200.f}, ImGuiChildFlags_Borders);
    for (int index = 0; index < int(script.items.size()); ++index)
    {
        std::vector<u8> bytes;
        try
        {
            bytes = resolve_recipe_item(script, script.items[size_t(index)], bundles_.source());
        }
        catch (const std::exception&)
        {
        }
        char label[220];
        if (bytes.size() >= 8)
        {
            const u32 type = read_u32(bytes, 4);
            std::snprintf(label, sizeof(label), "%3d  type 0x%02x %-12s %4zu bytes  %s##%d", index, type,
                node_type_name(type), bytes.size(), first_text(bytes, 8).c_str(), index);
        }
        else
        {
            std::snprintf(label, sizeof(label), "%3d  (unresolved)##%d", index, index);
        }
        if (ImGui::Selectable(label, selected_node_ == index))
        {
            selected_node_ = index;
        }
    }
    ImGui::EndChild();
    if (selected_node_ >= 0 && selected_node_ < int(script.items.size()))
    {
        if (ImGui::Button("Up"))
        {
            move_item(script.items, selected_node_, -1);
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Down"))
        {
            move_item(script.items, selected_node_, 1);
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate"))
        {
            script.items.insert(script.items.begin() + selected_node_ + 1, script.items[size_t(selected_node_)]);
            ++selected_node_;
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete"))
        {
            script.items.erase(script.items.begin() + selected_node_);
            selected_node_ = -1;
            dirty_ = true;
        }
        if (selected_node_ >= 0)
        {
            ImGui::PushID("node");
            draw_item_editor(script, script.items[size_t(selected_node_)], false);
            ImGui::PopID();
        }
    }
    ImGui::SeparatorText("Add a node");
    ImGui::SetNextItemWidth(100.f);
    ImGui::InputInt("Type", &new_type_, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300.f);
    ImGui::InputText("Bytes (hex)", new_hex_, sizeof(new_hex_));
    ImGui::SameLine();
    if (ImGui::Button("Add new node"))
    {
        try
        {
            const SceneRecipe parsed = parse_scene_recipe(std::string("[Bundle]\nPath = x\n[Script x]\nnew ")
                + std::to_string(new_type_) + (new_hex_[0] != 0 ? std::string(" ") + new_hex_ : ""), "new node");
            script.items.push_back(parsed.resources[0].items[0]);
            selected_node_ = int(script.items.size()) - 1;
            dirty_ = true;
        }
        catch (const std::exception& error)
        {
            status_ = error.what();
        }
    }
    ImGui::TextUnformatted("Or copy one from a disc scene's script:");
    draw_source_browser(ResourceType::Script, node_source_filter_, sizeof(node_source_filter_), node_source_);
    if (node_source_.empty() || index_.find(node_source_) == nullptr)
    {
        return;
    }
    const std::string bundle = index_.find(node_source_)->front().bundle;
    const BundleEntry* entry = find_entry(bundles_.open(bundle), node_source_);
    if (entry == nullptr)
    {
        return;
    }
    ImGui::BeginChild("source-nodes", {0.f, 150.f}, ImGuiChildFlags_Borders);
    const auto nodes = script_nodes(entry->cpu);
    for (u32 index = 0; index < nodes.size(); ++index)
    {
        const u32 type = read_u32(nodes[index], 4);
        ImGui::PushID(int(index));
        if (ImGui::SmallButton("add"))
        {
            RecipeItem& item = script.items.emplace_back();
            item.source = RecipeSource{bundle, entry->name};
            item.index = index;
            selected_node_ = int(script.items.size()) - 1;
            dirty_ = true;
        }
        ImGui::SameLine();
        ImGui::Text("%3u type 0x%02x %-12s %4zu bytes %s", index, type, node_type_name(type), nodes[index].size(),
            first_text(nodes[index], 8).c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void SceneBuilder::draw_resources_tab()
{
    ImGui::TextWrapped("Resources the scene owns besides its script and markers, such as a copy of a scene "
        "control's objparams with edited fields. Name it anew and point the script's node at the new name with a "
        "str edit.");
    std::vector<RecipeResource*> data;
    for (RecipeResource& resource : recipe_.resources)
    {
        if (resource.kind == RecipeResource::Kind::Data)
        {
            data.push_back(&resource);
        }
    }
    ImGui::BeginChild("resources", {0.f, 120.f}, ImGuiChildFlags_Borders);
    for (int index = 0; index < int(data.size()); ++index)
    {
        if (ImGui::Selectable((data[size_t(index)]->aid + "##" + std::to_string(index)).c_str(),
            selected_resource_ == index))
        {
            selected_resource_ = index;
        }
    }
    ImGui::EndChild();
    if (selected_resource_ >= 0 && selected_resource_ < int(data.size()))
    {
        RecipeResource& resource = *data[size_t(selected_resource_)];
        char name[128];
        copy_text(name, sizeof(name), resource.aid);
        if (ImGui::InputText("Name", name, sizeof(name)))
        {
            resource.aid = lower(name);
            dirty_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete resource"))
        {
            const RecipeResource* target = &resource;
            recipe_.resources.erase(std::find_if(recipe_.resources.begin(), recipe_.resources.end(),
                [target](const RecipeResource& candidate)
            {
                return &candidate == target;
            }));
            selected_resource_ = -1;
            dirty_ = true;
            return;
        }
        if (!resource.items.empty())
        {
            ImGui::PushID("resource");
            draw_item_editor(resource, resource.items.front(), false);
            ImGui::PopID();
        }
    }
    ImGui::SeparatorText("Copy a disc resource to edit");
    draw_source_browser(ResourceType::Misc, resource_filter_, sizeof(resource_filter_), resource_source_);
    if (!resource_source_.empty() && index_.find(resource_source_) != nullptr && ImGui::Button("Copy as a new resource"))
    {
        const std::string bundle = index_.find(resource_source_)->front().bundle;
        const BundleEntry* entry = find_entry(bundles_.open(bundle), resource_source_);
        RecipeResource& added = recipe_.resources.emplace_back();
        added.kind = RecipeResource::Kind::Data;
        added.aid = resource_source_ + "_" + lower(scene_name().substr(std::strlen("aid_script_")));
        RecipeItem& item = added.items.emplace_back();
        item.source = RecipeSource{bundle, entry != nullptr ? entry->name : resource_source_};
        selected_resource_ = int(data.size());
        dirty_ = true;
    }
}

} // namespace gbtg
#endif
