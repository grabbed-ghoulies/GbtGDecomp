#if !defined(GBTG_RETAIL_GAME_ONLY)
// Scene recipes (scene_recipe.h): parse, write and assemble.
#include "scene_recipe.h"

#include "../scene/executable_resources.h"
#include "gbtg/bundle_builder.h"

#include <bit>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace gbtg
{
namespace
{
// The marker blob's last row: size 0x28, kind 0, id 1, a unit scale at +0x24.
constexpr u8 kMarkerTerminator[0x28] = {0x28, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x80, 0x3F};

std::string trim(std::string_view text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first])))
    {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
    {
        --last;
    }
    return std::string(text.substr(first, last - first));
}

std::string lower(std::string_view text)
{
    std::string out(text);
    for (char& c : out)
    {
        c = char(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<std::string> words(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream stream(line);
    std::string word;
    while (stream >> word)
    {
        out.push_back(word);
    }
    return out;
}

[[noreturn]] void fail(const std::string& where, size_t line, const std::string& what)
{
    throw Error("scene recipe " + where + ":" + std::to_string(line) + ": " + what);
}

u32 parse_number(const std::string& text, const std::string& where, size_t line)
{
    u32 value = 0;
    const bool hex = text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const char* first = text.data() + (hex ? 2 : 0);
    const auto [end, error] = std::from_chars(first, text.data() + text.size(), value, hex ? 16 : 10);
    if (error != std::errc() || end != text.data() + text.size())
    {
        fail(where, line, "not a number: " + text);
    }
    return value;
}

float parse_float(const std::string& text, const std::string& where, size_t line)
{
    char* end = nullptr;
    const float value = std::strtof(text.c_str(), &end);
    if (end != text.c_str() + text.size())
    {
        fail(where, line, "not a number: " + text);
    }
    return value;
}

std::vector<u8> parse_hex(const std::string& text, const std::string& where, size_t line)
{
    if (text.size() % 2 != 0)
    {
        fail(where, line, "hex needs two digits a byte: " + text);
    }
    std::vector<u8> out(text.size() / 2);
    for (size_t index = 0; index < out.size(); ++index)
    {
        const auto [end, error] = std::from_chars(text.data() + index * 2, text.data() + index * 2 + 2, out[index], 16);
        if (error != std::errc() || end != text.data() + index * 2 + 2)
        {
            fail(where, line, "not hex: " + text);
        }
    }
    return out;
}

std::string format_hex(std::span<const u8> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const u8 byte : bytes)
    {
        out.push_back(digits[byte >> 4]);
        out.push_back(digits[byte & 15]);
    }
    return out;
}

std::string format_float(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", value);
    return text;
}

std::string format_type(u32 type)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", type);
    return text;
}

// The bytes a patch writes.
std::vector<u8> patch_bytes(const RecipePatch& patch)
{
    std::vector<u8> out;
    const auto put = [&out](u32 value, size_t size)
    {
        for (size_t index = 0; index < size; ++index)
        {
            out.push_back(u8(value >> (index * 8)));
        }
    };
    const std::string where = "patch";
    if (patch.kind == "u8" || patch.kind == "u16" || patch.kind == "u32" || patch.kind == "i32")
    {
        const size_t size = patch.kind == "u8" ? 1 : patch.kind == "u16" ? 2 : 4;
        u32 value = 0;
        if (patch.kind == "i32")
        {
            value = u32(std::stol(patch.value, nullptr, 0));
        }
        else
        {
            value = parse_number(patch.value, where, 0);
        }
        put(value, size);
    }
    else if (patch.kind == "f32")
    {
        put(std::bit_cast<u32>(parse_float(patch.value, where, 0)), 4);
    }
    else if (patch.kind == "hex")
    {
        out = parse_hex(patch.value, where, 0);
    }
    else if (patch.kind == "str")
    {
        out.assign(patch.value.begin(), patch.value.end());
        out.push_back(0);
    }
    else
    {
        throw Error("scene recipe: unknown set kind " + patch.kind);
    }
    return out;
}

const BundleEntry& source_entry(const RecipeSource& source, const RecipeBundles& bundles)
{
    const Bundle& bundle = bundles(source.bundle);
    const BundleEntry* entry = find_entry(bundle, source.aid);
    if (entry == nullptr)
    {
        throw Error("scene recipe: " + source.aid + " is not in " + source.bundle);
    }
    return *entry;
}

void write_item_fields(std::vector<u8>& bytes, const RecipeItem& item, const std::string& name)
{
    if (item.id)
    {
        if (bytes.size() < 8)
        {
            throw Error("scene recipe " + name + ": an id needs a marker row");
        }
        write_u16(bytes, 6, *item.id);
    }
    const auto put_vec = [&bytes, &name](size_t at, const Vec3& value)
    {
        if (bytes.size() < at + 12)
        {
            throw Error("scene recipe " + name + ": the row is too short for a pose");
        }
        write_u32(bytes, at, std::bit_cast<u32>(value.x));
        write_u32(bytes, at + 4, std::bit_cast<u32>(value.y));
        write_u32(bytes, at + 8, std::bit_cast<u32>(value.z));
    };
    if (item.position)
    {
        put_vec(0xC, *item.position);
    }
    if (item.rotation)
    {
        put_vec(0x18, *item.rotation);
    }
    for (const RecipePatch& patch : item.patches)
    {
        const std::vector<u8> value = patch_bytes(patch);
        if (size_t(patch.offset) + value.size() > bytes.size())
        {
            throw Error("scene recipe " + name + ": set 0x" + format_type(patch.offset) + " is past the item's end");
        }
        std::memcpy(bytes.data() + patch.offset, value.data(), value.size());
    }
}
}

std::vector<std::span<const u8>> script_nodes(std::span<const u8> script)
{
    std::vector<std::span<const u8>> nodes;
    size_t at = 0;
    while (at + 8 <= script.size())
    {
        const u32 size = read_u32(script, at);
        const u32 type = read_u32(script, at + 4);
        if (type == 0 || size < 8 || at + size > script.size())
        {
            break;
        }
        nodes.push_back(script.subspan(at, size));
        at += size;
    }
    return nodes;
}

std::vector<std::span<const u8>> marker_rows(std::span<const u8> markers)
{
    std::vector<std::span<const u8>> rows;
    size_t at = 0;
    while (at + 8 <= markers.size())
    {
        const u32 size = read_u32(markers, at);
        if (read_u16(markers, at + 4) == 0 || size < 8 || at + size > markers.size())
        {
            break;
        }
        rows.push_back(markers.subspan(at, size));
        at += size;
    }
    return rows;
}

std::vector<u8> resolve_recipe_item(const RecipeResource& resource, const RecipeItem& item,
    const RecipeBundles& bundles)
{
    std::vector<u8> bytes;
    if (item.source)
    {
        const BundleEntry& entry = source_entry(*item.source, bundles);
        if (resource.kind == RecipeResource::Kind::Script)
        {
            const auto nodes = script_nodes(entry.cpu);
            if (item.index >= nodes.size())
            {
                throw Error("scene recipe " + resource.aid + ": " + item.source->aid + " has no node "
                    + std::to_string(item.index));
            }
            bytes.assign(nodes[item.index].begin(), nodes[item.index].end());
        }
        else if (resource.kind == RecipeResource::Kind::Markers)
        {
            for (const auto row : marker_rows(entry.cpu))
            {
                if (read_u16(row, 6) == item.index)
                {
                    bytes.assign(row.begin(), row.end());
                    break;
                }
            }
            if (bytes.empty())
            {
                throw Error("scene recipe " + resource.aid + ": " + item.source->aid + " has no marker "
                    + std::to_string(item.index));
            }
        }
        else
        {
            bytes.assign(entry.cpu.begin(), entry.cpu.end());
        }
    }
    else if (resource.kind == RecipeResource::Kind::Data)
    {
        bytes = item.data;
    }
    else
    {
        // A node or row: its size, its type (a u32 node type, or a u16 marker kind and a zero id), then the data.
        bytes.resize(8 + item.data.size());
        write_u32(bytes, 0, u32(bytes.size()));
        if (resource.kind == RecipeResource::Kind::Script)
        {
            write_u32(bytes, 4, item.type);
        }
        else
        {
            write_u16(bytes, 4, u16(item.type));
        }
        std::memcpy(bytes.data() + 8, item.data.data(), item.data.size());
    }
    write_item_fields(bytes, item, resource.aid);
    return bytes;
}

std::vector<u8> resolve_recipe_resource(const RecipeResource& resource, const RecipeBundles& bundles,
    ResourceType& type)
{
    std::vector<u8> out;
    for (const RecipeItem& item : resource.items)
    {
        const std::vector<u8> bytes = resolve_recipe_item(resource, item, bundles);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }
    switch (resource.kind)
    {
    case RecipeResource::Kind::Script:
    {
        type = ResourceType::Script;
        const u8 terminator[8] = {8, 0, 0, 0, 0, 0, 0, 0};
        out.insert(out.end(), std::begin(terminator), std::end(terminator));
        break;
    }
    case RecipeResource::Kind::Markers:
        type = ResourceType::Marker;
        out.insert(out.end(), std::begin(kMarkerTerminator), std::end(kMarkerTerminator));
        break;
    case RecipeResource::Kind::Data:
        if (resource.items.size() != 1)
        {
            throw Error("scene recipe " + resource.aid + ": a resource is one copy or one new");
        }
        type = resource.items[0].source ? source_entry(*resource.items[0].source, bundles).type
                                        : ResourceType(resource.items[0].type);
        break;
    }
    return out;
}

Bundle executable_objparams_bundle(const SceneExecutableResources& executable)
{
    Bundle bundle;
    bundle.path = kExecutableSource;
    for (const size_t pool : {size_t(3), size_t(5)})
    {
        for (const ExecutableResourceRecord& record : executable.records(pool))
        {
            BundleEntry entry;
            entry.name = record.name;
            entry.type = ResourceType::Misc;
            entry.header.type = ResourceType::Misc;
            entry.header.magic = kResourceMagic;
            entry.header.file_chunk.size = u32(record.cpu.size());
            entry.cpu = record.cpu;
            bundle.entries.push_back(std::move(entry));
        }
    }
    bundle.file_count = u16(bundle.entries.size());
    return bundle;
}

bool recipe_objparams(const RecipeResource& resource)
{
    return resource.kind == RecipeResource::Kind::Data && lower(resource.aid).starts_with("aid_objparams_");
}

Bundle build_scene_recipe(const SceneRecipe& recipe, const RecipeBundles& bundles)
{
    BundleBuilder builder;
    for (const RecipeSource& part : recipe.parts)
    {
        builder.add(source_entry(part, bundles));
    }
    for (const RecipeResource& resource : recipe.resources)
    {
        if (recipe_objparams(resource))
        {
            continue; // It joins the executable's objparams (plugin_executable_objparams).
        }
        ResourceType type{};
        const std::vector<u8> bytes = resolve_recipe_resource(resource, bundles, type);
        builder.add_cpu(resource.aid, type, bytes);
    }
    return builder.build();
}

SceneRecipe parse_scene_recipe(std::string_view text, const std::string& where)
{
    SceneRecipe recipe;
    std::string section;
    RecipeResource* resource = nullptr;
    std::istringstream stream{std::string(text)};
    std::string raw;
    size_t number = 0;
    while (std::getline(stream, raw))
    {
        ++number;
        if (number == 1 && raw.size() >= 3 && std::memcmp(raw.data(), "\xEF\xBB\xBF", 3) == 0)
        {
            raw.erase(0, 3);
        }
        if (const size_t comment = raw.find(';'); comment != std::string::npos)
        {
            raw.resize(comment);
        }
        const std::string line = trim(raw);
        if (line.empty())
        {
            continue;
        }
        if (line.front() == '[')
        {
            if (line.back() != ']')
            {
                fail(where, number, "unclosed section");
            }
            const auto parts = words(line.substr(1, line.size() - 2));
            if (parts.empty())
            {
                fail(where, number, "empty section");
            }
            section = lower(parts[0]);
            resource = nullptr;
            if (section == "script" || section == "markers" || section == "resource")
            {
                if (parts.size() != 2)
                {
                    fail(where, number, "[" + parts[0] + "] needs the resource's AID");
                }
                resource = &recipe.resources.emplace_back();
                resource->aid = parts[1];
                resource->kind = section == "script" ? RecipeResource::Kind::Script
                    : section == "markers" ? RecipeResource::Kind::Markers : RecipeResource::Kind::Data;
            }
            else if (section != "bundle" && section != "parts")
            {
                fail(where, number, "unknown section [" + parts[0] + "]");
            }
            continue;
        }
        const auto parts = words(line);
        if (section == "bundle")
        {
            const size_t equals = line.find('=');
            const std::string key = equals == std::string::npos ? std::string() : lower(trim(line.substr(0, equals)));
            if (key == "path")
            {
                recipe.bundle = trim(line.substr(equals + 1));
            }
            else if (key == "transition")
            {
                recipe.transition = lower(trim(line.substr(equals + 1)));
            }
            else if (key == "fallback")
            {
                std::string list = line.substr(equals + 1);
                for (char& c : list)
                {
                    c = c == ',' ? ' ' : c;
                }
                for (const std::string& bundle : words(list))
                {
                    recipe.fallbacks.push_back(lower(bundle));
                }
            }
            else
            {
                fail(where, number, "[Bundle] has Path, Transition and Fallback");
            }
            continue;
        }
        if (section == "parts")
        {
            if (parts.size() != 2)
            {
                fail(where, number, "a part is <bundle> <aid>");
            }
            recipe.parts.push_back({parts[0], parts[1]});
            continue;
        }
        if (resource == nullptr)
        {
            fail(where, number, "a line outside a section");
        }
        const std::string verb = lower(parts[0]);
        const bool data = resource->kind == RecipeResource::Kind::Data;
        if (verb == "copy")
        {
            if (parts.size() != (data ? 3u : 4u))
            {
                fail(where, number, data ? "copy <bundle> <aid>" : "copy <bundle> <aid> <node index or marker id>");
            }
            RecipeItem& item = resource->items.emplace_back();
            item.source = RecipeSource{parts[1], parts[2]};
            if (!data)
            {
                item.index = parse_number(parts[3], where, number);
            }
            continue;
        }
        if (verb == "new")
        {
            if (parts.size() < 2 || parts.size() > 3)
            {
                fail(where, number, "new <type> [<hex>]");
            }
            RecipeItem& item = resource->items.emplace_back();
            item.type = parse_number(parts[1], where, number);
            if (parts.size() == 3)
            {
                item.data = parse_hex(parts[2], where, number);
            }
            continue;
        }
        if (resource->items.empty())
        {
            fail(where, number, verb + " before any copy or new");
        }
        RecipeItem& item = resource->items.back();
        if (verb == "id" && parts.size() == 2)
        {
            item.id = u16(parse_number(parts[1], where, number));
        }
        else if ((verb == "position" || verb == "rotation") && parts.size() == 4)
        {
            const Vec3 value{parse_float(parts[1], where, number), parse_float(parts[2], where, number),
                parse_float(parts[3], where, number)};
            (verb == "position" ? item.position : item.rotation) = value;
        }
        else if (verb == "set" && parts.size() >= 4)
        {
            RecipePatch patch;
            patch.offset = parse_number(parts[1], where, number);
            patch.kind = lower(parts[2]);
            // A str value runs to the end of the line.
            const size_t value = line.find(parts[3], line.find(parts[2]) + parts[2].size());
            patch.value = patch.kind == "str" ? trim(line.substr(value)) : parts[3];
            patch_bytes(patch); // Checks the kind and the value now, with the line number.
            item.patches.push_back(std::move(patch));
        }
        else
        {
            fail(where, number, "unknown line: " + line);
        }
    }
    if (recipe.bundle.empty())
    {
        fail(where, number, "no [Bundle] Path");
    }
    return recipe;
}

SceneRecipe read_scene_recipe(const std::filesystem::path& path)
{
    const std::vector<u8> bytes = read_file(path);
    return parse_scene_recipe(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()),
        path.string());
}

std::string format_scene_recipe(const SceneRecipe& recipe)
{
    std::ostringstream out;
    out << "; gbtg scene recipe (docs/PLUGINS.md, \"Scene recipes\"). gbtg builds the bundle from the disc.\n";
    out << "[Bundle]\nPath = " << recipe.bundle << "\n";
    if (!recipe.transition.empty())
    {
        out << "Transition = " << recipe.transition << "\n";
    }
    if (!recipe.fallbacks.empty())
    {
        out << "Fallback = ";
        for (size_t index = 0; index < recipe.fallbacks.size(); ++index)
        {
            out << (index == 0 ? "" : ", ") << recipe.fallbacks[index];
        }
        out << "\n";
    }
    if (!recipe.parts.empty())
    {
        out << "\n[Parts]\n";
        for (const RecipeSource& part : recipe.parts)
        {
            out << part.bundle << ' ' << part.aid << '\n';
        }
    }
    for (const RecipeResource& resource : recipe.resources)
    {
        const char* name = resource.kind == RecipeResource::Kind::Script ? "Script"
            : resource.kind == RecipeResource::Kind::Markers ? "Markers" : "Resource";
        out << "\n[" << name << ' ' << resource.aid << "]\n";
        for (const RecipeItem& item : resource.items)
        {
            if (item.source)
            {
                out << "copy " << item.source->bundle << ' ' << item.source->aid;
                if (resource.kind != RecipeResource::Kind::Data)
                {
                    out << ' ' << item.index;
                }
                out << '\n';
            }
            else
            {
                out << "new " << format_type(item.type);
                if (!item.data.empty())
                {
                    out << ' ' << format_hex(item.data);
                }
                out << '\n';
            }
            if (item.id)
            {
                out << "  id " << *item.id << '\n';
            }
            if (item.position)
            {
                out << "  position " << format_float(item.position->x) << ' ' << format_float(item.position->y) << ' '
                    << format_float(item.position->z) << '\n';
            }
            if (item.rotation)
            {
                out << "  rotation " << format_float(item.rotation->x) << ' ' << format_float(item.rotation->y) << ' '
                    << format_float(item.rotation->z) << '\n';
            }
            for (const RecipePatch& patch : item.patches)
            {
                out << "  set " << format_type(patch.offset) << ' ' << patch.kind << ' ' << patch.value << '\n';
            }
        }
    }
    return out.str();
}

void write_scene_recipe(const SceneRecipe& recipe, const std::filesystem::path& path)
{
    const std::string text = format_scene_recipe(recipe);
    write_file(path, std::span<const u8>(reinterpret_cast<const u8*>(text.data()), text.size()));
}

} // namespace gbtg
#endif
