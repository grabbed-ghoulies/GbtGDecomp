#pragma once
#include "gbtg/entity.h"
namespace gbtg
{
void initialize_camera_view(EntityCamNode& view, const char* name, u8 flags); // Xbox 0x000496C0
void link_camera_view(i32 slot, EntityCamNode& view); // Xbox 0x00048C20
}
