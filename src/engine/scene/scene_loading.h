#pragma once

#include "scene_types.h"

namespace gbtg
{
namespace scene_detail
{

} // namespace scene_detail

void script_load(Game& g, std::string_view name);

// DAT_004DEDAC

// The async loader's pending records (DAT_004DEDA8); see scene_loading.cpp.
int scene_check_pending_records(); // Xbox 0x000341E0

} // namespace gbtg
