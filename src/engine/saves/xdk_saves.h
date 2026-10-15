#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// XGAME_FIND_DATA (0x344 bytes): WIN32_FIND_DATAA at +0, szSaveGameDirectory at +0x140, the wide
// szSaveGameName at +0x244.
struct XGameFindData
{
    std::array<u8, 0x344> bytes{};
    const char* directory() const
    {
        return reinterpret_cast<const char*>(bytes.data() + 0x140);
    }
    const char16_t* name() const
    {
        return reinterpret_cast<const char16_t*>(bytes.data() + 0x244);
    }
};
// The XFindFirstSaveGame handle (0x110 bytes, 'EVBS'): the directory scan, the root's length and the root.
struct XSaveGameFind
{
    u32 magic = 0x53425645;
    std::uintptr_t find = 0;
    u32 root_length = 0;
    std::array<char, 0x104> root{};
};
// The XDK save-game calls. Their file access goes to the host through xbox_host_path.
u32 create_xbox_save_game(const char* root, const char16_t* name, u32 disposition, u32 flags, char* path,
    u32 path_size); // Xbox 0x0014C70F, XCreateSaveGame
u32 delete_xbox_save_game(const char* root, const char16_t* name); // Xbox 0x0014C972, XDeleteSaveGame
u32 xbox_file_attributes(const char* path); // Xbox 0x0014B280, GetFileAttributes: -1 and the last error on failure
XSaveGameFind* find_first_xbox_save_game(const char* root, XGameFindData& data); // Xbox 0x0014CA42, null for -1
bool find_next_xbox_save_game(XSaveGameFind& find, XGameFindData& data); // Xbox 0x0014CB4C
bool close_xbox_save_game_find(XSaveGameFind* find); // Xbox 0x0014CB93, XFindClose
// The volume queries (NtQueryVolumeInformationFile, FileFsSizeInformation) on a path's volume.
u32 xbox_volume_cluster_size(const char* path); // Xbox 0x0014C162, 0 and the last error on failure
u32 xbox_volume_sector_size(const char* path); // Xbox 0x0014C201
bool xbox_volume_space(const char* path, u64* free, u64* total, u64* available); // Xbox 0x0014BC90
// XCalculateSignature*: a signature handle (-1 on failure), its update and its end.
std::uintptr_t begin_xbox_signature(u32 flags); // Xbox 0x0014CEF5 -> 0x0014CEB4
u32 update_xbox_signature(std::uintptr_t handle, const u8* data, u32 size); // Xbox 0x0014CF09
u32 end_xbox_signature(std::uintptr_t handle, u8* signature); // Xbox 0x0014CF57
u32 xbox_signature_size(u32 flags); // Xbox 0x0014CE93
u32 xbox_last_error(); // GetLastError (0x0014AFE4)
}
