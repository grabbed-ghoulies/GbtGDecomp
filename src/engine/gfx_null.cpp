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

} // namespace

void gfx_end_frame()
{
    // Present 0x00033480, then 0x00032AB0, then the swap (0x00033A90 on Xbox).
    present_33480();
    present_32ab0();
}

bool gfx_readback(RgbaImage&)
{
    return false;
}

} // namespace gbtg
