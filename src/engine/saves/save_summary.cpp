#include "save_summary.h"
#include "../frontend/gameselect_rows.h"
#include "../scene/scene_types.h"

namespace gbtg
{
namespace
{
[[noreturn]] void untranslated(const char* what)
{
    throw Error(std::string(what) + " is not translated (save record lookup over DAT_005462E4)");
}
}

// Xbox 0x000F2F00
void refresh_runtime_save_summary(GameSelectControlServices& services)
{
    auto& profile = services.profile;
    if (profile.name[0] == 0)
    {
        return;
    }
    if ((profile.scene_flags->bytes[0] & 2) == 0)
    {
        profile.times = {};
    }
    else
    {
        untranslated("0x0010DDA0"); // Then 0x0010DD60, 0x0010DD20, 0x0010DCE0 and 0x0010DCA0, EBX the name.
    }
    untranslated("0x0010DDE0"); // Then 0x000F71F0, 0x0010DE80, 0x000F2AC0, 0x0010DE30, 0x0010DED0, 0x0010E050.
}
}
