#include "gbtg/script.hpp"

namespace gbtg
{
namespace
{

std::string first_text(std::span<const u8> payload)
{
    for (size_t i = 0; i < payload.size();)
    {
        const u8 c = payload[i];
        if (c < 32 || c >= 127)
        {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < payload.size() && payload[j] >= 32 && payload[j] < 127)
        {
            ++j;
        }
        if (j < payload.size() && payload[j] == 0 && j - i >= 4)
        {
            return std::string(reinterpret_cast<const char*>(payload.data() + i), j - i);
        }
        i = j > i ? j : i + 1;
    }
    return {};
}

void collect_strings(SceneScript& s, std::span<const u8> cpu)
{
    for (size_t i = 0; i < cpu.size();)
    {
        const u8 c = cpu[i];
        if (c < 32 || c >= 127)
        {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < cpu.size() && cpu[j] >= 32 && cpu[j] < 127)
        {
            ++j;
        }
        if (j < cpu.size() && cpu[j] == 0 && j - i >= 4)
        {
            std::string str(reinterpret_cast<const char*>(cpu.data() + i), j - i);
            s.strings.push_back(str);
            if (str.starts_with("aid_"))
            {
                s.aids.push_back(std::move(str));
            }
            i = j + 1;
            continue;
        }
        i = j > i ? j : i + 1;
    }
}

} // namespace

SceneScript parse_scene_script(std::span<const u8> cpu, std::string_view name)
{
    SceneScript s;
    s.name = std::string(name);
    s.bytes.assign(cpu.begin(), cpu.end());
    const size_t nwords = cpu.size() / 4;
    s.words.reserve(nwords);
    for (size_t i = 0; i < nwords; ++i)
    {
        s.words.push_back(read_u32(cpu, i * 4));
    }
    if (!s.words.empty() && s.words[0] < cpu.size())
    {
        s.root_offset = s.words[0];
    }

    u32 off = 0;
    while (off + 8 <= cpu.size())
    {
        const u32 size = read_u32(cpu, off);
        const u32 type = read_u32(cpu, off + 4);
        if (size < 8 || (size & 3) != 0 || off + size > cpu.size())
        {
            break;
        }
        ScriptNode node;
        node.offset = off;
        node.size = size;
        node.type = type;
        node.link = off + size;
        node.text = first_text(cpu.subspan(off + 8, size - 8));
        s.nodes.push_back(std::move(node));
        off += size;
        if (type == 0)
        {
            break;
        }
    }

    collect_strings(s, cpu);
    return s;
}

} // namespace gbtg
