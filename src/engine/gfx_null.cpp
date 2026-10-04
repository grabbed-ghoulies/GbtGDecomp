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

void gfx_release_mesh(const Mesh&)
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

// Object at the front of a DAT_0050E0E8 record. That span's stride is 0x50.
// DAT_0050E0EC is the record count; phase 1 reads record count-1.
struct PresentSlot
{
    int kind = 0;               // +0
    void* child = nullptr;      // +4
    int state = 0;              // +8, read by 0x00107CE0
    PresentSlot* ports[4] = {}; // +8 when kind is 4 (0x00107AA0)
    u16 show = 0;               // flag word 0x001083F0 masks with 0x10
    u16 hide = 0;
};

// DAT_004DEA34 cluster. fs_init (0x00033980) stores phase 4 and clears the pointers.
struct PresentInsert
{
    int phase = 4;                 // DAT_004DEA34
    int force = 0;                 // DAT_004DEA38
    int hold = 0;                  // DAT_004DEA24, nonzero skips the span poll
    u32 pad_mask = 0;              // DAT_0032E3B4, returned by 0x001554C0(0)
    u32 pad_filter = 0;            // DAT_0050E0F8
    u16 block_show = 0;            // words at 0x00507C98, kind 3 of 0x001083F0
    u16 block_hide = 0;
    PresentLoad* cursor = nullptr; // *DAT_004DEA2C, +4 / +8
    PresentSlot* actor = nullptr;  // DAT_004DEA28
    PresentSlot** records = nullptr;
    int record_count = 0; // (end - begin) / stride
    int record_index = 0; // DAT_0050E0EC
};

PresentInsert present_insert;

// Xbox 0x00033480
// 0x00107CE0. Idle when the pad word's 0x10<<state bit is clear.
bool present_port_idle(const PresentSlot& slot)
{
    if (slot.state < 0 || slot.state > 3)
    {
        return true;
    }
    const u32 bit = 0x10u << slot.state;
    return (present_insert.pad_mask & bit) == 0;
}

// Xbox 0x00033480
// 0x001082B0. Zero lets phase 1 tail-call 0x000337B0 with phase 2.
bool present_slot_busy(const PresentSlot& slot)
{
    if (slot.kind == 2)
    {
        return (present_insert.pad_mask & present_insert.pad_filter) != 0;
    }
    if (slot.kind == 3)
    {
        return present_insert.pad_mask != 0;
    }
    if (slot.kind == 4)
    {
        for (const PresentSlot* port : slot.ports)
        {
            if (port != nullptr && port->child != nullptr && present_port_idle(*port))
            {
                return false;
            }
        }
        return true;
    }
    if (slot.kind == 5)
    {
        return !present_port_idle(slot);
    }
    return false;
}

// Xbox 0x00033480
// 0x001083F0(actor, 0x10). Show bit set and hide bit clear.
bool present_flags_ready(u16 show, u16 hide)
{
    return (show & 0x10) != 0 && (hide & 0x10) == 0;
}

// Xbox 0x00033480
bool present_actor_ready(const PresentSlot* actor)
{
    if (actor == nullptr || actor->kind < 1 || actor->kind > 5)
    {
        return false;
    }
    if (actor->kind == 3)
    {
        return present_flags_ready(present_insert.block_show, present_insert.block_hide);
    }
    if (actor->kind == 4)
    {
        for (const PresentSlot* port : actor->ports)
        {
            if (port != nullptr && port->state != -1 && present_flags_ready(port->show, port->hide))
            {
                return true;
            }
        }
        return false;
    }
    return present_flags_ready(actor->show, actor->hide);
}

// Xbox 0x00033480
// 0x000337B0. EAX is the phase being entered. Leaving phase 3 drops the cursor.
void present_insert_set_phase(int next)
{
    if (next == present_insert.phase)
    {
        return;
    }
    if (present_insert.phase == 3)
    {
        present_insert.cursor = nullptr;
        if (present_insert.actor != nullptr && present_insert.actor->child != nullptr)
        {
            present_insert.actor = nullptr;
        }
    }
    present_insert.phase = next;
}

// Xbox 0x00033480
void present_33480()
{
    switch (present_insert.phase)
    {
    case 1:
        if (present_insert.force != 0)
        {
            present_insert.force = 0;
            present_insert_set_phase(2);
            return;
        }
        if (present_insert.hold == 0 && present_insert.record_count != 0 && present_insert.records != nullptr &&
            present_insert.record_index > 0)
        {
            PresentSlot* slot = present_insert.records[present_insert.record_index - 1];
            if (slot != nullptr && slot->child != nullptr && !present_slot_busy(*slot))
            {
                present_insert_set_phase(2);
                return;
            }
        }
        break;
    case 2:
        // 0x00109470(DAT_004DEA28) refreshes the span record, then 0x001083F0(actor, 0x10).
        if (present_actor_ready(present_insert.actor))
        {
            present_insert_set_phase(3);
            return;
        }
        break;
    case 3:
        if (present_insert.cursor != nullptr && present_insert.cursor->total <= present_insert.cursor->done)
        {
            present_insert_set_phase(1);
            return;
        }
        break;
    case 4:
        if (present_insert.pad_mask != 0)
        {
            present_insert.phase = 1;
        }
        break;
    default:
        break;
    }
}

// Movie node on the DAT_0050E0B8 list. 0x00107420 reads it from ESI.
// +4 is the next link, +8 the clip, +0x28 the open-failed latch, +0x34 the enable.
struct PresentMovie
{
    PresentMovie* next = nullptr;
    void* clip = nullptr;
    int failed = 0;
    int enabled = 0;
};

// One of the four camera slots at DAT_004E16A0. 0x000489E0 returns 0 when the slot is null.
struct PresentCamera
{
    int marker = 0;
};

// Overlay record. 0x00106CD0 walks 0x0050DD90 through 0x0050DFB0, stride 0x88 (four records).
struct PresentOverlay
{
    void* object = nullptr; // +0
    u8 kind = 0;             // +4, indexes the callback table at 0x00545DC0
};

// Clear and movie gates. BSS, so a fresh boot leaves every pointer clear.
struct PresentFrame
{
    int post_pass = 0;    // DAT_00512AF0, first dword of the 0x2C post table
    int cover = 0;        // DAT_004DEA50, mirrors scene+0x93C
    int clear_mode = 0;   // DAT_0054632C, nonzero forces the target clear
    int movie_lock_a = 0; // DAT_004E5CD8
    int movie_lock_b = 0; // DAT_004E5CDC
    PresentMovie* movies = nullptr;
    PresentCamera* cameras[4] = {};
    PresentOverlay overlays[4] = {};
    void (*swap_notify)(int phase) = nullptr; // DAT_0043427C
};

PresentFrame present_frame;

// Xbox 0x00033A90
// Xbox clear masks: ZBUFFER is 1, STENCIL is 2, TARGET is 0xF0.
// Boot (both retain gates clear) uses 0xF3. A post pass or cover keeps the color target
// unless clear_mode is set.
u32 present_clear_flags()
{
    u32 flags = 3;
    if ((present_frame.post_pass == 0 && present_frame.cover == 0) || present_frame.clear_mode != 0)
    {
        flags = 0xf3;
    }
    return flags;
}

// Xbox 0x00033A90
// 0x00107420. Opening the clip is Bink (0x001A6010) and stays in that library.
void present_movie_tick(PresentMovie* node)
{
    if (node->enabled == 0 || node->failed != 0)
    {
        return;
    }
    if (node->clip == nullptr)
    {
        return;
    }
}

// Xbox 0x00033A90
void present_movies()
{
    if (present_frame.movie_lock_a != 0 || present_frame.movie_lock_b != 0)
    {
        return;
    }
    for (PresentMovie* node = present_frame.movies; node != nullptr; node = node->next)
    {
        present_movie_tick(node);
    }
}

// Xbox 0x00033A90
// 0x00106CD0. A null object skips the record.
void present_overlays()
{
    for (const PresentOverlay& overlay : present_frame.overlays)
    {
        if (overlay.object == nullptr)
        {
            continue;
        }
    }
}

// Xbox 0x00033A90
// 0x00033A20. Each non-null slot is bound by 0x000489E0 and drawn by 0x00011C240.
void present_cameras()
{
    for (PresentCamera* camera : present_frame.cameras)
    {
        if (camera == nullptr)
        {
            continue;
        }
    }
}

// Xbox 0x00033A90
// DAT_0033A5B8 is the full backbuffer: origin 0, retail 640x480, z 0..1.
void present_set_viewport()
{
}

// Xbox 0x00033A90
// 0x00154A20: D3DDevice_Clear(0, nullptr, flags, DAT_00434264, 1.0f, 0).
void present_clear(u32 flags)
{
    (void)flags;
}

// Xbox 0x00033A90
// 0x00154A40. Notify 0, D3DDevice_Swap(0), notify 1.
void present_swap()
{
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(0);
    }
    if (present_frame.swap_notify != nullptr)
    {
        present_frame.swap_notify(1);
    }
}

// Xbox 0x00033A90
void present_33a90()
{
    const u32 flags = present_clear_flags();
    present_set_viewport();
    present_clear(flags);
    // 0x0005E110 installs the frame render states.
    // 0x000F9780 retires the deferred span at DAT_005079E0.
    // 0x00036180 submits the draw span at DAT_004DF240 and resets it.
    present_movies();
    present_overlays();
    present_cameras();
    // 0x000F6F70 restores the HUD viewport from DAT_005096E8 / DAT_005096EA.
    // 0x00066080 draws the sprite span at DAT_00504960.
    present_swap();
    present_movies();
}

} // namespace

void gfx_end_frame()
{
    // Present 0x00033480, then 0x00032AB0, then 0x00033A90 (clear and swap).
    present_33480();
    present_32ab0();
    present_33a90();
}

bool gfx_readback(RgbaImage&)
{
    return false;
}

} // namespace gbtg
