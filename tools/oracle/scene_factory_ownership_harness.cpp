#include "scene/factory_scene_runtime.h"
#include <cstring>
#include <iostream>

using namespace gbtg;

namespace
{
void require(bool value, const char* message)
{
    if (!value)
    {
        throw Error(message);
    }
}
template<class F>
void rejects(F operation)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const Error&)
    {
        rejected = true;
    }
    require(rejected, "invalid native address mapping was accepted");
}
}

int main(int argc, char** argv)
{
    try
    {
        AidStore aids;
        aids.game_dir = argc > 1 ? argv[1] : "OriginalFiles";
        auto executable = std::make_shared<SceneExecutableResources>(aids.game_dir);
        SceneFactoryRuntime scene(aids, executable);
        require(scene.types[0x1F].parent == 2 && scene.types[0x23].parent == 2,
            "area/trigger retail parent mapping");
        require(scene.retail_getters[0x35] == 0x7C520 && scene.retail_getters[0x46] == 0xB4480,
            "imp retail getters");
        rejects([&scene]()
        {
            scene.types[0x35].vtable();
        });
        std::array<u8, 64> bytes{};
        const u32 address = scene.bind_address(bytes);
        require(scene.bind_address(bytes) == address, "duplicate registration changed address");
        require(scene.address_of(bytes.data() + 23) == address + 23,
            "interior address mapping");
        require(scene.resolve_address(address + 8, 16).data() == bytes.data() + 8,
            "interior address resolution");
        rejects([&scene, &bytes]()
        {
            scene.bind_address(std::span<u8>{bytes}.subspan(1));
        });
        rejects([&scene, address]()
        {
            scene.resolve_address(address + 60, 8);
        });
        scene.unbind_address(bytes.data());
        rejects([&scene, address]()
        {
            scene.resolve_address(address, 1);
        });
        std::array<u8, 8> parameters{};
        const u16 type = 1;
        std::memcpy(parameters.data() + 4, &type, 2);
        for (u16 identity = 1; identity <= 200; ++identity)
        {
            std::memcpy(parameters.data() + 2, &identity, 2);
            auto* object = scene.keep_object(create_factory_object(scene.factory, parameters));
            require(object != nullptr && read_u16({object->bytes.get(), object->size}, 6) == identity,
                "base object construction");
        }
        require(scene.factory.count == 200 && scene.address_count() == 200,
            "scene object/address ownership counts");
        const u32 stale = scene.address_of(scene.objects.front().bytes.get());
        scene.release_objects();
        require(scene.objects.empty() && scene.factory.count == 0 && scene.address_count() == 0,
            "scene object release did not clear ownership");
        rejects([&scene, stale]()
        {
            scene.resolve_address(stale, 1);
        });
        std::cout << "scene factory ownership checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
