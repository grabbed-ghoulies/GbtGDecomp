#pragma once
#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"

#include <functional>

namespace gbtg
{
struct InspectorUI
{
    std::filesystem::path directory;
    std::string load_request;
    std::string capture_status;
    bool retry = false;
    bool show_issues = false;
    bool show_meshes = false;
    bool show_audio = false;
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
    void build(InspectorSession& session, void* viewport, float dt);
    void release_capture();
};
}
#endif
