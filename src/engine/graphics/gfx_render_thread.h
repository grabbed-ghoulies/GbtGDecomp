#pragma once

#include "gfx_backend.h"
#include "render_optimize.h"

#if GBTG_OPTIMIZE
namespace gbtg
{

// GBTG_OPTIMIZE step 11: the gfx backend on its own thread. The game thread keeps the tick, recording and the
// presenter's translation; the gfx_* calls it makes are copied into a queue the render thread replays, so a frame's
// submission and present overlap the presenter's later draws and the next tick. Calls that return results (target
// readbacks, readback_ui, the program query on a new program) drain the queue and run on the game thread while the
// render thread waits. Only the Vulkan backend runs this way; D3D11 and null stay synchronous.

// The threaded table wrapping `inner`, or nullptr when that backend stays on the game thread.
const GfxBackend* gfx_render_thread_backend(const GfxBackend& inner);

// Blocks until every frame and present_again queued so far has been presented. Plugin overlays (the debug UI) are
// built after it, since the render thread draws the previous build while presenting.
void gfx_render_thread_wait_presented();

} // namespace gbtg
#endif
