#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "inspection_view.h"
#include "inspector_audio.h"
#include "gbtg/game.h"
#include "../scene/scene_inspection.h"
#include <optional>

namespace gbtg
{
enum class InspectionState
{
    Empty, Loading, Ready, Failed
};

struct InspectionItem
{
    std::string label;
    std::string source;
    u32 address = 0;
    u32 type = 0;
    u32 vtable = 0;
    u32 offset = 0;
    bool marker = false;
    std::optional<Vec3> position;
    std::optional<Vec3> euler;
    std::string model_resource;
    std::string failure;
    std::vector<u8> bytes;
};

// Read-only decoder for records already loaded by the native scene owner.
std::vector<InspectionItem> inspect_marker_records(std::span<const u8> records);
Vec3 inspection_forward(const Quat& rotation);
Quat inspection_rotation(float yaw, float pitch);
// Project with the native view/projection; returns NDC x/y and D3D depth.
bool project_inspection_point(const InspectionFrame& frame, const Vec3& point, Vec3& projected);

class InspectorSession
{
public:
    ~InspectorSession();
    void load(const std::filesystem::path& directory, const std::string& scene);
    void advance_loading();
    void present();
    void fail(const std::string& message);
    InspectionState state = InspectionState::Empty;
    std::string requested;
    std::string error;
    std::vector<std::string> issues;
    std::filesystem::path log_path;
    std::string log_error;
    SceneInspectionPolicy inspection;
    std::unique_ptr<Game> game;
    std::unique_ptr<InspectorAudio> audio;
    InspectionView camera;
    InspectionView initial_camera;
    InspectionFrame frame;
    std::vector<InspectionItem> items;
    int selection = -1;
    int selected_draw = -1;
    u32 loading_frames = 0;
    u32 frozen_frame = 0;
    float frozen_time = 0.f;
private:
    void collect_items();
    void release();
    std::vector<u8> progression_snapshot() const;
    std::vector<u8> frozen_progression_;
    bool initialized_ = false;
    u64 previous_generation_ = 0;
    u32 attempt_pumps_ = 0;
    bool preserve_camera_ = false;
    InspectionView preserved_camera_;
    std::map<SceneInspectionMarker, std::string> marker_errors_;
    void record_issue(const std::string& message);
    void recover_marker_failure(const SceneInspectionFailure& failure);
};
}
#endif
