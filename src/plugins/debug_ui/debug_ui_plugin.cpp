// The host debug UI (Dear ImGui), which retail does not have, as a plugin overlay: F8 opens the menu bar and gives it
// the keyboard and mouse, F12 toggles the timing overlay. The diagnostics windows read the runtime through the host's
// read-only diagnostics calls. The plugin owns ImGui; the game only offers the overlay hooks (docs/PLUGINS.md).
#include "object_windows.h"
#include "renderer.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <string>

#include "imgui.h"
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
#else
#include <SDL3/SDL.h>

#include "imgui_impl_sdl3.h"
#endif

namespace debug_ui
{

namespace
{

const GbtgPluginHost* g_host = nullptr;
const Renderer* g_renderer = nullptr;
bool g_ready = false;
bool g_menu = false;
// The F12 timing overlay starts shown in Debug and hidden in Release, unless [DebugUi] TimingOverlay says.
#if defined(_DEBUG)
bool g_overlay = true;
#else
bool g_overlay = false;
#endif
bool g_scenes = false;
bool g_runtime = false;
bool g_performance = false;
bool g_objects = false;
bool g_actors = false;
bool g_resources = false;
ImGuiTextFilter g_resource_filter;

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

bool setting_flag(const char* key, bool fallback)
{
    const char* value = g_host->setting(g_host->plugin, "DebugUi", key);
    if (value == nullptr || *value == '\0')
    {
        return fallback;
    }
    return std::strcmp(value, "0") != 0 && std::strcmp(value, "false") != 0 && std::strcmp(value, "no") != 0;
}

// [DebugUi] Windows: a comma-separated list of windows to open at startup.
void open_listed_windows()
{
    const char* value = g_host->setting(g_host->plugin, "DebugUi", "Windows");
    if (value == nullptr)
    {
        return;
    }
    const struct
    {
        const char* name;
        bool* shown;
    } windows[] = {{"runtime", &g_runtime}, {"performance", &g_performance}, {"objects", &g_objects},
        {"actors", &g_actors}, {"resources", &g_resources}, {"scenes", &g_scenes}};
    std::string list(value);
    for (char& c : list)
    {
        c = c == ',' ? ' ' : char(std::tolower(static_cast<unsigned char>(c)));
    }
    size_t at = 0;
    while (at < list.size())
    {
        const size_t space = list.find(' ', at);
        const std::string word = list.substr(at, space == std::string::npos ? std::string::npos : space - at);
        for (const auto& window : windows)
        {
            if (word == window.name)
            {
                *window.shown = true;
            }
        }
        at = space == std::string::npos ? list.size() : space + 1;
    }
}

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

void draw_runtime(const GbtgGame* game, const GbtgRuntimeInfo& info)
{
    ImGui::SetNextWindowSize(ImVec2(460.f, 430.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Runtime", &g_runtime, diagnostic_window_flags()))
    {
        ImGui::TextWrapped("Scene: %s", info.scene_name);
        ImGui::Text("Frame %u / clock %u, time %.3f s", info.frame, info.clock_frame, info.clock_time);
        ImGui::Text("Target %d Hz (%s)", info.target_hz, info.pal ? "PAL" : "NTSC");
        ImGui::Text("Script phase %d, hold %s", info.script_phase, info.script_hold ? "yes" : "no");
        ImGui::TextWrapped("Requested script: %s", info.requested_script);
        ImGui::Text("Presenter: %s, %d x %d", info.presenter, info.width, info.height);
        if (info.has_native_scene)
        {
            ImGui::SeparatorText("Native scene");
            ImGui::Text("Actor start: %d (0 idle, 1 complete)", info.actor_start);
            ImGui::Text("Owned objects %zu / registered count %u", info.native_objects, info.registered_objects);
            ImGui::Text("Address bindings %zu / loading %u", info.address_bindings, info.loading);
            ImGui::TextWrapped("Last missing resource: %s", info.last_missing_resource[0] == '\0'
                ? "none recorded" : info.last_missing_resource);
        }
        else
        {
            ImGui::TextUnformatted("No native scene factory bound.");
        }
        if (ImGui::TreeNode("Delayed callbacks"))
        {
            g_host->delayed_callbacks(game, [](void*, size_t slot, uint32_t frames_left)
            {
                ImGui::Text("Slot %zu: %u frames remaining", slot, frames_left);
            }, nullptr);
            ImGui::Text("%zu / %zu slots active", info.delayed_active, info.delayed_slots);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void draw_resources(const GbtgGame* game, const GbtgRuntimeInfo& info)
{
    ImGui::SetNextWindowSize(ImVec2(620.f, 420.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Resource cache", &g_resources, diagnostic_window_flags()))
    {
        ImGui::Text("Cached bundles %zu / demand resources %zu", info.cached_bundles, info.demand_resources);
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
            g_host->resource_bundles(game, [](void*, const GbtgBundleInfo* bundle)
            {
                if (!g_resource_filter.PassFilter(bundle->name))
                {
                    return;
                }
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(bundle->name);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle->entries);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle->cpu_bytes);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", bundle->gpu_bytes);
            }, nullptr);
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void draw_scene_loader(GbtgGame* game, const GbtgRuntimeInfo& info)
{
    ImGui::SetNextWindowSize(ImVec2(480.f, 420.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenes", &g_scenes, diagnostic_window_flags()))
    {
        ImGui::End();
        return;
    }
    ImGui::InputText("Filter", g_scene_filter, sizeof(g_scene_filter));
    ImGui::BeginChild("list", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders);
    const size_t count = g_host->scene_count(game);
    if (count == 0)
    {
        ImGui::TextUnformatted("No scenes");
    }
    for (size_t i = 0; i < count; ++i)
    {
        const char* name = g_host->scene_at(game, i);
        if (g_scene_filter[0] != '\0' && std::strstr(name, g_scene_filter) == nullptr)
        {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const bool current = std::strcmp(name, info.scene_name) == 0;
        if (ImGui::Selectable(name, current))
        {
            g_host->request_scene(game, name);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();
}

void draw_frame_overlay()
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

void attach(void*, GbtgGame* game)
{
    GbtgGraphicsDevice device{};
    device.size = sizeof(device);
    if (g_ready || g_host->graphics_device(game, &device) == 0 || device.window == nullptr)
    {
        return;
    }
    const Renderer* renderer = std::strcmp(device.backend, "d3d11") == 0 ? d3d11_renderer()
        : std::strcmp(device.backend, "vulkan") == 0 ? vulkan_renderer() : nullptr;
    if (renderer == nullptr)
    {
        log(GBTG_LOG_WARN, (std::string("no renderer for the ") + device.backend + " backend in this build").c_str());
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
#if defined(_WIN32)
    if (!ImGui_ImplWin32_Init(device.window))
#else
    if (!ImGui_ImplSDL3_InitForVulkan(static_cast<SDL_Window*>(device.window)))
#endif
    {
        ImGui::DestroyContext();
        return;
    }
    if (!renderer->init(device))
    {
#if defined(_WIN32)
        ImGui_ImplWin32_Shutdown();
#else
        ImGui_ImplSDL3_Shutdown();
#endif
        ImGui::DestroyContext();
        log(GBTG_LOG_WARN, "the ImGui renderer did not start");
        return;
    }
    g_renderer = renderer;
    g_ready = true;
}

void detach(void*, GbtgGame*)
{
    if (!g_ready)
    {
        return;
    }
    g_renderer->shutdown();
#if defined(_WIN32)
    ImGui_ImplWin32_Shutdown();
#else
    ImGui_ImplSDL3_Shutdown();
#endif
    ImGui::DestroyContext();
    g_renderer = nullptr;
    g_ready = false;
}

int window_event(void*, const void* event)
{
    if (!g_ready)
    {
        return 0;
    }
    // F8 and F12 never reach the game; a held key's repeats toggle nothing.
#if defined(_WIN32)
    const auto& message = *static_cast<const GbtgWin32Message*>(event);
    if (message.message == WM_KEYDOWN && (message.wparam == VK_F8 || message.wparam == VK_F12))
    {
        if ((message.lparam & (1 << 30)) == 0)
        {
            bool& shown = message.wparam == VK_F8 ? g_menu : g_overlay;
            shown = !shown;
        }
        return 1;
    }
    if (!g_menu)
    {
        return 0;
    }
    return ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(message.hwnd), message.message,
        WPARAM(message.wparam), LPARAM(message.lparam)) != 0 ? 1 : 0;
#else
    const auto& sdl = *static_cast<const SDL_Event*>(event);
    if (sdl.type == SDL_EVENT_KEY_DOWN && (sdl.key.key == SDLK_F8 || sdl.key.key == SDLK_F12))
    {
        if (!sdl.key.repeat)
        {
            bool& shown = sdl.key.key == SDLK_F8 ? g_menu : g_overlay;
            shown = !shown;
        }
        return 1;
    }
    if (!g_menu)
    {
        return 0;
    }
    return ImGui_ImplSDL3_ProcessEvent(&sdl) ? 1 : 0;
#endif
}

int captures_input(void*)
{
    return g_ready && g_menu ? 1 : 0;
}

void build(void*, GbtgGame* game, float host_dt)
{
    if (!g_ready)
    {
        return;
    }
    GbtgRuntimeInfo info{};
    info.size = sizeof(info);
    g_host->runtime_info(game, &info);
    g_avg.push(host_dt, info.dt, info.dt_raw);
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
    g_renderer->new_frame();
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
                if (has_object_views(g_host))
                {
                    ImGui::MenuItem("Actors", nullptr, &g_actors);
                }
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
        draw_runtime(game, info);
    }
    if (g_performance)
    {
        draw_performance();
    }
    if (g_objects)
    {
        draw_object_table(g_host, game, info.has_native_scene, &g_objects, diagnostic_window_flags());
    }
    if (g_actors && has_object_views(g_host))
    {
        draw_actor_table(g_host, game, &g_actors, diagnostic_window_flags());
    }
    if (has_object_views(g_host))
    {
        draw_inspectors(g_host, game, diagnostic_window_flags());
    }
    if (g_resources)
    {
        draw_resources(game, info);
    }
    if (g_scenes)
    {
        draw_scene_loader(game, info);
    }
    if (g_overlay)
    {
        draw_frame_overlay();
    }
    ImGui::Render();
}

int has_draws(void*)
{
    if (!g_ready)
    {
        return 0;
    }
    const ImDrawData* data = ImGui::GetDrawData();
    return data != nullptr && data->CmdListsCount > 0 ? 1 : 0;
}

void render(void*, void* command)
{
    if (ImDrawData* data = g_ready ? ImGui::GetDrawData() : nullptr)
    {
        g_renderer->render(data, command);
    }
}

} // namespace

void log(int level, const char* message)
{
    g_host->log(g_host->plugin, level, message);
}

} // namespace debug_ui

extern "C" GBTG_PLUGIN_EXPORT int gbtg_plugin_load(const GbtgPluginHost* host)
{
    // Built for another gbtg, or one without the overlay and diagnostics calls: stay unloaded.
    if (host->api_version != GBTG_PLUGIN_API_VERSION ||
        host->size < offsetof(GbtgPluginHost, request_scene) + sizeof(host->request_scene))
    {
        return 0;
    }
    debug_ui::g_host = host;
    debug_ui::g_menu = debug_ui::setting_flag("OpenMenu", false);
    debug_ui::g_overlay = debug_ui::setting_flag("TimingOverlay", debug_ui::g_overlay);
    debug_ui::open_listed_windows();
    static const GbtgFeature feature = {"debug_ui", "F8 debug menu and F12 timing overlay (Dear ImGui)", nullptr,
        nullptr, debug_ui::attach, debug_ui::detach, nullptr};
    static const GbtgOverlay overlay = {"debug_ui", debug_ui::window_event, debug_ui::captures_input,
        debug_ui::build, debug_ui::has_draws, debug_ui::render, nullptr};
    return host->add_feature(host->plugin, &feature) != 0 && host->add_overlay(host->plugin, &overlay) != 0;
}
