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

namespace
{

// DAT_004DE9AC. The present step advances this job by DAT_005096C0.
struct PresentFade
{
    float elapsed = 0.f;  // +0x2C
    float previous = 0.f; // +0x30
    float duration = 0.f; // +0x34
    float step = 0.f;     // +0x38
    int paused = 0;       // +0x158
};

// DAT_004DE9A0. +0x94C is the gate the phase machine tests.
struct PresentScene
{
    int marker = 0;
};

// *DAT_004DEA10. Cursor at +4, limit at +8.
struct PresentLoad
{
    u32 done = 0;
    u32 total = 0;
    u32 streaming = 0; // an entry whose stream byte at +0x11 is still above 1
};

// BSS counterparts of the load/fade globals. fs_init stores phase 0; the pointers stay clear
// until a load is installed, which is the retail boot state.
struct PresentGate
{
    PresentFade* fade = nullptr;   // DAT_004DE9AC
    PresentScene* scene = nullptr; // DAT_004DE9A0
    void* widget = nullptr;        // DAT_004DE99C
    PresentLoad* load = nullptr;   // *DAT_004DEA10
    u32 id_a = 0;                  // DAT_004DEA14
    u32 id_b = 0;                  // DAT_004DEA18
    int phase = 0;                 // DAT_004DD900
    float dt = 1.f / 60.f;         // DAT_005096C0
};

PresentGate present_gate;

// Xbox 0x00032AB0
// 0x0005FFA0: 1 when `id` is still streaming inside the load span.
bool present_load_id_busy(const PresentLoad* load, u32 id)
{
    (void)id;
    if (load == nullptr || load->done >= load->total)
    {
        return false;
    }
    return load->streaming != 0;
}

// Xbox 0x00032AB0
void present_32ab0()
{
    PresentFade* fade = present_gate.fade;
    if (fade != nullptr)
    {
        if (fade->duration <= fade->elapsed)
        {
            // 0x0004E4A0(0, 0, 0). Elapsed becomes duration * 0 and the step clears.
            const float old = fade->elapsed;
            fade->elapsed = 0.f;
            fade->previous = old;
            fade->step = 0.f;
        }
        // 0x0004DA50(dt). ESI is the fade system at 0x004DE9B0.
        if (fade->paused == 0)
        {
            const float dt = present_gate.dt;
            fade->previous = fade->elapsed;
            fade->elapsed += dt;
            fade->step = dt;
            if (fade->elapsed > fade->duration)
            {
                fade->elapsed = fade->duration;
            }
            if (fade->elapsed < 0.f)
            {
                fade->elapsed = 0.f;
            }
        }
    }

    PresentScene* scene = present_gate.scene;
    if (scene != nullptr && present_gate.phase > 0 && present_gate.phase < 4 && present_gate.widget != nullptr)
    {
        // 0x00142460 ticks the load widget. EDI is the scene.
        (void)scene;
    }
    scene = present_gate.scene;

    if (present_gate.phase == 1)
    {
        if (scene != nullptr)
        {
            if (!present_load_id_busy(present_gate.load, present_gate.id_a) &&
                !present_load_id_busy(present_gate.load, present_gate.id_b) && scene->marker != 0)
            {
                present_gate.phase = 2;
            }
        }
    }
    else if (present_gate.phase == 3 && present_gate.load != nullptr &&
             present_gate.load->total <= present_gate.load->done && scene != nullptr && scene->marker == 0)
    {
        // Tail call 0x00032BA0 with phase 4. Leaving phase 3 drops the load set.
        present_gate.load = nullptr;
        present_gate.scene = nullptr;
        present_gate.fade = nullptr;
        present_gate.widget = nullptr;
        present_gate.phase = 4;
    }
}

} // namespace

void gfx_end_frame()
{
    // Present 0x00032AB0 runs before the swap (0x00033A90 on Xbox).
    present_32ab0();
}

bool gfx_readback(RgbaImage&)
{
    return false;
}

} // namespace gbtg
