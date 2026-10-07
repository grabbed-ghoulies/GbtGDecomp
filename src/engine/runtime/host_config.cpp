#include "gbtg/host_config.h"

#include <charconv>
#include <fstream>
#include <iostream>
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
    const bool width = section == "video" && key == "width";
    const bool height = section == "video" && key == "height";
    const bool volume = section == "audio" && key == "mastervolume";
    if (!width && !height && !volume)
    {
        return;
    }
    int number = 0;
    if (!parse_int(value, number))
    {
        std::cerr << "config: bad " << key << " in " << path.string() << "\n";
        return;
    }
    if (width)
    {
        const int clamped = clamp_dim(number);
        if (clamped != number)
        {
            std::cerr << "config: Width clamped to " << clamped << "\n";
        }
        cfg.width = clamped;
    }
    else if (height)
    {
        const int clamped = clamp_dim(number);
        if (clamped != number)
        {
            std::cerr << "config: Height clamped to " << clamped << "\n";
        }
        cfg.height = clamped;
    }
    else
    {
        const int clamped = clamp_volume(number);
        if (clamped != number)
        {
            std::cerr << "config: MasterVolume clamped to " << clamped << "\n";
        }
        cfg.master_volume = clamped;
    }
}

void parse_file(const std::filesystem::path& path, HostConfig& cfg)
{
    std::ifstream in(path);
    if (!in)
    {
        std::cerr << "config: could not read " << path.string() << "\n";
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
        std::cerr << "config: could not write " << path.string() << "\n";
        return false;
    }
    out << "; GbtGDecomp PC settings. Edit, then restart.\n"
           "\n"
           "[Video]\n"
           "Width=960\n"
           "Height=540\n"
           "\n"
           "[Audio]\n"
           "; 0 to 100\n"
           "MasterVolume=100\n";
    if (!out)
    {
        std::cerr << "config: could not write " << path.string() << "\n";
        return false;
    }
    return true;
}

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

} // namespace

HostConfig load_host_config(const std::filesystem::path& argv0)
{
    HostConfig cfg;
    const auto exe = executable_path(argv0);
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    const auto exe_dir = exe.empty() ? cwd : exe.parent_path();
    const auto beside = exe_dir / "gbtg.ini";
    ec.clear();
    if (std::filesystem::is_regular_file(beside, ec))
    {
        parse_file(beside, cfg);
        std::cout << "config " << beside.string() << "\n";
        return cfg;
    }
    if (!cwd.empty())
    {
        const auto cwd_ini = cwd / "gbtg.ini";
        ec.clear();
        if (cwd_ini != beside && std::filesystem::is_regular_file(cwd_ini, ec))
        {
            parse_file(cwd_ini, cfg);
            std::cout << "config " << cwd_ini.string() << "\n";
            return cfg;
        }
    }
    if (write_default(beside))
    {
        std::cout << "config " << beside.string() << " (created)\n";
    }
    return cfg;
}

} // namespace gbtg
