#pragma once
// The inspector's asset viewer (docs/INSPECTOR.md, "Asset viewer"): models and animations drawn and posed by the
// native runtime (InspectorSession::preview_model), and textures decoded for presentation. It reads resources only.
#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gbtg
{
struct AssetEntry
{
    std::string aid;
    ResourceType type{};
    std::string bundle; // Where the AID store finds it: "common.bnl", a bundle under bundles/, or "demand".
    size_t cpu_size = 0;
    size_t gpu_size = 0;
    u32 bones = 0; // Animations: the MINA's bone count (+0x14).
    bool loaded = false; // In the scene's native resource registry.
};

class AssetViewer
{
public:
    explicit AssetViewer(std::filesystem::path game_dir) : game_dir_(std::move(game_dir))
    {
    }
    ~AssetViewer();
    // Draws the window while `open`. `preview` is the preview viewport (capture slot 1), or null.
    void draw(InspectorSession& session, void* preview, bool& open);
    // Whether this frame should draw the model preview: the window is open on the Models tab with a model.
    bool wants_preview(const InspectorSession& session) const;
    // Opens a model or texture by AID (command line and tests).
    void select(InspectorSession& session, const std::string& aid);
    float preview_background[4]{0.16f, 0.17f, 0.2f, 1.f};
    // The orbit angles in radians (command line --asset-view).
    void set_view(float yaw, float pitch)
    {
        yaw_ = yaw;
        pitch_ = pitch;
    }

private:
    enum class Tab
    {
        Models, Animations, Textures
    };
    void refresh(InspectorSession& session);
    void draw_list(InspectorSession& session);
    void draw_bundles(InspectorSession& session);
    void draw_model(InspectorSession& session, void* preview);
    void draw_animations(InspectorSession& session, bool compact);
    void draw_texture(InspectorSession& session);
    void frame_preview(InspectorSession& session);
    void orbit(InspectorSession& session);
    const RgbaImage* texture_image(InspectorSession& session, const AssetEntry& entry, int frame, bool opaque);
    void clear_images();

    std::filesystem::path game_dir_;
    std::vector<AssetEntry> entries_;
    std::vector<std::string> disc_bundles_;
    u64 refreshed_generation_ = ~u64(0);
    size_t refreshed_bundles_ = 0;
    Tab tab_ = Tab::Models;
    bool tab_requested_ = false; // select() chose tab_; the tab bar selects it on the next draw.
    bool wants_preview_ = false;
    bool scene_only_ = true;
    bool matching_only_ = true;
    bool skeleton_ = true;
    bool grid_ = true;
    bool texture_alpha_ = true;
    float texture_zoom_ = 1.f;
    bool fit_texture_ = false;
    int texture_frame_ = 0;
    char filter_[128]{};
    char animation_filter_[128]{};
    char bundle_filter_[128]{};
    char export_path_[260] = "texture.png";
    std::string selected_texture_;
    std::string status_;
    // The orbit camera around the model at the origin.
    Vec3 target_{};
    float yaw_ = 0.7f;
    float pitch_ = -0.35f;
    float distance_ = 6.f;
    bool framed_ = false;
    // Decoded presentation copies; released from the D3D11 texture cache when dropped.
    std::map<std::string, std::unique_ptr<RgbaImage>> images_;
    u32 image_serial_ = 1;
};
}
#endif
