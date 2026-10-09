#pragma once

#include "gbtg/types.h"

#include <vector>

namespace gbtg
{
// Stack message sent through a controller's vtable +0xC by 0x0013E8D0.
struct GhoulyRequest
{
    u32 id = 0xA8;
    char box[0x80]{};       // +4
    char attributes[0x80]{}; // +0x84
    u32 result = 0;         // +0x104, written by the receiving controller
};

struct GhoulyController
{
    void (*message)(GhoulyController& controller, void* message) = nullptr;
};

struct GhoulyControllerEntry
{
    u32 key = 0;
    GhoulyController* controller = nullptr;
};

// Host representation of the stride-8 pool at DAT_00512B74.
extern std::vector<GhoulyControllerEntry> g_ghouly_controllers;

u32 request_ghoulybox(const char* box, u32 key, const char* attributes); // Xbox 0x0013E8D0
void remove_ghouly_controller(GhoulyController* controller); // Xbox 0x0012ACA0
}
