#pragma once

#include "scene_types.h"

namespace gbtg
{
namespace scene_detail
{
const BundleEntry* find_aid(const Game& g, std::string_view name);

} // namespace scene_detail

void script_load(Game& g, std::string_view name);
// Releases the legacy host meshes of g.models (the front end still draws them).
void release_models(Game& g);

// DAT_004DEDAC

// Xbox 0x000341E0. When DAT_004DEDA4 is nonzero, walk the pool from end
// backward by 0x2c. The dword at record+0x28 (end - 4 before the step) is
// compared with 0. Zero would call 0x000348D0 with EBX = the record and
// 0x00034110 with ESI = the record, then reload DAT_004DEDA8. Those calls
// are not translated, so they are not made. The flag is then cleared.
// When DAT_004DEDAC is 0 and DAT_004DEDA0 < DAT_004DED9C,
// walk from begin while the cursor is below end and the dword at +0x24 is
// nonzero, stepping by the stride u16. The slot where +0x24 is 0, or 0 when
// the walk finishes, would be ESI for 0x00034290. A nonzero return would
// call 0x00034370 with that return on the stack and EAX = ESI. 0x00034290
// is not translated, so that return is not taken. The function returns
// DAT_004DEDAC.
// Xbox 0x000341E0
int scene_check_pending_records(); // Xbox 0x000341E0

void scene_order_advance(Game& g);

} // namespace gbtg
