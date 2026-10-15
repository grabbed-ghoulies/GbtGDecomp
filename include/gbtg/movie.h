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

// Xbox 0x001071A0. Name is an aid_movie_* string. A failed open leaves the list unchanged.
void movie_alloc(const char* name, unsigned flags);


// 0x00106CD0. Copy the live frame into any texture whose movie slot matches.
void movie_apply(Mesh& mesh);

// One step of the movie clock. BinkWait inside the library reads a timer; the host uses this dt.
void movie_advance_time(float dt);

// Walk DAT_0050E0B8 and run 0x00107420 on each node.
void movie_present();


// Latest BGRA frame for that aid, or null before the first decoded frame.
const RgbaImage* movie_image(const char* name);

} // namespace gbtg
