#pragma once

#include <filesystem>
#include <string>

namespace gbtg
{
// Host presentation: a message box for an error that ends the run, so a player who started gbtg from Explorer or a
// launcher sees why it closed. Off until enable_error_dialogs; main turns it on only for interactive runs, never for
// tests or captures (--headless, --screenshot, --frames, profiling) or with GBTG_NO_ERROR_DIALOG set.
void enable_error_dialogs(const std::filesystem::path& log_file);
bool error_dialogs_enabled();
// Shows `message` with a pointer to the log, when enabled.
void show_error_dialog(const std::string& message);
// A dialog for a crash no handler catches (an access violation, a stack overflow), when enabled. Windows only.
void install_crash_dialog();
}
