#if defined(GBTG_INSPECTOR_APP)
#include "../src/engine/inspector/inspector_session.h"
#include "../src/engine/inspector/inspector_graphics.h"
#include "../src/engine/scene/factory_scene_runtime.h"
#include "../src/engine/audio/pcm_backend.h"
#include "gbtg/gfx.h"
#include "gbtg/engine_internal.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>

namespace
{
using namespace gbtg;
LONG WINAPI report_native_fault(EXCEPTION_POINTERS* fault)
{
    const HANDLE process = GetCurrentProcess();
    SymInitialize(process, nullptr, TRUE);
    CONTEXT context = *fault->ContextRecord;
    STACKFRAME64 stack{};
    stack.AddrPC.Offset = context.Rip;
    stack.AddrFrame.Offset = context.Rbp;
    stack.AddrStack.Offset = context.Rsp;
    stack.AddrPC.Mode = stack.AddrFrame.Mode = stack.AddrStack.Mode = AddrModeFlat;
    std::cerr << "native fault " << std::hex << fault->ExceptionRecord->ExceptionCode << "\n";
    for (int i = 0; i < 24; ++i)
    {
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, stack.AddrPC.Offset, &displacement, symbol))
        {
            std::cerr << symbol->Name << "+" << displacement << "\n";
        }
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &stack, &context,
            nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
        {
            break;
        }
    }
    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw Error(message);
    }
}
void put(std::vector<u8>& data, size_t offset, u32 value)
{
    std::memcpy(data.data() + offset, &value, sizeof(value));
}
int registered_bases = 0;
const ObjectFactoryVtable* recovery_base()
{
    static const ObjectFactoryVtable table{1, base_object_sizes,
        [](FactoryObject&, std::span<const u8>)
        {
            ++registered_bases;
            return true;
        },
        [](FactoryObject&)
        {
            --registered_bases;
        }};
    return &table;
}
const ObjectFactoryVtable* recovery_missing()
{
    throw Error("test missing derived getter");
}
void test_failed_factory_unwind()
{
    const std::array<ObjectFactoryType, 4> types{{{}, {0, recovery_base},
        {1, recovery_missing}, {2, recovery_base}}};
    ObjectFactoryRegistry factory{types};
    std::vector<u8> parameters(8);
    put(parameters, 4, 3);
    bool failed = false;
    try
    {
        create_factory_object(factory, parameters);
    }
    catch (const Error&)
    {
        failed = true;
    }
    require(failed && registered_bases == 0 && factory.count == 0,
        "missing derived getter must unwind completed base registrations");
}
void test_marker_records()
{
    std::vector<u8> data(8 + 0x28 + 12);
    put(data, 0, 2);
    put(data, 4, u32(data.size()));
    put(data, 8, 0x28);
    put(data, 12, (42u << 16) | 0x13u);
    const Vec3 position{12.f, 23.f, -45.f};
    std::memcpy(data.data() + 8 + 0xC, &position, sizeof(position));
    put(data, 8 + 0x28, 12);
    put(data, 8 + 0x28 + 4, (51u << 16) | 7u);
    const auto items = inspect_marker_records(data);
    require(items.size() == 2 && items[0].address == 42, "marker ID/count");
    require(items[0].position && items[0].position->z == -45.f, "verified marker position");
    require(!items[1].position, "chain marker must not invent a transform");
    require(items[1].offset == 0x30 && items[1].bytes.size() == 12, "marker provenance");
    for (u32 bad : {0u, 4u, 0xFFFFFFFFu})
    {
        put(data, 8, bad);
        bool threw = false;
        try
        {
            inspect_marker_records(data);
        }
        catch (const Error&)
        {
            threw = true;
        }
        require(threw, "malformed row must fail safely");
    }
}
void test_camera()
{
    const Vec3 forward = inspection_forward(inspection_rotation(0.f, 0.f));
    require(std::abs(forward.z + 1.f) < .0001f, "camera -Z forward");
    const Vec3 pitched = inspection_forward(inspection_rotation(0.f, .5f));
    require(pitched.y > 0.f, "positive pitch looks up");
    InspectionFrame frame;
    frame.display.transform = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    frame.display.near_clip = 1.f;
    frame.projection[0] = 1.f;
    frame.projection[5] = 1.f;
    frame.projection[10] = -11.f / 9.f;
    frame.projection[11] = -20.f / 9.f;
    Vec3 projected;
    require(project_inspection_point(frame, {0.f, 0.f, -5.f}, projected), "point in front");
    require(std::abs(projected.x) < .0001f && projected.z > 0.f && projected.z < 1.f, "native projection depth");
    require(!project_inspection_point(frame, {0.f, 0.f, 5.f}, projected), "point behind camera");
    require(!project_inspection_point(frame, {0.f, 0.f, -11.f}, projected), "point beyond far clip");
}
void test_inspection_policy()
{
    SceneInspectionPolicy policy;
    const SceneInspectionMarker marker{4, 11};
    int calls = 0;
    bool threw = false;
    try
    {
        policy.run_marker(marker, [&]()
        {
            ++calls;
            throw Error("untranslated getter 0x000BFA70");
        });
    }
    catch (const SceneInspectionFailure& failure)
    {
        threw = true;
        require(failure.marker == marker, "failed marker identity survives exception propagation");
        require(std::string(failure.what()).find("0x000BFA70") != std::string::npos,
            "missing native address survives diagnostic wrapping");
    }
    require(threw && calls == 1, "failing native marker must raise");
    policy.omitted.insert(marker);
    require(!policy.run_marker(marker, [&]()
        {
            ++calls;
        }), "omitted marker must not execute during a rebuilt attempt");
    require(calls == 1, "omission must not retry the failed callee in the rebuilt graph");
    require(policy.run_marker({4, 12}, [&]()
        {
            ++calls;
        }), "other native markers continue on the rebuilt attempt");
    require(calls == 2, "supported marker executed once");
}

void test_empty_and_failed_session()
{
    InspectorSession session;
    require(session.state == InspectionState::Empty, "initial empty state");
    session.advance_loading();
    session.present();
    session.requested = "test_scene";
    session.selection = 4;
    session.selected_draw = 3;
    session.items.push_back({});
    session.fail("unbound 0x00123456");
    session.fail("unbound 0x00123456");
    require(session.state == InspectionState::Failed && session.items.empty(), "failed session discards inspection data");
    require(session.selection == -1 && session.selected_draw == -1, "failed session invalidates selection");
    require(session.error.find("0x00123456") != std::string::npos, "failure retains callee address");
    require(session.issues.size() == 1, "repeated failures must not flood the diagnostic list");
    session.present();
    session.advance_loading();
    require(session.state == InspectionState::Failed, "failed session never continues");
}
void test_surface_resize()
{
    for (const auto& size : {std::pair{960, 540}, std::pair{800, 600}, std::pair{1600, 900}, std::pair{960, 540}})
    {
        inspector_resize_surface(size.first, size.second);
        gfx_begin_frame();
        require(inspector_capture_viewport(true) != nullptr, "resized viewport texture");
        RgbaImage image;
        require(inspector_viewport_readback(image), "resized viewport readback");
        require(image.width == size.first && image.height == size.second, "viewport texture follows render target size");
        int width, height;
        gfx_size(width, height);
        require(width == size.first && height == size.second, "reported graphics dimensions follow resize");
        require(inspector_depth_visible(320.f, 240.f, .5f), "depth readback is recreated after resize");
        inspector_resize_surface(0, 0);
        gfx_size(width, height);
        require(width == size.first && height == size.second, "zero-sized minimized targets are ignored");
    }
    inspector_release_graphics();
}

void test_retail_session(const std::filesystem::path& directory)
{
    // Set the test-volume policy before native boot creates the mastering voice.
    pcm_set_master_volume(.1f);
    InspectorSession session;
    session.log_path = std::filesystem::temp_directory_path()
        / ("gbtg-inspector-regression-" + std::to_string(GetCurrentProcessId()) + ".log");
    const std::string opening = "chapter1_scene1_2playcam";
    const auto load_ready = [&](const std::string& scene)
    {
        session.load(directory, scene);
        for (int i = 0; session.state == InspectionState::Loading && i < 12000; ++i)
        {
            session.advance_loading();
        }
        if (session.state != InspectionState::Ready)
        {
            throw Error(session.error);
        }
    };
    load_ready(opening);
    pcm_set_master_volume(0.f); // Exercise the application's mute/preview isolation.
    const auto* boot_factory = session.game->factory_scene.get();
    require(!session.items.empty(), "native scene inspection records");
    const auto saved_camera = entity_detail::g_cam_active[0];
    const auto saved_view = entity_detail::g_display_view;
    const u32 clock = session.game->clock.frame;
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "inspection camera check " << i << "\n";
        session.camera.position.x += 5.f;
        session.camera.rotation = inspection_rotation(float(i) * .15f, .05f);
        gfx_begin_frame();
        session.present();
        require(session.state == InspectionState::Ready, session.error.c_str());
        size_t preview_count = 0;
        for (const auto& draw : session.frame.draws)
        {
            for (const auto* image : draw.texture_images)
            {
                if (image != nullptr)
                {
                    require(image->width > 0 && image->height > 0, "native texture preview dimensions");
                    require(image->bgra.size() == size_t(image->width) * size_t(image->height) * 4,
                        "native texture preview must retain decoded pixels");
                    require(inspector_texture_preview(*image) != nullptr, "native texture preview GPU upload");
                    ++preview_count;
                }
            }
        }
        require(preview_count > 0, "opening scene exposes native texture previews");
        require(std::any_of(session.frame.draws.begin(), session.frame.draws.end(), [](const auto& draw)
            {
                return draw.owner != 0 && !draw.model_resource.empty();
            }), "native draw inspection identifies its actor and model resource");
        require(session.game->clock.frame == clock, "repeated rendering must freeze clocks");
        require(entity_detail::g_cam_active[0] == saved_camera && entity_detail::g_display_view == saved_view,
            "inspection must restore native camera bindings");
    }
    RgbaImage first_frame;
    RgbaImage repeated_frame;
    gfx_begin_frame();
    session.present();
    require(gfx_readback(first_frame), "static native frame readback");
    gfx_begin_frame();
    session.present();
    require(gfx_readback(repeated_frame), "repeated native frame readback");
    require(session.state == InspectionState::Ready, session.error.c_str());
    require(first_frame.bgra == repeated_frame.bgra, "repeated inspection presentation must be pixel stable");
    require(session.audio && !session.audio->sounds.empty(), "native sound browser enumerates cues");
    auto& audio = *session.audio;
    const auto sound = std::find_if(audio.sounds.begin(), audio.sounds.end(), [](const auto& entry)
    {
        return entry.name.find("HARPUPDOWN") != std::string::npos;
    });
    require(sound != audio.sounds.end(), "native test cue is in the sound browser");
    const size_t sound_index = size_t(sound - audio.sounds.begin());
    audio.set_volume(0.f); // Automated transport checks remain silent.
    audio.play(sound_index);
    require(audio.playing && audio.error.empty(), audio.error.c_str());
    require(std::abs(pcm_inspection_output_volume() - .1f) < .001f,
        "preview must open the muted output at the test master volume");
    Sleep(100);
    audio.update();
    require(audio.seconds > 0.f, "preview submits audio samples while the scene remains frozen");
    audio.pause();
    Sleep(30);
    audio.update();
    const float paused_seconds = audio.seconds;
    Sleep(80);
    audio.update();
    require(audio.paused && std::abs(audio.seconds - paused_seconds) < .015f, "preview pause holds its sample cursor");
    audio.pause();
    Sleep(80);
    audio.update();
    require(!audio.paused && audio.seconds > paused_seconds, "preview resume advances samples");
    audio.stop();
    require(!audio.playing && !audio.paused, "preview stop releases transport state");
    require(pcm_inspection_output_volume() == 0.f, "stopping preview restores the scene master mute");
    const u32 original_index = audio.sounds[sound_index].cue_index;
    audio.sounds[sound_index].cue_index = 0xFFFFFFFF;
    audio.play(sound_index);
    require(!audio.error.empty() && !audio.playing, "unsupported cue reports a recoverable preview error");
    audio.sounds[sound_index].cue_index = original_index;
    audio.play(sound_index);
    require(audio.playing && audio.error.empty(), "sound playback recovers after a failed cue");
    session.present();
    require(session.state == InspectionState::Ready && session.game->clock.frame == clock,
        "sound preview must leave native scene progression frozen");
    session.selection = 0;
    session.selected_draw = 0;
    session.fail("test fault");
    require(session.audio && !session.audio->playing, "scene failure stops preview playback");
    require(pcm_inspection_output_volume() == 0.f, "scene failure restores the master mute");
    load_ready(opening);
    require(session.selection == -1 && session.selected_draw == -1, "retry clears stale selection");
    const auto scenes = session.game->scene_order;
    const auto alternative = std::find_if(scenes.begin(), scenes.end(), [](const std::string& scene)
    {
        return scene.find("chapter1_scene1_2playcam") == std::string::npos && scene.find("playcam") != std::string::npos;
    });
    if (alternative != scenes.end())
    {
        session.load(directory, *alternative);
        require(session.selection == -1 && session.items.empty(), "scene switch invalidates old inspection data");
        for (int i = 0; session.state == InspectionState::Loading && i < 12000; ++i)
        {
            session.advance_loading();
        }
        require(session.state == InspectionState::Ready, session.error.c_str());
        if (session.state == InspectionState::Failed)
        {
            require(!session.error.empty(), "unsupported scene must retain its failure context");
            std::cout << "alternative scene frontier: " << session.error << "\n";
        }
        else
        {
            for (int i = 0; i < 12000; ++i)
            {
                if (session.state == InspectionState::Loading)
                {
                    session.advance_loading();
                    continue;
                }
                gfx_begin_frame();
                session.present();
                if (session.state != InspectionState::Loading)
                {
                    break;
                }
            }
            require(session.state == InspectionState::Ready, session.error.c_str());
            require(!session.inspection.omitted.empty(), "unsupported scene opens as an explicit partial scene");
            require(std::any_of(session.items.begin(), session.items.end(), [](const auto& item)
                {
                    return item.marker && !item.failure.empty();
                }), "omitted markers remain inspectable with their failures");
            std::ifstream log(session.log_path);
            const std::string text((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
            require(text.find("0x000BFA70") != std::string::npos, "omission log retains missing native getter");
        }
        load_ready(opening);
        require(session.game->factory_scene.get() == boot_factory,
            "scene switching retains the native boot-owned factory");
        require(session.inspection.omitted.empty() && session.issues.empty(), "new scene clears prior omissions");
        session.present();
        require(session.state == InspectionState::Ready, session.error.c_str());
    }
    size_t failed_scenes = 0;
    for (size_t index = 0; index < std::min(size_t(12), scenes.size()); ++index)
    {
        std::cout << "scene browsing recovery " << scenes[index] << "\n";
        session.load(directory, scenes[index]);
        for (int pump = 0; pump < 12000 && session.state == InspectionState::Loading; ++pump)
        {
            session.advance_loading();
        }
        require(session.state != InspectionState::Loading, "scene browsing must finish or report failure");
        if (session.state == InspectionState::Failed)
        {
            ++failed_scenes;
            require(!session.error.empty() && !session.issues.empty(), "failed scene retains diagnostics");
        }
        load_ready(opening);
        require(session.selection == -1 && session.selected_draw == -1, "scene recovery clears selection");
        session.present();
        require(session.state == InspectionState::Ready, session.error.c_str());
    }
    require(failed_scenes > 0, "browsing regression must exercise failed-load recovery");
}
}
int main(int argc, char** argv)
{
    std::cout << std::unitbuf;
    SetUnhandledExceptionFilter(report_native_fault);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportHook([](int, char*, int*) -> int
    {
        CONTEXT context{};
        RtlCaptureContext(&context);
        EXCEPTION_RECORD record{};
        EXCEPTION_POINTERS fault{&record, &context};
        report_native_fault(&fault);
        return FALSE;
    });
#endif
    try
    {
        pcm_set_master_volume(.1f);
        pcm_set_inspection_output_volume(.1f);
        test_failed_factory_unwind();
        test_marker_records();
        test_camera();
        test_inspection_policy();
        test_empty_and_failed_session();
        if (argc == 2)
        {
            GfxConfig graphics;
            graphics.visible = false;
            require(gfx_init(graphics), "retail inspector checks require D3D11");
            test_surface_resize();
            test_retail_session(argv[1]);
            gfx_shutdown();
        }
        std::cout << "inspector checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "inspector checks: " << error.what() << "\n";
        return 1;
    }
}
#endif
