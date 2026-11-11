#pragma once

// A host listening aid: an in-place effect on the mastering voice that writes the mix, before the master volume,
// to a 32-bit float WAV. pcm_out.cpp attaches it when the environment variable GBTG_RECORD_AUDIO names a file.

#include "xaudio2_host.h"

namespace gbtg
{
#if defined(_WIN32)
// A new recorder writing to path, or nullptr when the file cannot be opened. The caller releases its reference
// after putting it in an effect chain.
IUnknown* create_mix_recorder(const char* path);
#endif
}
