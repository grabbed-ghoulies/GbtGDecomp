#pragma once
#include "../runtime/name_lookup.h"

#include "../scene/scene_types.h"

namespace gbtg
{
// 0x00423194.
extern Game* g_transition_game;

extern i32 g_546338;

// transition+0x10: clip
extern i32 g_54633c;

// Xbox 0x000357D0.

// Xbox 0x0012C2D0. EDI receives the clip, ESI the signed map route.
int transition_set_clip_route(std::string_view name, i32& clip, i32& route); // Xbox 0x0012C2D0

// Xbox 0x0012C830.
void transition_select_clip(i32 phase); // Xbox 0x0012C830

// Xbox 0x0012C1A0

// Xbox 0x0012C1D0. Load the entering texture and start the slime cue.
void transition_load_enter_texture(Game& g); // Xbox 0x0012C1D0

// Xbox 0x0012C270. Replace the entering texture with the exit resource.
void transition_load_exit_texture(Game& g); // Xbox 0x0012C270

// Xbox 0x0012C1A0. Drop the transition's resource ownership.
void transition_release_texture(); // Xbox 0x0012C1A0

// Xbox 0x0012BDD0. The exit cue crosses 80 percent of the remaining time.
int transition_tick_exit_cue(Game& g); // Xbox 0x0012BDD0

// Xbox 0x0012B640. The map-book cases are 3..7.
void transition_initialize_map_book(Game& g); // Xbox 0x0012B640

// Xbox 0x0012B370. The optional chapter-title dialog is absent for maps.
int transition_initialize_chapter_title(Game& g); // Xbox 0x0012B370

// Xbox 0x0012B790 / 0x00047370.
void transition_release_book(); // Xbox 0x0012B790

// Xbox 0x0012B5A0. The allocation and meshes belong to this transition,
// independently of the scene teardown/load that runs while it is open.
// Xbox 0x0012B5A0
void transition_initialize_book(); // Xbox 0x0012B5A0

} // namespace gbtg
