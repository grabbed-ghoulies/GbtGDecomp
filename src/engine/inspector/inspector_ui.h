#pragma once
#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"

#include <array>
#include <functional>
#include <optional>

namespace gbtg
{
class AssetViewer;

struct ScreenPoint
{
    float x = 0.f;
    float y = 0.f;
};

// The scene editor behind the viewport gizmo and the Inspector's marker fields (docs/INSPECTOR.md, "Scene
// editing"). Plugin builds implement it over the scene builder's recipe: edits change the recipe, and Apply rebuilds
// the scene from it and loads it again, so the native loader constructs what is shown.
class InspectorEditor
{
public:
    virtual ~InspectorEditor() = default;
    // Once a frame, before any other call.
    virtual void frame(InspectorSession& session) = 0;
    // The loaded scene is the open recipe's, so its markers can be edited.
    virtual bool editing(const InspectorSession& session) const = 0;
    // Remakes the loaded disc scene as a recipe in the plugin folder (or opens the one already there) and loads it.
    virtual void start_editing(InspectorSession& session) = 0;
    // The selected marker item's recipe pose changed (gizmo drag); `item` holds the new position/euler.
    virtual bool set_marker_pose(const InspectionItem& item) = 0;
    // A new row copying `item` with a free id at `position`; returns the new id, or 0.
    virtual u16 duplicate_marker(InspectorSession& session, const InspectionItem& item, const Vec3& position) = 0;
    virtual bool delete_marker(InspectorSession& session, const InspectionItem& item) = 0;
    // The recipe row's id, position, rotation and field edits.
    virtual void draw_marker_fields(InspectorSession& session, int index) = 0;
    // Rebuilds the session's marker items from the recipe (after undo, redo, duplicate or delete).
    virtual void sync_items(InspectorSession& session) = 0;
    virtual bool can_undo() const = 0;
    virtual bool can_redo() const = 0;
    virtual void undo(InspectorSession& session) = 0;
    virtual void redo(InspectorSession& session) = 0;
    // Edits not yet built and loaded.
    virtual bool pending() const = 0;
    // Saves, builds and loads the recipe again with the current camera.
    virtual void apply(InspectorSession& session) = 0;
    virtual bool busy() const = 0;
    virtual const std::string& status() const = 0;
};

struct InspectorUI
{
    std::filesystem::path directory;
    std::string load_request;
    std::string capture_status;
    bool retry = false;
    bool show_issues = false;
    bool show_meshes = false;
    bool show_audio = false;
    bool show_assets = false;
    char sound_filter[160]{};
    bool full_scene_ids = false;
    float browser_width = 0.f;
    int selected_mesh = -1;
    int selected_submesh = -1;
    bool markers = true;
    bool objects = false;
    bool grid = true;
    bool axes = true;
    bool bounds = true;
    bool depth_test = false;
    bool capture = false;
    float speed = 20.f;
    float yaw = 0.f;
    float pitch = 0.f;
    bool flying = false;
    bool sync_angles = true;
    char scene_filter[160]{};
    char object_filter[160]{};
    char screenshot_path[260] = "inspector-viewport.bmp";
    // Plugin builds: the scene builder's window (scene_builder.h), drawn by extra_windows.
    bool show_scene_builder = false;
    std::function<void(InspectorSession&)> extra_windows;
    // The asset viewer window and its preview viewport (capture slot 1) for this frame.
    AssetViewer* assets = nullptr;
    void* preview_viewport = nullptr;
    // Plugin builds: the scene editor. Null hides the editing tools.
    InspectorEditor* editor = nullptr;
    // Gizmo state.
    enum class Gizmo
    {
        Move, Rotate
    };
    Gizmo gizmo = Gizmo::Move;
    bool snap = false;
    float snap_step = 0.5f;
    float snap_angle = 15.f;
    int drag_handle = -1; // 0..2 an axis, 3 the horizontal plane, 4 the yaw ring.
    Vec3 drag_start{};
    Vec3 drag_euler{};
    ScreenPoint drag_mouse{};
    Vec3 drag_hit{}; // The plane handle's ground point when the drag began.
    std::array<ScreenPoint, 3> drag_axes{}; // Each axis's screen pixels per world unit when the drag began.
    bool gizmo_hover = false; // A handle is under the mouse (last frame), so a click does not select a marker.
    // A marker (kind, id) to select once the next load is ready, after Apply.
    std::optional<std::pair<u32, u32>> reselect;
    // The viewport needs a depth readback for click placement.
    bool wants_depth() const
    {
        return depth_test || (editor != nullptr && editor_depth);
    }
    bool editor_depth = false;
    void build(InspectorSession& session, void* viewport, float dt);
    void release_capture();

private:
    void draw_editor_toolbar(InspectorSession& session);
    void apply_edits(InspectorSession& session);
    void draw_gizmo(InspectorSession& session, const ScreenPoint& origin, float scale, bool hovered);
    void place_at_cursor(InspectorSession& session, const ScreenPoint& origin, float scale, bool duplicate);
    void editor_keys(InspectorSession& session);
};
}
#endif
