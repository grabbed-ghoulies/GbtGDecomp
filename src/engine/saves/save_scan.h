#pragma once
#include "gbtg/retail_field.h"
#include "gbtg/types.h"
#include "runtime_profile.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace gbtg
{
struct RuntimeMovieFile;
// A save game's container (0x228 bytes, 0x000E97B0): the name, the drive root ("U:\"), the save's directory
// (XCreateSaveGame's path, ending in '\'), its length, where file names go and the open file.
struct SaveContainer
{
    std::array<char16_t, 0x80> name{}; // +0
    std::array<char, 4> root{}; // +0x100
    std::array<char, 0x104> directory{}; // +0x104
    i32 directory_length = 0; // +0x208
    char* file_name = nullptr; // +0x20C, directory + directory_length.
    RuntimeMovieFile* file = nullptr; // +0x210
};
// A heap block of the 0x000F72D0 kind: kind, size, then the inline bytes. 0x0010EB60 copies the challenge
// block's size from every block, so each keeps room past its own size.
struct SaveBlock
{
    RuntimeProfileBlock block;
    std::array<u8, 0x10> storage{};
    SaveBlock(i32 kind, i32 size)
        : block{kind, size, {storage.data(), size_t(size)}}
    {
    }
};
// A save game record (0x268 bytes, 0x0010E9D0). The bytes are the record as written; the three blocks at
// +0x1A8, +0x1AC and +0x1B0 are pointers in the record, kept here (their words are written as 0).
// The save record's fields the challenge save reads and writes, at their retail offsets (docs/RETAIL_LAYOUTS.md).
namespace save_record_field
{
// 0x0010DC20 returns it and 0x0010E2C0 clears it; the challenge save (0x000F24C0) clears it when it is set.
constexpr RetailField<u32, 0x184> unk_184{};
} // namespace save_record_field
struct SaveGameRecord
{
    std::array<u8, 0x268> bytes{};
    std::unique_ptr<SaveBlock> progress; // +0x1A8, kind 0x3E, 8 bytes.
    std::unique_ptr<SaveBlock> challenges; // +0x1AC, kind 100, 13 bytes.
    std::unique_ptr<SaveBlock> unlocks; // +0x1B0, kind 4, 1 byte.
    const char16_t* name() const
    {
        return reinterpret_cast<const char16_t*>(bytes.data() + 2);
    }
};
// A callback the save code calls with its argument: a name or a container (game functions such as
// 0x000DD2F0).
using SaveCallback = std::function<void(const void* argument)>;
// One loaded save (0x18 bytes in the stride-0x18 pool at DAT_005462E4 +0).
struct SaveSlot
{
    SaveContainer* container = nullptr; // +0
    SaveGameRecord* save = nullptr; // +4
    i32 dirty = 0; // +8, queued for 0x0010F440.
    i32 loading = 0; // +0xC, 0x0010DBA0: slot select shows the no-write disc (status 4).
    i32 writing = 0; // +0x10, 0x0010DBE0: slot select shows the damaged game (status 3).
    SaveCallback callback = nullptr; // +0x14, the queued write's failure callback.
};
// DAT_005462E4 (0x18 bytes, 0x0010D3B0).
struct SaveScan
{
    std::vector<SaveSlot> slots; // +0, the loaded saves.
    i32* slot_enumerator = nullptr; // +4, the 0x0010D710 / 0x0010D7A0 cursor.
    struct SavePool* names = nullptr; // +8, the stride-0x100 save names on U:.
    i32* enumerator = nullptr; // +0xC, the 0x0010D810 / 0x0010D8A0 cursor.
    i32 pending = 0; // +0x10, saves queued for writing.
    float timer = 0.f; // +0x14, the frame tick (0x00033BF2) counts it down.
};
extern SaveScan* g_save_scan;
void save_add_name(const char16_t* name); // Xbox 0x0010F290
void remove_save_name(const char16_t* name); // Xbox 0x000663B0 (0x0010E9B0) and 0x00066380 on the name pool
}
