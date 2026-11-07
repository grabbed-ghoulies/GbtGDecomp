#pragma once
// The debug UI's views of the native scene: the factory objects, the actors, and an inspector per object that shows
// its named retail struct (docs/RETAIL_LAYOUTS.md) field by field. Everything is read only.
#include "gbtg/plugin_api.h"

#include "imgui.h"

namespace debug_ui
{

// 1 when the host has the struct views (object_view and the calls after it).
bool has_object_views(const GbtgPluginHost* host);

void draw_object_table(const GbtgPluginHost* host, const GbtgGame* game, int has_native_scene, bool* open,
    ImGuiWindowFlags flags);
void draw_actor_table(const GbtgPluginHost* host, const GbtgGame* game, bool* open, ImGuiWindowFlags flags);
// One window per inspected object.
void draw_inspectors(const GbtgPluginHost* host, const GbtgGame* game, ImGuiWindowFlags flags);

} // namespace debug_ui
