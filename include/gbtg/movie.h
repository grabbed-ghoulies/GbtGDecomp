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

// One step of the movie clock. BinkWait inside the library reads a timer; the host uses this dt.
void movie_advance_time(float dt);

} // namespace gbtg
