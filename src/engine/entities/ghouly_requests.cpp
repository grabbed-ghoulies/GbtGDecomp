#include "ghouly_requests.h"

#include <cstddef>
#include <cstring>

namespace gbtg
{
std::vector<GhoulyControllerEntry> g_ghouly_controllers;

static_assert(offsetof(GhoulyRequest, box) == 4);
static_assert(offsetof(GhoulyRequest, attributes) == 0x84);
static_assert(offsetof(GhoulyRequest, result) == 0x104);
static_assert(sizeof(GhoulyRequest) == 0x108);

// Xbox 0x0013E8D0
u32 request_ghoulybox(const char* box, u32 key, const char* attributes)
{
    GhoulyRequest request;
    for (const GhoulyControllerEntry& entry : g_ghouly_controllers)
    {
        if (entry.key == key)
        {
            if (entry.controller != nullptr)
            {
                std::strcpy(request.box, box);
                if (attributes != nullptr)
                {
                    std::strcpy(request.attributes, attributes);
                }
                entry.controller->message(*entry.controller, &request);
            }
            return request.result;
        }
    }
    return request.result;
}

// Xbox 0x0012ACA0
void remove_ghouly_controller(GhoulyController* controller)
{
    for (auto entry = g_ghouly_controllers.begin(); entry != g_ghouly_controllers.end(); ++entry)
    {
        if (entry->controller == controller)
        {
            g_ghouly_controllers.erase(entry);
            return;
        }
    }
}
}
