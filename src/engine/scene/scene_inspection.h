#pragma once
#if defined(GBTG_INSPECTOR_APP)

#include "gbtg/types.h"
#include <map>
#include <set>
#include <utility>

namespace gbtg
{
using SceneInspectionMarker = std::pair<u16, u16>; // Kind, retail marker ID.

// Host inspection diagnostics, never installed by the game. A failed construction is abandoned;
// the inspector rebuilds a fresh native scene before applying the omission on the next attempt.
class SceneInspectionFailure : public Error
{
public:
    SceneInspectionFailure(SceneInspectionMarker source, const std::string& phase, const std::string& cause)
        : Error("marker kind " + std::to_string(source.first) + " id " + std::to_string(source.second)
            + " / " + phase + ": " + cause), marker(source)
    {
    }
    SceneInspectionMarker marker;
};

struct SceneInspectionPolicy
{
    std::set<SceneInspectionMarker> omitted;
    std::map<u32, SceneInspectionMarker> owners;
    bool drawing = false;

    template<class Function>
    bool run_marker(SceneInspectionMarker marker, Function&& function)
    {
        if (omitted.contains(marker))
        {
            return false;
        }
        try
        {
            function();
        }
        catch (const std::exception& error)
        {
            throw SceneInspectionFailure(marker, "construction", error.what());
        }
        return true;
    }
};
}
#endif
