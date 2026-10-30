#include "gbtg/host_config.h"
#include "gbtg/language.h"
#include "gbtg/log.h"

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace gbtg
{
namespace
{

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r'))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
    {
        text.remove_suffix(1);
    }
    return text;
}

std::string lower_copy(std::string_view text)
{
    std::string out(text);
    for (char& ch : out)
    {
        if (ch >= 'A' && ch <= 'Z')
        {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return out;
}

bool parse_int(std::string_view text, int& out)
{
    text = trim(text);
    if (text.empty())
    {
        return false;
    }
    int value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto parsed = std::from_chars(begin, end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end)
    {
        return false;
    }
    out = value;
    return true;
}

int clamp_dim(int value)
{
    if (value < 320)
    {
        return 320;
    }
    if (value > 7680)
    {
        return 7680;
    }
    return value;
}

int clamp_vsync(int value)
{
    if (value > 2)
    {
        return 2;
    }
    if (value < 0)
    {
        return 0;
    }
    return value;
}

int clamp_volume(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 100)
    {
        return 100;
    }
    return value;
}

void apply_key(HostConfig& cfg, const std::filesystem::path& path, const std::string& section, const std::string& key,
               std::string_view value)
{
    if (section == "game" && key == "language")
    {
        if (const auto language = parse_language_name(value))
        {
            cfg.language = *language;
        }
        else
        {
            log::warn("config: unknown Language '%.*s' in %s (English, French, German, Spanish, Italian)",
                      static_cast<int>(value.size()), value.data(), path.string().c_str());
        }
        return;
    }
    if (section == "video" && key == "backend")
    {
        cfg.gfx_backend = lower_copy(value);
        return;
    }
    if (section == "video" && key == "widescreen")
    {
        int number = 0;
        if (!parse_int(value, number) || (number != 0 && number != 1))
        {
            log::warn("config: Widescreen must be 0 or 1 in %s", path.string().c_str());
            return;
        }
        cfg.widescreen = number != 0;
        return;
    }
    const bool width = section == "video" && key == "width";
    const bool height = section == "video" && key == "height";
    const bool vsync = section == "video" && key == "vsync";
    const bool volume = section == "audio" && key == "mastervolume";
    if (!width && !height && !volume && !vsync)
    {
        return;
    }
    int number = 0;
    if (!parse_int(value, number))
    {
        log::warn("config: bad %s in %s", key.c_str(), path.string().c_str());
        return;
    }
    if (width)
    {
        const int clamped = clamp_dim(number);
        if (clamped != number)
        {
            log::warn("config: Width clamped to %d", clamped);
        }
        cfg.width = clamped;
    }
    else if (height)
    {
        const int clamped = clamp_dim(number);
        if (clamped != number)
        {
            log::warn("config: Height clamped to %d", clamped);
        }
        cfg.height = clamped;
    }
    else if (vsync)
    {
        const int clamped = clamp_vsync(number);
        if (clamped != number)
        {
            log::warn("config: Vsync clamped to %d", clamped);
        }
        cfg.vsync = clamped;
    }
    else
    {
        const int clamped = clamp_volume(number);
        if (clamped != number)
        {
            log::warn("config: MasterVolume clamped to %d", clamped);
        }
        cfg.master_volume = clamped;
    }
}

void parse_file(const std::filesystem::path& path, HostConfig& cfg)
{
    std::ifstream in(path);
    if (!in)
    {
        log::warn("config: could not read %s", path.string().c_str());
        return;
    }
    std::string section;
    std::string line;
    while (std::getline(in, line))
    {
        std::string_view text = trim(line);
        const auto comment = text.find_first_of(";#");
        if (comment != std::string_view::npos)
        {
            text = trim(text.substr(0, comment));
        }
        if (text.empty())
        {
            continue;
        }
        if (text.front() == '[' && text.back() == ']')
        {
            section = lower_copy(trim(text.substr(1, text.size() - 2)));
            continue;
        }
        const auto eq = text.find('=');
        if (eq == std::string_view::npos)
        {
            continue;
        }
        const std::string key = lower_copy(trim(text.substr(0, eq)));
        const std::string_view value = trim(text.substr(eq + 1));
        apply_key(cfg, path, section, key, value);
    }
}

bool write_default(const std::filesystem::path& path)
{
    std::ofstream out(path);
    if (!out)
    {
        log::warn("config: could not write %s", path.string().c_str());
        return false;
    }
    out << "; GbtGDecomp PC settings. Edit, then restart.\n"
           "\n"
           "[Video]\n"
           "Width=960\n"
           "Height=540\n"
           "VSync=1\n"
           "; 1 plays in 16:9, as the Xbox's widescreen video setting does.\n"
           "Widescreen=0\n"
           "; d3d11 or vulkan. Empty uses the build's default.\n"
           "Backend=\n"
           "\n"
           "[Audio]\n"
           "; 0 to 100\n"
           "MasterVolume=100\n"
           "\n"
           "[Game]\n"
           "; English, French, German, Spanish or Italian\n"
           "Language=English\n";
    if (!out)
    {
        log::warn("config: could not write %s", path.string().c_str());
        return false;
    }
    return true;
}

} // namespace

std::filesystem::path executable_path(const std::filesystem::path& argv0)
{
#if defined(_WIN32)
    std::wstring buf(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size())
    {
        return std::filesystem::path(buf.substr(0, n));
    }
#endif
    if (!argv0.empty())
    {
        std::error_code ec;
        const auto absolute = std::filesystem::absolute(argv0, ec);
        if (!ec)
        {
            return absolute;
        }
    }
    return {};
}

std::filesystem::path host_exe_dir(const std::filesystem::path& argv0)
{
    const auto exe = executable_path(argv0);
    if (!exe.empty())
    {
        return exe.parent_path();
    }
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

HostConfig load_host_config(const std::filesystem::path& argv0)
{
    HostConfig cfg;
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    const auto beside = host_exe_dir(argv0) / "gbtg.ini";
    ec.clear();
    if (std::filesystem::is_regular_file(beside, ec))
    {
        parse_file(beside, cfg);
        log::info("config %s", beside.string().c_str());
        return cfg;
    }
    if (!cwd.empty())
    {
        const auto cwd_ini = cwd / "gbtg.ini";
        ec.clear();
        if (cwd_ini != beside && std::filesystem::is_regular_file(cwd_ini, ec))
        {
            parse_file(cwd_ini, cfg);
            log::info("config %s", cwd_ini.string().c_str());
            return cfg;
        }
    }
    if (write_default(beside))
    {
        log::info("config %s (created)", beside.string().c_str());
    }
    return cfg;
}

} // namespace gbtg
