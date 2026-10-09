#include "../src/engine/scene/cutscene_resource_views.h"
#include "../src/engine/camera/display_clip_limits.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace
{
using namespace gbtg;
void require(bool value, const char* message)
{
    if (!value)
    {
        throw Error(message);
    }
}
template<class T>
void store(std::vector<u8>& bytes, size_t offset, T value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
std::vector<u8> make_resource()
{
    std::vector<u8> bytes(0x400);
    bytes[2] = 1;
    bytes[3] = 1;
    store(bytes, 4, 2.f);
    store(bytes, 8, u32{0x90});
    store(bytes, 12, u32{0x20});
    store(bytes, 0x20, u32{0x350});
    store(bytes, 0x28, u32{0x370});
    bytes[0x30] = 0xFE;
    store(bytes, 0x94, u32{0xA0});
    std::strcpy(reinterpret_cast<char*>(bytes.data() + 0x350), "aid_model_fixture");
    std::memcpy(bytes.data() + 0x370, "MINA", 4);
    for (size_t index = 0; index < 6; ++index)
    {
        const size_t curve = 0x3C + index * 12;
        store(bytes, curve, u16{2});
        store(bytes, curve + 4, 2.f);
        store(bytes, curve + 8, u32{0x300});
    }
    for (size_t index = 0; index < 8; ++index)
    {
        const size_t curve = 0xA0 + index * 12;
        store(bytes, curve, u16{2});
        store(bytes, curve + 4, 2.f);
        store(bytes, curve + 8, u32{0x260});
    }
    store(bytes, 0x300, 7.f);
    store(bytes, 0x304, 11.f);
    return bytes;
}
void test_resource_provider()
{
    auto bytes = make_resource();
    std::array<AnimationBundleRecord, 1> records
    {{
        {"aid_cutscene_fixture", 0, bytes, {}, {}, {}}
    }};
    std::array<AnimationBundlePool, 1> pools{{{records, true}}};
    AnimationResourceRegistry registry;
    SceneCutsceneResourceViews output;
    register_scene_cutscene_bundle_provider(registry, pools, output, 3, &output);
    const auto resource = acquire_animation_resource(registry, "aid_cutscene_fixture");
    require(resource.data() == bytes.data(), "provider changed raw resource storage");
    require(records[0].references == 1, "provider did not retain the real bundle record");
    const auto& view = output.views.at(resource.data());
    require(view.tracks.size() == 1 && view.cameras.size() == 1, "resource counts changed");
    require(view.tracks[0].id == -2, "signed actor track id lost");
    require(std::strcmp(view.tracks[0].model, "aid_model_fixture") == 0, "model pointer relocation failed");
    require(view.tracks[0].animation.data() == bytes.data() + 0x370, "embedded animation relocation failed");
    require(view.tracks[0].position.position[0].values == reinterpret_cast<const float*>(bytes.data() + 0x300),
        "actor curve should be resource relative");
    require(view.cameras[0].position[0].values == reinterpret_cast<const float*>(bytes.data() + 0x300),
        "camera curve should be channel relative");
    require(view.cameras[0].aspect.values == nullptr, "nullable aspect curve pointer changed");
    release_animation_resource(registry, resource);
    require(output.views.contains(resource.data()), "release discarded storage before provider unload");
    registry.providers[0].unload(registry.providers[0].handle, "aid_cutscene_fixture", registry.loaded[0]);
    require(records[0].references == 0 && output.views.empty() && output.storage.empty(),
        "provider unload did not release views and record");
}
void test_clip_limits()
{
    const std::array<float, 12> identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    DisplayClipBounds bounds{};
    float far_clip = 100.f;
    float near_clip = 19.f;
    extend_display_clip_limits(identity, bounds, far_clip, near_clip);
    require(std::fabs(far_clip - 1650.f) < 0.001f && near_clip == 0.5f, "default clip extension failed");
    bounds.active = 1;
    bounds.corners[7].z = -2000.f;
    far_clip = 100.f;
    extend_display_clip_limits(identity, bounds, far_clip, near_clip);
    require(std::fabs(far_clip - 2310.f) < 0.001f, "eighth scene corner was not considered");
    far_clip = std::numeric_limits<float>::quiet_NaN();
    extend_display_clip_limits(identity, bounds, far_clip, near_clip);
    require(std::isnan(far_clip), "unordered clip comparison changed");
}
}
int main()
{
    try
    {
        test_resource_provider();
        test_clip_limits();
        std::cout << "cutscene resource provider and clip limits passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
