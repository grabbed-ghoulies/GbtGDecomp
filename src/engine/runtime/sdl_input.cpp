#include "sdl_input.h"

#include "gbtg/debug_ui.h" // Defines GBTG_DEBUG_UI when the debug UI is enabled.

#include <SDL3/SDL.h>

#include <array>
#include <utility>

namespace gbtg
{
namespace
{
// The gamepad on each XInput port, in connection order.
std::array<SDL_Gamepad*, 4> g_ports{};

void refresh_ports()
{
    for (SDL_Gamepad*& pad : g_ports)
    {
        if (pad != nullptr && !SDL_GamepadConnected(pad))
        {
            SDL_CloseGamepad(pad);
            pad = nullptr;
        }
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr)
    {
        return;
    }
    for (int index = 0; index < count; ++index)
    {
        bool assigned = false;
        for (SDL_Gamepad* pad : g_ports)
        {
            assigned = assigned || (pad != nullptr && SDL_GetGamepadID(pad) == ids[index]);
        }
        if (assigned)
        {
            continue;
        }
        for (SDL_Gamepad*& pad : g_ports)
        {
            if (pad == nullptr)
            {
                pad = SDL_OpenGamepad(ids[index]);
                break;
            }
        }
    }
    SDL_free(ids);
}

// XInput's thumbstick Y is up-positive; SDL's is down-positive.
i16 flip_axis(Sint16 value)
{
    return value == -32768 ? i16(32767) : i16(-value);
}

// XInput trigger bytes are 0..255; SDL triggers are 0..32767.
u8 trigger_byte(Sint16 value)
{
    return value <= 0 ? u8(0) : u8((int(value) * 255 + 16383) / 32767);
}
}

void poll_sdl_input(InputState& in)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    // Xbox pad is read in frame_tick. On PC the host copies the keyboard into that same pad state.
#if defined(GBTG_DEBUG_UI)
    // F8 and F12 never enter InputState. While the menu bar is open, keyboard
    // mappings stay clear so the scene list can use those keys.
    static bool f8_down = false;
    static bool f12_down = false;
    const bool f8 = keys[SDL_SCANCODE_F8];
    const bool f12 = keys[SDL_SCANCODE_F12];
    if (f8 && !f8_down)
    {
        debug_ui_toggle_menu();
    }
    if (f12 && !f12_down)
    {
        debug_ui_toggle_overlay();
    }
    f8_down = f8;
    f12_down = f12;
    if (!debug_ui_menu_open())
#endif
    {
        in.up = in.up || keys[SDL_SCANCODE_UP];
        in.down = in.down || keys[SDL_SCANCODE_DOWN];
        in.left = in.left || keys[SDL_SCANCODE_LEFT];
        in.right = in.right || keys[SDL_SCANCODE_RIGHT];
        in.accept = in.accept || keys[SDL_SCANCODE_RETURN];
        in.back = in.back || keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_B];
        // Start is its own bit. Enter stays A so the two are never down together.
        in.start = in.start || keys[SDL_SCANCODE_SPACE];
        // 0x00014C10. X is bit 0x0400 (keyboard code 5). '[' and ']' stand in for
        // the trigger bits 0x4000 and 0x8000 (codes 9 and 10).
        in.page = in.page || keys[SDL_SCANCODE_X];
        in.caret_left = in.caret_left || keys[SDL_SCANCODE_LEFTBRACKET];
        in.caret_right = in.caret_right || keys[SDL_SCANCODE_RIGHTBRACKET];
        // Keyboard stand-ins for the sticks on port 0: WASD is the left stick, IJKL the right.
        const auto axis = [keys](SDL_Scancode negative, SDL_Scancode positive) -> i16
        {
            const bool low = keys[negative];
            const bool high = keys[positive];
            return i16(high == low ? 0 : high ? 32767 : -32767);
        };
        in.pads[0].lx = axis(SDL_SCANCODE_A, SDL_SCANCODE_D);
        in.pads[0].ly = axis(SDL_SCANCODE_S, SDL_SCANCODE_W);
        in.pads[0].rx = axis(SDL_SCANCODE_J, SDL_SCANCODE_L);
        in.pads[0].ry = axis(SDL_SCANCODE_K, SDL_SCANCODE_I);
    }

    if (SDL_WasInit(SDL_INIT_GAMEPAD) == 0)
    {
        return;
    }
    refresh_ports();
    // 0x001555B0 polls XInput into the four ports: thumbsticks, trigger bytes and the button word. The shoulders
    // stand in for Black (RB) and White (LB); 0x00154E50 sets 0x4000/0x8000 from trigger bytes >= 30. SDL names
    // face buttons by position, which is the Xbox layout (south A, east B, west X, north Y).
    for (size_t port = 0; port < g_ports.size(); ++port)
    {
        SDL_Gamepad* pad = g_ports[port];
        if (pad == nullptr)
        {
            continue;
        }
        auto& packet = in.pads[port];
        packet.lx = i16(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
        packet.ly = flip_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
        packet.rx = i16(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX));
        packet.ry = flip_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
        packet.trigger_l = trigger_byte(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
        packet.trigger_r = trigger_byte(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
        const std::pair<SDL_GamepadButton, u16> map[]{{SDL_GAMEPAD_BUTTON_DPAD_UP, 0x0001},
            {SDL_GAMEPAD_BUTTON_DPAD_DOWN, 0x0002}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, 0x0004},
            {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, 0x0008}, {SDL_GAMEPAD_BUTTON_START, 0x0010},
            {SDL_GAMEPAD_BUTTON_BACK, 0x0020}, {SDL_GAMEPAD_BUTTON_LEFT_STICK, 0x0040},
            {SDL_GAMEPAD_BUTTON_RIGHT_STICK, 0x0080}, {SDL_GAMEPAD_BUTTON_SOUTH, 0x0100},
            {SDL_GAMEPAD_BUTTON_EAST, 0x0200}, {SDL_GAMEPAD_BUTTON_WEST, 0x0400},
            {SDL_GAMEPAD_BUTTON_NORTH, 0x0800}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0x1000},
            {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0x2000}};
        for (const auto& [from, to] : map)
        {
            if (SDL_GetGamepadButton(pad, from))
            {
                packet.buttons = u16(packet.buttons | to);
            }
        }
        if (packet.trigger_l >= 30)
        {
            packet.buttons = u16(packet.buttons | 0x4000);
        }
        if (packet.trigger_r >= 30)
        {
            packet.buttons = u16(packet.buttons | 0x8000);
        }
        // The host frontend still reads these booleans directly.
        in.up = in.up || (packet.buttons & 0x0001) != 0;
        in.down = in.down || (packet.buttons & 0x0002) != 0;
        in.left = in.left || (packet.buttons & 0x0004) != 0;
        in.right = in.right || (packet.buttons & 0x0008) != 0;
        in.start = in.start || (packet.buttons & 0x0010) != 0;
        in.accept = in.accept || (packet.buttons & 0x0100) != 0;
        in.back = in.back || (packet.buttons & 0x0200) != 0;
        in.page = in.page || (packet.buttons & 0x0400) != 0;
        in.caret_left = in.caret_left || (packet.buttons & 0x4000) != 0;
        in.caret_right = in.caret_right || (packet.buttons & 0x8000) != 0;
    }
}
}
