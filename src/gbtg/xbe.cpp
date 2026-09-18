#include "gbtg/xbe.hpp"

#include <algorithm>
#include <cstring>
#include <set>

namespace gbtg {
namespace {

constexpr u32 kEntryRetail = 0xA8FC57AB;
constexpr u32 kThunkRetail = 0x5B6D40B6;

std::string cstr_at(std::span<const u8> b, size_t off, size_t maxn = 256) {
    std::string s;
    for (size_t i = 0; i < maxn && off + i < b.size(); ++i) {
        if (!b[off + i]) {
            break;
        }
        if (b[off + i] < 32 || b[off + i] > 126) {
            break;
        }
        s.push_back(char(b[off + i]));
    }
    return s;
}

}  // namespace

XbeInfo parse_xbe(const std::filesystem::path& path) {
    auto bytes = read_file(path);
    if (bytes.size() < 0x178 || std::memcmp(bytes.data(), "XBEH", 4) != 0) {
        throw Error("not an XBE");
    }
    XbeInfo info;
    info.path = path;
    info.base = read_u32(bytes, 0x104);
    const u32 cert_addr = read_u32(bytes, 0x118);
    const u32 nsect = read_u32(bytes, 0x11C);
    const u32 sect_addr = read_u32(bytes, 0x120);
    const u32 entry = read_u32(bytes, 0x128);
    const u32 dbg_path = read_u32(bytes, 0x14C);
    const u32 dbg_file = read_u32(bytes, 0x150);
    const u32 thunk = read_u32(bytes, 0x158);
    const u32 nlib = read_u32(bytes, 0x160);
    const u32 lib_addr = read_u32(bytes, 0x164);
    info.entry_decoded = entry ^ kEntryRetail;
    info.thunk_decoded = thunk ^ kThunkRetail;

    auto va_off = [&](u32 va) -> size_t {
        if (va < info.base) {
            return size_t(-1);
        }
        return va - info.base;
    };

    const size_t coff = va_off(cert_addr);
    if (coff + 92 < bytes.size()) {
        info.title_id = read_u32(bytes, coff + 8);
        std::u16string title;
        for (int i = 0; i < 40; ++i) {
            const char16_t ch = char16_t(read_u16(bytes, coff + 12 + i * 2));
            if (!ch) {
                break;
            }
            title.push_back(ch);
        }
        info.title.reserve(title.size());
        for (char16_t ch : title) {
            if (ch < 128) {
                info.title.push_back(char(ch));
            }
        }
    }
    info.debug_path = cstr_at(bytes, va_off(dbg_path), 240);
    info.debug_file = cstr_at(bytes, va_off(dbg_file), 80);

    const size_t soff = va_off(sect_addr);
    for (u32 i = 0; i < nsect && i < 64; ++i) {
        const size_t rec = soff + i * 56;
        if (rec + 24 > bytes.size()) {
            break;
        }
        XbeSection s;
        s.flags = read_u32(bytes, rec + 0);
        s.virt_addr = read_u32(bytes, rec + 4);
        s.virt_size = read_u32(bytes, rec + 8);
        s.raw_addr = read_u32(bytes, rec + 12);
        s.raw_size = read_u32(bytes, rec + 16);
        const u32 name_addr = read_u32(bytes, rec + 20);
        s.name = cstr_at(bytes, va_off(name_addr), 32);
        info.sections.push_back(std::move(s));
    }

    const size_t loff = va_off(lib_addr);
    for (u32 i = 0; i < nlib && i < 32; ++i) {
        const size_t rec = loff + i * 16;
        if (rec + 16 > bytes.size()) {
            break;
        }
        XbeLibrary lib;
        char n[9]{};
        std::memcpy(n, bytes.data() + rec, 8);
        lib.name = n;
        lib.major = read_u16(bytes, rec + 8);
        lib.minor = read_u16(bytes, rec + 10);
        lib.build = read_u16(bytes, rec + 12);
        lib.qfe = read_u16(bytes, rec + 14);
        info.libraries.push_back(std::move(lib));
    }

    std::set<std::string> aids;
    std::set<std::string> interesting;
    const char* needles[] = {"audioLoad", "actorMind", "actorStrategy", "actorBody", "scriptCondition",
                             "objMsgId_", "entity", "ndVertex", "ndPush", "bundles\\", "demand\\",
                             "There is no script", "inflate ", "XACT::", "SetRenderTarget", "SetTexture"};
    size_t i = 0;
    while (i < bytes.size()) {
        if (bytes[i] < 32 || bytes[i] > 126) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < bytes.size() && bytes[j] >= 32 && bytes[j] <= 126) {
            ++j;
        }
        if (j - i >= 6) {
            std::string s(reinterpret_cast<const char*>(bytes.data() + i), j - i);
            if (s.rfind("aid_", 0) == 0) {
                const auto cut = s.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
                std::string name = cut == std::string::npos ? s : s.substr(0, cut);
                const auto type_end = name.find('_', 4);
                if (type_end != std::string::npos && type_end >= 7) {
                    bool type_ok = true;
                    for (size_t k = 4; k < type_end; ++k) {
                        if (name[k] < 'a' || name[k] > 'z') {
                            type_ok = false;
                            break;
                        }
                    }
                    if (type_ok) {
                        aids.insert(std::move(name));
                    }
                }
            }
            for (const char* n : needles) {
                if (s.find(n) != std::string::npos) {
                    interesting.insert(s);
                    break;
                }
            }
        }
        i = j + 1;
    }
    info.aid_strings.assign(aids.begin(), aids.end());
    info.interesting_strings.assign(interesting.begin(), interesting.end());
    return info;
}

}  // namespace gbtg
