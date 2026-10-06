#if !defined(GBTG_DEBUG_UI)
#error debug_ui.cpp is compiled only for Debug D3D11
#endif

#include "gbtg/debug_ui.hpp"
#include "gbtg/engine_internal.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

namespace gbtg
{
namespace
{

bool g_ready = false;
bool g_menu = false;
bool g_overlay = true;
bool g_scenes = false;
bool g_cutscenes = false;
char g_scene_filter[128]{};
char g_cutscene_filter[128]{};

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

void draw_scene_loader(Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(480.f, 420.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Scenes", &g_scenes))
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

void draw_cutscene_loader(Game& game)
{
    ImGui::SetNextWindowSize(ImVec2(480.f, 420.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Cutscenes", &g_cutscenes))
    {
        ImGui::End();
        return;
    }
    ImGui::InputText("Filter", g_cutscene_filter, sizeof(g_cutscene_filter));
    ImGui::BeginChild("list", ImVec2(0.f, 0.f), ImGuiChildFlags_Borders);
    const CutsceneQueue& queue = game.cutscenes;
    if (queue.items.empty())
    {
        ImGui::TextUnformatted("No cutscenes");
    }
    for (size_t i = 0; i < queue.items.size(); ++i)
    {
        const std::string& name = queue.items[i].name;
        if (g_cutscene_filter[0] != '\0' && name.find(g_cutscene_filter) == std::string::npos)
        {
            continue;
        }
        ImGui::PushID(static_cast<int>(i));
        const bool current = queue.playing && queue.index == i;
        if (ImGui::Selectable(name.c_str(), current))
        {
            game.cutscenes.index = i;
            game.cutscenes.time = 0.f;
            game.cutscenes.playing = true;
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

void debug_ui_init(void* hwnd, void* device, void* context)
{
    if (g_ready || hwnd == nullptr || device == nullptr || context == nullptr)
    {
        return;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    if (!ImGui_ImplWin32_Init(hwnd))
    {
        ImGui::DestroyContext();
        return;
    }
    if (!ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(device), static_cast<ID3D11DeviceContext*>(context)))
    {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return;
    }
    g_ready = true;
}

void debug_ui_shutdown()
{
    if (!g_ready)
    {
        return;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_ready = false;
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

void debug_ui_build(Game& game, float host_dt)
{
    if (!g_ready)
    {
        return;
    }
    g_avg.push(host_dt, game.clock.dt, game.clock.dt_raw);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    if (!g_menu)
    {
        // The Win32 backend queues mouse and key events during NewFrame. Drop them
        // so the frame-time overlay cannot take a click or a key while the menu is hidden.
        ImGuiIO& io = ImGui::GetIO();
        io.ClearEventsQueue();
        io.ClearInputKeys();
        io.ClearInputMouse();
    }
    ImGui::NewFrame();
    if (g_menu)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::Button("Scenes"))
            {
                g_scenes = !g_scenes;
            }
            if (ImGui::Button("Cutscenes"))
            {
                g_cutscenes = !g_cutscenes;
            }
            ImGui::EndMainMenuBar();
        }
        if (g_scenes)
        {
            draw_scene_loader(game);
        }
        if (g_cutscenes)
        {
            draw_cutscene_loader(game);
        }
    }
    if (g_overlay)
    {
        draw_frame_overlay(game);
    }
    ImGui::Render();
}

void debug_ui_draw()
{
    if (!g_ready)
    {
        return;
    }
    ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr)
    {
        return;
    }
    ImGui_ImplDX11_RenderDrawData(data);
}

} // namespace gbtg
