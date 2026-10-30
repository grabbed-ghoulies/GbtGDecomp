#include "gbtg/game.h"
#include "../src/engine/scene/scene_actor_services.h"
#include "../src/engine/scene/factory_scene_runtime.h"
#include "../src/engine/graphics/scene_d3d_device.h"
#include "../src/engine/inspector/inspector_session.h"

template<class T>
concept HasInspectionPolicy = requires(T& value)
{
    value.inspection;
};

template<class T>
concept HasInspectionView = requires
{
    &T::inspect_view;
    &T::inspection_camera;
};

template<class T>
concept HasInspectionGeneration = requires(T& value)
{
    value.inspection_generation;
};

template<class T>
concept HasInspectionStates = requires(T& value)
{
    value.values();
};

#if defined(GBTG_INSPECTOR_APP)
static_assert(HasInspectionPolicy<gbtg::Game>);
static_assert(HasInspectionGeneration<gbtg::SceneFactoryRuntime>);
static_assert(HasInspectionView<gbtg::SceneActorServices>);
static_assert(HasInspectionStates<gbtg::SceneD3DStateSnapshot>);
#else
static_assert(!HasInspectionPolicy<gbtg::Game>);
static_assert(!HasInspectionGeneration<gbtg::SceneFactoryRuntime>);
static_assert(!HasInspectionView<gbtg::SceneActorServices>);
static_assert(!HasInspectionStates<gbtg::SceneD3DStateSnapshot>);
#endif

int main()
{
    return 0;
}
