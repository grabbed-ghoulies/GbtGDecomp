#include "gbtg/gfx.hpp"

namespace gbtg
{

bool gfx_init(const GfxConfig&)
{
    return true;
}

void gfx_shutdown()
{
}

const char* gfx_backend_name()
{
    return "null";
}

bool gfx_has_window()
{
    return false;
}

void gfx_pump(bool&)
{
}

void gfx_begin_frame()
{
}

void gfx_set_camera(const Vec3&, const Quat&, float)
{
}

void gfx_set_bones(const float*, int)
{
}

void gfx_draw_mesh(const Mesh&, Vec3, Vec3)
{
}

void gfx_draw_sprite(const RgbaImage&, const GfxRect&)
{
}

void gfx_size(int& width, int& height)
{
    width = 0;
    height = 0;
}

void gfx_end_frame()
{
}

bool gfx_readback(RgbaImage&)
{
    return false;
}

} // namespace gbtg
