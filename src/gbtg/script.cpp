#include "gbtg/script.hpp"

namespace gbtg
{

SceneScript parse_scene_script(std::span<const u8> cpu, std::string_view name)
{
    SceneScript s;
    s.name = std::string(name);
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
    return s;
}

} // namespace gbtg
