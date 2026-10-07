#pragma once

#include "frontend_types.h"

namespace gbtg
{
namespace frontend_detail
{
void draw_move_captions(const Game& g, bool texture_pass = false);

} // namespace frontend_detail
void caption_accept(const Game& g);

} // namespace gbtg
