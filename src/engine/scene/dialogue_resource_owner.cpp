#include "dialogue_resource_owner.h"
#include "gbtg/language.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../plugins/plugins.h"
#endif

#include <algorithm>
#include <cstring>

namespace gbtg
{
SceneDialogueResourceOwner::SceneDialogueResourceOwner(AidStore& aids, std::span<ExecutableResourcePool> executable)
    : aids_(aids)
{
    if (!executable.empty())
    {
        register_animation_resource_provider(registry,
            [executable](u32, const AnimationResourceRecord& request, const void*& resolved) -> u32
            {
                return find_executable_resource_provider(executable, request.name.c_str(), resolved);
            },
            [this](u32, const void* resolved, AnimationLoadedResource& output)
            {
                const bool loaded = load_executable_resource_provider(resolved, output);
                // Executable record +0x80/+0x84: the CPU pointer and its recorded size; no resource chunk.
                loaded_row(output) = {output.cpu.data(), u32(output.cpu.size()), nullptr, 0};
                return loaded;
            }, {}, 4, nullptr);
    }
    register_animation_resource_provider(registry,
        [this](u32, const AnimationResourceRecord& request, const void*& resolved) -> u32
        {
            Resource* resource = find_resource(request.name.c_str());
            if (resource == nullptr)
            {
                return 0;
            }
            std::array<AnimationBundlePool, 1> pools{
                AnimationBundlePool{{&resource->record, 1}, true}};
            return find_bundle_resource_provider(pools, request.name.c_str(), resolved);
        },
        [this](u32, const void* resolved, AnimationLoadedResource& output)
        {
            const bool loaded = load_bundle_resource_provider(resolved, output);
            const auto& record = *static_cast<const AnimationBundleRecord*>(resolved);
            record_bundle_size(record, output);
            if (loaded && record.name.starts_with("aid_cutscene_"))
            {
                register_scene_cutscene_resource(output.cpu, cutscene_views);
            }
            return loaded;
        },
        [this](u32, const char* name, const AnimationLoadedResource& loaded)
        {
            unregister_scene_cutscene_resource(loaded.cpu.data(), cutscene_views);
            auto& resource = resources_.at(name);
            std::array<AnimationBundlePool, 1> pools{
                AnimationBundlePool{{&resource.record, 1}, true}};
            unload_bundle_resource_provider(pools, name);
        }, 3, this);
    register_runtime_model_resource_hook(registry, models);
    // 0x00032700 shows the disc error screen and never returns; the scene raises instead.
    error.boundaries.delete_file = [this](const char*) -> bool
    {
        throw Error("retail resource error screen (code " + std::to_string(error.code) + "), last missing resource: "
            + last_missing);
    };
    for (auto& hook : registry.hooks)
    {
        if (hook.type != nullptr && std::strcmp(hook.type, "model") == 0)
        {
            hook.post_load = [this, post_load = std::move(hook.post_load)](const AnimationLoadedResource& loaded)
            {
                post_load(loaded);
                publish_actor_model(loaded);
            };
            hook.unload = [this, unload = std::move(hook.unload)](const AnimationLoadedResource& loaded)
            {
                actor_models.erase(loaded.cpu.data());
                unload(loaded);
            };
        }
    }
}

RuntimeBodyLoadedResourceSize& SceneDialogueResourceOwner::loaded_row(const AnimationLoadedResource& output)
{
    const size_t index = size_t(&output - registry.loaded.data());
    if (index >= loaded_sizes.size())
    {
        throw Error("scene loaded-resource size view is full");
    }
    return loaded_sizes[index];
}

// Bundle record +0x90..+0x9C: the CPU pointer, the file chunk size, and the resource chunk, which
// 0x00034590 relocates to its metadata only when its size is nonzero.
void SceneDialogueResourceOwner::record_bundle_size(const AnimationBundleRecord& record,
    const AnimationLoadedResource& output)
{
    auto& row = loaded_row(output);
    row = {};
    for (const auto& [name, resource] : resources_)
    {
        if (&resource.record != &record)
        {
            continue;
        }
        if (resource.entry == nullptr)
        {
            // Loose localisation and demand files come from other retail providers; their rows stay unmapped.
            return;
        }
        const auto& header = resource.entry->header;
        row.cpu = output.cpu.data();
        row.secondary = header.file_chunk.size;
        row.tail = header.resource_chunk.size;
        if (header.resource_chunk.size != 0)
        {
            const auto contains = [&](const Bundle& bundle)
            {
                return !bundle.entries.empty() && resource.entry >= bundle.entries.data()
                    && resource.entry < bundle.entries.data() + bundle.entries.size();
            };
            const Bundle* owner = contains(aids_.common) ? &aids_.common : nullptr;
            for (const auto& [path, bundle] : aids_.bundles)
            {
                if (contains(bundle))
                {
                    owner = &bundle;
                }
            }
            if (owner == nullptr)
            {
                throw Error("scene bundle resource has no owning bundle: " + name);
            }
            row.storage = owner->res_meta.data() + header.resource_chunk.offset;
        }
        return;
    }
}

// Pointer-width adapter for actor +0x204. Readers take the model directory as the base of one offset
// space; the runtime header is a separate retail heap block, so it is appended to the original bytes.
void SceneDialogueResourceOwner::publish_actor_model(const AnimationLoadedResource& loaded)
{
    Resource* resource = nullptr;
    for (auto& [name, candidate] : resources_)
    {
        if (candidate.record.cpu.data() == loaded.cpu.data())
        {
            resource = &candidate;
        }
    }
    if (resource == nullptr || resource->serialized.empty())
    {
        throw Error("loaded model has no scene resource");
    }
    const auto& prepared = models.resources.at(loaded.cpu.data());
    const u32 base = resource->address;
    const u32 size = u32(resource->serialized.size());
    const u32 header = (size + 15) & ~15u;
    auto& bytes = resource->actor_cpu;
    bytes.assign(resource->serialized.begin(), resource->serialized.end());
    bytes.resize(header + 0x30);
    const auto offset_of = [base, size](u32 address)
    {
        if (address == 0)
        {
            return 0u;
        }
        if (address < base || address - base >= size)
        {
            throw Error("model runtime header points outside its CPU block");
        }
        return address - base;
    };
    std::memcpy(bytes.data() + 0x10, &header, 4);
    for (u32 word = 0; word < 0x30; word += 4)
    {
        const u32 value = offset_of(read_u32(prepared.runtime.bytes, word));
        std::memcpy(bytes.data() + header + word, &value, 4);
    }
    ActorModel model{bytes};
    const u32 shapes = read_u32(bytes, header + 4);
    if (shapes != 0)
    {
        model.physics_shapes = std::span<const u8>(bytes).subspan(shapes);
        publish_shape_views(bytes, shapes);
    }
    actor_models[loaded.cpu.data()] = model;
}

// The serialized shape words are offsets in the actor model's address space. Shape rows follow the
// list header at +0x10 and advance by their +8 size; compound rows (type 1) hold +0x20 lists from +0x2C,
// single rows (type 2) one list at +0x10 (0x00054110 / 0x00054DB0). Event records are 0x14 bytes.
void SceneDialogueResourceOwner::publish_shape_views(std::span<const u8> bytes, u32 shapes)
{
    const auto view = bytes.subspan(shapes);
    const u32 vertices = read_u32(view, 0xC);
    if (vertices > bytes.size())
    {
        throw Error("model collision vertices lie outside the actor model");
    }
    collision_vertices[view.data()] = vertices == 0 ? std::span<const Vec3>{}
        : std::span<const Vec3>(reinterpret_cast<const Vec3*>(bytes.data() + vertices), (bytes.size() - vertices) / 12);
    const auto publish = [&](const u8* key)
    {
        const std::span<const u8> words(key, 8);
        const u32 count = read_u32(words, 0);
        const u32 records = read_u32(words, 4);
        if (records > bytes.size() || u64(count) * 0x14 > bytes.size() - records)
        {
            throw Error("model shape event list lies outside the actor model");
        }
        shape_lists[key] = {count, records == 0 ? nullptr : bytes.data() + records};
    };
    auto shape = view.subspan(0x10);
    for (i32 index = 0; index < i32(read_u32(view, 0)); ++index)
    {
        if (shape[0] == 1)
        {
            for (u32 list = 0; list < read_u16(shape, 0x20); ++list)
            {
                publish(shape.data() + 0x2C + list * 8);
            }
        }
        else if (shape[0] == 2)
        {
            publish(shape.data() + 0x10);
        }
        shape = shape.subspan(read_u32(shape, 8));
    }
}

SceneDialogueResourceOwner::Resource* SceneDialogueResourceOwner::find_resource(const char* name)
{
    const auto found = resources_.find(name);
    if (found != resources_.end())
    {
        return &found->second;
    }
    const std::string key(name);
    if (key.starts_with("aid_loctext_"))
    {
        // 0x00034F00: "%s%s%08x" over the disc root, the loctext folder (0x000FF040) and the name's hash.
        std::string folder = g_loctext_folder;
        std::replace(folder.begin(), folder.end(), '\\', '/');
        auto path = aids_.game_dir / folder / aid_hash_hex(aid_hash(key));
#if !defined(GBTG_RETAIL_GAME_ONLY)
        path = plugin_disc_path(path);
#endif
        if (!std::filesystem::exists(path))
        {
            return nullptr;
        }
        auto& resource = resources_[key];
        resource.loose = parse_loose_resource(read_file(path));
        resource.record = {key, 0, resource.loose.cpu, resource.loose.gpu, {}, {}};
        prepare_text(resource);
        return &resource;
    }
    if (key.starts_with("aid_font_"))
    {
        // Fonts are loose files named by their hash, as load_fonts reads them.
        auto path = aids_.game_dir / "font" / aid_hash_hex(aid_hash(key));
#if !defined(GBTG_RETAIL_GAME_ONLY)
        path = plugin_disc_path(path);
#endif
        if (!std::filesystem::exists(path))
        {
            last_missing = key;
            return nullptr;
        }
        auto& resource = resources_[key];
        resource.loose = parse_loose_resource(read_file(path));
        resource.record = {key, 0, resource.loose.cpu, resource.loose.gpu, {}, {}};
        if (!resource.loose.gpu_chunks.empty())
        {
            // The glyph frames are the first GPU chunk's data, as for a bundle texture below; the payload before
            // the chunk's offset is not texel data.
            const auto& chunk = resource.loose.gpu_chunks[0];
            if (chunk.offset > resource.loose.gpu.size() || chunk.size > resource.loose.gpu.size() - chunk.offset)
            {
                throw Error("font GPU chunk lies outside its file");
            }
            resource.record.gpu = resource.loose.gpu.subspan(chunk.offset, chunk.size);
        }
        return &resource;
    }
    const BundleEntry* entry = aids_.load_aid(key);
    if (entry == nullptr)
    {
        last_missing = key;
        return nullptr;
    }
    auto& resource = resources_[key];
    resource.record = {key, 0, entry->cpu, entry->gpu, {}, {}};
    if (entry->type != ResourceType::Model && !entry->gpu_chunks.empty())
    {
        // A texture's data is its first GPU chunk, as image_from_entry and transition_sprite read it.
        const auto& chunk = entry->gpu_chunks[0];
        if (chunk.offset > entry->gpu.size() || chunk.size > entry->gpu.size() - chunk.offset)
        {
            throw Error("resource GPU chunk lies outside its bundle entry");
        }
        resource.record.gpu = entry->gpu.subspan(chunk.offset, chunk.size);
    }
    if (!aids_.demand_entries.contains(key))
    {
        resource.entry = entry;
    }
    if (entry->type != ResourceType::Model)
    {
        // Retail resources have heap addresses that objects store; the copy gives this one an address.
        resource.record.cpu = memory.copy(entry->cpu, 0x10).bytes;
    }
    if (entry->type == ResourceType::Model)
    {
        prepare_model(resource, *entry);
        // Retail relocates every record when its bundle opens (0x00034730); the native provider
        // prepares records one at a time, so each runs that pass when it is first found.
        AnimationBundlePool pool{{&resource.record, 1}, true};
        relocate_animation_bundle_pool(pool, registry, loading, []
        {
            throw Error("scene resource relocation has no asynchronous audio pump");
        });
    }
    return &resource;
}

// Pointer-width adapter for a model record: a relocatable copy of its CPU block and of each GPU chunk,
// each at its own address, as the retail loader leaves them before the relocation pass.
void SceneDialogueResourceOwner::prepare_model(Resource& resource, const BundleEntry& entry)
{
    const auto cpu = memory.copy(entry.cpu, 0x10);
    for (const auto& chunk : entry.gpu_chunks)
    {
        if (chunk.offset > entry.gpu.size() || chunk.size > entry.gpu.size() - chunk.offset)
        {
            throw Error("scene model GPU chunk is outside its resource data: " + entry.name);
        }
        const auto gpu = memory.copy(entry.gpu.subspan(chunk.offset, chunk.size), 0x80);
        resource.segments.push_back({gpu.address, chunk.size});
    }
    resource.gpu_segments = {resource.segments};
    resource.record.cpu = cpu.bytes;
    resource.serialized = entry.cpu;
    resource.address = cpu.address;
    models.resources.emplace(cpu.bytes.data(),
        RuntimePreparedModelResource{{cpu.bytes, cpu.address}, &resource.gpu_segments});
}

void SceneDialogueResourceOwner::keep_texture_original(u32 data, size_t size)
{
    if (texture_originals_.contains(data))
    {
        return;
    }
    const auto bytes = memory.resolve(data, size);
    texture_originals_.emplace(data, std::vector<u8>(bytes.begin(), bytes.end()));
}

void SceneDialogueResourceOwner::restore_scene_textures()
{
    for (auto kept = texture_originals_.begin(); kept != texture_originals_.end();)
    {
        const u32 data = kept->first;
        bool restore = false;
        for (const auto& [name, resource] : resources_)
        {
            for (const auto& segment : resource.segments)
            {
                if (segment.address <= data && data - segment.address < segment.size)
                {
                    // A scene bundle's resource is read again with its bundle whatever still holds it (retired
                    // actors let go of their models later); common.bnl stays open, and a demand file goes when
                    // nothing references it.
                    const auto& common = aids_.common.entries;
                    const bool in_common = resource.entry != nullptr && resource.entry >= common.data()
                        && resource.entry < common.data() + common.size();
                    restore = resource.entry != nullptr ? !in_common : resource.record.references == 0;
                }
            }
        }
        if (restore)
        {
            const auto target = memory.resolve(data, kept->second.size());
            std::memcpy(target.data(), kept->second.data(), kept->second.size());
            if (on_texture_restored)
            {
                on_texture_restored(data);
            }
            kept = texture_originals_.erase(kept);
        }
        else
        {
            ++kept;
        }
    }
}

void SceneDialogueResourceOwner::prepare_text(Resource& resource)
{
    // Pointer-width adapter for the disk INFO/LSBL representation. Retain the
    // retail stride-6 string table and string IDs. Build the native key-name
    // lookup hash with the translated hash routine; disk key metadata is not
    // the native resource's hash table. The translated lookup searches both.
    const auto cpu = resource.record.cpu;
    size_t cursor = 12 + size_t(read_u32(cpu, 8)) * 8;
    for (u32 index = 0; index < read_u32(cpu, 8); ++index)
    {
        while (cursor < cpu.size() && cpu[cursor] != 0)
        {
            ++cursor;
        }
        ++cursor;
    }
    if (cursor + 36 > cpu.size() || std::memcmp(cpu.data() + cursor, "LSBL", 4) != 0)
    {
        throw Error("scene dialogue: invalid localization LSBL");
    }
    const size_t table = cursor + 28;
    const u32 count = read_u32(cpu, table + 4);
    cursor = table + 8 + size_t(count + 1) * 6;
    for (u32 index = 0; index <= count; ++index)
    {
        while (cursor + 2 <= cpu.size() && read_u16(cpu, cursor) != 0)
        {
            cursor += 2;
        }
        cursor += 2;
    }
    const u32 key_count = read_u32(cpu, cursor + 4);
    cursor += 8;
    const size_t names = cursor + size_t(key_count) * 8;
    if (names > cpu.size())
    {
        throw Error("scene dialogue: invalid localization key table");
    }
    resource.text_file.assign(cpu.begin(), cpu.end());
    const u32 native_table_offset = static_cast<u32>(table);
    std::memcpy(resource.text_file.data() + 12, &native_table_offset, 4);
    resource.keys.reserve(key_count);
    for (u32 index = 0; index < key_count; ++index)
    {
        const size_t row = cursor + size_t(index) * 8;
        const size_t offset = names + read_u32(cpu, row + 4);
        if (offset >= cpu.size())
        {
            throw Error("scene dialogue: invalid localization key offset");
        }
        const char* name = reinterpret_cast<const char*>(cpu.data() + offset);
        resource.keys.push_back({name, static_cast<u16>(runtime_localtext_hash(name)),
            read_u16(cpu, row)});
    }
    text_views.emplace(cpu.data(), LocalTextResourceView{resource.text_file, resource.keys});
}

void SceneDialogueResourceOwner::load_fonts()
{
    const char* names[]{"aid_font_localisation_gloopy_english_gloopyfont",
        "aid_font_localisation_dialog_english_dialogfont"};
    for (size_t index = 0; index < fonts.size(); ++index)
    {
        auto& file = font_files_[index];
        auto path = aids_.game_dir / "font" / aid_hash_hex(aid_hash(names[index]));
#if !defined(GBTG_RETAIL_GAME_ONLY)
        path = plugin_disc_path(path);
#endif
        file = parse_loose_resource(read_file(path));
        publish_font(index, file.cpu);
    }
}

// Metrics view of one font resource: first/last character, cell, drop and 24-byte glyph rows from +0x18.
void SceneDialogueResourceOwner::publish_font(size_t index, std::span<const u8> cpu)
{
    auto& font = fonts.at(index);
    font.first = static_cast<i32>(read_u32(cpu, 0));
    font.last = static_cast<i32>(read_u32(cpu, 4));
    font.cell = static_cast<i32>(read_u32(cpu, 12));
    font.drop = static_cast<i32>(read_u32(cpu, 16));
    auto& glyphs = glyphs_[index];
    glyphs.resize(size_t(font.last - font.first + 1));
    for (size_t glyph = 0; glyph < glyphs.size(); ++glyph)
    {
        const size_t row = 24 + glyph * 24;
        const u32 texture = read_u32(cpu, row);
        glyphs[glyph] = {texture == 0 ? nullptr : cpu.data() + texture,
            read_u16(cpu, row + 4), static_cast<i32>(read_u32(cpu, row + 8)),
            static_cast<i32>(read_u32(cpu, row + 16))};
    }
    font.glyphs = glyphs;
}
}
