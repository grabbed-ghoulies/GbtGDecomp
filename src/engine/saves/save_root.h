#pragma once
#include <filesystem>
#include <string>

namespace gbtg
{
// The one place the save path leaves the retail flow: the Xbox's U: drive (the title's UDATA folder) is a host
// folder. By default it is "saves" beside the executable, created on first use.
const std::filesystem::path& save_root();
void set_save_root(const std::filesystem::path& root); // Swaps the folder (tests use their own).
// The Xbox's Z: drive (the utility cache) is a host folder too: "cache/xbox_utility" by default. It must stay
// inside the working directory's cache folder, which the utility format checks before it wipes the folder.
std::filesystem::path utility_root();
void set_utility_root(const std::filesystem::path& root); // Swaps the folder (parallel test runs use their own).
// An Xbox path on U: as a host path; any other path is returned unchanged.
std::string xbox_host_path(const char* path);
}
