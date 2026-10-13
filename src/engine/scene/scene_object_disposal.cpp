#include "scene_object_disposal.h"

namespace gbtg
{
// Xbox 0x00115F70
void dispose_runtime_scene_objects(RuntimeSceneObjectDisposalLink* first,
    ObjectFactoryRegistry& registry)
{
    auto* current = first;
    while (current != nullptr)
    {
        FactoryObject& object = current->object;
        current = current->next;
        destroy_factory_object(registry, read_u16({object.bytes.get(), object.size}, 4),
            object, nullptr);
        // The factory allocation's native heap boundary owns the matching free.
        object.bytes.reset();
        --registry.count;
    }
}
}
