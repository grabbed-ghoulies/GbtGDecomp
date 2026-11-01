#include "gbtg/debug_ui.h"
// Empty unless the debug UI is enabled (GBTG_DEBUG_UI_ENABLED in debug_ui.h).
#if defined(GBTG_DEBUG_UI)
#include "gbtg/engine_internal.h"
#include "gbtg/gfx.h"
#include "gfx_render_thread.h"
#include "../scene/factory_scene_runtime.h"

#include <algorithm>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "imgui.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
#else
#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#endif

namespace gbtg
{
namespace
{

bool g_ready = false;
DebugUiRenderer g_renderer;
bool g_menu = false;
// The F12 timing overlay starts shown in Debug and hidden in Release.
#if defined(_DEBUG)
bool g_overlay = true;
#else
bool g_overlay = false;
#endif
bool g_scenes = false;
bool g_runtime = false;
bool g_performance = false;
bool g_objects = false;
bool g_resources = false;
ImGuiTextFilter g_resource_filter;
int g_object_type = -1;

constexpr int kHistorySize = 240;
float g_frame_history[kHistorySize]{};
int g_history_count = 0;
int g_history_cursor = 0;
bool g_freeze_history = false;
char g_scene_filter[128]{};

// Half a second at 60 Hz. The overlay shows the mean so a single hitch does not rewrite the text.
constexpr int kFrameAvg = 30;

struct FrameAvg
{
    float host = 0.f;
    float dt = 0.f;
    float dt_raw = 0.f;
    float host_sum = 0.f;
    float dt_sum = 0.f;
    float raw_sum = 0.f;
    float host_ring[kFrameAvg]{};
    float dt_ring[kFrameAvg]{};
    float raw_ring[kFrameAvg]{};
    int count = 0;
    int cursor = 0;

    void push(float host_dt, float game_dt, float game_raw)
    {
        if (count == kFrameAvg)
        {
            host_sum -= host_ring[cursor];
            dt_sum -= dt_ring[cursor];
            raw_sum -= raw_ring[cursor];
        }
        else
        {
            ++count;
        }
        host_ring[cursor] = host_dt;
        dt_ring[cursor] = game_dt;
        raw_ring[cursor] = game_raw;
        host_sum += host_dt;
        dt_sum += game_dt;
        raw_sum += game_raw;
        cursor = (cursor + 1) % kFrameAvg;
        const float n = float(count);
        host = host_sum / n;
        dt = dt_sum / n;
        dt_raw = raw_sum / n;
    }
};

FrameAvg g_avg;

// Keep opened diagnostics visible while F8 gives input back to the game.
ImGuiWindowFlags diagnostic_window_flags()
{
    return g_menu ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing;
}

void draw_performance()
{
    ImGui::SetNextWindowSize(ImVec2(560.f, 300.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance", &g_performance, diagnostic_window_flags()))
    {
        ImGui::Text("Host average %.2f ms (%.1f FPS)", g_avg.host * 1000.f,
            g_avg.host > 0.f ? 1.f / g_avg.host : 0.f);
        ImGui::Text("Game dt %.4f s / raw %.4f s", g_avg.dt, g_avg.dt_raw);
        ImGui::Checkbox("Freeze history", &g_freeze_history);
        ImGui::SameLine();
        if (ImGui::Button("Clear history"))
        {
            g_history_count = 0;
            g_history_cursor = 0;
        }
        if (g_history_count > 0)
        {
            const auto range = std::minmax_element(g_frame_history, g_frame_history + g_history_count);
            ImGui::Text("Last %d samples: min %.2f / max %.2f ms", g_history_count, *range.first, *range.second);
            ImGui::PlotLines("Host ms", g_frame_history, g_history_count,
                g_history_count == kHistorySize ? g_history_cursor : 0,
                nullptr, 0.f, std::max(33.33f, *range.second), ImVec2(0.f, 120.f));
        }
        ImGui::TextWrapped("Host samples measure loop-to-loop elapsed time, including pacing and presentation; they are not GPU timings.");
    }
    ImGui::End();
}

void draw_runtime(const Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(460.f, 430.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Runtime", &g_runtime, diagnostic_window_flags()))
    {
        ImGui::TextWrapped("Scene: %s", game.scene_name.c_str());
        ImGui::Text("Frame %u / clock %u, time %.3f s", game.frame, game.clock.frame, game.clock.time);
        ImGui::Text("Target %d Hz (%s)", clock_fps(game.clock), game.clock.pal ? "PAL" : "NTSC");
        ImGui::Text("Script phase %d, hold %s", game.script_phase, game.script_hold ? "yes" : "no");
        ImGui::TextWrapped("Requested script: %.*s", static_cast<int>(sizeof(game.requested_script)), game.requested_script);
        int width = 0;
        int height = 0;
        gfx_size(width, height);
        ImGui::Text("Presenter: %s, %d x %d", gfx_backend_name(), width, height);
        if (game.factory_scene)
        {
            const auto& scene = *game.factory_scene;
            ImGui::SeparatorText("Native scene");
            ImGui::Text("Actor start: %d (0 idle, 1 complete)", scene.actor_start);
            ImGui::Text("Owned objects %zu / registered count %u", scene.objects.size(), scene.factory.count);
            ImGui::Text("Address bindings %zu / loading %u", scene.address_count(), scene.resources.loading);
            ImGui::TextWrapped("Last missing resource: %s", scene.resources.last_missing.empty()
                ? "none recorded" : scene.resources.last_missing.c_str());
        }
        else
        {
            ImGui::TextUnformatted("No native scene factory bound.");
        }
        if (ImGui::TreeNode("Delayed callbacks"))
        {
            size_t active = 0;
            for (size_t i = 0; i < game.clock.delayed.size(); ++i)
            {
                const auto& slot = game.clock.delayed[i];
                if (slot.fn != nullptr)
                {
                    ++active;
                    ImGui::Text("Slot %zu: %u frames remaining", i, slot.frames_left);
                }
            }
            ImGui::Text("%zu / %zu slots active", active, game.clock.delayed.size());
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void draw_objects(const Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(600.f, 420.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Native objects", &g_objects, diagnostic_window_flags()))
    {
        ImGui::InputInt("Type (-1 = all)", &g_object_type);
        ImGui::TextUnformatted("Read-only native factory ownership; addresses are retail tokens.");
        if (game.factory_scene && ImGui::BeginTable("objects", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0.f, 0.f)))
        {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Type (hex)");
            ImGui::TableSetupColumn("Bytes");
            ImGui::TableSetupColumn("Vtable");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            const auto& scene = *game.factory_scene;
            for (const auto& object : scene.objects)
            {
                if (g_object_type >= 0 && object.type != g_object_type)
                {
                    continue;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%08X", scene.find_address(object.bytes.get()));
                ImGui::TableNextColumn();
                ImGui::Text("%04X", unsigned(object.type));
                ImGui::TableNextColumn();
                ImGui::Text("%u", object.size);
                ImGui::TableNextColumn();
                ImGui::Text("%08X", object.vtable ? object.vtable->address : 0);
            }
            ImGui::EndTable();
        }
        if (!game.factory_scene)
        {
            ImGui::TextUnformatted("No native scene factory bound.");
        }
    }
    ImGui::End();
}

void draw_resources(const Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(620.f, 420.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Resource cache", &g_resources, diagnostic_window_flags()))
    {
        ImGui::Text("Cached bundles %zu / demand resources %zu", game.aids.bundles.size(), game.aids.demand_resources.size());
        ImGui::TextUnformatted("Host cache contents; not a list of active native references. Sizes are heap bytes.");
        g_resource_filter.Draw("Filter (include,-exclude)");
        if (ImGui::BeginTable("bundles", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
            ImGuiTableFlags_ScrollY, ImVec2(0.f, 0.f)))
        {
            ImGui::TableSetupColumn("Bundle");
            ImGui::TableSetupColumn("Entries");
            ImGui::TableSetupColumn("CPU bytes");
            ImGui::TableSetupColumn("GPU bytes");
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            const auto row = [](const char* name, const Bundle& bundle)
            {
                if (!g_resource_filter.PassFilter(name))
                {
                    return;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle.entries.size());
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle.cpu_heap.size());
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle.gpu_heap.size());
            };
            row("common", game.aids.common);
            for (const auto& [name, bundle] : game.aids.bundles)
            {
                row(name.c_str(), bundle);
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void draw_scene_loader(Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(480.f, 420.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenes", &g_scenes, diagnostic_window_flags()))
    {
        ImGui::End();
        return;
    }
    ImGui::InputText("Filter", g_scene_filter, sizeof(g_scene_filter));
    ImGui::BeginChild("list", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders);
    if (game.scene_order.empty())
    {
        ImGui::TextUnformatted("No scenes");
    }
    for (size_t i = 0; i < game.scene_order.size(); ++i)
    {
        const std::string& name = game.scene_order[i];
        if (g_scene_filter[0] != '\0' && name.find(g_scene_filter) == std::string::npos)
        {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const bool current = name == game.scene_name;
        if (ImGui::Selectable(name.c_str(), current))
        {
            script_request(game, name);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void draw_frame_overlay(const Game& game)
{
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
    const float top = g_menu ? ImGui::GetFrameHeight() : 0.f;
    ImGui::SetNextWindowPos(ImVec2(8.f, top + 8.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (!ImGui::Begin("Frame times", nullptr, flags))
    {
        ImGui::End();
        return;
    }
    const float ms = g_avg.host * 1000.f;
    const float fps = g_avg.host > 1.0e-6f ? 1.f / g_avg.host : 0.f;
    ImGui::Text("host  %.2f ms   %.1f fps", ms, fps);
    ImGui::Text("game  dt %.4f   raw %.4f", g_avg.dt, g_avg.dt_raw);
    ImGui::End();
}

} // namespace

void debug_ui_attach(void* hwnd, const DebugUiRenderer& renderer)
{
    if (g_ready || hwnd == nullptr || renderer.init == nullptr)
    {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
#if defined(_WIN32)
    if (!ImGui_ImplWin32_Init(hwnd))
#else
    if (!ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(hwnd)))
#endif
    {
        ImGui::DestroyContext();
        return;
    }
    if (!renderer.init(renderer.user))
    {
#if defined(_WIN32)
        ImGui_ImplWin32_Shutdown();
#else
        ImGui_ImplSDL3_Shutdown();
#endif
        ImGui::DestroyContext();
        return;
    }
    g_renderer = renderer;
    g_ready = true;
}

void debug_ui_shutdown()
{
    if (!g_ready)
    {
        return;
    }
    g_renderer.shutdown();
#if defined(_WIN32)
    ImGui_ImplWin32_Shutdown();
#else
    ImGui_ImplSDL3_Shutdown();
#endif
    ImGui::DestroyContext();
    g_renderer = {};
    g_ready = false;
}

ImDrawData* debug_ui_draw_data()
{
    return g_ready ? ImGui::GetDrawData() : nullptr;
}

bool debug_ui_menu_open()
{
    return g_ready && g_menu;
}

void debug_ui_toggle_menu()
{
    g_menu = !g_menu;
}

void debug_ui_toggle_overlay()
{
    g_overlay = !g_overlay;
}

#if defined(_WIN32)
bool debug_ui_wndproc(void* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam)
{
    if (!g_ready || !g_menu)
    {
        return false;
    }
    const LRESULT handled = ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, static_cast<WPARAM>(wparam),
                                                           static_cast<LPARAM>(lparam));
    return handled != 0;
}
#else
bool debug_ui_sdl_event(const void* event)
{
    if (!g_ready || !g_menu)
    {
        return false;
    }
    return ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(event));
}
#endif

void debug_ui_build(Game& game, float host_dt)
{
    if (!g_ready)
    {
        return;
    }
#if GBTG_OPTIMIZE
    // GBTG_OPTIMIZE step 11: the render thread draws the previous ImGui frame while presenting it.
    gfx_render_thread_wait_presented();
#endif
    g_avg.push(host_dt, game.clock.dt, game.clock.dt_raw);
    if (!g_freeze_history)
    {
        g_frame_history[g_history_cursor] = host_dt * 1000.f;
        g_history_cursor = (g_history_cursor + 1) % kHistorySize;
        g_history_count = std::min(g_history_count + 1, kHistorySize);
    }
    ImGuiIO& io = ImGui::GetIO();
    if (g_menu)
    {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }
    else
    {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
    g_renderer.new_frame();
#if defined(_WIN32)
    ImGui_ImplWin32_NewFrame();
#else
    ImGui_ImplSDL3_NewFrame();
#endif
    if (!g_menu)
    {
        // The platform backend queues mouse and key events during NewFrame. Drop them
        // so visible diagnostics cannot take a click or a key while F8 is inactive.
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    ImGui::NewFrame();
    if (g_menu)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("Diagnostics"))
            {
                ImGui::MenuItem("Runtime", nullptr, &g_runtime);
                ImGui::MenuItem("Performance", nullptr, &g_performance);
                ImGui::MenuItem("Native objects", nullptr, &g_objects);
                ImGui::MenuItem("Resource cache", nullptr, &g_resources);
                ImGui::MenuItem("Timing overlay", "F12", &g_overlay);
                ImGui::EndMenu();
            }
            if (ImGui::Button("Scenes"))
            {
                g_scenes = !g_scenes;
            }
            ImGui::TextUnformatted("F8: debug menu | F12: timing overlay");
            ImGui::EndMainMenuBar();
        }
    }
    if (g_runtime)
    {
        draw_runtime(game);
    }
    if (g_performance)
    {
        draw_performance();
    }
    if (g_objects)
    {
        draw_objects(game);
    }
    if (g_resources)
    {
        draw_resources(game);
    }
    if (g_scenes)
    {
        draw_scene_loader(game);
    }
    if (g_overlay)
    {
        draw_frame_overlay(game);
    }
    ImGui::Render();
}

} // namespace gbtg

#endif // GBTG_DEBUG_UI
