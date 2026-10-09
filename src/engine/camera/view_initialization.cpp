#include "view_initialization.h"
#include "display_view.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00048C20
void link_camera_view(i32 slot, EntityCamNode& view)
{
    using namespace entity_detail;
    EntityCamNode* root = &g_cam_anchor[slot];
    while (root->prev != nullptr)
    {
        root = root->prev;
    }
    EntityCamNode* next = root->next;
    root->next = &view;
    view.prev = root;
    view.next = next;
    if (next != nullptr)
    {
        next->prev = &view;
    }
    if (g_cam_active[slot] == nullptr)
    {
        camera_bind_named_view(slot, view.name);
    }
}

// Xbox 0x000496C0
void initialize_camera_view(EntityCamNode& view, const char* name, u8 flags)
{
    view.position.z = 0.f;
    view.position.y = 0.f;
    view.position.x = 0.f;
    view.flags = flags;
    if ((flags & 1) == 0)
    {
        view.rotation.x = 0.f; // DAT_002038F0
        view.rotation.y = 0.f;
        view.rotation.z = 0.f;
        view.rotation.w = 1.f; // DAT_002038F4
    }
    else
    {
        view.rotation.z = 0.f;
        view.rotation.y = 0.f;
        view.rotation.x = 0.f;
    }
    view.focal = -1.f; // DAT_0032B120
    view.aspect_slot = 3;
    view.clip_limit = std::bit_cast<float>(0x4E6E6B28u); // DAT_0032B30C
    view.blend = 0.f;
    char* output = view.name;
    char unit;
    do
    {
        unit = *name;
        *output = unit;
        ++name;
        ++output;
    }
    while (unit != 0);
    view.prev = nullptr;
    view.next = nullptr;
    i32 fit_width;
    i32 fit_x;
    i32 fit_height;
    i32 fit_y;
    float field_of_view;
    camera_fit_display_view(view.focal, view.aspect_slot, fit_width, fit_x,
        fit_height, fit_y, field_of_view, view.pixel_aspect, view.focal_pixels);
}
}
