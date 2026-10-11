#pragma once
#include "ghouly_spawn_record.h"
#include "../runtime/reference_names.h"
#include "gbtg/anim.h"

namespace gbtg
{
RuntimeReferenceNames** collect_runtime_ghouly_names(const GhoulySpawnRecord& record); // Xbox 0x000F3C80
u16 runtime_executable_actor_type(const char* name,
    std::span<ExecutableResourcePool> resources); // Xbox 0x0013E820
}
