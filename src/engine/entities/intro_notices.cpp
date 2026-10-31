#include "intro_notices.h"
#include "../frontend/dialogue_activation.h"

namespace gbtg
{
// Xbox 0x0013FF70
u32 runtime_intro_notice_ready(const RuntimeIntroNotices& context)
{
    const u32 flags = context.runtime_flags;
    if ((context.scene_flags & 8) != 0 || (flags & 0x200000) != 0 || (flags & 0x4000) != 0
        || context.intro_pending != 0)
    {
        return 0;
    }
    const auto* node = static_cast<const entity_detail::ListHead*>(context.cases.next);
    if (node == nullptr)
    {
        return 1;
    }
    const EntityBitBlock* bits = context.case_body_bits(node);
    if (bits == nullptr)
    {
        return 1;
    }
    return (bits->bytes[3] & 8) != 0 ? 0 : 1;
}

// Xbox 0x0013FEB0
u32 queue_runtime_death_curse_intro(u8& story_byte, RuntimeIntroNotices& context)
{
    if (runtime_intro_notice_ready(context) == 0) // 0x0013FF70
    {
        return 0;
    }
    if ((story_byte & 0x10) == 0) // **(DAT_00506C6C+8)
    {
        story_byte = u8(story_byte | 0x10);
        const char* name = "intros_deathcurse"; // 0x00315060
        size_t index = 0;
        char value;
        do
        {
            value = name[index];
            context.intro_name[index] = value;
            ++index;
        } while (value != '\0');
        context.intro_pending = 1; // DAT_00545B38
    }
    return 1;
}

// Xbox 0x0013FF00
u32 queue_runtime_intro_notice(const char* name, RuntimeIntroNotices& context)
{
    if (runtime_intro_notice_ready(context) == 0)
    {
        return 0;
    }
    size_t index = 0;
    char value;
    do
    {
        value = name[index];
        context.intro_name[index] = value;
        ++index;
    } while (value != '\0');
    context.intro_pending = 1;
    return 1;
}

u32 g_545b38 = 0;
u32 g_545b3c = 0;
float g_545b40 = 0.f;
std::array<char, 0x80> g_545b44{};
std::array<char, 0x80> g_545bc4{};

// Xbox 0x0013FD60
void tick_runtime_intro_notice(RuntimeIntroNoticeTick& context)
{
    if (g_545b38 == 1)
    {
        if (context.clock.block_decay == 0 && context.clock.decay_enabled[context.clock.decay_index] != 0)
        {
            context.pause(u8(0xFFFF), 0); // 0x00117400
            ++context.pause_requests;
            g_545b38 = 2;
            g_545b40 = 1.f; // DAT_0032B0F8.
            g_545b3c = 1;
        }
        return;
    }
    if (g_545b38 == 2)
    {
        const float step = runtime_frame_step(context.clock);
        if (!(g_545b40 > 0.f))
        {
            return;
        }
        g_545b40 = g_545b40 - step;
        if (!(g_545b40 <= 0.f)) // JC: above zero or unordered.
        {
            return;
        }
        g_545b40 = 0.f;
        g_545b38 = 3;
        enqueue_runtime_dialogue_key(g_545b44.data(), 0, context.dialogue); // 0x000582A0, ECX DAT_00545B44
        if (g_545bc4[0] != 0)
        {
            enqueue_runtime_dialogue_key(g_545bc4.data(), 0, context.dialogue); // 0x000582A0, ECX DAT_00545BC4
        }
        return;
    }
    if (g_545b38 == 3)
    {
        finish_runtime_intro_notice(context); // 0x0013FFD0
    }
}

// Xbox 0x0013FFD0
void finish_runtime_intro_notice(RuntimeIntroNoticeTick& context)
{
    if (context.prompt_owner != 0)
    {
        return;
    }
    if (g_545b3c != 0)
    {
        context.pause(u8(0xFFFF), 1); // 0x00117400
        --context.pause_requests;
        g_545b3c = 0;
    }
    g_545b38 = 0;
    g_545b44[0] = 0;
    g_545bc4[0] = 0;
}
}
