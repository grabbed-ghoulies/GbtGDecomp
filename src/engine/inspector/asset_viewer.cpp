#if defined(GBTG_INSPECTOR_APP)
#include "asset_viewer.h"
#include "inspector_graphics.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"
#include "../scene/factory_scene_runtime.h"
#include "../scene/dialogue_resource_owner.h"
#include "imgui.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "model_io.h"
#include "../plugins/plugins.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace gbtg
{
namespace
{
const char* type_name(ResourceType type)
{
    switch (type)
    {
    case ResourceType::Texture: return "texture";
    case ResourceType::Anim: return "animation";
    case ResourceType::Model: return "model";
    default: return "other";
    }
}

bool contains(const std::string& text, const char* filter)
{
    if (filter[0] == 0)
    {
        return true;
    }
    std::string wanted(filter);
    std::string lowered(text);
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c)
    {
        return char(std::tolower(c));
    });
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c)
    {
        return char(std::tolower(c));
    });
    return lowered.find(wanted) != std::string::npos;
}

void add_entry(std::vector<AssetEntry>& entries, std::set<std::string>& seen, const BundleEntry& entry,
    const std::string& bundle)
{
    if (entry.type != ResourceType::Model && entry.type != ResourceType::Anim && entry.type != ResourceType::Texture)
    {
        return;
    }
    if (!seen.insert(entry.name).second)
    {
        return;
    }
    AssetEntry asset;
    asset.aid = entry.name;
    asset.type = entry.type;
    asset.bundle = bundle;
    asset.cpu_size = entry.cpu.size();
    asset.gpu_size = entry.gpu.size();
    if (entry.type == ResourceType::Anim && entry.cpu.size() >= 0x16)
    {
        asset.bones = read_u16(entry.cpu, 0x14);
    }
    entries.push_back(std::move(asset));
}

// A texture's frame as the native draw decodes it: its header at the CPU block, its data in the first GPU chunk.
RgbaImage decode_entry_texture(const BundleEntry& entry, int frame, TextureHeader& header)
{
    if (!parse_texture_header(entry.cpu, header))
    {
        throw Error(entry.name + " has no texture header");
    }
    std::span<const u8> gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        if (chunk.offset > gpu.size() || chunk.size > gpu.size() - chunk.offset)
        {
            throw Error(entry.name + "'s GPU chunk lies outside its data");
        }
        gpu = gpu.subspan(chunk.offset, chunk.size);
    }
    const size_t offset = size_t(header.texture_offset) + size_t(frame) * header.data_size;
    if (offset > gpu.size() || header.data_size > gpu.size() - offset)
    {
        throw Error(entry.name + "'s frame " + std::to_string(frame) + " lies outside its data");
    }
    TextureHeader tile = header;
    tile.texture_offset = 0;
    return decode_xbox_texture(tile, gpu.subspan(offset, header.data_size));
}
}

AssetViewer::~AssetViewer()
{
    clear_images();
}

void AssetViewer::clear_images()
{
    for (auto& [key, image] : images_)
    {
        gfx_release_sprite(*image);
    }
    images_.clear();
}

bool AssetViewer::wants_preview(const InspectorSession& session) const
{
    return wants_preview_ && session.preview_ready();
}

void AssetViewer::refresh(InspectorSession& session)
{
    if (!session.game)
    {
        entries_.clear();
        return;
    }
    Game& game = *session.game;
    const u64 generation = game.factory_scene ? game.factory_scene->inspection_generation : 0;
    if (generation == refreshed_generation_ && game.aids.bundles.size() == refreshed_bundles_)
    {
        return;
    }
    refreshed_generation_ = generation;
    refreshed_bundles_ = game.aids.bundles.size();
    entries_.clear();
    std::set<std::string> seen;
    std::set<std::string> loaded;
    if (game.factory_scene)
    {
        for (const auto& record : game.factory_scene->resources.registry.records)
        {
            loaded.insert(record.name);
        }
    }
    for (const auto& entry : game.aids.common.entries)
    {
        add_entry(entries_, seen, entry, "common.bnl");
    }
    for (const auto& [bundle, contents] : game.aids.bundles)
    {
        for (const auto& entry : contents.entries)
        {
            add_entry(entries_, seen, entry, bundle);
        }
    }
    for (const auto& [name, entry] : game.aids.demand_entries)
    {
        add_entry(entries_, seen, entry, "demand");
    }
    for (auto& entry : entries_)
    {
        entry.loaded = loaded.contains(entry.aid);
    }
    std::sort(entries_.begin(), entries_.end(), [](const AssetEntry& a, const AssetEntry& b)
    {
        return a.aid < b.aid;
    });
    if (disc_bundles_.empty())
    {
        std::error_code error;
        const auto root = game_dir_ / "bundles";
        for (auto it = std::filesystem::recursive_directory_iterator(root, error);
            !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
        {
            if (it->is_regular_file() && it->path().extension() == ".bnl")
            {
                disc_bundles_.push_back(std::filesystem::relative(it->path(), root).generic_string());
            }
        }
        std::sort(disc_bundles_.begin(), disc_bundles_.end());
    }
}

void AssetViewer::select(InspectorSession& session, const std::string& aid)
{
    refresh(session);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const AssetEntry& entry)
    {
        return entry.aid == aid;
    });
    const ResourceType type = found != entries_.end() ? found->type
        : session.game && session.game->aids.load_aid(aid) ? session.game->aids.load_aid(aid)->type : ResourceType{};
    tab_requested_ = true;
    if (type == ResourceType::Texture)
    {
        tab_ = Tab::Textures;
        selected_texture_ = aid;
        texture_frame_ = 0;
        fit_texture_ = true;
        return;
    }
    if (type == ResourceType::Anim)
    {
        tab_ = Tab::Animations;
        session.preview_animation(aid);
        return;
    }
    tab_ = Tab::Models;
    session.preview_model(aid);
    framed_ = false;
}

void AssetViewer::draw(InspectorSession& session, void* preview, bool& open)
{
    wants_preview_ = false;
    refresh(session);
    if (!pending_model_.empty() && session.state == InspectionState::Ready && restart_scene.empty())
    {
        if (session.preview.model != pending_model_)
        {
            select(session, pending_model_);
        }
        pending_model_.clear();
    }
    ImGui::SetNextWindowSize({1120.f, 720.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(1.f);
    if (!ImGui::Begin("Asset viewer", &open))
    {
        ImGui::End();
        return;
    }
    if (session.state != InspectionState::Ready)
    {
        ImGui::TextWrapped("The asset viewer loads models and animations through a ready scene's native services. "
            "Wait for the scene to load, or reload it.");
        ImGui::End();
        return;
    }
    if (ImGui::BeginTable("asset-layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("list", ImGuiTableColumnFlags_WidthFixed, 330.f);
        ImGui::TableSetupColumn("view", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextColumn();
        draw_list(session);
        ImGui::TableNextColumn();
        if (!status_.empty())
        {
            ImGui::TextWrapped("%s", status_.c_str());
        }
        if (!session.preview_error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 150, 100, 255));
            ImGui::TextWrapped("%s", session.preview_error.c_str());
            ImGui::PopStyleColor();
        }
        switch (tab_)
        {
        case Tab::Models:
            draw_model(session, preview);
            break;
        case Tab::Animations:
            draw_model(session, preview);
            break;
        case Tab::Textures:
            draw_texture(session);
            break;
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void AssetViewer::draw_list(InspectorSession& session)
{
    if (ImGui::BeginTabBar("asset-types"))
    {
        const Tab requested = tab_;
        const auto tab = [&](const char* label, Tab value)
        {
            const ImGuiTabItemFlags flags = tab_requested_ && requested == value ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label, nullptr, flags))
            {
                if (!tab_requested_)
                {
                    tab_ = value;
                }
                ImGui::EndTabItem();
            }
        };
        tab("Models", Tab::Models);
        tab("Animations", Tab::Animations);
        tab("Textures", Tab::Textures);
        tab_requested_ = false;
        ImGui::EndTabBar();
    }
    ImGui::Checkbox("Loaded by the scene only", &scene_only_);
    if (tab_ == Tab::Animations)
    {
        ImGui::SameLine();
        ImGui::Checkbox("Fits the model", &matching_only_);
    }
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##asset-filter", "Find AID...", filter_, sizeof(filter_));
    const ResourceType wanted = tab_ == Tab::Models ? ResourceType::Model
        : tab_ == Tab::Animations ? ResourceType::Anim : ResourceType::Texture;
    const float list_height = std::max(120.f, ImGui::GetContentRegionAvail().y - 190.f);
    ImGui::BeginChild("asset-list", {0.f, list_height}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    size_t shown = 0;
    for (const AssetEntry& entry : entries_)
    {
        if (entry.type != wanted || (scene_only_ && !entry.loaded) || !contains(entry.aid, filter_))
        {
            continue;
        }
        if (tab_ == Tab::Animations && matching_only_ && entry.bones != session.preview.joints)
        {
            continue;
        }
        ++shown;
        const bool selected = tab_ == Tab::Models ? entry.aid == session.preview.model
            : tab_ == Tab::Animations ? entry.aid == session.preview.animation : entry.aid == selected_texture_;
        ImGui::PushID(entry.aid.c_str());
        if (ImGui::Selectable(entry.aid.c_str(), selected))
        {
            status_.clear();
            if (tab_ == Tab::Models)
            {
                session.preview_model(entry.aid);
                framed_ = false;
            }
            else if (tab_ == Tab::Animations)
            {
                session.preview_animation(entry.aid);
            }
            else
            {
                selected_texture_ = entry.aid;
                texture_frame_ = 0;
                fit_texture_ = true;
            }
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s\n%s | %s | CPU %zu bytes, GPU %zu bytes%s%s", entry.aid.c_str(), type_name(entry.type),
                entry.bundle.c_str(), entry.cpu_size, entry.gpu_size, entry.loaded ? " | loaded by the scene" : "",
                entry.type == ResourceType::Anim ? (" | " + std::to_string(entry.bones) + " bones").c_str() : "");
        }
        ImGui::PopID();
    }
    if (shown == 0)
    {
        ImGui::TextWrapped(tab_ == Tab::Animations && session.preview.model.empty()
            ? "Choose a model in Models first; animations play on it."
            : "Nothing matches. Clear 'Loaded by the scene only' or open a disc bundle below.");
    }
    ImGui::EndChild();
    draw_bundles(session);
}

void AssetViewer::draw_bundles(InspectorSession& session)
{
    ImGui::SeparatorText("Disc bundles");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputTextWithHint("##bundle-filter", "Find bundle (e.g. actor, background)...", bundle_filter_,
        sizeof(bundle_filter_));
    ImGui::BeginChild("bundle-list", {0.f, 0.f}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    for (const std::string& bundle : disc_bundles_)
    {
        if (!contains(bundle, bundle_filter_))
        {
            continue;
        }
        const bool opened = bundle == "common.bnl" || session.game->aids.bundles.contains(bundle);
        ImGui::PushID(bundle.c_str());
        ImGui::BeginDisabled(opened);
        if (ImGui::SmallButton(opened ? "open" : "Open"))
        {
            try
            {
                // Host file IO: the AID store holds the bundle, so the native provider finds its resources.
                session.game->aids.load_bundle_rel(bundle);
                scene_only_ = false;
                status_ = "Opened " + bundle + "; its resources load through the native provider when chosen.";
            }
            catch (const std::exception& error)
            {
                status_ = bundle + ": " + error.what();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextUnformatted(bundle.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void AssetViewer::frame_preview(InspectorSession& session)
{
    const auto& info = session.preview;
    const auto& points = info.joint_points;
    target_ = {};
    if (points.empty() && !info.has_bounds)
    {
        distance_ = 6.f;
    }
    else
    {
        Vec3 low = info.has_bounds ? info.bounds_min : points[0];
        Vec3 high = info.has_bounds ? info.bounds_max : points[0];
        for (const Vec3& point : points)
        {
            low = {std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z)};
            high = {std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z)};
        }
        target_ = {(low.x + high.x) * .5f, (low.y + high.y) * .5f, (low.z + high.z) * .5f};
        const float dx = high.x - low.x;
        const float dy = high.y - low.y;
        const float dz = high.z - low.z;
        const float radius = std::max(0.25f, std::sqrt(dx * dx + dy * dy + dz * dz) * .5f);
        distance_ = radius / std::tan(session.preview_camera.fov * 0.00872664626f) * 1.4f;
    }
    framed_ = true;
}

void AssetViewer::orbit(InspectorSession& session)
{
    auto& camera = session.preview_camera;
    camera.rotation = inspection_rotation(yaw_, pitch_);
    const Vec3 forward = inspection_forward(camera.rotation);
    camera.position = {target_.x - forward.x * distance_, target_.y - forward.y * distance_,
        target_.z - forward.z * distance_};
    camera.near_clip = std::max(0.01f, distance_ * 0.01f);
    camera.far_clip = std::max(100.f, distance_ * 100.f);
}

void AssetViewer::draw_model(InspectorSession& session, void* preview)
{
    if (session.preview.model.empty())
    {
        ImGui::TextWrapped("Choose a model on the left. It is loaded by the native resource provider, instanced by "
            "0x000F9840 and drawn alone by 0x000F9080 in an actor's passes with this view's camera; animations are posed by the native "
            "controller.");
        return;
    }
    wants_preview_ = true;
    if (!framed_)
    {
        frame_preview(session);
    }
    const auto& info = session.preview;
    ImGui::Text("%s | %u joints", info.model.c_str(), info.joints);
    if (info.has_bounds)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("| bounds %.1f %.1f %.1f .. %.1f %.1f %.1f", info.bounds_min.x, info.bounds_min.y,
            info.bounds_min.z, info.bounds_max.x, info.bounds_max.y, info.bounds_max.z);
    }
    ImGui::Checkbox("Skeleton", &skeleton_);
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &grid_);
    ImGui::SameLine();
    ImGui::Checkbox("Full light", &session.preview_full_light);
    ImGui::SameLine();
    if (ImGui::Button("Frame"))
    {
        frame_preview(session);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.f);
    ImGui::ColorEdit3("Background", preview_background, ImGuiColorEditFlags_NoInputs);
    const bool animation_panel = tab_ == Tab::Animations || !info.animation.empty();
    const float controls = animation_panel ? 120.f : 30.f;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale = std::max(0.01f, std::min(avail.x / 640.f, std::max(60.f, avail.y - controls) / 480.f));
    const ImVec2 size{640.f * scale, 480.f * scale};
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    // One button over the whole view owns its mouse input: drags orbit/pan the camera instead of moving the window,
    // and the wheel zooms instead of scrolling it.
    ImGui::InvisibleButton("preview-view", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
        | ImGuiButtonFlags_MouseButtonMiddle);
    const bool view_hovered = ImGui::IsItemHovered();
    const bool view_active = ImGui::IsItemActive();
    const bool wheel = ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    if (preview != nullptr)
    {
        int target_width, target_height;
        gfx_size(target_width, target_height);
        const float source_scale = std::min(float(target_width) / 640.f, float(target_height) / 480.f);
        const ImVec2 uv0{(target_width - source_scale * 640.f) / (2.f * target_width),
            (target_height - source_scale * 480.f) / (2.f * target_height)};
        const ImVec2 uv1{1.f - uv0.x, 1.f - uv0.y};
        ImGui::GetWindowDrawList()->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(preview)), origin,
            {origin.x + size.x, origin.y + size.y}, uv0, uv1);
    }
    auto& io = ImGui::GetIO();
    if (view_active)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            yaw_ -= io.MouseDelta.x * 0.01f;
            pitch_ = std::clamp(pitch_ - io.MouseDelta.y * 0.01f, -1.55f, 1.55f);
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            const Vec3 forward = inspection_forward(session.preview_camera.rotation);
            const Vec3 side{std::cos(yaw_), 0.f, -std::sin(yaw_)};
            const Vec3 up{side.y * forward.z - side.z * forward.y, side.z * forward.x - side.x * forward.z,
                side.x * forward.y - side.y * forward.x};
            const float step = distance_ * 0.002f;
            target_.x += (-io.MouseDelta.x * side.x + io.MouseDelta.y * up.x) * step;
            target_.y += (-io.MouseDelta.x * side.y + io.MouseDelta.y * up.y) * step;
            target_.z += (-io.MouseDelta.x * side.z + io.MouseDelta.y * up.z) * step;
        }
    }
    if (view_hovered && wheel && io.MouseWheel != 0.f)
    {
        distance_ = std::clamp(distance_ * std::pow(0.85f, io.MouseWheel), 0.05f, 5000.f);
    }
    if (view_hovered && !view_active)
    {
        ImGui::SetTooltip("LMB: orbit | RMB/MMB: pan | wheel: zoom");
    }
    orbit(session);
    if (preview != nullptr)
    {
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
        const auto project = [&](const Vec3& world, ImVec2& pixel) -> bool
        {
            Vec3 ndc;
            if (!project_inspection_point(session.preview_frame, world, ndc))
            {
                return false;
            }
            const auto& display = session.preview_frame.display;
            const float x = display.fitted_x + (ndc.x + 1.f) * .5f * display.fitted_width;
            const float y = display.fitted_y + (1.f - ndc.y) * .5f * display.fitted_height;
            pixel = {origin.x + x * scale, origin.y + y * scale};
            return true;
        };
        if (grid_)
        {
            const float step = std::pow(10.f, std::floor(std::log10(std::max(0.1f, distance_ * .25f))));
            for (int i = -10; i <= 10; ++i)
            {
                ImVec2 a, b;
                if (project({-10.f * step, 0.f, i * step}, a) && project({10.f * step, 0.f, i * step}, b))
                {
                    draw->AddLine(a, b, IM_COL32(120, 130, 145, i == 0 ? 160 : 60));
                }
                if (project({i * step, 0.f, -10.f * step}, a) && project({i * step, 0.f, 10.f * step}, b))
                {
                    draw->AddLine(a, b, IM_COL32(120, 130, 145, i == 0 ? 160 : 60));
                }
            }
        }
        if (skeleton_)
        {
            for (size_t joint = 0; joint < info.joint_points.size(); ++joint)
            {
                ImVec2 a;
                if (!project(info.joint_points[joint], a))
                {
                    continue;
                }
                const int parent = info.joint_parents[joint];
                ImVec2 b;
                if (parent >= 0 && size_t(parent) < info.joint_points.size() && project(info.joint_points[size_t(parent)], b))
                {
                    draw->AddLine(a, b, IM_COL32(255, 210, 90, 220), 1.5f);
                }
                draw->AddCircleFilled(a, 2.5f, IM_COL32(255, 120, 60, 255));
            }
        }
        draw->PopClipRect();
    }
    if (animation_panel)
    {
        draw_animations(session, true);
    }
    draw_model_files(session);
    // Textures the model's own draws bound, from the recorded commands.
    std::vector<const RgbaImage*> textures;
    for (const auto& command : session.preview_frame.draws)
    {
        for (const RgbaImage* image : command.texture_images)
        {
            if (image != nullptr && std::find(textures.begin(), textures.end(), image) == textures.end())
            {
                textures.push_back(image);
            }
        }
    }
    if (!textures.empty() && ImGui::CollapsingHeader(("Bound textures (" + std::to_string(textures.size()) + ") and "
        + std::to_string(session.preview_frame.draws.size()) + " recorded draws").c_str()))
    {
        for (const RgbaImage* image : textures)
        {
            const float side = 96.f;
            const float fit = side / float(std::max(image->width, image->height));
            ImGui::Image(ImTextureRef(reinterpret_cast<ImTextureID>(inspector_texture_preview(*image))),
                {image->width * fit, image->height * fit});
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%d x %d", image->width, image->height);
            }
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < side)
            {
                ImGui::NewLine();
            }
        }
        ImGui::NewLine();
    }
}

void AssetViewer::draw_animations(InspectorSession& session, bool)
{
    const auto& info = session.preview;
    if (info.animation.empty())
    {
        ImGui::TextWrapped(tab_ == Tab::Animations ? "Choose an animation on the left; those that fit the model's %u "
            "joints are listed." : "No animation: the model shows its bind pose.", info.joints);
        return;
    }
    ImGui::Text("%s | %u bones | %.2f s per cycle", info.animation.c_str(), info.animation_bones, info.duration);
    if (ImGui::Button(session.preview_paused ? "Play" : "Pause"))
    {
        session.preview_paused = !session.preview_paused;
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart"))
    {
        session.seek_preview(0.f);
        session.preview_paused = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Step"))
    {
        const float step = info.duration > 0.f ? 1.f / (info.duration * 30.f) : 0.01f;
        session.seek_preview(std::fmod(info.position + step, 1.f));
    }
    ImGui::SameLine();
    if (ImGui::Button("Bind pose"))
    {
        session.preview_animation({});
    }
    float position = info.position;
    ImGui::SetNextItemWidth(-160.f);
    if (ImGui::SliderFloat("##position", &position, 0.f, 1.f, "%.3f"))
    {
        session.seek_preview(position);
    }
    ImGui::SameLine();
    ImGui::Text("%.2f / %.2f s", position * info.duration, info.duration);
    ImGui::SetNextItemWidth(200.f);
    ImGui::SliderFloat("Speed", &session.preview_speed, 0.f, 4.f, "%.2fx");
}

void AssetViewer::watch(InspectorSession& session)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    const double now = ImGui::GetTime();
    if (!live_reload || watch_dirs.empty() || now - watch_clock_ < 1.0)
    {
        return;
    }
    watch_clock_ = now;
    std::vector<std::filesystem::path> settled;
    std::error_code error;
    for (const auto& root : watch_dirs)
    {
        for (const auto& plugin : std::filesystem::directory_iterator(root, error))
        {
            const std::filesystem::path aid_dir = plugin.path() / "aid";
            if (!plugin.is_directory(error) || !std::filesystem::is_directory(aid_dir, error))
            {
                continue;
            }
            for (const auto& file : std::filesystem::directory_iterator(aid_dir, error))
            {
                const std::string name = file.path().filename().string();
                if (!file.is_regular_file(error) || name.rfind("aid_", 0) != 0 || file.path().has_extension())
                {
                    continue; // Skips aid_x.partial, written before the rename over aid_x.
                }
                const Watched seen{file.last_write_time(error), file.file_size(error)};
                auto [it, added] = watched_.try_emplace(file.path(), seen);
                if (!watch_primed_)
                {
                    continue;
                }
                if (added || it->second.time != seen.time || it->second.size != seen.size)
                {
                    it->second = seen;
                    it->second.pending = true;
                }
                else if (it->second.pending)
                {
                    it->second.pending = false;
                    settled.push_back(file.path());
                }
            }
        }
    }
    watch_primed_ = true;
    if (settled.empty() || session.state != InspectionState::Ready || !restart_scene.empty() || !session.game)
    {
        for (const auto& file : settled)
        {
            watched_[file].pending = true; // Try again once the scene is ready.
        }
        return;
    }
    import_notes_.clear();
    for (const auto& file : settled)
    {
        try
        {
            const std::string aid = plugin_register_aid(file);
            import_notes_.push_back("Live reload: " + file.string() + " changed.");
            log::info("asset viewer: live reload of %s", file.string().c_str());
        }
        catch (const std::exception& failure)
        {
            import_notes_.push_back(std::string("Live reload failed: ") + failure.what());
        }
    }
    // The model on view comes back once the restarted runtime is ready (the camera is kept by the restart).
    pending_model_ = session.preview.model;
    restart_scene = session.game->scene_script.name;
#else
    (void)session;
#endif
}

bool AssetViewer::import_glb(InspectorSession& session, const std::string& aid, const std::filesystem::path& glb,
    const std::filesystem::path& folder)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    import_notes_.clear();
    try
    {
        const std::filesystem::path written = import_model_file(glb, aid, game_dir_, folder, import_notes_);
        plugin_register_aid(written);
        // Boot a new runtime so bundles are opened again with the new model in place of the disc's.
        restart_scene = session.game->scene_script.name;
        pending_model_ = aid;
        import_notes_.push_back("Reloading " + restart_scene + " with the imported model.");
        return true;
    }
    catch (const std::exception& error)
    {
        import_notes_.push_back(std::string("Import failed: ") + error.what());
        return false;
    }
#else
    (void)session;
    (void)aid;
    (void)glb;
    (void)folder;
    return false;
#endif
}

void AssetViewer::draw_model_files(InspectorSession& session)
{
#if !defined(GBTG_RETAIL_GAME_ONLY)
    const std::string& aid = session.preview.model;
    if (aid.empty() || !ImGui::CollapsingHeader("Export / import (FBX, glTF)"))
    {
        return;
    }
    if (import_for_ != aid)
    {
        import_for_ = aid;
        const std::string file = aid + ".fbx";
        std::snprintf(glb_path_, sizeof(glb_path_), "%s", file.c_str());
        const std::filesystem::path folder = plugin_dir.empty() ? std::filesystem::path("plugins/my_models") : plugin_dir;
        std::snprintf(import_dir_, sizeof(import_dir_), "%s", folder.string().c_str());
        import_notes_.clear();
    }
    ImGui::Checkbox("Reload when a plugin model file changes", &live_reload);
    ImGui::SetNextItemWidth(-140.f);
    ImGui::InputText("Model file (.fbx, .glb)", glb_path_, sizeof(glb_path_));
    if (ImGui::Button("Export"))
    {
        import_notes_.clear();
        try
        {
            const DiscModel disc = load_disc_model(aid, game_dir_);
            import_notes_.push_back(export_model_file(*disc.entry, glb_path_));
            import_notes_.push_back("The bones keep their names; skin the new mesh to them, keep the material "
                "names (shader_...) and the Morpher channels' order, and export a binary FBX (or .glb) with skin and morphs.");
        }
        catch (const std::exception& error)
        {
            import_notes_.push_back(std::string("Export failed: ") + error.what());
        }
    }
    ImGui::SetNextItemWidth(-140.f);
    ImGui::InputText("Plugin folder", import_dir_, sizeof(import_dir_));
    if (ImGui::Button("Import as this model"))
    {
        import_glb(session, aid, glb_path_, import_dir_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Built on the disc model; it replaces it from the plugin folder's aid/.");
    for (const auto& line : import_notes_)
    {
        ImGui::TextWrapped("%s", line.c_str());
    }
#else
    (void)session;
#endif
}

const RgbaImage* AssetViewer::texture_image(InspectorSession& session, const AssetEntry& entry, int frame, bool opaque)
{
    const std::string key = entry.aid + "#" + std::to_string(frame) + (opaque ? "#opaque" : "");
    if (const auto found = images_.find(key); found != images_.end())
    {
        return found->second.get();
    }
    const BundleEntry* bundle = session.game->aids.load_aid(entry.aid);
    if (bundle == nullptr)
    {
        throw Error("the AID store no longer finds " + entry.aid);
    }
    TextureHeader header;
    auto image = std::make_unique<RgbaImage>(decode_entry_texture(*bundle, frame, header));
    if (opaque)
    {
        for (size_t at = 3; at < image->bgra.size(); at += 4)
        {
            image->bgra[at] = 255;
        }
    }
    image->serial = image_serial_++;
    if (images_.size() >= 48)
    {
        clear_images();
    }
    return images_.emplace(key, std::move(image)).first->second.get();
}

void AssetViewer::draw_texture(InspectorSession& session)
{
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const AssetEntry& entry)
    {
        return entry.aid == selected_texture_;
    });
    if (found == entries_.end())
    {
        ImGui::TextWrapped("Choose a texture on the left. Textures are decoded for presentation with the same decoder "
            "the native draw's texture cache uses; nothing is written back.");
        return;
    }
    const BundleEntry* bundle = session.game->aids.load_aid(found->aid);
    TextureHeader header;
    if (bundle == nullptr || !parse_texture_header(bundle->cpu, header))
    {
        ImGui::TextWrapped("%s has no texture header.", found->aid.c_str());
        return;
    }
    const int frames = std::max<int>(1, header.tile_count);
    texture_frame_ = std::clamp(texture_frame_, 0, frames - 1);
    ImGui::Text("%s", found->aid.c_str());
    ImGui::Text("%u x %u | %s | %d frame%s | %u bytes per frame | flags %08X | %s", header.width, header.height,
        xbox_format_name(header.format), frames, frames == 1 ? "" : "s", header.data_size, header.flags,
        found->bundle.c_str());
    ImGui::Checkbox("Alpha", &texture_alpha_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.f);
    ImGui::SliderFloat("Zoom", &texture_zoom_, 0.125f, 8.f, "%.3fx", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    if (ImGui::Button("1:1"))
    {
        texture_zoom_ = 1.f;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit"))
    {
        fit_texture_ = true;
    }
    if (frames > 1)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.f);
        ImGui::SliderInt("Frame", &texture_frame_, 0, frames - 1);
    }
    ImGui::SetNextItemWidth(300.f);
    ImGui::InputText("##export", export_path_, sizeof(export_path_));
    ImGui::SameLine();
    const RgbaImage* image = nullptr;
    try
    {
        image = texture_image(session, *found, texture_frame_, !texture_alpha_);
    }
    catch (const std::exception& error)
    {
        ImGui::TextWrapped("%s", error.what());
        return;
    }
    if (ImGui::Button("Save PNG"))
    {
        try
        {
            write_png(export_path_, *image);
            status_ = std::string("Saved ") + export_path_ + ". It is a disc texture: keep it local.";
        }
        catch (const std::exception& error)
        {
            status_ = error.what();
        }
    }
    if (fit_texture_)
    {
        // Fit the view, up to 8x for small textures.
        const ImVec2 room = ImGui::GetContentRegionAvail();
        texture_zoom_ = std::clamp(std::min((room.x - 16.f) / float(image->width), (room.y - 16.f) / float(image->height)),
            0.125f, 8.f);
        fit_texture_ = false;
    }
    ImGui::BeginChild("texture-view", {0.f, 0.f}, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    const ImVec2 size{image->width * texture_zoom_, image->height * texture_zoom_};
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    // A checkerboard under the image shows its alpha.
    const float cell = 8.f;
    for (float y = 0.f; y < size.y; y += cell)
    {
        for (float x = 0.f; x < size.x; x += cell)
        {
            const bool dark = (int(x / cell) + int(y / cell)) % 2 == 0;
            draw->AddRectFilled({origin.x + x, origin.y + y},
                {origin.x + std::min(size.x, x + cell), origin.y + std::min(size.y, y + cell)},
                dark ? IM_COL32(70, 70, 76, 255) : IM_COL32(110, 110, 118, 255));
        }
    }
    draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(inspector_texture_preview(*image))), origin,
        {origin.x + size.x, origin.y + size.y});
    // One button over the image and the view's free space owns the mouse: drag pans, the wheel zooms.
    const ImVec2 room = ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("texture-input", {std::max(size.x, room.x), std::max(size.y, room.y)},
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool wheel = ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
    const auto& io = ImGui::GetIO();
    if (ImGui::IsItemActive())
    {
        ImGui::SetScrollX(ImGui::GetScrollX() - io.MouseDelta.x);
        ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
    }
    if (ImGui::IsItemHovered())
    {
        const int x = int((io.MousePos.x - origin.x) / texture_zoom_);
        const int y = int((io.MousePos.y - origin.y) / texture_zoom_);
        if (!ImGui::IsItemActive() && x >= 0 && y >= 0 && x < image->width && y < image->height)
        {
            const u8* texel = image->bgra.data() + (size_t(y) * image->width + x) * 4;
            ImGui::SetTooltip("%d, %d\nR %u G %u B %u A %u", x, y, texel[2], texel[1], texel[0], texel[3]);
        }
        if (wheel && io.MouseWheel != 0.f)
        {
            texture_zoom_ = std::clamp(texture_zoom_ * std::pow(1.25f, io.MouseWheel), 0.125f, 8.f);
        }
    }
    ImGui::EndChild();
}
}
#endif
