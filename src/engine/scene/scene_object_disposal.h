#pragma once
#include "gbtg/object_factory.h"

namespace gbtg
{
struct RuntimeSceneObjectDisposalLink
{
    RuntimeSceneObjectDisposalLink* next; // Object +0x1C.
    FactoryObject& object; // Object = link -0x18.
};
void dispose_runtime_scene_objects(RuntimeSceneObjectDisposalLink* first,
    ObjectFactoryRegistry& registry); // Xbox 0x00115F70
}
