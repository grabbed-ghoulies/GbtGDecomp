#if defined(GBTG_INSPECTOR_APP)
#include "inspector_session.h"
#include "gbtg/engine_internal.h"
#include "gbtg/gfx.h"
#include "gbtg/log.h"
#include "gbtg/model_runtime.h"
#include "../scene/factory_scene_runtime.h"
#include "../scene/scene_actor_services.h"
#include "../scene/executable_resources.h"
#include "../scripts/script_conditions.h"
#include "../audio/pcm_backend.h"
#include "../scene/scene.h"
#include "../scene/scene_object_requests.h"
#include "../scene/background_ambient.h"
#include "../scene/cutscene_scene_runtime.h"
#include "../transitions/scene_transitions.h"
#include "../entities/entity.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace gbtg
{
namespace
{
// Inspector cancellation, not a retail transition. A failed/interrupted load must
// never resume its script or transition handler while another scene is requested.
// Retain the boot Game and its providers; the next native load replaces the factory.
void cancel_inspection_load(Game& game)
{
    suspend_pcm_audio();
    if (g_transition_services != nullptr && g_scene_transition.phase != 0)
    {
        end_scene_transition();
    }
    script_set_default_name(game);
    // Requests and ambient registrations belong to the abandoned scene. In
    // particular a failed object construction leaves the request count intact.
    g_scene_object_requests = {};
    auto ambient = runtime_background_ambient();
    ambient.count = 0;
    ambient.vary_enabled = 0;
    ambient.phase_enabled = 0;
    // Preserve the host refresh-object binding at +0x230, installed at boot.
    std::memset(g_510e58, 0, 0x230);
    g_scene_transition = {};
    g_50484c = 0;
    g_510cec = 0;
    g_4dea24 = 0;
    g_510ce8 = 0;
}

template<class T>
T value_at(std::span<const u8> bytes, size_t offset)
{
    if (offset > bytes.size() || sizeof(T) > bytes.size() - offset)
    {
        throw Error("inspector field is outside the native record");
    }
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::string hex(u32 value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%08X", value);
    return text;
}

bool spatial_marker(u16 kind)
{
    // These handlers read Vec3 at +0xC. Chain/name/attachment-only records are excluded.
    switch (kind)
    {
    case 2: case 3: case 4: case 5: case 6: case 8: case 0xA: case 0xB:
    case 0xD: case 0xE: case 0xF: case 0x10: case 0x11: case 0x13:
    case 0x14: case 0x15: case 0x16: case 0x17: case 0x18:
        return true;
    default:
        return false;
    }
}
}

std::vector<InspectionItem> inspect_marker_records(std::span<const u8> records)
{
    std::vector<InspectionItem> items;
    if (records.empty())
    {
        return items;
    }
    const u32 count = value_at<u32>(records, 0);
    const u32 used = value_at<u32>(records, 4);
    if (used < 8 || used > records.size())
    {
        throw Error("inspector: native marker block has an invalid used size");
    }
    size_t offset = 8;
    for (u32 index = 0; index < count; ++index)
    {
        const u32 size = value_at<u32>(records.first(used), offset);
        if (size < 10 || size > used - offset)
        {
            throw Error("inspector: native marker row has an invalid size");
        }
        const auto row = records.subspan(offset, size);
        InspectionItem item;
        item.marker = true;
        item.type = value_at<u16>(row, 4);
        item.address = value_at<u16>(row, 6);
        item.offset = u32(offset);
        item.label = "Marker " + std::to_string(item.address) + " / kind " + std::to_string(item.type);
        item.source = "DAT_0051108C + 0x" + hex(item.offset);
        item.bytes.assign(row.begin(), row.end());
        if (spatial_marker(u16(item.type)) && row.size() >= 0x18)
        {
            item.position = value_at<Vec3>(row, 0xC);
        }
        if (item.position && item.type != 8 && row.size() >= 0x24)
        {
            item.euler = value_at<Vec3>(row, 0x18);
        }
        items.push_back(std::move(item));
        offset += size;
    }
    if (offset != used)
    {
        throw Error("inspector: native marker count and used size disagree");
    }
    return items;
}

Vec3 inspection_forward(const Quat& q)
{
    return {-2.f * (q.x * q.z + q.w * q.y),
        -2.f * (q.y * q.z - q.w * q.x), -(1.f - 2.f * (q.x * q.x + q.y * q.y))};
}

Quat inspection_rotation(float yaw, float pitch)
{
    const float y = yaw * 0.5f;
    const float p = pitch * 0.5f;
    return {std::sin(p) * std::cos(y), std::cos(p) * std::sin(y),
        -std::sin(p) * std::sin(y), std::cos(p) * std::cos(y)};
}

bool project_inspection_point(const InspectionFrame& frame, const Vec3& point, Vec3& projected)
{
    const auto& v = frame.display.transform;
    const float x = v[0] * point.x + v[1] * point.y + v[2] * point.z + v[3];
    const float y = v[4] * point.x + v[5] * point.y + v[6] * point.z + v[7];
    const float z = v[8] * point.x + v[9] * point.y + v[10] * point.z + v[11];
    if (!std::isfinite(z) || z >= -frame.display.near_clip)
    {
        return false;
    }
    const auto& p = frame.projection;
    projected = {p[0] * x / -z, p[5] * y / -z, ((p[10] * z + p[11]) / -z + 1.f) * .5f};
    return std::isfinite(projected.x) && std::isfinite(projected.y)
        && projected.z >= 0.f && projected.z <= 1.f;
}

InspectorSession::~InspectorSession()
{
    release();
}

void InspectorSession::release()
{
    audio.reset();
    initialized_ = false;
    selection = -1;
    selected_draw = -1;
    items.clear();
    frame = {};
    if (game)
    {
        if (g_transition_services != nullptr && g_scene_transition.phase != 0)
        {
            end_scene_transition();
        }
        game->factory_scene.reset();
        script_set_default_name(*game);
        shutdown_pcm_audio();
        release_native_audio_boot();
        game.reset();
    }
}

void InspectorSession::fail(const std::string& message)
{
    if (audio)
    {
        audio->stop();
    }
    suspend_pcm_audio();
    state = InspectionState::Failed;
    error = "Scene " + requested + ": " + message;
    record_issue("FATAL: " + error);
    selection = -1;
    selected_draw = -1;
    items.clear();
    frame = {};
    // Keep failed storage alive until retry/teardown, but never draw or tick it again.
}

void InspectorSession::load(const std::filesystem::path& directory, const std::string& scene)
{
    if (audio)
    {
        audio->stop();
        audio->error.clear();
    }
    selection = -1;
    selected_draw = -1;
    items.clear();
    frame = {};
    previous_generation_ = game && game->factory_scene ? game->factory_scene->inspection_generation : 0;
    requested = scene;
    error.clear();
    issues.clear();
    log_error.clear();
    inspection = {};
    marker_errors_.clear();
    attempt_pumps_ = 0;
    preserve_camera_ = false;
    state = InspectionState::Loading;
    loading_frames = 0;
    try
    {
        if (!initialized_)
        {
            release();
            game = std::make_unique<Game>();
            game->inspection = &inspection;
            game->init(directory, scene);
            audio = std::make_unique<InspectorAudio>(*game, *g_native_audio);
            suspend_pcm_audio();
            initialized_ = true;
        }
        else
        {
            // Retail transitions/providers retain the boot Game and factory. The native
            // loader replaces scene contents while preserving those bindings.
            std::string aid;
            if (!scene.empty() && std::all_of(scene.begin(), scene.end(), [](unsigned char c)
                {
                    return c >= '0' && c <= '9';
                }))
            {
                const size_t index = std::stoul(scene);
                if (index < game->scene_order.size())
                {
                    aid = game->scene_order[index];
                }
            }
            else
            {
                const auto found = std::find_if(game->scene_order.begin(), game->scene_order.end(), [&](const auto& name)
                {
                    return name == scene || name.find(scene) != std::string::npos;
                });
                if (found != game->scene_order.end())
                {
                    aid = *found;
                }
            }
#if !defined(GBTG_RETAIL_GAME_ONLY)
            // A plugin's scene, outside the scene order (docs/PLUGINS.md).
            if (aid.empty() && scene.starts_with("aid_script_"))
            {
                aid = scene;
            }
#endif
            if (aid.empty())
            {
                throw Error("scene selection does not match the native scene directory");
            }
            cancel_inspection_load(*game);
            script_request(*game, aid);
        }
    }
    catch (const std::exception& ex)
    {
        fail(ex.what());
    }
}

void InspectorSession::restart(const std::filesystem::path& directory, const std::string& scene)
{
    release();
    // A load that threw part-way through construction can leave its objects linked on the native world lists
    // (DAT_00511AA0..), which outlive the Game. The new runtime starts with none.
    entity_detail::g_world = {};
    load(directory, scene);
}

void InspectorSession::advance_loading()
{
    if (state != InspectionState::Loading)
    {
        return;
    }
    try
    {
        ++loading_frames;
        if (++attempt_pumps_ > 1200)
        {
            throw Error("native scene initialization did not finish within 1200 loading pumps (scene "
                + game->scene_name + ", actor start " + std::to_string(game->factory_scene
                ? game->factory_scene->actor_start : -1) + ")");
        }
        file_pump();
        // Loading uses the existing retail state machine, never a fabricated asset scene.
        clock_pad(game->clock, 1.f / 60.f);
        clock_frame_time(game->clock);
        script_pump(*game);
        if (!game->factory_scene || game->factory_scene->actor_start != 1)
        {
            return;
        }
        if (game->factory_scene->inspection_generation == previous_generation_)
        {
            return; // The boot-owned factory has not completed teardown for the new request yet.
        }
        auto& services = game->factory_scene->start_actors(*game);
        if (services.activation_pending())
        {
            services.activate_actors();
        }
        std::string camera_source;
        initial_camera = services.inspection_camera(&camera_source);
        if (camera_source != "active runtime camera")
        {
            record_issue("CAMERA: no active runtime camera; using " + camera_source + " for inspection");
        }
        camera = preserve_camera_ ? preserved_camera_ : initial_camera;
        preserve_camera_ = false;
        frozen_frame = game->clock.frame;
        frozen_time = game->clock.time;
        collect_items();
        frozen_progression_ = progression_snapshot();
        suspend_pcm_audio();
        state = InspectionState::Ready;
        error.clear();
    }
    catch (const SceneInspectionFailure& failure)
    {
        recover_marker_failure(failure);
    }
    catch (const std::exception& ex)
    {
        fail(ex.what());
    }
}

void InspectorSession::collect_items()
{
    auto& scene = *game->factory_scene;
    auto& services = scene.start_actors(*game);
    const auto& actors = services.actor_addresses();
    items = inspect_marker_records(scene.markers());
    for (auto& item : items)
    {
        const auto failure = marker_errors_.find({u16(item.type), u16(item.address)});
        if (failure != marker_errors_.end())
        {
            item.label += " [not loaded]";
            item.failure = failure->second;
        }
    }
    for (const auto& object : scene.objects)
    {
        InspectionItem item;
        item.address = scene.find_address(object.bytes.get());
        item.type = object.type;
        item.vtable = object.vtable ? object.vtable->address : 0;
        item.source = "Native factory";
        item.label = "Object " + hex(item.address) + " / type " + std::to_string(item.type);
        if (object.type < scene.retail_names.size() && scene.retail_names[object.type] != 0)
        {
            item.label += " / " + std::string(scene.executable->string(scene.retail_names[object.type]));
        }
        const auto actor = actors.find(item.address);
        if (actor != actors.end() && actor->second != nullptr)
        {
            item.position = actor->second->position;
            item.euler = actor->second->euler;
            if (actor->second->model != nullptr)
            {
                const auto* model = actor->second->model;
                const auto cpu = model->runtime != nullptr && model->runtime->source != nullptr
                    ? model->runtime->source->cpu.data() : model->cpu.data();
                const auto& registry = scene.resources.registry;
                for (const auto& record : registry.records)
                {
                    if (record.resource_slot > 0 && record.resource_slot <= registry.loaded.size()
                        && registry.loaded[record.resource_slot - 1].cpu.data() == cpu)
                    {
                        item.model_resource = record.name;
                        break;
                    }
                }
            }
        }
        item.bytes.assign(object.bytes.get(), object.bytes.get() + object.size);
        items.push_back(std::move(item));
    }
}

void InspectorSession::present()
{
    if (state != InspectionState::Ready)
    {
        return;
    }
    try
    {
        if (audio)
        {
            audio->update();
            if (!audio->error.empty())
            {
                record_issue("AUDIO: " + audio->error);
            }
        }
        if (game->clock.frame != frozen_frame || game->clock.time != frozen_time)
        {
            throw Error("inspection advanced the native gameplay clock");
        }
        game->factory_scene->start_actors(*game).inspect_view(camera, frame);
        if (progression_snapshot() != frozen_progression_)
        {
            throw Error("inspection drawing changed actor poses or script progression");
        }
    }
    catch (const SceneInspectionFailure& failure)
    {
        recover_marker_failure(failure);
    }
    catch (const std::exception& ex)
    {
        fail(ex.what());
    }
}

void InspectorSession::record_issue(const std::string& message)
{
    if (std::find(issues.begin(), issues.end(), message) != issues.end())
    {
        return;
    }
    issues.push_back(message);
    log::error("inspector: %s", message.c_str());
    if (!log_path.empty())
    {
        std::ofstream log(log_path, std::ios::app);
        log << "[Scene " << requested << "] " << message << '\n';
        if (!log)
        {
            log_error = "Could not append to " + log_path.string();
        }
    }
}

void InspectorSession::recover_marker_failure(const SceneInspectionFailure& failure)
{
    if (audio)
    {
        audio->stop();
    }
    // Never render or resume the graph that failed mid-construction/draw. It may contain
    // incomplete registrations. A native scene request replaces that graph before Ready.
    if (!game || !game->factory_scene || inspection.omitted.contains(failure.marker) || fail_on_marker_errors)
    {
        fail(failure.what());
        return;
    }
    try
    {
        const auto markers = inspect_marker_records(game->factory_scene->markers());
        const bool exists = std::any_of(markers.begin(), markers.end(), [&](const auto& marker)
        {
            return marker.type == failure.marker.first && marker.address == failure.marker.second;
        });
        if (!exists)
        {
            fail(failure.what());
            return;
        }
        if (state == InspectionState::Ready)
        {
            preserved_camera_ = camera;
            preserve_camera_ = true;
        }
        inspection.omitted.insert(failure.marker);
        marker_errors_[failure.marker] = failure.what();
        record_issue("OMITTED: " + std::string(failure.what()));
        error = "Rebuilding scene without marker kind " + std::to_string(failure.marker.first)
            + " id " + std::to_string(failure.marker.second);
        previous_generation_ = game->factory_scene->inspection_generation;
        const std::string aid = game->scene_script.name;
        cancel_inspection_load(*game);
        script_request(*game, aid);
        state = InspectionState::Loading;
        attempt_pumps_ = 0;
        selection = -1;
        selected_draw = -1;
        items.clear();
        frame = {};
        suspend_pcm_audio();
    }
    catch (const std::exception& error)
    {
        fail(error.what());
    }
}

std::vector<u8> InspectorSession::progression_snapshot() const
{
    std::vector<u8> result;
    const auto append = [&result](const auto& value)
    {
        const auto* bytes = reinterpret_cast<const u8*>(&value);
        result.insert(result.end(), bytes, bytes + sizeof(value));
    };
    append(game->clock.frame);
    append(game->clock.time);
    append(game->clock.frame2);
    append(game->clock.ticks300);
    append(game->clock.acc300);
    append(game->script_phase);
    if (auto* cutscene = find_scene_cutscene_runtime(*game))
    {
        append(cutscene->time);
        append(cutscene->previous_time);
        append(cutscene->index);
        append(cutscene->linked);
    }
    append(g_script_conds.size());
    for (const auto& condition : g_script_conds)
    {
        append(condition.raw);
    }
    append(g_script_fired.size());
    for (u32 fired : g_script_fired)
    {
        append(fired);
    }
    auto& scene = *game->factory_scene;
    // Address order is stable, unlike the service's unordered address map.
    const auto& actors = scene.start_actors(*game).actor_addresses();
    for (const auto& object : scene.objects)
    {
        const auto found = actors.find(scene.find_address(object.bytes.get()));
        if (found != actors.end() && found->second != nullptr)
        {
            append(found->second->position);
            append(found->second->euler);
            append(found->second->quat);
        }
    }
    return result;
}
}
#endif
