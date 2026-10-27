// dxcapi.h needs the COM declarations WIN32_LEAN_AND_MEAN leaves out.
#define NOMINMAX
#include <windows.h>

#include "vk_shader_compiler.h"

#include "gbtg/log.h"

#include <dxc/dxcapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using Microsoft::WRL::ComPtr;

namespace gbtg::vk
{
namespace
{

struct Dxc
{
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> compiler;
};

// The SPIR-V capable dxcompiler.dll: beside the executable, else the Vulkan SDK's. The Windows SDK ships a
// dxcompiler.dll without SPIR-V code generation, so the loader's search path is not used.
HMODULE load_dxcompiler()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    path.resize(length);
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"dxcompiler.dll";
    if (HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
    {
        return module;
    }
    if (const wchar_t* sdk = _wgetenv(L"VULKAN_SDK"))
    {
        path = std::wstring(sdk) + L"\\Bin\\dxcompiler.dll";
        if (HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH))
        {
            return module;
        }
    }
    throw Error("vulkan: dxcompiler.dll (Vulkan SDK) was not found beside the executable or in %VULKAN_SDK%\\Bin");
}

Dxc& dxc()
{
    static Dxc instance = []
    {
        const auto create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(load_dxcompiler(), "DxcCreateInstance"));
        Dxc created;
        if (create == nullptr || FAILED(create(CLSID_DxcUtils, IID_PPV_ARGS(created.utils.GetAddressOf())))
            || FAILED(create(CLSID_DxcCompiler, IID_PPV_ARGS(created.compiler.GetAddressOf()))))
        {
            throw Error("vulkan: dxcompiler.dll has no usable DxcCreateInstance");
        }
        return created;
    }();
    return instance;
}

void replace_all(std::string& text, std::string_view from, std::string_view to)
{
    for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
    {
        text.replace(at, from.size(), to);
    }
}

std::wstring widen(const char* text)
{
    return std::wstring(text, text + std::strlen(text));
}

// Bump when the DXC arguments change: cached SPIR-V is keyed by its inputs, not by them.
constexpr u64 kSpirvCacheVersion = 1;

// cache/spirv/<FNV-1a of the inputs>.spv, beside cache/shaders.gbtg. Compiling every permutation through DXC
// takes several times as long as D3DCompile, so warm-up reads these instead.
std::filesystem::path spirv_cache_path(std::string_view source, const char* entry, const char* profile, bool strict)
{
    u64 hash = 0xcbf29ce484222325ull;
    const auto mix = [&](std::string_view bytes)
    {
        for (const char c : bytes)
        {
            hash = (hash ^ u8(c)) * 0x100000001b3ull;
        }
        hash = (hash ^ 0xFF) * 0x100000001b3ull;
    };
    mix(source);
    mix(entry);
    mix(profile);
    mix(strict ? "strict" : "");
    mix(std::to_string(kSpirvCacheVersion));
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.spv", static_cast<unsigned long long>(hash));
    return std::filesystem::path("cache") / "spirv" / name;
}

std::vector<u32> read_spirv(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    std::vector<u32> words(bytes.size() / 4);
    // A SPIR-V module starts with its magic number; anything else is a damaged cache entry.
    if (bytes.size() % 4 != 0 || words.empty())
    {
        return {};
    }
    std::memcpy(words.data(), bytes.data(), bytes.size());
    return words[0] == 0x07230203u ? words : std::vector<u32>{};
}

void write_spirv(const std::filesystem::path& path, const std::vector<u32>& words)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const std::filesystem::path partial = path.string() + ".tmp";
    {
        std::ofstream output(partial, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(words.data()), std::streamsize(words.size() * 4));
        if (!output)
        {
            return;
        }
    }
    std::filesystem::rename(partial, path, error);
}

} // namespace

std::string vulkan_hlsl(std::string source)
{
    // stage 0 and 1 share t0/s0 and t1/s1 with the host shader's g_tex and g_light in D3D11.
    replace_all(source, "Texture2D stage_tex[4] : register(t0);",
                "Texture2D stage_tex_2 : register(t2);\nTexture2D stage_tex_3 : register(t3);");
    replace_all(source, "SamplerState stage_samp[4] : register(s0);",
                "SamplerState stage_samp_2 : register(s2);\nSamplerState stage_samp_3 : register(s3);");
    replace_all(source, "stage_tex[0]", "g_tex");
    replace_all(source, "stage_tex[1]", "g_light");
    replace_all(source, "stage_tex[2]", "stage_tex_2");
    replace_all(source, "stage_tex[3]", "stage_tex_3");
    replace_all(source, "stage_samp[0]", "g_samp");
    replace_all(source, "stage_samp[1]", "g_light_samp");
    replace_all(source, "stage_samp[2]", "stage_samp_2");
    replace_all(source, "stage_samp[3]", "stage_samp_3");
    return source;
}

std::vector<u32> compile_hlsl_spirv(std::string_view source, const char* entry, const char* profile, bool strict)
{
    const std::filesystem::path cached = spirv_cache_path(source, entry, profile, strict);
    if (std::vector<u32> words = read_spirv(cached); !words.empty())
    {
        return words;
    }
    Dxc& compiler = dxc();
    const std::wstring entry_name = widen(entry);
    const std::wstring profile_name = widen(profile);
    const std::wstring b_shift = std::to_wstring(kConstantBinding);
    const std::wstring t_shift = std::to_wstring(kTextureBinding);
    const std::wstring s_shift = std::to_wstring(kSamplerBinding);
    // HLSL 2018 keeps the FXC-era semantics the D3D11 backend compiles under. The D3D11 clip space and depth
    // range are kept; the backend flips the viewport instead of the shaders.
    std::vector<const wchar_t*> arguments = {
        L"-spirv", L"-fspv-target-env=vulkan1.3", L"-fvk-use-dx-layout", L"-HV", L"2018", L"-O3",
        L"-fvk-b-shift", b_shift.c_str(), L"0",
        L"-fvk-t-shift", t_shift.c_str(), L"0",
        L"-fvk-s-shift", s_shift.c_str(), L"0",
        L"-E", entry_name.c_str(), L"-T", profile_name.c_str(),
    };
    if (strict)
    {
        arguments.push_back(L"-Gis");
    }
    DxcBuffer buffer{source.data(), source.size(), DXC_CP_UTF8};
    ComPtr<IDxcResult> result;
    if (FAILED(compiler.compiler->Compile(&buffer, arguments.data(), UINT(arguments.size()), nullptr,
                                          IID_PPV_ARGS(result.GetAddressOf()))))
    {
        throw Error("vulkan: DXC compile call failed");
    }
    HRESULT status = E_FAIL;
    result->GetStatus(&status);
    if (FAILED(status))
    {
        ComPtr<IDxcBlobUtf8> errors;
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr);
        log::error("hlsl: %s (%s): %s", entry, profile,
                   errors && errors->GetStringLength() != 0 ? errors->GetStringPointer() : "no diagnostics");
        return {};
    }
    ComPtr<IDxcBlob> object;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(object.GetAddressOf()), nullptr);
    if (!object || object->GetBufferSize() % 4 != 0)
    {
        return {};
    }
    std::vector<u32> words(object->GetBufferSize() / 4);
    std::memcpy(words.data(), object->GetBufferPointer(), object->GetBufferSize());
    write_spirv(cached, words);
    return words;
}

} // namespace gbtg::vk
