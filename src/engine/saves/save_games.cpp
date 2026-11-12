#include "save_games.h"
#include "xdk_saves.h"
#include "../runtime/name_lookup.h"
#include "../video/movie_files.h"
#include "gbtg/entity.h"

#include <bit>
#include <cstring>
#include <string_view>
#include <vector>

namespace gbtg
{
void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

namespace
{
constexpr const char* kSaveRoot = "U:\\"; // 0x0032A3C8
constexpr const char* kSaveFile = "progress.xsv"; // 0x0032A3B8

bool is_error_reported(i32 status)
{
    // The common tail: 0x3E4, 0x3E5 and 0x4DE (I/O pending) pass back untouched, 0 is success, anything else is
    // looked up (0x000E96A0) first.
    if (status < 0x3E6)
    {
        return !(0x3E3 < status) && status != 0;
    }
    return status != 0x4DE;
}

void report_file_error(i32 status, SaveGameContext& context)
{
    const char* title;
    const char* message;
    lookup_movie_error_message(status, title, message, context.files.error); // 0x000E96A0
}

i32 checked(i32 status, SaveGameContext& context)
{
    if (is_error_reported(status))
    {
        report_file_error(status, context);
    }
    return status;
}

// Xbox 0x000E9640, EAX the name: the name narrowed for 0x000E96A0's message.
void report_save_error(const char16_t* name, i32 status, SaveGameContext& context)
{
    (void)name;
    report_file_error(status, context); // 0x000E96A0
}

u32 word(const SaveGameRecord& record, size_t offset)
{
    u32 value;
    std::memcpy(&value, record.bytes.data() + offset, 4);
    return value;
}

void put_word(SaveGameRecord& record, size_t offset, u32 value)
{
    std::memcpy(record.bytes.data() + offset, &value, 4);
}

i32 profile_size(const RuntimeProfileBlock& block)
{
    return block.size;
}

// The write and read length: the signature, the three profile blocks and the record, rounded up to the
// volume's sector size (0x0014C201 on the container's root).
i32 save_file_length(const SaveContainer& container, SaveGameContext& context)
{
    const i32 challenges = profile_size(context.profile.challenges); // *(DAT_00506C54 + 4)
    const i32 progress = profile_size(context.profile.progress); // *(DAT_00506C6C + 4)
    const i32 unlocks = profile_size(context.profile.unlocks); // *(DAT_00506C70 + 4)
    const i32 sector = i32(xbox_volume_sector_size(container.root.data())); // 0x0014C201
    i32 length = i32(xbox_signature_size(0)) + unlocks + challenges + 0x268 + progress; // 0x0014CE93(0)
    const i32 remainder = length % sector;
    if (remainder < 1)
    {
        if (remainder < 0)
        {
            length -= remainder;
        }
    }
    else
    {
        length += sector - remainder;
    }
    return length;
}

// Xbox 0x0010E9D0: a new save record for the name. Its script is the scene after game select.
SaveGameRecord* create_save_record(const char16_t* name, SaveGameContext& context)
{
    char select[0x80]{};
    std::memcpy(select, "aid_script_ghoulies_gameselect_gameselect", 42); // 0x00322B8C
    const char* script = next_scene_order_name(context.scene_order, select, 1); // 0x00035960, EAX DAT_00511B64
    auto* record = new SaveGameRecord; // 0x000F72D0(0x268)
    record->progress = std::make_unique<SaveBlock>(0x3E, 8); // 0x000F72D0(0x14)
    record->challenges = std::make_unique<SaveBlock>(100, 0xD); // 0x000F72D0(0x19)
    record->unlocks = std::make_unique<SaveBlock>(4, 1); // 0x000F72D0(0xD)
    record->bytes[0] = 0xD;
    if (script == nullptr)
    {
        throw Error("0x0010E9D0: the held scene order (DAT_00511B64) has no scene after game select");
    }
    std::strcpy(reinterpret_cast<char*>(record->bytes.data() + 0x102), script);
    put_word(*record, 0x184, 1);
    for (size_t offset = 0x188; offset <= 0x1A4; offset += 4)
    {
        put_word(*record, offset, 0);
    }
    record->bytes[0x25C] = 0;
    record->bytes[0x25D] = 0;
    put_word(*record, 0x260, 0);
    put_word(*record, 0x264, 0);
    for (size_t index = 0; index < 0x15; ++index)
    {
        put_word(*record, 0x1B4 + index * 4, 0xFFFFFFFFu);
        put_word(*record, 0x208 + index * 4, 0);
    }
    auto* output = reinterpret_cast<char16_t*>(record->bytes.data() + 2);
    while ((*output++ = *name++) != 0) // lstrcpyW
    {
    }
    return record;
}

// Xbox 0x000EB220, EAX the root: the free and total 0x4000-byte blocks, at most 50001.
i32 query_save_blocks(const char* root, u32* free, u32* total, SaveGameContext& context)
{
    i32 status = 0;
    const u32 cluster = xbox_volume_cluster_size(root); // 0x0014C162
    u64 available = 0;
    u64 size = 0;
    if ((cluster != 0 || (status = i32(xbox_last_error())) == 0)
        && (xbox_volume_space(root, &available, &size, nullptr) || (status = i32(xbox_last_error())) == 0))
    {
        const auto blocks = [cluster](u64 bytes)
        {
            const u64 count = bytes / cluster;
            return count > 0xC351 ? 0xC351u : u32(count);
        };
        if (total != nullptr)
        {
            *total = blocks(size);
        }
        if (free != nullptr)
        {
            *free = blocks(available);
        }
    }
    return checked(status, context);
}

// Xbox 0x000E9700, EAX zero, EDI the root: room for the blocks, then a new save of that name.
i32 make_save_directory(const char16_t* name, u32 blocks, u32 copyable, SaveGameContext& context)
{
    u32 free = 0;
    char path[0x104]{};
    i32 status = query_save_blocks(kSaveRoot, &free, nullptr, context); // 0x000EB220
    if (status == 0)
    {
        if (i32(free) < i32(blocks))
        {
            report_save_error(name, 0x70, context);
            return 0x70;
        }
        status = i32(create_xbox_save_game(kSaveRoot, name, 3, 0, path, 0x103)); // XCreateSaveGame, OPEN_EXISTING
        if (status == 0 || (status = i32(xbox_last_error())) == 0xB7)
        {
            report_save_error(name, 0xB7, context);
            return 0xB7;
        }
        status = i32(create_xbox_save_game(kSaveRoot, name, 1, copyable == 0 ? 1 : 0, path, 0x103)); // CREATE_NEW
        if (status == 0)
        {
            return 0;
        }
    }
    if (status == 0x3E5 || status == 0x4DE)
    {
        return status;
    }
    report_save_error(name, status, context);
    return status;
}

// Xbox 0x000E97B0: the container for an existing save of that name.
i32 open_save_container(SaveContainer*& output, const char* root, const char16_t* name,
    SaveGameContext& context)
{
    i32 status;
    auto* container = new SaveContainer; // 0x000F72D0(0x228)
    container->file = nullptr;
    std::strncpy(container->root.data(), root, 3); // lstrcpynA(.., 4)
    container->root[3] = 0;
    size_t length = 0;
    while (length < 0x7F && name[length] != 0) // lstrcpynW(.., 0x80)
    {
        container->name[length] = name[length];
        ++length;
    }
    container->name[length] = 0;
    status = i32(create_xbox_save_game(container->root.data(), container->name.data(), 3, 0,
        container->directory.data(), 0x104)); // XCreateSaveGame, OPEN_EXISTING
    if (status == 0)
    {
        container->directory_length = i32(std::strlen(container->directory.data()));
        container->file_name = container->directory.data() + container->directory_length;
    }
    else
    {
        delete container; // 0x000663E0
        container = nullptr;
    }
    output = container;
    if (status != 0 && status != 0x3E5 && status != 0x4DE)
    {
        report_save_error(name, status, context);
    }
    return status;
}

// The 0x000E9A00 / 0x000E9880 completion payload (0x14 bytes).
struct SaveFileRequest
{
    SaveContainer* container;
    std::function<void(RuntimeMovieFile&, i32, void*)> callback;
    void* argument;
};

// Xbox 0x000E9980: the caller's completion, then the file is closed.
void finish_save_file(RuntimeMovieFile& file, i32 status, void* payload, SaveGameContext& context)
{
    auto* request = static_cast<SaveFileRequest*>(payload);
    SaveContainer* container = request->container;
    if (request->callback)
    {
        request->callback(file, status, request->argument);
    }
    close_runtime_movie_file(*container->file, context.files); // 0x000EAC20
    delete request; // HeapFree
    container->file = nullptr;
}

// Xbox 0x000E9C20, EAX the flags: the buffer's signature.
i32 sign_save_data(u32 flags, const u8* data, u32 size, u8* signature, SaveGameContext& context)
{
    i32 status = 0;
    const std::uintptr_t handle = begin_xbox_signature(flags); // XCalculateSignatureBegin
    if ((handle != std::uintptr_t(-1) || (status = i32(xbox_last_error())) == 0)
        && (status = i32(update_xbox_signature(handle, data, size))) == 0) // XCalculateSignatureUpdate
    {
        status = i32(end_xbox_signature(handle, signature)); // XCalculateSignatureEnd
    }
    return checked(status, context);
}

// Xbox 0x000E9AF0, EAX the flags, ECX the length, EDX the buffer: the signature at the buffer's end.
i32 sign_save_buffer(u32 flags, i32 length, u8* buffer, SaveGameContext& context)
{
    const i32 size = i32(xbox_signature_size(flags)); // 0x0014CE93
    if (length < size)
    {
        report_file_error(8, context);
        return 8;
    }
    return checked(sign_save_data(flags, buffer, u32(length - size), buffer + (length - size), context), context);
}

// Xbox 0x000E9B70, EAX the length: whether the buffer's signature matches.
i32 verify_save_buffer(i32 length, u8* buffer, u32 flags, SaveGameContext& context)
{
    const i32 size = i32(xbox_signature_size(flags)); // 0x0014CE93
    std::vector<u8> signature(static_cast<size_t>(size)); // 0x000F72D0(size)
    i32 status = sign_save_data(flags, buffer, u32(length - size), signature.data(), context); // 0x000E9C20
    if (status == 0)
    {
        for (i32 index = 0; index < size; ++index)
        {
            if (buffer[length - size + index] != signature[size_t(index)])
            {
                status = 0xD;
                break;
            }
        }
    }
    return checked(status, context);
}

// Xbox 0x000E9A00, ECX the file name: the container's file written from the buffer (CREATE_ALWAYS).
i32 write_save_file(const char* file_name, SaveContainer& container, const u8* data, u32 size,
    std::function<void(RuntimeMovieFile&, i32, void*)> callback, void* argument, SaveGameContext& context)
{
    std::strcpy(container.file_name, file_name);
    i32 status = open_runtime_movie_file(container.file, container.directory.data(), 0x40000000, 2,
        context.files); // 0x000EAA90, EAX 2
    SaveFileRequest* request = nullptr;
    if (status == 0)
    {
        request = new SaveFileRequest{&container, std::move(callback), argument}; // 0x000F72D0(0x14)
        auto* files = &context.files;
        auto* profile = &context.profile;
        const std::span<const u8> order = context.scene_order;
        status = write_runtime_movie_file(*container.file, data, size,
            [files, profile, order](RuntimeMovieFile& file, i32 result, void* payload)
            {
                SaveGameContext completion{*files, *profile, order};
                finish_save_file(file, result, payload, completion); // 0x000E9980
            },
            request, context.files); // 0x000EB180
        if (status == 0)
        {
            if (argument != nullptr)
            {
                return 0;
            }
            status = wait_runtime_movie_file(*container.file, 0, context.files); // 0x000EA850
            if (status == 0)
            {
                return checked(status, context);
            }
        }
    }
    if (container.file != nullptr)
    {
        close_runtime_movie_file(*container.file, context.files); // 0x000EAC20
        container.file = nullptr;
    }
    delete request; // 0x000663E0
    return checked(status, context);
}

// Xbox 0x000E9880, EAX the file name: the container's file read into the buffer when it has the length.
i32 read_save_file(const char* file_name, SaveContainer& container, u8* data, u32 size,
    std::function<void(RuntimeMovieFile&, i32, void*)> callback, void* argument, SaveGameContext& context)
{
    std::strcpy(container.file_name, file_name);
    SaveFileRequest* request = nullptr;
    i32 status = open_runtime_movie_file(container.file, container.directory.data(), 0x80000000, 3,
        context.files); // 0x000EAA90, EAX 3
    if (status == 0)
    {
        u64 length = 0;
        context.files.io.size(container.file->handle, length); // GetFileSize
        if (u32(length) == size)
        {
            request = new SaveFileRequest{&container, std::move(callback), argument}; // 0x000F72D0(0x14)
            auto* files = &context.files;
            auto* profile = &context.profile;
            const std::span<const u8> order = context.scene_order;
            status = read_runtime_movie_file(*container.file, data, size,
                [files, profile, order](RuntimeMovieFile& file, i32 result, void* payload)
                {
                    SaveGameContext completion{*files, *profile, order};
                    finish_save_file(file, result, payload, completion); // 0x000E9980
                },
                request, context.files); // 0x000EB0E0
            if (status == 0)
            {
                if (argument != nullptr)
                {
                    return 0;
                }
                status = wait_runtime_movie_file(*container.file, 0, context.files); // 0x000EA850
                if (status == 0)
                {
                    return checked(status, context);
                }
            }
        }
        else
        {
            status = 0x570;
        }
    }
    if (container.file != nullptr)
    {
        close_runtime_movie_file(*container.file, context.files);
        container.file = nullptr;
    }
    delete request;
    return checked(status, context);
}

// The 0x0010EB60 completion payload (0x24 bytes): the container and the failure callback.
struct SaveWriteRequest
{
    SaveContainer* container;
    SaveCallback failed;
};

// Xbox 0x0010ED20: the timer restarts; a failed write calls the failure callback.
void finish_save_write(i32 status, SaveWriteRequest* request)
{
    g_save_scan->timer = std::bit_cast<float>(0x40466666u); // DAT_0042AC54
    const u32 always_report = 0; // DAT_0042AC78
    if ((always_report != 0 || status != 0) && request->failed)
    {
        request->failed(request->container);
    }
    delete request; // 0x000663E0
}

// Xbox 0x0010EB60, EBX the slot: the record and the profile's blocks written to progress.xsv.
void write_save_game(SaveSlot& slot, SaveCallback failed, SaveGameContext& context)
{
    const u32 challenges = u32(context.profile.challenges.size);
    const u32 progress = u32(context.profile.progress.size);
    const i32 length = save_file_length(*slot.container, context);
    std::vector<u8> buffer(static_cast<size_t>(length)); // 0x000F72D0(length)
    auto* request = new SaveWriteRequest{slot.container, failed}; // 0x000F72D0(0x24)
    SaveGameRecord& record = *slot.save;
    std::memcpy(buffer.data(), record.bytes.data(), 0x268); // 0x9A dwords
    u8* blocks = buffer.data() + 0x268;
    std::memcpy(blocks, record.progress->block.bytes.data(), progress);
    // The challenge block's size is used for the unlock block too (13 bytes from its 1-byte block).
    std::memcpy(blocks + progress, record.challenges->block.bytes.data(), challenges);
    std::memcpy(blocks + progress + challenges, record.unlocks->storage.data(), challenges);
    slot.dirty = 0;
    if (0 < g_save_scan->pending)
    {
        --g_save_scan->pending;
    }
    g_save_scan->timer = std::bit_cast<float>(0x40466666u); // DAT_0042AC54
    const u32 skip_writes = 0; // DAT_0042AC74
    if (skip_writes == 0 && sign_save_buffer(0, length, buffer.data(), context) == 0) // 0x000E9AF0, EAX 0
    {
        write_save_file(kSaveFile, *slot.container, buffer.data(), u32(length),
            [](RuntimeMovieFile&, i32 status, void* argument)
            {
                finish_save_write(status, static_cast<SaveWriteRequest*>(argument)); // 0x0010ED20
            },
            request, context); // 0x000E9A00
        return; // HeapFree of the buffer (the host write keeps its own copy).
    }
    if (failed)
    {
        failed(slot.container);
    }
    delete request; // 0x000663E0
}

// The 0x0010ED70 read request (0x134 bytes).
struct SaveLoadRequest
{
    std::array<char16_t, 0x80> name{}; // +0
    SaveContainer* container = nullptr; // +0x100
    std::vector<u8> buffer; // +0x104, +0x108 its length.
    SaveCallback loaded; // +0x10C
    SaveCallback failed; // +0x110
    size_t slot = 0; // +0x114, the slot record it fills.
};

// Xbox 0x0010EF10: a verified read becomes the slot's record; the blocks are copied out of the file.
void finish_save_load(i32 status, SaveLoadRequest* request, SaveGameContext& context)
{
    const u32 fail_reads = 0; // DAT_0042AC6C
    const u32 fail_checks = 0; // DAT_0042AC70
    if (fail_reads == 0 && status == 0 && fail_checks == 0
        && verify_save_buffer(i32(request->buffer.size()), request->buffer.data(), 0, context) == 0) // 0x000E9B70
    {
        SaveSlot& slot = g_save_scan->slots[request->slot];
        SaveGameRecord& record = *slot.save;
        slot.container = request->container;
        std::memcpy(record.bytes.data(), request->buffer.data(), 0x268); // 0x000F71F0
        put_word(record, 0x1A8, 0); // The blocks stay the record's own (the file holds old pointers).
        put_word(record, 0x1AC, 0);
        put_word(record, 0x1B0, 0);
        const u8* blocks = request->buffer.data() + 0x268;
        const u32 progress = u32(context.profile.progress.size);
        const u32 challenges = u32(context.profile.challenges.size);
        const u32 unlocks = u32(context.profile.unlocks.size);
        engine_copy_bytes(record.progress->block.bytes.data(), blocks, progress); // 0x000F71F0
        engine_copy_bytes(record.challenges->block.bytes.data(), blocks + progress, challenges);
        engine_copy_bytes(record.unlocks->block.bytes.data(), blocks + progress + challenges, unlocks);
        if (request->loaded)
        {
            request->loaded(nullptr);
        }
    }
    else if (request->failed)
    {
        request->failed(request);
    }
    delete request; // HeapFree of the buffer and the request.
}

// Xbox 0x0010ED70: a slot record for the container and the asynchronous read of its progress.xsv.
i32 read_save_game(const char16_t* name, SaveContainer* container, SaveCallback loaded, SaveCallback failed,
    SaveGameContext& context)
{
    auto* request = new SaveLoadRequest; // 0x000F72D0(0x134)
    request->buffer.resize(size_t(save_file_length(*container, context))); // 0x000F72D0(length)
    request->container = container;
    size_t length = 0;
    while (name[length] != 0) // lstrcpyW
    {
        request->name[length] = name[length];
        ++length;
    }
    request->loaded = std::move(loaded);
    request->failed = std::move(failed);
    // +0x118 "Load Duration" and the timing words +0x11C..+0x130 are not read.
    g_save_scan->slots.push_back({}); // 0x000662A0 grows a full pool.
    request->slot = g_save_scan->slots.size() - 1;
    SaveSlot& slot = g_save_scan->slots.back();
    slot.save = create_save_record(request->name.data(), context); // 0x0010E9D0
    slot.container = nullptr;
    slot.dirty = 0;
    slot.loading = 0;
    slot.writing = 0;
    auto* files = &context.files;
    auto* profile = &context.profile;
    const std::span<const u8> order = context.scene_order;
    const i32 status = read_save_file(kSaveFile, *container, request->buffer.data(), u32(request->buffer.size()),
        [files, profile, order](RuntimeMovieFile&, i32 result, void* argument)
        {
            SaveGameContext completion{*files, *profile, order};
            finish_save_load(result, static_cast<SaveLoadRequest*>(argument), completion); // 0x0010EF10
        },
        request, context); // 0x000E9880
    if (status != 0)
    {
        if (request->failed)
        {
            request->failed(name);
        }
        return 0;
    }
    return 1;
}

// Xbox 0x0010F320, EAX the container: one of the three savegame textures as the save's SaveImage.xbx.
void write_save_image(SaveContainer& container, SaveImageBoundaries& image)
{
    static constexpr std::array<const char*, 3> kImages{"aid_texture_ghoulies_misc_savegame",
        "aid_texture_ghoulies_misc_savegametwo", "aid_texture_ghoulies_misc_savegamethree"}; // 0x0030E250 + n*0x80
    image.random = image.random * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((image.random & 0x7FFFFFu) | 0x3F800000u);
    const i32 index = -i32((unit - 1.f) * -3.f); // DAT_0032B0F8, DAT_0032B1A8; the name steps back 0x80 a step.
    std::span<const u8> texture = image.acquire(kImages[size_t(index)]); // 0x00035E90
    if (texture.data() == nullptr)
    {
        image.missing(); // 0x00032700(2)
    }
    const u32 headers = read_u32(texture, 4);
    const u32 header = headers + u32(texture[0x12]) * 0x14;
    char path[0x110]{};
    if (container.directory_length < 0x105)
    {
        std::strncpy(path, container.directory.data(), size_t(container.directory_length));
        if (container.directory_length < 0x104)
        {
            path[container.directory_length] = 0;
        }
        std::strcat(path, "SaveImage.xbx"); // 0x0032A398
        image.write_xpr(texture.subspan(header, 0x14), path); // XGWriteSurfaceOrTextureToXPR(.., 0)
        image.release(texture); // 0x00035C00
    }
}
}

SaveSlot* find_save_slot(const char16_t* name)
{
    for (SaveSlot& slot : g_save_scan->slots)
    {
        if (std::u16string_view(slot.save->name()) == name) // lstrcmpW(+4 +2, name)
        {
            return &slot;
        }
    }
    return nullptr;
}

// Xbox 0x0010D430
i32 create_save_game(const char16_t* name, u8 slot, const SaveCreateCallbacks& callbacks,
    SaveImageBoundaries& image, SaveGameContext& context)
{
    // DAT_0042AC5C, DAT_0042AC60 and DAT_0042AC64 force 0x70, 0x52 and 0xB7; all three are 0.
    i32 status = make_save_directory(name, 4, 0, context); // 0x000E9700(name, 4), EAX 0
    if (status == 0)
    {
        SaveContainer* container = nullptr;
        status = open_save_container(container, kSaveRoot, name, context); // 0x000E97B0
        if (status == 0)
        {
            write_save_image(*container, image); // 0x0010F320
            save_add_name(name); // 0x0010F290
            SaveGameRecord* save = create_save_record(name, context); // 0x0010E9D0
            g_save_scan->slots.push_back({container, save, 0, 0, 0, nullptr}); // 0x000662A0 grows a full pool.
            set_save_game_slot(name, slot); // 0x0010E270
            write_save_game(g_save_scan->slots.back(), callbacks.corrupt, context); // 0x0010EB60
            return 1;
        }
        if (callbacks.corrupt)
        {
            callbacks.corrupt(name);
        }
        return 0;
    }
    if (status == 0x52)
    {
        if (callbacks.too_many)
        {
            callbacks.too_many(name);
        }
    }
    else if (status == 0x70)
    {
        if (callbacks.no_space)
        {
            callbacks.no_space(name);
        }
    }
    else if (status == 0xB7 && callbacks.run_again)
    {
        callbacks.run_again(name);
    }
    return 0;
}

// Xbox 0x0010D910
i32 load_save_game(const char16_t* name, SaveCallback loaded, SaveCallback failed, SaveGameContext& context)
{
    if (find_save_slot(name) != nullptr)
    {
        return 0;
    }
    const u32 fail_opens = 0; // DAT_0042AC68
    if (fail_opens == 0)
    {
        SaveContainer* container = nullptr;
        const i32 status = open_save_container(container, kSaveRoot, name, context); // 0x000E97B0
        if (status == 0)
        {
            read_save_game(name, container, std::move(loaded), failed, context); // 0x0010ED70
            return 1;
        }
        if (status != 0x491 && status != 3)
        {
            return 0;
        }
    }
    if (failed)
    {
        failed(nullptr);
    }
    return 0;
}

// Xbox 0x0010D9B0
i32 unload_save_game(const char16_t* name, SaveGameContext& context)
{
    (void)context;
    auto& slots = g_save_scan->slots;
    for (size_t index = 0; index < slots.size(); ++index)
    {
        SaveSlot& slot = slots[index];
        if (std::u16string_view(slot.save->name()) != name)
        {
            continue;
        }
        if (save_game_loading(name) == 0 && slot.container != nullptr) // 0x0010DBA0
        {
            delete slot.container; // 0x00031470: HeapFree, which returns 0.
            slot.container = nullptr;
        }
        delete slot.save; // 0x000663E0 on +0x1A8, +0x1AC, +0x1B0 and the record.
        slot.save = nullptr;
        slots.erase(slots.begin() + std::ptrdiff_t(index)); // memmove down a stride
        return 1;
    }
    return 0;
}

namespace
{
// Xbox 0x000E9CA0, EAX the container, EBX the output, EDI its size: the save's directory, or 8 when it does not
// fit.
i32 copy_save_directory(const SaveContainer& container, char* output, i32 size)
{
    if (size < container.directory_length)
    {
        return 8;
    }
    std::strncpy(output, container.directory.data(), size_t(container.directory_length));
    if (container.directory_length < size)
    {
        output[container.directory_length] = 0;
    }
    return 0;
}

// DAT_0042AC58: .data 0, and nothing writes it (its one reference is 0x0010F100's read).
constexpr u32 kSaveDeleteLocked = 0;

// Xbox 0x0010F100, EAX the name: XDeleteSaveGame on U:\, then the name leaves the name pool.
i32 delete_save_game_files(const char16_t* name, SaveGameContext& context)
{
    // The lstrcmpW walk over the loaded saves runs first; nothing reads where it stops.
    if (kSaveDeleteLocked == 0)
    {
        const u32 status = delete_xbox_save_game(kSaveRoot, name); // 0x0014C972
        if (status == 0)
        {
            remove_save_name(name); // 0x000663B0 (0x0010E9B0), then the 0x00165AF0 memmove
            return 1;
        }
        if (status != 0x3E5 && status != 0x4DE)
        {
            report_save_error(name, i32(status), context); // 0x000E9640, EAX the name
        }
    }
    return 0;
}
}

// Xbox 0x0010D5E0
i32 delete_save_game(const char16_t* name, SaveGameContext& context)
{
    auto& slots = g_save_scan->slots;
    for (SaveSlot& slot : slots)
    {
        if (std::u16string_view(slot.save->name()) != name) // lstrcmpW(+4 +2, name)
        {
            continue;
        }
        const i32 loading = save_game_loading(name); // 0x0010DBA0, EBX the name
        if (loading == 0 && save_game_writing(name) == 0) // 0x0010DBE0
        {
            char directory[0x104]{};
            copy_save_directory(*slot.container, directory, 0x104); // 0x000E9CA0
            const u32 attributes = xbox_file_attributes(directory); // 0x0014B280
            const u32 status = attributes == 0xFFFFFFFFu ? xbox_last_error() // 0x0014AFE4
                                                         : ((attributes & 0x10) != 0 ? 0x10Bu : 0u);
            if (status != 0x10B)
            {
                remove_save_name(name); // 0x000663B0 (0x0010E9B0) and 0x00066380 on DAT_005462E4 +8
                delete slot.container; // 0x00031470
                slot.container = nullptr;
                mark_save_game_loading(name); // 0x0010E1A0, EBX the name
                return 0;
            }
        }
        unload_save_game(name, context); // 0x0010D9B0, EAX the name
        if (loading == 0 && delete_save_game_files(name, context) == 0) // 0x0010F100
        {
            return 0;
        }
        return 1;
    }
    return 0;
}

// Xbox 0x0010DA80
i32 queue_save_game(const char16_t* name, SaveCallback failed, SaveGameContext& context)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot == nullptr)
    {
        return 0;
    }
    if (slot->loading == 0 && slot->writing == 0)
    {
        if (g_save_scan->timer <= 0.f)
        {
            write_save_game(*slot, std::move(failed), context); // 0x0010EB60
            return 1;
        }
        slot->callback = std::move(failed);
        mark_save_game_dirty(name); // 0x0010F2D0
    }
    return 1;
}

// Xbox 0x0010F440: the first dirty save in the enumeration is written.
void write_queued_save_games(SaveGameContext& context)
{
    if (g_save_scan->pending <= 0)
    {
        return;
    }
    std::array<char16_t, 0x80> name{};
    if (!save_first_slot_name(name.data())) // 0x0010D710
    {
        return;
    }
    SaveSlot* slot = find_save_slot(name.data()); // 0x000663B0 with the comparison at 0x0010E990
    if (slot == nullptr)
    {
        return;
    }
    if (slot->dirty != 0)
    {
        write_save_game(*slot, slot->callback, context); // 0x0010EB60
        return;
    }
    while (save_next_slot_name(name.data())) // 0x0010D7A0
    {
        for (SaveSlot& candidate : g_save_scan->slots)
        {
            if (std::u16string_view(candidate.save->name()) == name.data())
            {
                if (candidate.dirty != 0)
                {
                    write_save_game(candidate, candidate.callback, context); // 0x0010EB60
                    return;
                }
                break;
            }
        }
    }
}

// The frame tick's save step (0x00033BF2): the timer runs down, then queued saves are written.
void tick_save_games(float dt, SaveGameContext& context)
{
    if (g_save_scan == nullptr)
    {
        return;
    }
    if (g_save_scan->timer > 0.f)
    {
        g_save_scan->timer -= dt; // DAT_005096C0
    }
    if (g_save_scan->pending != 0 && !(0.f < g_save_scan->timer))
    {
        write_queued_save_games(context); // 0x0010F440
    }
}

// Xbox 0x0010DB60
u8 save_game_slot(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? 0 : slot->save->bytes[0x25C];
}

// Xbox 0x0010DB10
void save_game_script(const char16_t* name, char* output)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::strcpy(output, reinterpret_cast<const char*>(slot->save->bytes.data() + 0x102));
    }
}

// Xbox 0x0010DBA0
i32 save_game_loading(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? 0 : slot->loading;
}

// Xbox 0x0010DBE0
i32 save_game_writing(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? 0 : slot->writing;
}

// One getter per word of the named save, EBX the name, 0 with no save of that name: +0x184, +0x188, +0x18C, +0x190,
// +0x194, +0x198, +0x19C, +0x1A0, the float at +0x1A4 (XMM0, 0.0 with none) and +0x260.
// Xbox 0x0010DC20
// Xbox 0x0010DC60
// Xbox 0x0010DCA0
// Xbox 0x0010DCE0
// Xbox 0x0010DD20
// Xbox 0x0010DD60
// Xbox 0x0010DDA0
// Xbox 0x0010E050
// Xbox 0x0010E090
// Xbox 0x0010E120
i32 save_game_word(const char16_t* name, size_t offset)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? 0 : i32(word(*slot->save, offset));
}

// Xbox 0x0010E0E0: AL is the byte, 0 for no save (the rest of EAX is not a result).
u8 save_game_byte(const char16_t* name, size_t offset)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? 0 : slot->save->bytes[offset];
}

// Xbox 0x0010DDE0
RuntimeProfileBlock* save_game_progress(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? nullptr : &slot->save->progress->block;
}

// Xbox 0x0010DE30
RuntimeProfileBlock* save_game_challenges(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? nullptr : &slot->save->challenges->block;
}

// Xbox 0x0010DE80
RuntimeProfileBlock* save_game_unlocks(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    return slot == nullptr ? nullptr : &slot->save->unlocks->block;
}

// Xbox 0x0010DED0: the 0x54 bytes at +0x1B4 into DAT_004E18F8.
void load_save_game_words_1b4(const char16_t* name, std::span<u8> output)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::memcpy(output.data(), slot->save->bytes.data() + 0x1B4, 0x54);
    }
}

// Xbox 0x0010DF90: the 0x54 bytes at +0x208 into DAT_004E194C.
void load_save_game_words_208(const char16_t* name, std::span<u8> output)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::memcpy(output.data(), slot->save->bytes.data() + 0x208, 0x54);
    }
}

// Xbox 0x0010E680: DAT_004E18F8's 0x54 bytes (EBX) into the save's +0x1B4.
void store_save_game_words_1b4(const char16_t* name, std::span<const u8> words)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::memcpy(slot->save->bytes.data() + 0x1B4, words.data(), 0x54);
    }
}

// Xbox 0x0010E740: DAT_004E194C's 0x54 bytes (EBX) into the save's +0x208.
void store_save_game_words_208(const char16_t* name, std::span<const u8> words)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::memcpy(slot->save->bytes.data() + 0x208, words.data(), 0x54);
    }
}

// Xbox 0x0010E270
void set_save_game_slot(const char16_t* name, u8 slot)
{
    SaveSlot* found = find_save_slot(name);
    if (found != nullptr)
    {
        found->save->bytes[0x25C] = slot;
    }
}

// Xbox 0x0010E5B0
void store_save_game_challenges(const char16_t* name, const RuntimeProfileBlock& block)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        engine_copy_bytes(slot->save->challenges->block.bytes.data(), block.bytes.data(),
            u32(block.size)); // 0x000F71F0, EDX the size, ECX the bytes
    }
}

// Xbox 0x0010E610
void store_save_game_unlocks(const char16_t* name, const RuntimeProfileBlock& block)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        auto& unlocks = slot->save->unlocks->block;
        std::fill_n(unlocks.bytes.data(), size_t(unlocks.size), u8(0)); // rep stos over +4 bytes
        engine_copy_bytes(unlocks.bytes.data(), block.bytes.data(), u32(block.size)); // 0x000F71F0
    }
}

// Xbox 0x0010E2C0
void clear_save_game_word_184(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        set(slot->save->bytes, save_record_field::unk_184, 0);
    }
}

namespace
{
void set_save_game_word(const char16_t* name, size_t offset, u32 value)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::memcpy(slot->save->bytes.data() + offset, &value, 4);
    }
}
}

// Xbox 0x0010E360
void set_save_game_word_18c(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x18C, value);
}

// Xbox 0x0010E3B0
void set_save_game_word_190(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x190, value);
}

// Xbox 0x0010E400
void set_save_game_word_194(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x194, value);
}

// Xbox 0x0010E450
void set_save_game_word_198(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x198, value);
}

// Xbox 0x0010E4A0
void set_save_game_word_19c(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x19C, value);
}

// Xbox 0x0010E310
void set_save_game_finished(const char16_t* name)
{
    set_save_game_word(name, 0x188, 1);
}

// Xbox 0x0010E220
void set_save_game_script(const char16_t* name, const char* script)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        std::strcpy(reinterpret_cast<char*>(slot->save->bytes.data() + 0x102), script);
    }
}

// Xbox 0x0010E800
void set_save_game_word_1a0(const char16_t* name, u32 value)
{
    set_save_game_word(name, 0x1A0, value);
}

// Xbox 0x0010E850
void set_save_game_real_1a4(const char16_t* name, float value)
{
    set_save_game_word(name, 0x1A4, std::bit_cast<u32>(value));
}

// Xbox 0x0010E8A0
void set_save_game_byte_25d(const char16_t* name, u8 value)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        slot->save->bytes[0x25D] = value;
    }
}

// Xbox 0x0010E8F0
void set_save_game_word_260(const char16_t* name)
{
    set_save_game_word(name, 0x260, 1);
}

// Xbox 0x0010E940
void set_save_game_word_264(const char16_t* name)
{
    set_save_game_word(name, 0x264, 1);
}

// Xbox 0x0010E4F0
void store_save_game_progress(const char16_t* name, const RuntimeProfileBlock& block)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot == nullptr)
    {
        return;
    }
    RuntimeProfileBlock& progress = slot->save->progress->block;
    std::fill(progress.bytes.begin(), progress.bytes.end(), u8(0));
    for (u32 bit = 0; bit < 0x3E; ++bit)
    {
        const u8 mask = u8(1u << (bit & 7));
        if ((block.bytes[bit >> 3] & mask) != 0)
        {
            progress.bytes[bit >> 3] = u8(progress.bytes[bit >> 3] | mask);
        }
    }
}

// Xbox 0x0010F2D0
void mark_save_game_dirty(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        if (slot->dirty == 0)
        {
            ++g_save_scan->pending;
        }
        slot->dirty = 1;
    }
}

// Xbox 0x0010F530
bool save_game_name_exists(const char16_t* name)
{
    std::array<char16_t, 0x80> found{};
    if (save_first_name(found.data()) && std::u16string_view(name) == found.data()) // 0x0010D810, lstrcmpW
    {
        return true;
    }
    // The 0x0010D8A0 walk, inline: its cursor is freed when the names run out.
    while (g_save_scan->enumerator != nullptr)
    {
        if (!save_next_name(found.data()))
        {
            return false;
        }
        if (std::u16string_view(name) == found.data())
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0010E1E0, EBX the name: the slot shows the damaged game (+0x10).
void mark_save_game_damaged(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        slot->writing = 1;
    }
}

// Xbox 0x0010E1A0, EBX the name: the slot shows the no-write disc (+0xC).
void mark_save_game_loading(const char16_t* name)
{
    SaveSlot* slot = find_save_slot(name);
    if (slot != nullptr)
    {
        slot->loading = 1;
    }
}

// Xbox 0x0010D580, EAX the name: a save record with no container, so the game plays on without saving.
i32 add_unsaved_save_game(const char16_t* name, u8 slot, SaveGameContext& context)
{
    SaveGameRecord* save = create_save_record(name, context); // 0x0010E9D0
    g_save_scan->slots.push_back({nullptr, save, 0, 0, 0, nullptr}); // 0x000662A0 grows a full pool.
    set_save_game_slot(name, slot); // 0x0010E270, EBX the name
    mark_save_game_loading(name); // 0x0010E1A0
    return 1;
}

// Xbox 0x000EB220 on a root, for callers outside the save code.
i32 query_save_volume_blocks(const char* root, u32* free, u32* total, SaveGameContext& context)
{
    return query_save_blocks(root, free, total, context);
}
}
