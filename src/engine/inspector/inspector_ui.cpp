#if defined(GBTG_INSPECTOR_APP)
#include "inspector_ui.h"
#include "inspector_graphics.h"
#include "asset_viewer.h"
#include "gbtg/debug_ui.h"
#include "gbtg/gfx.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace gbtg
{
namespace
{
bool ready = false;
bool window_backend = false;

void texture_preview(const RgbaImage* image)
{
    if (image == nullptr || image->width <= 0 || image->height <= 0 || image->bgra.empty())
    {
        ImGui::TextUnformatted("No texture bound");
        return;
    }
    ImGui::Text("%d x %d | decoded BGRA: %zu bytes", image->width, image->height, image->bgra.size());
    const float available = std::max(32.f, ImGui::GetContentRegionAvail().x);
    const float scale = std::min(256.f, available) / float(std::max(image->width, image->height));
    ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(inspector_texture_preview(*image))),
        {image->width * scale, image->height * scale});
}

void panel(const char* title, float x, float y, float w, float h)
{
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({std::max(w, 1.f), std::max(h, 1.f)});
    ImGui::Begin(title, nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
}

const char* state_name(InspectionState state)
{
    switch (state)
    {
    case InspectionState::Empty: return "Empty";
    case InspectionState::Loading: return "Loading";
    case InspectionState::Ready: return "Ready / static";
    case InspectionState::Failed: return "Failed";
    }
    return "Unknown";
}

// A world point on the scene view's screen, as the overlays draw it.
bool to_screen(const InspectionFrame& frame, const ScreenPoint& origin, float scale, const Vec3& world, ImVec2& pixel)
{
    Vec3 ndc;
    if (!project_inspection_point(frame, world, ndc))
    {
        return false;
    }
    const auto& display = frame.display;
    const float x = display.fitted_x + (ndc.x + 1.f) * .5f * display.fitted_width;
    const float y = display.fitted_y + (1.f - ndc.y) * .5f * display.fitted_height;
    pixel = {origin.x + x * scale, origin.y + y * scale};
    return true;
}

// The screen position as NDC and as a 640x480 retail pixel.
void from_screen(const InspectionFrame& frame, const ScreenPoint& origin, float scale, const ImVec2& pixel,
    float& ndc_x, float& ndc_y, float& retail_x, float& retail_y)
{
    const auto& display = frame.display;
    retail_x = (pixel.x - origin.x) / scale;
    retail_y = (pixel.y - origin.y) / scale;
    ndc_x = (retail_x - display.fitted_x) / display.fitted_width * 2.f - 1.f;
    ndc_y = 1.f - (retail_y - display.fitted_y) / display.fitted_height * 2.f;
}

// Where the ray under the screen position meets the horizontal plane at `height`.
bool plane_hit(const InspectionFrame& frame, const ScreenPoint& origin, float scale, const ImVec2& pixel, float height,
    Vec3& hit)
{
    float nx, ny, rx, ry;
    from_screen(frame, origin, scale, pixel, nx, ny, rx, ry);
    const Vec3 a = unproject_inspection_point(frame, nx, ny, 0.f);
    const Vec3 b = unproject_inspection_point(frame, nx, ny, 0.999f);
    const float dy = b.y - a.y;
    if (std::abs(dy) < 1e-6f)
    {
        return false;
    }
    const float t = (height - a.y) / dy;
    if (t < 0.f)
    {
        return false;
    }
    hit = {a.x + (b.x - a.x) * t, height, a.z + (b.z - a.z) * t};
    return true;
}

float segment_distance(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = dx * dx + dy * dy;
    const float t = length > 0.f ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length, 0.f, 1.f) : 0.f;
    const float x = a.x + dx * t - p.x;
    const float y = a.y + dy * t - p.y;
    return std::sqrt(x * x + y * y);
}

float snapped(float value, float step)
{
    return step > 0.f ? std::round(value / step) * step : value;
}

void select_marker(InspectorSession& session, u32 kind, u32 id)
{
    for (size_t index = 0; index < session.items.size(); ++index)
    {
        if (session.items[index].marker && session.items[index].type == kind && session.items[index].address == id)
        {
            session.selection = int(index);
            return;
        }
    }
}

void frame_item(InspectorSession& session)
{
    if (session.selection < 0 || size_t(session.selection) >= session.items.size())
    {
        return;
    }
    const auto& item = session.items[size_t(session.selection)];
    if (item.position)
    {
        const auto forward = inspection_forward(session.camera.rotation);
        session.camera.position = {item.position->x - forward.x * 10.f,
            item.position->y - forward.y * 10.f, item.position->z - forward.z * 10.f};
    }
}
}

// Reuse the existing renderer's ImGui lifecycle hooks, with inspector widgets in every build config.
void debug_ui_init(void* hwnd, void* device, void* context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Floating windows move by their title bar only, so drags inside views (orbit, gizmo, pan) stay with the view.
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::StyleColorsDark();
    if (hwnd != nullptr)
    {
        window_backend = ImGui_ImplWin32_Init(hwnd);
        if (!window_backend)
        {
            throw Error("inspector Win32 UI initialization failed");
        }
    }
    if (!ImGui_ImplDX11_Init(static_cast<ID3D11Device*>(device), static_cast<ID3D11DeviceContext*>(context)))
    {
        throw Error("inspector D3D11 UI initialization failed");
    }
    ready = true;
}

void debug_ui_shutdown()
{
    if (ready)
    {
        ImGui_ImplDX11_Shutdown();
        if (window_backend)
        {
            ImGui_ImplWin32_Shutdown();
        }
        ImGui::DestroyContext();
    }
    ready = false;
    window_backend = false;
}

bool debug_ui_menu_open()
{
    return ready;
}
void debug_ui_toggle_menu()
{
}
void debug_ui_toggle_overlay()
{
}
bool debug_ui_wndproc(void* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam)
{
    if (!ready || !window_backend)
    {
        return false;
    }
    return ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, WPARAM(wparam), LPARAM(lparam)) != 0;
}
void debug_ui_draw()
{
    if (ready)
    {
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
}

void InspectorUI::release_capture()
{
    if (flying)
    {
        flying = false;
        ReleaseCapture();
        ClipCursor(nullptr);
    }
}

void InspectorUI::build(InspectorSession& session, void* viewport, float dt)
{
    if (!ready)
    {
        throw Error("inspector UI is not initialized");
    }
    ImGui_ImplDX11_NewFrame();
    auto& io = ImGui::GetIO();
    if (window_backend)
    {
        ImGui_ImplWin32_NewFrame();
    }
    else
    {
        int width, height;
        gfx_size(width, height);
        io.DisplaySize = {float(width), float(height)};
        io.DeltaTime = 1.f / 60.f;
    }
    ImGui::NewFrame();
    if (editor != nullptr)
    {
        editor->frame(session);
        editor_depth = editor->editing(session);
    }
    if (reselect && session.state == InspectionState::Ready && !session.items.empty()
        && (editor == nullptr || !editor->busy()))
    {
        select_marker(session, reselect->first, reselect->second);
        reselect.reset();
    }
    const float width = io.DisplaySize.x;
    const float height = io.DisplaySize.y;
    const float right = std::clamp(width * .24f, 230.f, 320.f);
    if (browser_width == 0.f)
    {
        browser_width = std::clamp(width * .28f, 240.f, 420.f);
    }
    browser_width = std::clamp(browser_width, 185.f, std::max(185.f, width - right - 160.f));
    const float left = browser_width;
    const float status_height = std::clamp(height * .19f, 120.f, 180.f);
    const float content_height = std::max(100.f, height - status_height);
    const float middle = std::max(100.f, width - left - right);

    panel("Scenes", 0, 0, left - 6.f, content_height * .44f);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##scene-filter", "Find scene...", scene_filter, sizeof(scene_filter));
    ImGui::Checkbox("Full resource IDs", &full_scene_ids);
    if (session.game)
    {
        ImGui::BeginChild("scene-list", {}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        for (const auto& name : session.game->scene_order)
        {
            if (scene_filter[0] != 0 && name.find(scene_filter) == std::string::npos)
            {
                continue;
            }
            // Shorten only the shared resource prefix; keep the actual scene suffix intact.
            const std::string prefix = "aid_script_ghoulies_";
            const std::string label = !full_scene_ids && name.starts_with(prefix) ? name.substr(prefix.size()) : name;
            ImGui::PushID(name.c_str());
            if (ImGui::Selectable(label.c_str(), name == session.requested, ImGuiSelectableFlags_None,
                {std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(label.c_str()).x), 0.f}))
            {
                load_request = name;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", name.c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
    ImGui::End();

    panel("Objects and markers", 0, content_height * .44f, left - 6.f, content_height * .56f);
    ImGui::InputTextWithHint("##object-filter", "Find object...", object_filter, sizeof(object_filter));
    ImGui::Text("%zu native records", session.items.size());
    ImGui::BeginChild("object-list");
    for (size_t index = 0; index < session.items.size(); ++index)
    {
        const auto& item = session.items[index];
        if (object_filter[0] != 0 && item.label.find(object_filter) == std::string::npos)
        {
            continue;
        }
        ImGui::PushID(int(index));
        if (ImGui::Selectable(item.label.c_str(), session.selection == int(index)))
        {
            session.selection = int(index);
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::End();

    ImGui::SetNextWindowPos({left - 6.f, 0.f});
    ImGui::SetNextWindowSize({6.f, content_height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.f, 0.f});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {1.f, 1.f});
    ImGui::Begin("##browser-divider", nullptr, ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::InvisibleButton("resize-browser", {6.f, content_height});
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActive())
    {
        browser_width += io.MouseDelta.x;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Drag to resize the scene browser");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);

    panel("Scene view", left, 0, middle, content_height);
    ImGui::Checkbox("Markers", &markers);
    ImGui::SameLine();
    ImGui::Checkbox("Objects", &objects);
    ImGui::Checkbox("Grid", &grid);
    ImGui::SameLine();
    ImGui::Checkbox("Axes", &axes);
    ImGui::SameLine();
    ImGui::Checkbox("Bounds", &bounds);
    ImGui::Checkbox("Depth-tested overlays", &depth_test);
    ImGui::TextUnformatted("RMB: fly | WASD + Q/E | Shift: fast | F: frame");
    if (editor != nullptr)
    {
        draw_editor_toolbar(session);
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale = std::max(0.01f, std::min(avail.x / 640.f, avail.y / 480.f));
    const ImVec2 size{640.f * scale, 480.f * scale};
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (avail.x - size.x) * .5f));
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    if (session.state == InspectionState::Ready && viewport != nullptr)
    {
        int target_width, target_height;
        gfx_size(target_width, target_height);
        const float source_scale = std::min(float(target_width) / 640.f, float(target_height) / 480.f);
        const ImVec2 uv0{(target_width - source_scale * 640.f) / (2.f * target_width),
            (target_height - source_scale * 480.f) / (2.f * target_height)};
        const ImVec2 uv1{1.f - uv0.x, 1.f - uv0.y};
        ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(viewport)), size, uv0, uv1);
    }
    else
    {
        ImGui::InvisibleButton("empty-viewport", size);
        ImGui::GetWindowDrawList()->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(18, 22, 28, 255));
        ImGui::GetWindowDrawList()->AddText({origin.x + 16, origin.y + 16}, IM_COL32(180, 190, 205, 255), state_name(session.state));
    }
    const bool hovered = ImGui::IsItemHovered();
    const bool focused = inspector_window() != nullptr && GetForegroundWindow() == inspector_window();
    if (!focused || !ImGui::IsMouseDown(ImGuiMouseButton_Right) || session.state != InspectionState::Ready)
    {
        release_capture();
    }
    if (focused && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && session.state == InspectionState::Ready)
    {
        flying = true;
        SetCapture(static_cast<HWND>(inspector_window()));
        POINT top_left{LONG(origin.x), LONG(origin.y)};
        POINT bottom_right{LONG(origin.x + size.x), LONG(origin.y + size.y)};
        ClientToScreen(static_cast<HWND>(inspector_window()), &top_left);
        ClientToScreen(static_cast<HWND>(inspector_window()), &bottom_right);
        RECT rect{top_left.x, top_left.y, bottom_right.x, bottom_right.y};
        ClipCursor(&rect);
        sync_angles = true;
    }
    if (sync_angles)
    {
        const Vec3 forward = inspection_forward(session.camera.rotation);
        yaw = std::atan2(-forward.x, -forward.z);
        pitch = std::asin(std::clamp(forward.y, -1.f, 1.f));
        sync_angles = false;
    }
    if (flying)
    {
        io.MouseDrawCursor = false;
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        POINT pointer{};
        GetCursorPos(&pointer);
        POINT center{LONG(origin.x + size.x * .5f), LONG(origin.y + size.y * .5f)};
        ClientToScreen(static_cast<HWND>(inspector_window()), &center);
        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            yaw -= float(pointer.x - center.x) * .003f;
            pitch = std::clamp(pitch - float(pointer.y - center.y) * .003f, -1.55f, 1.55f);
        }
        session.camera.rotation = inspection_rotation(yaw, pitch);
        const float movement = speed * std::min(dt, .1f) * (io.KeyShift ? 5.f : 1.f);
        const Vec3 forward = inspection_forward(session.camera.rotation);
        const Vec3 side{std::cos(yaw), 0.f, -std::sin(yaw)};
        const float f = float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S));
        const float s = float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A));
        const float up = float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q));
        session.camera.position.x += movement * (forward.x * f + side.x * s);
        session.camera.position.y += movement * (forward.y * f + up);
        session.camera.position.z += movement * (forward.z * f + side.z * s);
        SetCursorPos(center.x, center.y);
    }
    if (hovered && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F))
    {
        frame_item(session);
    }
    if (session.state == InspectionState::Ready)
    {
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
        const auto project = [&](const Vec3& world, ImVec2& pixel) -> bool
        {
            Vec3 ndc;
            if (!project_inspection_point(session.frame, world, ndc))
            {
                return false;
            }
            const auto& display = session.frame.display;
            const float x = display.fitted_x + (ndc.x + 1.f) * .5f * display.fitted_width;
            const float y = display.fitted_y + (1.f - ndc.y) * .5f * display.fitted_height;
            pixel = {origin.x + x * scale, origin.y + y * scale};
            return !depth_test || inspector_depth_visible(x, y, ndc.z);
        };
        const auto line = [&](const Vec3& a, const Vec3& b, ImU32 color)
        {
            // Subdivide for depth-tested overlays, including grid portions behind scene geometry.
            for (int part = 0; part < 32; ++part)
            {
                const float t0 = float(part) / 32.f;
                const float t1 = float(part + 1) / 32.f;
                const Vec3 p0{a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0, a.z + (b.z - a.z) * t0};
                const Vec3 p1{a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1, a.z + (b.z - a.z) * t1};
                ImVec2 s0, s1;
                if (project(p0, s0) && project(p1, s1))
                {
                    draw->AddLine(s0, s1, color);
                }
            }
        };
        if (grid)
        {
            for (int i = -10; i <= 10; ++i)
            {
                const float value = float(i) * 10.f;
                line({-100.f, 0.f, value}, {100.f, 0.f, value}, IM_COL32(95, 110, 125, 90));
                line({value, 0.f, -100.f}, {value, 0.f, 100.f}, IM_COL32(95, 110, 125, 90));
            }
        }
        if (axes)
        {
            line({}, {10.f, 0.f, 0.f}, IM_COL32(245, 90, 80, 255));
            line({}, {0.f, 10.f, 0.f}, IM_COL32(100, 235, 110, 255));
            line({}, {0.f, 0.f, 10.f}, IM_COL32(90, 160, 255, 255));
        }
        for (size_t index = 0; index < session.items.size(); ++index)
        {
            const auto& item = session.items[index];
            if (!item.position || (item.marker ? !markers : !objects))
            {
                continue;
            }
            ImVec2 point;
            if (!project(*item.position, point))
            {
                continue;
            }
            const bool selected = int(index) == session.selection;
            const ImU32 color = selected ? IM_COL32(255, 220, 80, 255)
                : !item.failure.empty() ? IM_COL32(255, 130, 80, 255)
                : item.marker ? IM_COL32(80, 215, 235, 255) : IM_COL32(190, 145, 245, 255);
            draw->AddCircleFilled(point, selected ? 6.f : 4.f, color);
            if (selected)
            {
                draw->AddText({point.x + 8.f, point.y - 8.f}, color, item.label.c_str());
            }
            const float dx = io.MousePos.x - point.x;
            const float dy = io.MousePos.y - point.y;
            if (hovered && !flying && drag_handle < 0 && !gizmo_hover && dx * dx + dy * dy < 64.f)
            {
                ImGui::SetTooltip("%s", item.label.c_str());
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl)
                {
                    session.selection = int(index);
                }
            }
            if (bounds && item.marker && item.type == 8 && item.bytes.size() >= 0x30)
            {
                // 0x00100210 uses an integer radius at +0x2C for the occupancy footprint.
                i32 radius;
                std::memcpy(&radius, item.bytes.data() + 0x2C, 4);
                if (radius > 0 && radius < 100000)
                {
                    for (int step = 0; step < 32; ++step)
                    {
                        const float a = float(step) * .1963495408f;
                        const float b = float(step + 1) * .1963495408f;
                        const Vec3 p = *item.position;
                        line({p.x + radius * std::cos(a), p.y, p.z + radius * std::sin(a)},
                            {p.x + radius * std::cos(b), p.y, p.z + radius * std::sin(b)}, color);
                    }
                }
            }
            if (bounds && item.marker && item.type == 0x14 && item.bytes.size() >= 0x34)
            {
                // 0x001002B0's occupancy box: integer X/Z extents and yaw, with no invented height.
                i32 width, depth;
                float angle;
                std::memcpy(&width, item.bytes.data() + 0x2C, 4);
                std::memcpy(&depth, item.bytes.data() + 0x30, 4);
                std::memcpy(&angle, item.bytes.data() + 0x1C, 4);
                if (width > 0 && depth > 0 && width < 100000 && depth < 100000 && std::isfinite(angle))
                {
                    const Vec3 p = *item.position;
                    const auto corner = [&](float x, float z)
                    {
                        return Vec3{p.x + x * std::cos(angle) + z * std::sin(angle), p.y,
                            p.z - x * std::sin(angle) + z * std::cos(angle)};
                    };
                    const std::array<Vec3, 4> corners{corner(float(-width), float(-depth)),
                        corner(float(width), float(-depth)), corner(float(width), float(depth)),
                        corner(float(-width), float(depth))};
                    for (size_t side = 0; side < 4; ++side)
                    {
                        line(corners[side], corners[(side + 1) % 4], color);
                    }
                }
            }
        }
        draw->PopClipRect();
        if (editor != nullptr && editor->editing(session))
        {
            const ScreenPoint at{origin.x, origin.y};
            draw_gizmo(session, at, scale, hovered && !flying);
            if (hovered && !flying && io.KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && drag_handle < 0)
            {
                place_at_cursor(session, at, scale, io.KeyShift);
            }
            if ((hovered || ImGui::IsWindowFocused()) && !flying)
            {
                editor_keys(session);
            }
        }
    }
    ImGui::End();

    panel("Inspector", left + middle, 0, right, content_height);
    ImGui::TextUnformatted("Inspection camera");
    ImGui::DragFloat3("Position", &session.camera.position.x, .1f);
    float angles[2]{yaw * 57.29577951f, pitch * 57.29577951f};
    if (ImGui::DragFloat2("Yaw / pitch", angles, .2f))
    {
        yaw = angles[0] * .01745329252f;
        pitch = std::clamp(angles[1] * .01745329252f, -1.55f, 1.55f);
        session.camera.rotation = inspection_rotation(yaw, pitch);
    }
    ImGui::SliderFloat("FOV", &session.camera.fov, 10.f, 150.f);
    ImGui::DragFloat("Near", &session.camera.near_clip, .01f, .01f, 100.f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::DragFloat("Far", &session.camera.far_clip, 10.f, 1.f, 5000.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    session.camera.far_clip = std::max(session.camera.far_clip, session.camera.near_clip + 1.f);
    ImGui::DragFloat("Speed", &speed, .5f, .1f, 1000.f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::Button("Reset Camera"))
    {
        session.camera = session.initial_camera;
        sync_angles = true;
    }
    ImGui::Separator();
    if (session.selection >= 0 && size_t(session.selection) < session.items.size())
    {
        const auto& item = session.items[size_t(session.selection)];
        ImGui::TextWrapped("%s", item.label.c_str());
        ImGui::TextWrapped("%s", item.source.c_str());
        ImGui::Text("%s %08X", item.marker ? "Marker ID" : "Address", item.address);
        ImGui::Text("Type %X / vtable %08X", item.type, item.vtable);
        if (!item.failure.empty())
        {
            ImGui::TextWrapped("Not loaded: %s", item.failure.c_str());
        }
        if (item.position)
        {
            ImGui::Text("Position %.3f %.3f %.3f", item.position->x, item.position->y, item.position->z);
            if (ImGui::Button("Frame Selection"))
            {
                frame_item(session);
            }
        }
        else
        {
            ImGui::TextUnformatted("Spatial fields not verified");
        }
        if (item.euler)
        {
            ImGui::Text("Euler %.3f %.3f %.3f", item.euler->x, item.euler->y, item.euler->z);
        }
        if (!item.model_resource.empty())
        {
            ImGui::TextWrapped("Model: %s", item.model_resource.c_str());
        }
        const bool editable = editor != nullptr && item.marker && editor->editing(session);
        if (ImGui::CollapsingHeader("Raw record (read only)"))
        {
            ImGui::BeginChild("raw-bytes", {0.f, 180.f}, ImGuiChildFlags_Borders);
            ImGuiListClipper clipper;
            clipper.Begin(int((item.bytes.size() + 15) / 16));
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    char text[96];
                    int at = std::snprintf(text, sizeof(text), "+%04X  ", row * 16);
                    for (size_t byte = size_t(row) * 16; byte < std::min(item.bytes.size(), size_t(row + 1) * 16); ++byte)
                    {
                        at += std::snprintf(text + at, sizeof(text) - size_t(at), "%02X ", item.bytes[byte]);
                    }
                    ImGui::TextUnformatted(text);
                }
            }
            ImGui::EndChild();
        }
        // Last: the editor may rebuild the item list, so `item` is not used after it.
        if (editable && ImGui::CollapsingHeader("Edit marker (recipe)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::PushID("marker-fields");
            editor->draw_marker_fields(session, session.selection);
            ImGui::PopID();
        }
    }
    else
    {
        ImGui::TextWrapped("Select a native object or scene marker. Unknown fields remain raw bytes.");
    }
    ImGui::End();

    panel("Session and rendering", 0, content_height, width, status_height);
    ImGui::Text("%s%s | load pumps %u | frozen native frame %u / %.3fs", state_name(session.state),
        session.state == InspectionState::Ready && !session.issues.empty() ? " / PARTIAL" : "",
        session.loading_frames, session.frozen_frame, session.frozen_time);
    if (!session.issues.empty())
    {
        ImGui::SameLine();
        const std::string label = "Errors / omissions (" + std::to_string(session.issues.size()) + ")";
        if (ImGui::Button(label.c_str()))
        {
            show_issues = true;
        }
    }
    if (!session.error.empty())
    {
        ImGui::TextWrapped("%s", session.error.c_str());
    }
    if (ImGui::Button("Reload / Retry"))
    {
        retry = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240.f);
    ImGui::InputText("##screenshot-path", screenshot_path, sizeof(screenshot_path));
    ImGui::SameLine();
    if (ImGui::Button("Capture viewport"))
    {
        capture = true;
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(capture_status.c_str());
    ImGui::Text("Native recorded commands: %zu", session.frame.draws.size());
    ImGui::SameLine();
    if (ImGui::Button("Meshes / textures"))
    {
        show_meshes = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Sound player"))
    {
        show_audio = true;
    }
    if (assets != nullptr)
    {
        ImGui::SameLine();
        if (ImGui::Button("Asset viewer"))
        {
            show_assets = true;
        }
    }
    if (extra_windows)
    {
        ImGui::SameLine();
        if (ImGui::Button("Scene builder"))
        {
            show_scene_builder = true;
        }
    }
    if (ImGui::BeginTable("draws", 2, ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableNextColumn();
        ImGui::BeginChild("draw-list", {0.f, 65.f});
        for (size_t index = 0; index < session.frame.draws.size(); ++index)
        {
            const auto& draw = session.frame.draws[index];
            char label[100];
            std::snprintf(label, sizeof(label), "%zu: %s / prim %X / count %u", index, draw.kind.c_str(), draw.primitive, draw.count);
            if (ImGui::Selectable(label, session.selected_draw == int(index)))
            {
                session.selected_draw = int(index);
                show_meshes = true;
            }
        }
        ImGui::EndChild();
        ImGui::TableNextColumn();
        ImGui::BeginChild("draw-state", {0.f, 65.f});
        if (session.selected_draw >= 0 && size_t(session.selected_draw) < session.frame.draws.size())
        {
            const auto& draw = session.frame.draws[size_t(session.selected_draw)];
            ImGui::Text("Texture records %08X %08X %08X %08X", draw.textures[0], draw.textures[1], draw.textures[2], draw.textures[3]);
            for (const auto& [key, value] : draw.states)
            {
                ImGui::Text("State %08X = %08X%s", key, value,
                    key == SceneD3DDevice::kVertexShader ? " (vertex shader)"
                    : key == SceneD3DDevice::kPixelShader ? " (pixel shader)" : "");
            }
        }
        ImGui::EndChild();
        ImGui::EndTable();
    }
    ImGui::End();
    if (session.state != InspectionState::Ready)
    {
        selected_mesh = -1;
        selected_submesh = -1;
    }
    if (show_meshes)
    {
        ImGui::SetNextWindowSize({760.f, 640.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Mesh statistics and textures", &show_meshes))
        {
            if (ImGui::BeginTabBar("mesh-sources"))
            {
                if (ImGui::BeginTabItem("Native draw"))
                {
                    ImGui::TextWrapped("Select a recorded draw in Session and rendering. Each submission shows its native actor/model where identified. Unique vertex counts are not yet verified.");
                    if (session.selected_draw >= 0 && size_t(session.selected_draw) < session.frame.draws.size())
                    {
                        const auto& draw = session.frame.draws[size_t(session.selected_draw)];
                        if (draw.owner != 0)
                        {
                            ImGui::Text("Native actor %08X", draw.owner);
                        }
                        if (!draw.model_resource.empty())
                        {
                            ImGui::TextWrapped("Model: %s", draw.model_resource.c_str());
                        }
                        ImGui::Text("Draw %d: %s | primitive %X | submitted count %u", session.selected_draw,
                            draw.kind.c_str(), draw.primitive, draw.count);
                        ImGui::Text("Index address %08X | streams %zu | state bindings %zu",
                            draw.index_address, draw.streams.size(), draw.states.size());
                        if (draw.kind == "Indexed" || draw.kind == "Inline")
                        {
                            if (draw.primitive >= 5 && draw.primitive <= 8)
                            {
                                const u32 triangles = draw.primitive == 5 ? draw.count / 3
                                    : draw.primitive == 8 ? (draw.count / 4) * 2
                                    : draw.count > 2 ? draw.count - 2 : 0;
                                ImGui::Text("Submitted triangles %u (includes degenerate triangles)", triangles);
                            }
                        }
                        for (const auto& [slot, stream] : draw.streams)
                        {
                            ImGui::Text("Stream %u: resource %08X, stride %u bytes", slot, stream.resource, stream.stride);
                        }
                        for (size_t stage = 0; stage < draw.textures.size(); ++stage)
                        {
                            ImGui::PushID(int(stage));
                            ImGui::Separator();
                            ImGui::Text("Texture stage %zu | record %08X", stage, draw.textures[stage]);
                            texture_preview(draw.texture_images[stage]);
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    if (show_audio)
    {
        ImGui::SetNextWindowSize({820.f, 560.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Sound player", &show_audio))
        {
            if (session.audio)
            {
                auto& audio = *session.audio;
                ImGui::Text("%zu native bank cues | scene simulation remains frozen", audio.sounds.size());
                ImGui::SetNextItemWidth(-1.f);
                ImGui::InputTextWithHint("##sound-filter", "Find bank or cue...", sound_filter, sizeof(sound_filter));
                ImGui::BeginChild("sound-cues", {0.f, 300.f}, ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);
                ImGuiTextFilter filter(sound_filter);
                for (size_t index = 0; index < audio.sounds.size(); ++index)
                {
                    const auto& sound = audio.sounds[index];
                    if (!filter.PassFilter(sound.name.c_str()) && !filter.PassFilter(sound.bank.c_str()))
                    {
                        continue;
                    }
                    ImGui::PushID(int(index));
                    if (ImGui::Selectable(sound.name.c_str(), audio.selected == int(index),
                        ImGuiSelectableFlags_None,
                        {std::max(ImGui::GetContentRegionAvail().x, ImGui::CalcTextSize(sound.name.c_str()).x), 0.f}))
                    {
                        audio.selected = int(index);
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                if (audio.selected >= 0 && size_t(audio.selected) < audio.sounds.size())
                {
                    const auto& selected = audio.sounds[size_t(audio.selected)];
                    ImGui::TextWrapped("%s", selected.name.c_str());
                    ImGui::Text("Bank %s | cue index %u", selected.bank.c_str(), selected.cue_index);
                }
                ImGui::BeginDisabled(session.state != InspectionState::Ready || audio.selected < 0);
                if (ImGui::Button("Play / Restart"))
                {
                    audio.play(size_t(audio.selected));
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::BeginDisabled(!audio.playing);
                if (ImGui::Button(audio.paused ? "Resume" : "Pause"))
                {
                    audio.pause();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Stop"))
                {
                    audio.stop();
                }
                float volume = audio.volume;
                if (ImGui::SliderFloat("Volume", &volume, 0.f, 1.f, "%.2f"))
                {
                    audio.set_volume(volume);
                }
                ImGui::Text("%s | %.2f seconds played", audio.status.c_str(), audio.seconds);
                if (!audio.current.empty())
                {
                    ImGui::TextWrapped("Preview: %s", audio.current.c_str());
                }
                if (!audio.error.empty())
                {
                    ImGui::TextWrapped("%s", audio.error.c_str());
                }
                ImGui::TextWrapped("Playback uses native cue preparation and the existing audio decoder. Authored loops play until stopped. Switching scenes stops the preview.");
            }
            else
            {
                ImGui::TextUnformatted("Load the runtime to browse its audio banks.");
            }
        }
        ImGui::End();
    }
    if (show_issues)
    {
        ImGui::SetNextWindowSize({720.f, 360.f}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Scene load errors and omissions", &show_issues))
        {
            ImGui::TextWrapped("Log: %s", session.log_path.string().c_str());
            if (!session.log_error.empty())
            {
                ImGui::TextWrapped("%s", session.log_error.c_str());
            }
            ImGui::TextWrapped("Unsupported markers are omitted only after rebuilding the native scene. "
                "Their original records remain inspectable and appear orange in the viewport.");
            ImGui::Separator();
            for (const auto& issue : session.issues)
            {
                ImGui::TextWrapped("%s", issue.c_str());
                ImGui::Separator();
            }
        }
        ImGui::End();
    }
    if (assets != nullptr && show_assets)
    {
        assets->draw(session, preview_viewport, show_assets);
    }
    if (extra_windows)
    {
        extra_windows(session);
    }
    ImGui::Render();
}
void InspectorUI::draw_editor_toolbar(InspectorSession& session)
{
    ImGui::Separator();
    if (!editor->editing(session))
    {
        ImGui::BeginDisabled(session.state != InspectionState::Ready || editor->busy());
        if (ImGui::Button("Edit this scene"))
        {
            editor->start_editing(session);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("Remakes it as a recipe in the plugin folder to move, add and delete markers.");
        if (!editor->status().empty())
        {
            ImGui::TextWrapped("%s", editor->status().c_str());
        }
        return;
    }
    if (ImGui::RadioButton("Move (G)", gizmo == Gizmo::Move))
    {
        gizmo = Gizmo::Move;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (R)", gizmo == Gizmo::Rotate))
    {
        gizmo = Gizmo::Rotate;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snap);
    if (snap)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.f);
        if (gizmo == Gizmo::Move)
        {
            ImGui::DragFloat("##step", &snap_step, 0.05f, 0.01f, 100.f, "%.2f");
        }
        else
        {
            ImGui::DragFloat("##angle", &snap_angle, 1.f, 1.f, 90.f, "%.0f deg");
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(!editor->can_undo());
    if (ImGui::Button("Undo"))
    {
        editor->undo(session);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!editor->can_redo());
    if (ImGui::Button("Redo"))
    {
        editor->redo(session);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(editor->busy());
    if (ImGui::Button(editor->pending() ? "Apply (rebuild)*" : "Apply (rebuild)"))
    {
        apply_edits(session);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("Drag a handle | Ctrl+click: move to surface | Ctrl+Shift+click: copy there | Ctrl+D | Del "
        "| Ctrl+Z/Y | Ctrl+Enter: apply");
    if (!editor->status().empty())
    {
        ImGui::TextWrapped("%s", editor->status().c_str());
    }
}

void InspectorUI::apply_edits(InspectorSession& session)
{
    if (session.selection >= 0 && size_t(session.selection) < session.items.size()
        && session.items[size_t(session.selection)].marker)
    {
        reselect = std::pair{session.items[size_t(session.selection)].type,
            session.items[size_t(session.selection)].address};
    }
    editor->apply(session);
}

void InspectorUI::draw_gizmo(InspectorSession& session, const ScreenPoint& origin, float scale, bool hovered)
{
    gizmo_hover = false;
    auto& io = ImGui::GetIO();
    if (session.selection < 0 || size_t(session.selection) >= session.items.size())
    {
        drag_handle = -1;
        return;
    }
    InspectionItem& item = session.items[size_t(session.selection)];
    if (!item.marker || !item.position)
    {
        drag_handle = -1;
        return;
    }
    const Vec3 p = *item.position;
    ImVec2 center;
    if (!to_screen(session.frame, origin, scale, p, center))
    {
        drag_handle = -1;
        return;
    }
    const Vec3& eye = session.camera.position;
    const float distance = std::sqrt((p.x - eye.x) * (p.x - eye.x) + (p.y - eye.y) * (p.y - eye.y)
        + (p.z - eye.z) * (p.z - eye.z));
    const float length = std::max(0.05f, distance * 0.12f);
    const std::array<Vec3, 3> axes{Vec3{1.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 0.f, 1.f}};
    const std::array<ImU32, 3> colors{IM_COL32(245, 90, 80, 255), IM_COL32(100, 235, 110, 255),
        IM_COL32(90, 160, 255, 255)};
    std::array<ImVec2, 3> ends{};
    std::array<bool, 3> visible{};
    for (size_t axis = 0; axis < 3; ++axis)
    {
        visible[axis] = to_screen(session.frame, origin, scale,
            {p.x + axes[axis].x * length, p.y + axes[axis].y * length, p.z + axes[axis].z * length}, ends[axis]);
    }
    std::vector<ImVec2> ring;
    const bool can_rotate = item.euler.has_value();
    if (gizmo == Gizmo::Rotate && can_rotate)
    {
        for (int step = 0; step <= 48; ++step)
        {
            const float a = float(step) / 48.f * 6.2831853f;
            ImVec2 point;
            if (to_screen(session.frame, origin, scale, {p.x + length * std::cos(a), p.y, p.z + length * std::sin(a)},
                point))
            {
                ring.push_back(point);
            }
        }
    }
    int hot = -1;
    const ImVec2 mouse = io.MousePos;
    if (hovered && drag_handle < 0)
    {
        if (gizmo == Gizmo::Move)
        {
            if (std::abs(mouse.x - center.x) < 7.f && std::abs(mouse.y - center.y) < 7.f)
            {
                hot = 3;
            }
            for (size_t axis = 0; axis < 3 && hot < 0; ++axis)
            {
                if (visible[axis] && segment_distance(mouse, center, ends[axis]) < 6.f)
                {
                    hot = int(axis);
                }
            }
        }
        else
        {
            for (size_t point = 1; point < ring.size() && hot < 0; ++point)
            {
                if (segment_distance(mouse, ring[point - 1], ring[point]) < 6.f)
                {
                    hot = 4;
                }
            }
        }
    }
    gizmo_hover = hot >= 0 || drag_handle >= 0;
    if (hot >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyCtrl)
    {
        drag_handle = hot;
        drag_start = p;
        drag_euler = item.euler.value_or(Vec3{});
        drag_mouse = {mouse.x, mouse.y};
        drag_axes = {};
        for (size_t axis = 0; axis < 3; ++axis)
        {
            if (visible[axis])
            {
                drag_axes[axis] = {(ends[axis].x - center.x) / length, (ends[axis].y - center.y) / length};
            }
        }
        if (hot == 3 && !plane_hit(session.frame, origin, scale, mouse, p.y, drag_hit))
        {
            drag_handle = -1;
        }
    }
    if (drag_handle >= 0)
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            drag_handle = -1;
        }
        else
        {
            Vec3 position = drag_start;
            Vec3 euler = drag_euler;
            const float mx = mouse.x - drag_mouse.x;
            const float my = mouse.y - drag_mouse.y;
            if (drag_handle < 3)
            {
                // Along the axis as the screen showed it when the drag began: pixels per world unit.
                const float sx = drag_axes[size_t(drag_handle)].x;
                const float sy = drag_axes[size_t(drag_handle)].y;
                const float squared = sx * sx + sy * sy;
                if (squared > 1e-6f)
                {
                    const float t = (mx * sx + my * sy) / squared;
                    const Vec3& axis = axes[size_t(drag_handle)];
                    position = {drag_start.x + axis.x * t, drag_start.y + axis.y * t, drag_start.z + axis.z * t};
                    if (snap)
                    {
                        if (drag_handle == 0)
                        {
                            position.x = snapped(position.x, snap_step);
                        }
                        else if (drag_handle == 1)
                        {
                            position.y = snapped(position.y, snap_step);
                        }
                        else
                        {
                            position.z = snapped(position.z, snap_step);
                        }
                    }
                }
            }
            else if (drag_handle == 3)
            {
                Vec3 hit;
                if (plane_hit(session.frame, origin, scale, mouse, drag_start.y, hit))
                {
                    position.x = drag_start.x + hit.x - drag_hit.x;
                    position.z = drag_start.z + hit.z - drag_hit.z;
                    if (snap)
                    {
                        position.x = snapped(position.x, snap_step);
                        position.z = snapped(position.z, snap_step);
                    }
                }
            }
            else if (drag_handle == 4)
            {
                euler.y = drag_euler.y + mx * 0.01f;
                if (snap)
                {
                    euler.y = snapped(euler.y, snap_angle * 0.01745329252f);
                }
            }
            const bool moved = position.x != p.x || position.y != p.y || position.z != p.z
                || (item.euler && (euler.x != item.euler->x || euler.y != item.euler->y || euler.z != item.euler->z));
            if (moved)
            {
                item.position = position;
                if (item.euler)
                {
                    item.euler = euler;
                }
                editor->set_marker_pose(item);
            }
        }
    }
    auto* draw = ImGui::GetWindowDrawList();
    if (gizmo == Gizmo::Move)
    {
        for (size_t axis = 0; axis < 3; ++axis)
        {
            if (visible[axis])
            {
                const bool bright = hot == int(axis) || drag_handle == int(axis);
                const ImU32 color = bright ? IM_COL32(255, 255, 255, 255) : colors[axis];
                draw->AddLine(center, ends[axis], color, 3.f);
                draw->AddCircleFilled(ends[axis], 4.5f, color);
            }
        }
        const bool bright = hot == 3 || drag_handle == 3;
        draw->AddRectFilled({center.x - 5.f, center.y - 5.f}, {center.x + 5.f, center.y + 5.f},
            bright ? IM_COL32(255, 255, 255, 230) : IM_COL32(255, 220, 80, 200));
    }
    else if (can_rotate)
    {
        const bool bright = hot == 4 || drag_handle == 4;
        if (ring.size() > 1)
        {
            draw->AddPolyline(ring.data(), int(ring.size()), bright ? IM_COL32(255, 255, 255, 255)
                : IM_COL32(100, 235, 110, 255), ImDrawFlags_None, 2.5f);
        }
        const float yaw = item.euler->y;
        ImVec2 heading;
        if (to_screen(session.frame, origin, scale, {p.x + length * std::sin(yaw), p.y, p.z + length * std::cos(yaw)},
            heading))
        {
            draw->AddLine(center, heading, IM_COL32(255, 220, 80, 255), 2.f);
        }
        if (drag_handle == 4)
        {
            char text[48];
            std::snprintf(text, sizeof(text), "yaw %.1f deg", item.euler->y * 57.29577951f);
            draw->AddText({center.x + 10.f, center.y + 10.f}, IM_COL32(255, 255, 255, 255), text);
        }
    }
    else
    {
        draw->AddText({center.x + 10.f, center.y + 10.f}, IM_COL32(255, 200, 120, 255), "no rotation field");
    }
}

void InspectorUI::place_at_cursor(InspectorSession& session, const ScreenPoint& origin, float scale, bool duplicate)
{
    if (session.selection < 0 || size_t(session.selection) >= session.items.size())
    {
        return;
    }
    const InspectionItem item = session.items[size_t(session.selection)];
    if (!item.marker || !item.position)
    {
        return;
    }
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    float nx, ny, rx, ry, depth;
    from_screen(session.frame, origin, scale, mouse, nx, ny, rx, ry);
    Vec3 target;
    // The scene's own surface under the cursor, from the depth readback; the marker's height plane without one.
    if (inspector_depth_at(rx, ry, depth) && depth < 0.99999f)
    {
        target = unproject_inspection_point(session.frame, nx, ny, depth);
    }
    else if (!plane_hit(session.frame, origin, scale, mouse, item.position->y, target))
    {
        return;
    }
    if (snap)
    {
        target.x = snapped(target.x, snap_step);
        target.z = snapped(target.z, snap_step);
    }
    if (duplicate)
    {
        const u16 id = editor->duplicate_marker(session, item, target);
        if (id != 0)
        {
            select_marker(session, item.type, id);
        }
        return;
    }
    InspectionItem& moved = session.items[size_t(session.selection)];
    moved.position = target;
    editor->set_marker_pose(moved);
}

void InspectorUI::editor_keys(InspectorSession& session)
{
    auto& io = ImGui::GetIO();
    if (io.WantTextInput)
    {
        return;
    }
    if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false))
    {
        gizmo = Gizmo::Move;
    }
    if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R, false))
    {
        gizmo = Gizmo::Rotate;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
    {
        if (io.KeyShift)
        {
            editor->redo(session);
        }
        else
        {
            editor->undo(session);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
    {
        editor->redo(session);
    }
    if (io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))
        && !editor->busy())
    {
        apply_edits(session);
        return;
    }
    if (session.selection < 0 || size_t(session.selection) >= session.items.size())
    {
        return;
    }
    const InspectionItem item = session.items[size_t(session.selection)];
    if (!item.marker)
    {
        return;
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && item.position)
    {
        const Vec3 offset{item.position->x + 1.f, item.position->y, item.position->z};
        const u16 id = editor->duplicate_marker(session, item, offset);
        if (id != 0)
        {
            select_marker(session, item.type, id);
        }
    }
    else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
    {
        if (editor->delete_marker(session, item))
        {
            session.selection = -1;
        }
    }
}
}
#endif
