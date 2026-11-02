#include "frontend_state.h"
#include "frontend_sounds.h"
#include "input_activation.h"

#include <unordered_map>

namespace gbtg
{
extern i32 g_frontend_next_row; // DAT_00429BC0, scene.cpp.
extern u16 g_frontend_selected_game[0x80]; // DAT_00429BC4, scene.cpp.

RuntimeFrontendState g_frontend;

// Xbox 0x000E5A80
void reset_runtime_frontend(RuntimeFrontendState& state, FrontendPauseDitty& ditty)
{
    g_frontend_next_row = 1;
    g_frontend_selected_game[0] = 0;
    state.selected_name[0] = 0;
    ditty.playing = 0;
    state.real_0042a0cc = 0.f;
    state.word_0042a0c8 = 0;
    state.word_0042a0c4 = 0;
    for (auto& name : state.slot_names)
    {
        name[0] = 0;
    }
    // 0x00107EF0, ESI DAT_00429BBC. A keyboard device resets without reaching any other device.
    static const std::unordered_map<u32, std::span<u8>> none;
    state.input = create_runtime_keyboard_input(RuntimeInputReleaseContext{none});
}
}
