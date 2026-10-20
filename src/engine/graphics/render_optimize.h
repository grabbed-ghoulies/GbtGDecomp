#pragma once
// GBTG_OPTIMIZE selects host presenter optimizations at compile time (CMake option, OFF by
// default). Native code and the recorded SceneD3DDevice stream are identical either way; only
// how the presenter turns that stream into D3D11 calls changes. The retail presenter path stays
// in the source, in the #else branch of each optimization.
#ifndef GBTG_OPTIMIZE
#define GBTG_OPTIMIZE 0
#endif
