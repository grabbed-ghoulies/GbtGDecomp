#include "dialogue_resource_owner.h"
#include <cstring>

namespace gbtg
{
SceneDialogueResourceOwner::SceneDialogueResourceOwner(AidStore& aids) : aids_(aids)
{
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
        [](u32, const void* resolved, AnimationLoadedResource& output)
        {
            return load_bundle_resource_provider(resolved, output);
        },
        [this](u32, const char* name, const AnimationLoadedResource&)
        {
            auto& resource = resources_.at(name);
            std::array<AnimationBundlePool, 1> pools{
                AnimationBundlePool{{&resource.record, 1}, true}};
            unload_bundle_resource_provider(pools, name);
        }, 3, this);
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
        const auto path = aids_.game_dir / "loctext" / "english" / aid_hash_hex(aid_hash(key));
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
    const BundleEntry* entry = aids_.load_aid(key);
    if (entry == nullptr)
    {
        return nullptr;
    }
    auto& resource = resources_[key];
    resource.record = {key, 0, entry->cpu, entry->gpu, {}, {}};
    return &resource;
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
        file = parse_loose_resource(read_file(aids_.game_dir / "font" / aid_hash_hex(aid_hash(names[index]))));
        const auto cpu = file.cpu;
        auto& font = fonts[index];
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
}
