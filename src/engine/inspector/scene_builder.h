#pragma once
// The inspector's scene builder (docs/INSPECTOR.md, "Scene builder"): makes and edits a plugin's scene recipe
// (docs/PLUGINS.md, "Scene recipes") from parts of the disc's scenes, builds it from the disc and loads it in the
// inspector. Plugin builds only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "inspector_session.h"
#include "inspector_ui.h"
#include "scene_build_tool.h"

#include <deque>
#include <future>

namespace gbtg
{

// Also the viewport's scene editor (InspectorEditor): it edits the open recipe's markers when the loaded scene is the
// recipe's.
class SceneBuilder : public InspectorEditor
{
public:
    SceneBuilder(std::filesystem::path game_dir, std::filesystem::path plugin_dir);
    // Draws the window while `open`. A Build & Load sets restart_scene; the inspector's loop restarts the session on
    // it and clears it.
    void draw(InspectorSession& session, bool& open);
    std::string restart_scene;
    // Opens an existing recipe file.
    void open_recipe(const std::filesystem::path& path);
    // Build, load and resolve parts, as the Scene tab's button does (--resolve).
    void resolve();
    std::string scene_name() const;

    // InspectorEditor.
    void frame(InspectorSession& session) override;
    bool editing(const InspectorSession& session) const override;
    void start_editing(InspectorSession& session) override;
    bool set_marker_pose(const InspectionItem& item) override;
    u16 duplicate_marker(InspectorSession& session, const InspectionItem& item, const Vec3& position) override;
    bool delete_marker(InspectorSession& session, const InspectionItem& item) override;
    void draw_marker_fields(InspectorSession& session, int index) override;
    void sync_items(InspectorSession& session) override;
    bool can_undo() const override
    {
        return !undo_.empty() || burst_open_;
    }
    bool can_redo() const override
    {
        return !redo_.empty();
    }
    void undo(InspectorSession& session) override;
    void redo(InspectorSession& session) override;
    bool pending() const override
    {
        return has_recipe_ && (dirty_ || unapplied_);
    }
    void apply(InspectorSession& session) override;
    bool busy() const override
    {
        return waiting_ || !restart_scene.empty();
    }
    const std::string& status() const override
    {
        return status_;
    }

private:
    void draw_scene_tab(InspectorSession& session);
    void draw_parts_tab();
    void draw_markers_tab(InspectorSession& session);
    void draw_script_tab();
    void draw_resources_tab();
    void draw_item_editor(RecipeResource& resource, RecipeItem& item, bool marker);
    void draw_source_browser(ResourceType type, char* filter, size_t filter_size, std::string& chosen);
    void new_scene(const std::string& scene, bool from_scene, const std::string& source);
    bool save();
    bool build_and_load();
    void export_bundle();
    // Adds the AID as a part from the disc bundle that best fits (one the recipe already uses). False when the
    // index has no bundle for it or it is already in the recipe.
    bool add_part(const std::string& aid);
    // Adds every AID the recipe's own resources and parts name that is not yet in it. Returns how many.
    size_t add_embedded_parts();
    // Fallback: the bundles the parts come from.
    void update_fallbacks();
    bool in_recipe(const std::string& aid) const;
    RecipeResource& resource_of(RecipeResource::Kind kind, const std::string& aid);
    void after_load(InspectorSession& session);
    // A change to the recipe: one undo step for each run of changes on consecutive frames (a drag is one step).
    void mark_changed();
    void reset_history();
    // The marker row with this kind and id, or -1.
    int marker_row(RecipeResource& markers, u16 kind, u16 id);
    RecipeResource& marker_resource();

    std::filesystem::path game_dir_;
    char plugin_dir_[260]{};
    char recipe_file_[260]{};
    char new_name_[128] = "aid_script_dlcstory_myscene";
    char from_scene_[128] = "aid_script_ghoulies_chapter1_scene1_2playcam";
    SceneRecipe recipe_;
    bool has_recipe_ = false;
    bool dirty_ = false;
    bool use_fallbacks_ = true;
    std::string status_;
    RecipeBundleCache bundles_;
    AidIndex index_;
    std::future<AidIndex> indexing_;
    char part_filter_[128]{};
    int part_type_ = 0;
    char marker_source_filter_[128] = "aid_marker_";
    std::string marker_source_;
    char node_source_filter_[128] = "aid_script_";
    std::string node_source_;
    char resource_filter_[128] = "aid_objparams_";
    std::string resource_source_;
    int selected_part_ = -1;
    int selected_row_ = -1;
    int selected_node_ = -1;
    int selected_resource_ = -1;
    int new_type_ = 0;
    char new_hex_[512]{};
    // Build & Load with dependency resolution: rounds left, and the session load it waits for.
    int resolve_rounds_ = 0;
    bool waiting_ = false;
    std::string requested_;
    // Undo history: whole recipes, the state before each step.
    std::deque<SceneRecipe> undo_;
    std::deque<SceneRecipe> redo_;
    SceneRecipe committed_; // The recipe as the last step left it.
    bool burst_open_ = false;
    int last_change_frame_ = -10;
    bool unapplied_ = false; // Changed since the scene last loaded from the recipe.
};

} // namespace gbtg
#endif
