#pragma once

#include "gbtg/model.h"
#include "gbtg/texture.h"

#include <filesystem>

namespace gbtg
{

struct Game;

// Disc root. Paths are `<root>/movie/<aid hash>`, the file `d:\movie\%08x` names.
void movie_set_root(const std::filesystem::path& game_dir);

// WinMain 0x00032390. GetFileAttributes on `d:\movie\0de65e94`. A miss calls GetLastError and continues.
void movie_boot_probe();

// Closes every node. Scene entry calls this before the scene's own opens.
void movie_reset();

// Xbox 0x001071A0. Name is an aid_movie_* string. A failed open leaves the list unchanged.
void movie_alloc(const char* name, unsigned flags);

// 0x000E01B0. Empty-slot movies, then the chapter movies. Flags are 0x14.
void movie_open_gameselect();

// Attract logo. 0x00107030 compares the opened name with this aid.
void movie_open_attract();


// 0x00106CD0. Copy the live frame into any texture whose movie slot matches.
void movie_apply(Mesh& mesh);

// Contents plates. Flags 6: open now, loop, and start the play count.
void movie_open_contents();

// One step of the movie clock. BinkWait inside the library reads a timer; the host uses this dt.
void movie_advance_time(float dt);

// Walk DAT_0050E0B8 and run 0x00107420 on each node.
void movie_present();

// The draw that shows a plate increments the play count at +0x18 while it is below 1.
void movie_note_visible(const char* name);
// Menu widget movie ownership (+0x18), released when its selection changes.
void movie_release_visible(const char* name);
// Xbox 0x00107360: copies one pending decoded frame into the widget texture.
bool movie_copy_frame(const char* name, RgbaImage& target);
bool movie_is_open(const char* name);

// Latest BGRA frame for that aid, or null before the first decoded frame.
const RgbaImage* movie_image(const char* name);

} // namespace gbtg
