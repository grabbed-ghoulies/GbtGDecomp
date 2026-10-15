#pragma once
#include "gbtg/types.h"

#include <cstdint>

namespace gbtg
{
namespace scene_detail
{
// The XDK file and save-game pieces in xapi_runtime.cpp. Xbox paths on U: reach the host through
// xbox_host_path (src/engine/saves/save_root.h).
std::uintptr_t xapi_find_first_file(const char* path, void* find_data); // Xbox 0x0014B122
bool xapi_find_next_file(std::uintptr_t find_handle, void* find_data); // Xbox 0x0014B22F
void xapi_close_find_file(std::uintptr_t find_handle); // CloseHandle (0x0014B385) on a find handle.
void xapi_append_save_directory_hash(const u16* name, char* dest, int remaining); // Xbox 0x0014C2D5
int xapi_read_save_directory_entry(u8* find_data, const char* root, int root_len); // Xbox 0x0014C532
u32 xapi_create_save_metadata(const char* path, const u16* expected); // Xbox 0x0014C633
void xapi_set_save_directory_time(const char* path); // Xbox 0x0014C6B3
bool xapi_create_directory(const char* path, const void* security); // Xbox 0x0014D217
bool xapi_remove_directory(const char* path); // Xbox 0x0014D283
struct XapiCalcSigContext;
std::uintptr_t xapi_begin_keyed_signature(u32 flags, u32 title_id); // Xbox 0x0014CD37
std::uintptr_t xapi_begin_sha_signature(); // Xbox 0x0014CE19
u32 xapi_update_signature(XapiCalcSigContext* ctx, const u8* data, u32 size); // Xbox 0x0014CE4F
u32 xapi_end_keyed_signature(XapiCalcSigContext* ctx, u8* signature); // Xbox 0x0014CDB7
u32 xapi_end_sha_signature(XapiCalcSigContext* ctx, u8* signature); // Xbox 0x0014CE69
}
}
