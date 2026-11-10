#pragma once

#include <filesystem>

// thirdparty/ beside the executable, where the build puts FFmpeg and DXC (cmake/GbtgThirdParty.cmake).
std::filesystem::path thirdparty_library_dir();
