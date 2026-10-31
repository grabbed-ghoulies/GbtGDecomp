#include "xdk_saves.h"
#include "save_root.h"
#include "../runtime/crt_runtime.h"
#include "../runtime/xapi_runtime.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace gbtg
{
using namespace scene_detail;

namespace
{
size_t narrow_length(const char* text)
{
    return std::strlen(text);
}

void append(char* path, const char* tail)
{
    std::strcpy(path + narrow_length(path), tail);
}

// lstrcpynA (0x0014ACB2): at most size - 1 characters and a NUL.
void copy_narrow(char* output, const char* input, u32 size)
{
    if (size == 0)
    {
        return;
    }
    u32 index = 0;
    while (index + 1 < size && input[index] != 0)
    {
        output[index] = input[index];
        ++index;
    }
    output[index] = 0;
}

// The SaveMeta.xbx write of 0x0014C70F: NtCreateFile (GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE,
// FILE_ATTRIBUTE_SYSTEM, no sharing, FILE_OPEN_IF), then WriteFile until the text is written. A failure
// returns its Win32 error.
u32 write_save_metadata(const char* path, const std::u16string& text)
{
    const std::string host = xbox_host_path(path);
    std::fstream file(host, std::ios::binary | std::ios::in | std::ios::out);
    if (!file)
    {
        file.clear();
        file.open(host, std::ios::binary | std::ios::out);
    }
    if (!file)
    {
        return 3; // ERROR_PATH_NOT_FOUND
    }
    file.write(reinterpret_cast<const char*>(text.data()), std::streamsize(text.size() * 2));
    if (!file)
    {
        return 0x70; // ERROR_DISK_FULL
    }
    return 0;
}
}

// Xbox 0x0014C70F
u32 create_xbox_save_game(const char* root, const char16_t* name, u32 disposition, u32 flags, char* path,
    u32 path_size)
{
    char directory[0x10C]{};
    std::strcpy(directory, root);
    const size_t root_length = narrow_length(directory);
    xapi_append_save_directory_hash(reinterpret_cast<const u16*>(name), directory + root_length,
        int(0xF7 - root_length)); // 0x0014C2D5
    const size_t length = narrow_length(directory);
    const auto expected = reinterpret_cast<const u16*>(name);
    if (disposition == 3)
    {
        append(directory, "\\SaveMeta.xbx"); // 0x0020329C
        const u32 status = xapi_create_save_metadata(directory, expected); // 0x0014C633
        directory[length] = 0;
        if (status != 0)
        {
            return status;
        }
    }
    else if (xapi_create_directory(directory, nullptr)) // 0x0014D217
    {
        append(directory, "\\SaveMeta.xbx");
        // swprintf(%lc%ls%lc%ls%ls[%ls]): the byte-order mark, "Name", '=', the name, CR LF, and "NoCopy=1" CR LF
        // when flags bit 0 is set.
        std::u16string text;
        text += char16_t(0xFEFF);
        text += u"Name";
        text += u'=';
        text += name;
        text += u"\r\n";
        if ((flags & 1) != 0)
        {
            text += u"NoCopy=1\r\n";
        }
        const u32 status = write_save_metadata(directory, text);
        directory[length] = 0;
        if (status != 0)
        {
            xapi_remove_directory(directory); // 0x0014D283
            return status;
        }
    }
    else
    {
        const u32 error = g_xapi_last_error; // GetLastError (0x0014AFE4)
        u32 status = 0x491;
        if (error == 0xB7)
        {
            append(directory, "\\SaveMeta.xbx");
            status = xapi_create_save_metadata(directory, expected);
            directory[length] = 0;
            if (status != 0)
            {
                return 0x52;
            }
            xapi_set_save_directory_time(directory); // 0x0014C6B3
        }
        if (disposition == 1 || status != 0)
        {
            return error;
        }
    }
    if (path != nullptr)
    {
        copy_narrow(path, directory, path_size); // lstrcpynA
        if (length < path_size - 1)
        {
            path[length] = '\\';
            path[length + 1] = 0;
        }
    }
    return 0;
}

// Xbox 0x0014C972
u32 delete_xbox_save_game(const char* root, const char16_t* name)
{
    char directory[0x10C]{};
    std::strcpy(directory, root);
    const size_t root_length = narrow_length(directory);
    xapi_append_save_directory_hash(reinterpret_cast<const u16*>(name), directory + root_length,
        int(0xF7 - root_length)); // 0x0014C2D5
    const size_t length = narrow_length(directory);
    append(directory, "\\SaveMeta.xbx"); // 0x0020329C
    const u32 status = xapi_create_save_metadata(directory, reinterpret_cast<const u16*>(name)); // 0x0014C633
    directory[length] = 0;
    if (status != 0)
    {
        return status;
    }
    directory[length + 1] = 0;
    directory[length] = '\\';
    // 0x0015162F: NtOpenFile on the directory, then 0x001514B2 deletes its tree; RtlNtStatusToDosError.
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(xbox_host_path(directory)), ec);
    return ec ? 5u : 0u; // ERROR_ACCESS_DENIED
}

// Xbox 0x0014B280
u32 xbox_file_attributes(const char* path)
{
    const std::filesystem::path host(xbox_host_path(path));
    std::error_code ec;
    const auto status = std::filesystem::status(host, ec);
    if (ec || !std::filesystem::exists(status))
    {
        g_xapi_last_error = 2; // XapiSetLastNTError(STATUS_OBJECT_NAME_NOT_FOUND)
        return 0xFFFFFFFFu;
    }
    return std::filesystem::is_directory(status) ? 0x10u : 0x80u; // FILE_ATTRIBUTE_DIRECTORY / NORMAL
}

// Xbox 0x0014CA42
XSaveGameFind* find_first_xbox_save_game(const char* root, XGameFindData& data)
{
    char pattern[0x104]{};
    std::strcpy(pattern, root);
    const u32 root_length = u32(narrow_length(pattern));
    append(pattern, "*");
    std::uintptr_t find = xapi_find_first_file(pattern, data.bytes.data()); // 0x0014B122
    if (find == std::uintptr_t(-1))
    {
        return nullptr;
    }
    if (xapi_read_save_directory_entry(data.bytes.data(), root, int(root_length)) == 0) // 0x0014C532
    {
        bool more = xapi_find_next_file(find, data.bytes.data()); // 0x0014B22F
        while (more)
        {
            if (xapi_read_save_directory_entry(data.bytes.data(), root, int(root_length)) != 0)
            {
                break;
            }
            more = xapi_find_next_file(find, data.bytes.data());
        }
        if (!more)
        {
            xapi_close_find_file(find); // CloseHandle
            g_xapi_last_error = 0x12; // SetLastError(ERROR_NO_MORE_FILES)
            return nullptr;
        }
    }
    auto* handle = new XSaveGameFind; // XMemAlloc(0x110, 0x24830000)
    handle->magic = 0x53425645;
    handle->find = find;
    handle->root_length = root_length;
    copy_narrow(handle->root.data(), root, 0x104); // lstrcpynA
    return handle;
}

// Xbox 0x0014CB4C
bool find_next_xbox_save_game(XSaveGameFind& find, XGameFindData& data)
{
    bool more = xapi_find_next_file(find.find, data.bytes.data()); // 0x0014B22F
    while (more)
    {
        if (xapi_read_save_directory_entry(data.bytes.data(), find.root.data(), int(find.root_length)) != 0)
        {
            return true;
        }
        more = xapi_find_next_file(find.find, data.bytes.data());
    }
    return false;
}

// Xbox 0x0014CB93: only the save-game handle ('EVBS') is made by the translated code.
bool close_xbox_save_game_find(XSaveGameFind* find)
{
    if (find->magic != 0x53425645)
    {
        throw Error("XFindClose (0x0014CB93) on a handle other than a save-game find");
    }
    const std::uintptr_t handle = find->find;
    delete find; // XMemFree (0x000F7280)
    xapi_close_find_file(handle); // CloseHandle, whose result is returned.
    return true;
}

u32 xbox_last_error()
{
    return g_xapi_last_error;
}

namespace
{
// NtQueryVolumeInformationFile(FileFsSizeInformation) on the host folder: total and available allocation units,
// sectors per unit and bytes per sector.
bool query_volume(const char* path, u64& total, u64& available, u32& sectors, u32& bytes)
{
    const std::filesystem::path host(xbox_host_path(path));
    std::error_code ec;
    const auto space = std::filesystem::space(host, ec);
    if (ec)
    {
        g_xapi_last_error = 3; // ERROR_PATH_NOT_FOUND (2 becomes 3 in the callers)
        return false;
    }
    // The Xbox hard disk's FATX volume: 0x4000-byte clusters of 0x200-byte sectors.
    bytes = 0x200;
    sectors = 0x20;
    total = space.capacity / (u64(sectors) * bytes);
    available = space.available / (u64(sectors) * bytes);
    return true;
}
}

// Xbox 0x0014C162
u32 xbox_volume_cluster_size(const char* path)
{
    u64 total = 0;
    u64 available = 0;
    u32 sectors = 0;
    u32 bytes = 0;
    if (!query_volume(path, total, available, sectors, bytes))
    {
        return 0;
    }
    return sectors * bytes;
}

// Xbox 0x0014C201
u32 xbox_volume_sector_size(const char* path)
{
    u64 total = 0;
    u64 available = 0;
    u32 sectors = 0;
    u32 bytes = 0;
    if (!query_volume(path, total, available, sectors, bytes))
    {
        return 0;
    }
    return bytes;
}

// Xbox 0x0014BC90
bool xbox_volume_space(const char* path, u64* free, u64* total, u64* available)
{
    u64 units = 0;
    u64 left = 0;
    u32 sectors = 0;
    u32 bytes = 0;
    if (!query_volume(path, units, left, sectors, bytes))
    {
        return false;
    }
    const u64 cluster = u64(sectors) * bytes;
    if (free != nullptr)
    {
        *free = left * cluster;
    }
    if (total != nullptr)
    {
        *total = units * cluster;
    }
    if (available != nullptr)
    {
        *available = left * cluster;
    }
    return true;
}

// Xbox 0x0014CEF5: XCalculateSignatureBeginEx (0x0014CEB4) with the certificate's TitleID.
std::uintptr_t begin_xbox_signature(u32 flags)
{
    constexpr u32 kTitleId = 0x4D530053; // [0x00010118] + 8
    if (flags < 2)
    {
        return xapi_begin_keyed_signature(flags, kTitleId); // 0x0014CD37
    }
    if (flags == 2 || flags == 4)
    {
        return xapi_begin_sha_signature(); // 0x0014CE19
    }
    if (flags == 8)
    {
        // DAT_00434228 is a BSS hook that nothing in the XBE sets, so retail calls address zero here.
        throw Error("XCalculateSignatureBeginEx flag 8 calls the null hook DAT_00434228");
    }
    g_xapi_last_error = 0x32;
    return std::uintptr_t(-1);
}

// Xbox 0x0014CF09
u32 update_xbox_signature(std::uintptr_t handle, const u8* data, u32 size)
{
    auto* context = reinterpret_cast<XapiCalcSigContext*>(handle);
    const u32 flags = *reinterpret_cast<const u32*>(context);
    if (flags < 2 || flags == 2 || flags == 4)
    {
        return xapi_update_signature(context, data, size); // 0x0014CE4F
    }
    if (flags == 8)
    {
        // DAT_0043422C is a BSS hook that nothing in the XBE sets, so retail calls address zero here.
        throw Error("XCalculateSignatureUpdate flag 8 calls the null hook DAT_0043422C");
    }
    return 0x32;
}

// Xbox 0x0014CF57
u32 end_xbox_signature(std::uintptr_t handle, u8* signature)
{
    auto* context = reinterpret_cast<XapiCalcSigContext*>(handle);
    const u32 flags = *reinterpret_cast<const u32*>(context);
    if (flags < 2)
    {
        return xapi_end_keyed_signature(context, signature); // 0x0014CDB7
    }
    if (flags == 2 || flags == 4)
    {
        return xapi_end_sha_signature(context, signature); // 0x0014CE69
    }
    if (flags == 8)
    {
        // DAT_00434230 is a BSS hook that nothing in the XBE sets, so retail calls address zero here.
        throw Error("XCalculateSignatureEnd flag 8 calls the null hook DAT_00434230");
    }
    return 0x32;
}

// Xbox 0x0014CE93
u32 xbox_signature_size(u32 flags)
{
    if (flags < 3 || flags == 4)
    {
        return 0x14;
    }
    if (flags == 8)
    {
        return 100;
    }
    return 0;
}
}

