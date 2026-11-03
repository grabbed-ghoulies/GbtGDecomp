#include "gbtg/model_relocation.h"

namespace gbtg
{

// Xbox 0x000534A0.
void postload_runtime_texture(const RuntimeRelocationImage& image, u32 texture,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 data = image.get(texture, model_texture_field::data);
    image.set(texture, model_texture_field::frame, 0);
    image.set(texture, model_texture_field::animated,
        u8(-int(image.get(texture, model_texture_field::frame_count) > 1)));
    if (data != 0 && image.get(texture, model_texture_field::frame_count) != 0)
    {
        i32 index = 0;
        u32 offset = 0;
        do
        {
            const u32 resource = image.get(texture, model_texture_field::frames) + offset + texture;
            d3d.set_texture_header(
                image.get(texture, model_texture_field::width), image.get(texture, model_texture_field::height),
                image.get(texture, model_texture_field::levels), 0, image.word(texture), 0, resource, 0, 0);
            d3d.register_resource(resource, data);
            data += image.get(texture, model_texture_field::frame_size);
            ++index;
            offset += model_texture_field::kFrameHeaderSize;
        } while (index < i32(image.get(texture, model_texture_field::frame_count)));
    }
}

// Xbox 0x000528C0. A missing type-7 table is dereferenced by the original.
u32 lookup_runtime_model_texture(const RuntimeRelocationImage& image, u32 directory, u32 index)
{
    u32 position = 0;
    u32 table = 0;
    if (image.get(directory, model_directory_field::count) != 0)
    {
        u32 entry = image.word(directory);
        do
        {
            if (image.word(entry) == 7)
            {
                table = image.word(image.word(directory) + position * 8, 4);
                break;
            }
            ++position;
            entry += 8;
        } while (position < image.get(directory, model_directory_field::count));
    }
    const u32 texture = image.word(image.get(table, model_texture_table_field::textures) + index * 4);
    return image.get(texture, model_texture_field::frames) +
           u32(image.get(texture, model_texture_field::frame)) * model_texture_field::kFrameHeaderSize + texture;
}

// Xbox 0x001417D0. The code arrives in AL; the locked texture remains locked.
void clear_runtime_special_texture(RuntimeSpecialTextures& textures, u32 code,
    const RuntimeNodeRelocationBoundaries& d3d)
{
    auto& entry = textures[code & 0xFF];
    u32 resource;
    if (!entry.get_texture)
    {
        resource = 0;
    }
    else
    {
        resource = entry.get_texture(entry.model, entry.index);
    }
    const auto locked = d3d.lock_texture(resource, 0, 0, 0);
    u32 offset = 0;
    for (u32 count = u32(entry.height) * u32(entry.width) & 0x3FFFFFFF; count != 0; --count)
    {
        const u32 zero = 0;
        std::memcpy(locked.data() + offset, &zero, 4);
        offset += 4;
    }
}

// Xbox 0x000527C0.
void postload_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    u32 directory, RuntimeSpecialTextures& textures, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.word(table) != 0)
    {
        do
        {
            const u32 texture = image.word(image.get(table, model_texture_table_field::textures) + index * 4);
            postload_runtime_texture(image, texture, d3d);
            const u8 code = image.get(texture, model_texture_field::code);
            if (code != 0 && code < 0x28)
            {
                auto& entry = textures[code];
                entry.get_texture = [image](u32 model, u32 texture_index)
                {
                    return lookup_runtime_model_texture(image, model, texture_index);
                };
                entry.model = directory;
                entry.index = index;
                entry.width = image.get(texture, model_texture_field::width);
                entry.height = image.get(texture, model_texture_field::height);
                entry.format = image.word(texture);
            }
            ++index;
        } while (index < image.word(table));
    }
}

// Xbox 0x00052850.
void unload_runtime_texture_table(const RuntimeRelocationImage& image, u32 table,
    RuntimeSpecialTextures& textures, const RuntimeNodeRelocationBoundaries& d3d)
{
    u32 index = 0;
    if (image.word(table) != 0)
    {
        do
        {
            const u8 code =
                image.byte(image.word(image.get(table, model_texture_table_field::textures) + index * 4), 0xE);
            if (code != 0 && code < 0x28)
            {
                if (code == 0x1E || code == 0x1F)
                {
                    clear_runtime_special_texture(textures, code, d3d);
                }
                auto& entry = textures[code];
                entry.get_texture = {};
                entry.model = 0;
                entry.index = 0;
                entry.width = 0;
                entry.height = 0;
            }
            ++index;
        } while (index < image.word(table));
    }
}

} // namespace gbtg
