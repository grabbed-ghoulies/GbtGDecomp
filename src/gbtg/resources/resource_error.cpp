#include "gbtg/resource_error.h"

namespace gbtg
{

// Xbox 0x00056D80.
void fill_error_surface(LockedErrorSurface target, int width, int top, int left,
                          int height, u32 color)
{
    const int bottom = height + top;
    if (top < bottom)
    {
        int x = left;
        do
        {
            for (; x < width + left; ++x)
            {
                u8* pixel = target.pixels + target.pitch * top + x * 4;
                pixel[0] = u8(color >> 16);
                pixel[2] = u8(color);
                pixel[1] = u8(color >> 8);
                pixel[3] = u8(color >> 24);
            }
            ++top;
            x = left;
        } while (top < bottom);
    }
}

// Xbox 0x00032770.
void draw_resource_error(ResourceErrorState& state, int message)
{
    auto& api = state.boundaries;
    const int handle = api.section_handle("dirtydisk_bitmap");
    if (handle != -1 || api.last_error() == 0)
    {
        const std::span<const u8> bitmap = api.load_section(handle);
        if (bitmap.data() != nullptr || api.last_error() == 0)
        {
            if (api.section_size(handle) != 0 || api.last_error() == 0)
            {
                if (state.surface_locks == 0)
                {
                    state.surface = api.back_buffer();
                    state.locked = api.lock_surface(state.surface);
                }
                ++state.surface_locks;
                fill_error_surface(state.locked, 640, 0, 0, 480, state.background);
                int width = int(read_u32(bitmap, 0x12));
                const int left = 320 - width;
                int row = 0;
                int y = 0xd0;
                do
                {
                    int x = 0;
                    if (0 < width)
                    {
                        int offset = left * 4;
                        do
                        {
                            const int bit = ((int(read_u32(bitmap, 0x16)) - row) +
                                              message * -32 - 1) * width + x;
                            if ((bitmap[size_t((bit >> 3) + 0x3e)] &
                                 u8(1u << ((7 - (u8(bit) & 7)) & 0x1f))) != 0)
                            {
                                u8* first = state.locked.pixels + y * state.locked.pitch + offset;
                                first[2] = 0xff;
                                first[1] = 0xff;
                                first[0] = 0xff;
                                first[3] = 0xff;
                                first[6] = 0xff;
                                first[5] = 0xff;
                                first[4] = 0xff;
                                first[7] = 0xff;
                                u8* second = state.locked.pixels + (y + 1) * state.locked.pitch + offset;
                                second[2] = 0xff;
                                second[1] = 0xff;
                                second[0] = 0xff;
                                second[3] = 0xff;
                                second[6] = 0xff;
                                second[5] = 0xff;
                                second[4] = 0xff;
                                second[7] = 0xff;
                            }
                            width = int(read_u32(bitmap, 0x12));
                            ++x;
                            offset += 8;
                        } while (x < width);
                    }
                    ++row;
                    y += 2;
                } while (y < 0x110);
                --state.surface_locks;
                if (state.surface_locks == 0)
                {
                    api.release_surface(state.surface);
                    state.surface = nullptr;
                    const u32 fence = api.insert_fence();
                    api.swap();
                    api.block_on_fence(fence);
                }
            }
        }
    }
}

// Xbox 0x00032700. The assembly passes DAT_0042A710 on both draw calls.
void resource_error(ResourceErrorState& state, int code)
{
    if (state.suppressed == 0)
    {
        state.boundaries.delete_file("Z:\\index.dat");
        const int message = state.message;
        state.code = code;
        draw_resource_error(state, message);
        draw_resource_error(state, message);
        do
        {
            int fps;
            if (state.pal == false || state.pal_override != false)
            {
                fps = 60;
            }
            else
            {
                fps = 50;
            }
            wait_performance_counter(state.counter, 1.f / float(fps));
        } while (true);
    }
}

} // namespace gbtg
