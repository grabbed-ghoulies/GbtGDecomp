#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
const Game::UiImage* find_ui(const Game& g, std::string_view key);

bool scene_control_starts(const Game& g, std::string_view suffix);

// Xbox 0x0005CA60
// Submits the quad 0x0005E380 pushes. x,y is the top-left in the 640x480 HUD.
void draw_sprite(const Game::UiImage* image, float x, float y, float w, float h); // Xbox 0x0005CA60

} // namespace frontend_detail
} // namespace gbtg
