#pragma once

namespace gbtg
{
// Host diagnostics. Every std::out_of_range thrown (an unordered_map or vector .at() miss) prints the stack at its
// throw site to stderr before any handler runs; the first eight are reported. Each gbtg::Error thrown keeps its stack
// for its thread instead, as some callers catch one, and print_last_error_stack prints the last one. Stacks are
// symbolized where the build has a pdb (every Windows configuration of gbtg writes one) and are module offsets
// otherwise. Windows only; elsewhere both do nothing.
void install_throw_trace();
void print_last_error_stack(); // Where the last gbtg::Error on this thread was thrown, to stderr.
}
