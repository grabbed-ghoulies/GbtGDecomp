#pragma once
#include "gbtg/types.h"

#include <array>

namespace gbtg
{
// The running challenge's BSS state (DAT_004E17C8 .. DAT_004E19B0). Boot clears it; 0x00049FF0 ends a run at
// scene teardown.
struct RuntimeChallengeRun
{
    i32 score = 0; // DAT_004E18F0, the run's score, which the results page and its trophy read
};
extern RuntimeChallengeRun g_challenge_run;
}
