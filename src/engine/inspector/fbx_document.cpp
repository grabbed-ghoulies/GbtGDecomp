#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "fbx_document.h"

#include <cstring>
#include <zlib.h>

namespace gbtg
{
FbxProperty FbxProperty::i16(int value)
{
    FbxProperty p;
    p.type = 'Y';
    p.integer = value;
    return p;
}
FbxProperty FbxProperty::boolean(bool value)
{
    FbxProperty p;
    p.type = 'C';
    p.integer = value ? 1 : 0;
    return p;
}
FbxProperty FbxProperty::i32(std::int64_t value)
{
    FbxProperty p;
    p.type = 'I';
    p.integer = value;
    return p;
}
FbxProperty FbxProperty::f32(double value)
{
    FbxProperty p;
    p.type = 'F';
    p.real = value;
    return p;
}
FbxProperty FbxProperty::f64(double value)
{
    FbxProperty p;
    p.type = 'D';
    p.real = value;
    return p;
}
FbxProperty FbxProperty::i64(std::int64_t value)
{
    FbxProperty p;
    p.type = 'L';
    p.integer = value;
    return p;
}
FbxProperty FbxProperty::string(std::string value)
{
    FbxProperty p;
    p.type = 'S';
    p.text = std::move(value);
    return p;
}
FbxProperty FbxProperty::raw(std::string value)
{
    FbxProperty p;
    p.type = 'R';
    p.text = std::move(value);
    return p;
}
FbxProperty FbxProperty::f64s(std::vector<double> values)
{
    FbxProperty p;
    p.type = 'd';
    p.reals = std::move(values);
    return p;
}
FbxProperty FbxProperty::f32s(std::vector<double> values)
{
    FbxProperty p;
    p.type = 'f';
    p.reals = std::move(values);
    return p;
}
FbxProperty FbxProperty::i32s(std::vector<std::int64_t> values)
{
    FbxProperty p;
    p.type = 'i';
    p.integers = std::move(values);
    return p;
}
FbxProperty FbxProperty::i64s(std::vector<std::int64_t> values)
{
    FbxProperty p;
    p.type = 'l';
    p.integers = std::move(values);
    return p;
}

double FbxProperty::number() const
{
    switch (type)
    {
    case 'F':
    case 'D':
        return real;
    case 'Y':
    case 'C':
    case 'I':
    case 'L':
        return double(integer);
    default:
        return 0.0;
    }
}

std::vector<double> FbxProperty::numbers() const
{
    if (type == 'f' || type == 'd')
    {
        return reals;
    }
    if (type == 'i' || type == 'l' || type == 'b')
    {
        return std::vector<double>(integers.begin(), integers.end());
    }
    if (type == 'S' || type == 'R')
    {
        return {};
    }
    return {number()};
}

FbxNode& FbxNode::add(std::string child_name)
{
    children.push_back(FbxNode{std::move(child_name), {}, {}});
    return children.back();
}

FbxNode& FbxNode::add(std::string child_name, std::vector<FbxProperty> child_properties)
{
    children.push_back(FbxNode{std::move(child_name), std::move(child_properties), {}});
    return children.back();
}

const FbxNode* FbxNode::find(std::string_view child_name) const
{
    for (const FbxNode& child : children)
    {
        if (child.name == child_name)
        {
            return &child;
        }
    }
    return nullptr;
}

std::vector<const FbxNode*> FbxNode::all(std::string_view child_name) const
{
    std::vector<const FbxNode*> out;
    for (const FbxNode& child : children)
    {
        if (child.name == child_name)
        {
            out.push_back(&child);
        }
    }
    return out;
}

const FbxNode* FbxNode::p(std::string_view property_name) const
{
    const FbxNode* properties70 = find("Properties70");
    if (properties70 == nullptr)
    {
        return nullptr;
    }
    for (const FbxNode& entry : properties70->children)
    {
        if (!entry.properties.empty() && entry.properties[0].text == property_name)
        {
            return &entry;
        }
    }
    return nullptr;
}

namespace
{
constexpr char kMagic[] = "Kaydara FBX Binary  \x00\x1a\x00"; // 23 bytes with its NULs.

struct Reader
{
    std::span<const u8> bytes;
    bool wide = false; // Version 7500 and later: 64-bit record offsets.

    template <class T>
    T read(size_t& at) const
    {
        if (at + sizeof(T) > bytes.size())
        {
            throw Error("truncated FBX file");
        }
        T value;
        std::memcpy(&value, bytes.data() + at, sizeof(T));
        at += sizeof(T);
        return value;
    }

    FbxProperty property(size_t& at) const
    {
        FbxProperty p;
        p.type = char(read<u8>(at));
        switch (p.type)
        {
        case 'Y':
            p.integer = read<std::int16_t>(at);
            break;
        case 'C':
            p.integer = read<u8>(at);
            break;
        case 'I':
            p.integer = read<std::int32_t>(at);
            break;
        case 'F':
            p.real = read<float>(at);
            break;
        case 'D':
            p.real = read<double>(at);
            break;
        case 'L':
            p.integer = read<std::int64_t>(at);
            break;
        case 'S':
        case 'R':
        {
            const u32 length = read<u32>(at);
            if (at + length > bytes.size())
            {
                throw Error("truncated FBX string");
            }
            p.text.assign(reinterpret_cast<const char*>(bytes.data() + at), length);
            at += length;
            break;
        }
        case 'f':
        case 'd':
        case 'l':
        case 'i':
        case 'b':
        {
            const u32 count = read<u32>(at);
            const u32 encoding = read<u32>(at);
            const u32 length = read<u32>(at);
            if (at + length > bytes.size())
            {
                throw Error("truncated FBX array");
            }
            const size_t element = p.type == 'f' || p.type == 'i' ? 4 : p.type == 'b' ? 1 : 8;
            std::vector<u8> data(size_t(count) * element);
            if (encoding == 1)
            {
                uLongf size = uLongf(data.size());
                if (uncompress(data.data(), &size, bytes.data() + at, uLong(length)) != Z_OK || size != data.size())
                {
                    throw Error("an FBX array does not inflate");
                }
            }
            else
            {
                if (length != data.size())
                {
                    throw Error("an FBX array has the wrong length");
                }
                std::memcpy(data.data(), bytes.data() + at, data.size());
            }
            at += length;
            for (u32 index = 0; index < count; ++index)
            {
                const u8* q = data.data() + size_t(index) * element;
                switch (p.type)
                {
                case 'f':
                {
                    float v;
                    std::memcpy(&v, q, 4);
                    p.reals.push_back(v);
                    break;
                }
                case 'd':
                {
                    double v;
                    std::memcpy(&v, q, 8);
                    p.reals.push_back(v);
                    break;
                }
                case 'i':
                {
                    std::int32_t v;
                    std::memcpy(&v, q, 4);
                    p.integers.push_back(v);
                    break;
                }
                case 'l':
                {
                    std::int64_t v;
                    std::memcpy(&v, q, 8);
                    p.integers.push_back(v);
                    break;
                }
                default:
                    p.integers.push_back(*q);
                    break;
                }
            }
            break;
        }
        default:
            throw Error(std::string("unknown FBX property type '") + p.type + "'");
        }
        return p;
    }

    // One record; false at a NULL record (the end of a list).
    bool node(size_t& at, FbxNode& out, int depth) const
    {
        if (depth > 64)
        {
            throw Error("FBX records nest too deep");
        }
        const u64 end = wide ? read<u64>(at) : read<u32>(at);
        const u64 count = wide ? read<u64>(at) : read<u32>(at);
        const u64 list = wide ? read<u64>(at) : read<u32>(at);
        const u8 name_length = read<u8>(at);
        if (end == 0)
        {
            return false;
        }
        if (end > bytes.size() || at + name_length > bytes.size())
        {
            throw Error("an FBX record runs past the file");
        }
        out.name.assign(reinterpret_cast<const char*>(bytes.data() + at), name_length);
        at += name_length;
        const size_t properties_end = at + size_t(list);
        for (u64 index = 0; index < count; ++index)
        {
            out.properties.push_back(property(at));
        }
        at = properties_end;
        while (at < end)
        {
            FbxNode child;
            if (!node(at, child, depth + 1))
            {
                break;
            }
            out.children.push_back(std::move(child));
        }
        at = size_t(end);
        return true;
    }
};

struct Writer
{
    std::vector<u8> out;

    template <class T>
    void put(T value)
    {
        const u8* p = reinterpret_cast<const u8*>(&value);
        out.insert(out.end(), p, p + sizeof(T));
    }
    void set32(size_t at, u32 value)
    {
        std::memcpy(out.data() + at, &value, 4);
    }

    template <class T>
    void array(const FbxProperty& p, const std::vector<T>& values)
    {
        std::vector<u8> raw(values.size() * sizeof(T));
        if (!raw.empty())
        {
            std::memcpy(raw.data(), values.data(), raw.size());
        }
        put<u32>(u32(values.size()));
        // Compress arrays of any size worth it, as the SDK does.
        if (raw.size() > 128)
        {
            uLongf size = compressBound(uLong(raw.size()));
            std::vector<u8> packed(size);
            if (compress2(packed.data(), &size, raw.data(), uLong(raw.size()), Z_DEFAULT_COMPRESSION) != Z_OK)
            {
                throw Error("FBX array compression failed");
            }
            packed.resize(size);
            put<u32>(1);
            put<u32>(u32(packed.size()));
            out.insert(out.end(), packed.begin(), packed.end());
        }
        else
        {
            put<u32>(0);
            put<u32>(u32(raw.size()));
            out.insert(out.end(), raw.begin(), raw.end());
        }
        (void)p;
    }

    void property(const FbxProperty& p)
    {
        put<u8>(u8(p.type));
        switch (p.type)
        {
        case 'Y':
            put<std::int16_t>(std::int16_t(p.integer));
            break;
        case 'C':
            put<u8>(u8(p.integer != 0));
            break;
        case 'I':
            put<std::int32_t>(std::int32_t(p.integer));
            break;
        case 'F':
            put<float>(float(p.real));
            break;
        case 'D':
            put<double>(p.real);
            break;
        case 'L':
            put<std::int64_t>(p.integer);
            break;
        case 'S':
        case 'R':
            put<u32>(u32(p.text.size()));
            out.insert(out.end(), p.text.begin(), p.text.end());
            break;
        case 'f':
            array(p, std::vector<float>(p.reals.begin(), p.reals.end()));
            break;
        case 'd':
            array(p, p.reals);
            break;
        case 'i':
            array(p, std::vector<std::int32_t>(p.integers.begin(), p.integers.end()));
            break;
        case 'l':
            array(p, p.integers);
            break;
        case 'b':
            array(p, std::vector<u8>(p.integers.begin(), p.integers.end()));
            break;
        default:
            throw Error("unknown FBX property type");
        }
    }

    void null_record()
    {
        out.insert(out.end(), 13, 0);
    }

    void node(const FbxNode& n)
    {
        const size_t start = out.size();
        put<u32>(0); // End offset, patched below.
        put<u32>(u32(n.properties.size()));
        put<u32>(0); // Property list length.
        put<u8>(u8(n.name.size()));
        out.insert(out.end(), n.name.begin(), n.name.end());
        const size_t properties = out.size();
        for (const FbxProperty& p : n.properties)
        {
            property(p);
        }
        set32(start + 8, u32(out.size() - properties));
        // A child list ends with a NULL record; so does a record with no properties (as the SDK writes it).
        if (!n.children.empty() || n.properties.empty())
        {
            for (const FbxNode& child : n.children)
            {
                node(child);
            }
            null_record();
        }
        set32(start, u32(out.size()));
    }
};
}

std::vector<FbxNode> read_fbx(std::span<const u8> bytes)
{
    if (bytes.size() < 27 || std::memcmp(bytes.data(), kMagic, 23) != 0)
    {
        if (bytes.size() > 0 && (bytes[0] == ';' || bytes[0] == 'F'))
        {
            throw Error("this is an ASCII FBX file; export it as Binary");
        }
        throw Error("not a binary FBX file");
    }
    Reader reader{bytes};
    u32 version;
    std::memcpy(&version, bytes.data() + 23, 4);
    reader.wide = version >= 7500;
    std::vector<FbxNode> top;
    size_t at = 27;
    while (at < bytes.size())
    {
        FbxNode node;
        if (!reader.node(at, node, 0))
        {
            break;
        }
        top.push_back(std::move(node));
    }
    return top;
}

std::vector<u8> write_fbx(const std::vector<FbxNode>& top, u32 version)
{
    Writer writer;
    writer.out.assign(kMagic, kMagic + 23);
    writer.put<u32>(version);
    for (const FbxNode& node : top)
    {
        writer.node(node);
    }
    writer.null_record();
    // The footer: an ID, padding to 16 bytes, the version, zeros, and the closing magic.
    static constexpr u8 kFooterId[16]{0xfa, 0xbc, 0xab, 0x09, 0xd0, 0xc8, 0xd4, 0x66, 0xb1, 0x76, 0xfb, 0x83, 0x1c,
        0xf7, 0x26, 0x7e};
    static constexpr u8 kFooterMagic[16]{0xf8, 0x5a, 0x8c, 0x6a, 0xde, 0xf5, 0xd9, 0x7e, 0xec, 0xe9, 0x0c, 0xe3, 0x75,
        0x8f, 0x29, 0x0b};
    writer.out.insert(writer.out.end(), std::begin(kFooterId), std::end(kFooterId));
    writer.out.insert(writer.out.end(), 4, 0);
    const size_t padding = (16 - writer.out.size() % 16) % 16;
    writer.out.insert(writer.out.end(), padding, 0);
    writer.put<u32>(version);
    writer.out.insert(writer.out.end(), 120, 0);
    writer.out.insert(writer.out.end(), std::begin(kFooterMagic), std::end(kFooterMagic));
    return writer.out;
}
}
#endif
