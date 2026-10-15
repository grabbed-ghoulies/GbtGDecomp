#pragma once
#include <filesystem>
#include <string>

namespace gbtg
{
// The one place the save path leaves the retail flow: the Xbox's U: drive (the title's UDATA folder) is a host
// folder. By default it is "saves" beside the executable, created on first use.
const std::filesystem::path& save_root();
void set_save_root(const std::filesystem::path& root); // Swaps the folder (tests use their own).
// An Xbox path on U: as a host path; any other path is returned unchanged.
std::string xbox_host_path(const char* path);
}
