#pragma once

namespace gbtg
{
// Host diagnostics. Every std::out_of_range thrown (an unordered_map or vector .at() miss) prints the stack at its
// throw site to stderr before any handler runs, symbolized where the build has a pdb (RelWithDebInfo, Debug), and as
// module offsets otherwise. The first eight are reported. Windows only; elsewhere it installs nothing.
void install_out_of_range_trace();
}
