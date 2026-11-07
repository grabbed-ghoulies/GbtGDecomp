#if !defined(GBTG_RETAIL_GAME_ONLY)
// The named retail structs as field lists (object_schemas.h). Host diagnostics only: nothing here is retail.
#include "object_schemas.h"

#include "gbtg/plugin_api.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <type_traits>

#define GBTG_OBJECT_SCHEMA_INCLUDES
#include "object_schemas_generated.inc"
#undef GBTG_OBJECT_SCHEMA_INCLUDES

namespace gbtg
{
namespace
{
template <typename T>
const ObjectSchema* schema_of()
{
    return nullptr;
}

// Pass 1: declare every struct's schema, so a member of a struct listed later still finds it.
#define GBTG_SCHEMA_BEGIN(Struct) \
    template <> \
    const ObjectSchema* schema_of<Struct>();
#define GBTG_SCHEMA_FIELD(Struct, member, note)
#define GBTG_SCHEMA_END(Struct)
#include "object_schemas_generated.inc"
#undef GBTG_SCHEMA_BEGIN
#undef GBTG_SCHEMA_FIELD
#undef GBTG_SCHEMA_END

template <typename T>
struct Element
{
    using type = T;
    static constexpr u32 count = 1;
};

template <typename T, size_t N>
struct Element<std::array<T, N>>
{
    using type = typename Element<T>::type;
    static constexpr u32 count = u32(N) * Element<T>::count;
};

template <typename T, size_t N>
struct Element<T[N]>
{
    using type = typename Element<T>::type;
    static constexpr u32 count = u32(N) * Element<T>::count;
};

template <typename T>
constexpr u32 scalar_kind()
{
    if constexpr (std::is_enum_v<T>)
    {
        return scalar_kind<std::underlying_type_t<T>>();
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return GBTG_FIELD_BOOL;
    }
    else if constexpr (std::is_same_v<T, char>)
    {
        return GBTG_FIELD_TEXT;
    }
    else if constexpr (std::is_same_v<T, char16_t> || (std::is_same_v<T, wchar_t> && sizeof(wchar_t) == 2))
    {
        return GBTG_FIELD_WTEXT;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        return sizeof(T) == 4 ? GBTG_FIELD_F32 : GBTG_FIELD_F64;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        constexpr bool is_signed = std::is_signed_v<T>;
        switch (sizeof(T))
        {
        case 1:
            return is_signed ? GBTG_FIELD_I8 : GBTG_FIELD_U8;
        case 2:
            return is_signed ? GBTG_FIELD_I16 : GBTG_FIELD_U16;
        case 4:
            return is_signed ? GBTG_FIELD_I32 : GBTG_FIELD_U32;
        default:
            return is_signed ? GBTG_FIELD_I64 : GBTG_FIELD_U64;
        }
    }
    else if constexpr (std::is_pointer_v<T>)
    {
        return GBTG_FIELD_POINTER;
    }
    else if constexpr (std::is_same_v<T, Vec3>)
    {
        return GBTG_FIELD_VEC3;
    }
    else if constexpr (std::is_same_v<T, Quat>)
    {
        return GBTG_FIELD_QUAT;
    }
    else
    {
        return GBTG_FIELD_BYTES;
    }
}

template <typename Member>
ObjectSchemaField make_field(const char* name, const char* note, size_t offset)
{
    using Value = std::remove_cv_t<Member>;
    using E = std::remove_cv_t<typename Element<Value>::type>;
    ObjectSchemaField field{name, note, u32(offset), u32(sizeof(Value)), scalar_kind<E>(), Element<Value>::count,
        nullptr};
    if constexpr (std::is_class_v<E> && !std::is_same_v<E, Vec3> && !std::is_same_v<E, Quat>)
    {
        if (const ObjectSchema* nested = schema_of<E>())
        {
            field.kind = GBTG_FIELD_GROUP;
            field.nested = nested;
        }
    }
    if (field.kind == GBTG_FIELD_BYTES)
    {
        // Bytes of an undescribed type are one run, not count elements.
        field.count = field.size;
    }
    return field;
}
} // namespace

// Pass 2: each struct's fields. offsetof on these structs is fine: they are standard layout (public members only, no
// bases), which the static_assert below checks.
#define GBTG_SCHEMA_BEGIN(Struct) \
    template <> \
    const ObjectSchema* schema_of<Struct>() \
    { \
        static_assert(std::is_standard_layout_v<Struct>, #Struct " is not standard layout"); \
        using Self = Struct; \
        static const ObjectSchemaField fields[] = {
#define GBTG_SCHEMA_FIELD(Struct, member, note) \
            make_field<decltype(Self::member)>(#member, note, offsetof(Self, member)),
#define GBTG_SCHEMA_END(Struct) \
        }; \
        static const ObjectSchema schema{#Struct, u32(sizeof(Struct)), fields}; \
        return &schema; \
    }
namespace
{
#include "object_schemas_generated.inc"
}
#undef GBTG_SCHEMA_BEGIN
#undef GBTG_SCHEMA_FIELD
#undef GBTG_SCHEMA_END

namespace
{
// Pass 3: the list, in the generated (name) order.
#define GBTG_SCHEMA_BEGIN(Struct) schema_of<Struct>,
#define GBTG_SCHEMA_FIELD(Struct, member, note)
#define GBTG_SCHEMA_END(Struct)
constexpr const ObjectSchema* (*schema_getters[])() = {
#include "object_schemas_generated.inc"
};
#undef GBTG_SCHEMA_BEGIN
#undef GBTG_SCHEMA_FIELD
#undef GBTG_SCHEMA_END

// The factory types whose own objects a named struct lays out, from the type comments beside each struct (getter
// and vtable addresses) and the tables in docs/RETAIL_LAYOUTS.md. Types not listed are shown as a parent's.
struct TypeSchema
{
    u32 type;
    const ObjectSchema* (*schema)();
};
const TypeSchema type_schemas[] = {
    {0x02, schema_of<ActorBase>},
    {0x03, schema_of<SceneControlBase>},
    {0x05, schema_of<PlayerRecord>},
    {0x08, schema_of<StrategyBase>},
    {0x09, schema_of<BodyStateBase>},
    {0x0B, schema_of<WeaponBase>},
    {0x0C, schema_of<FxCustomBase>},
    {0x0D, schema_of<AnimEmitterState>},
    {0x0F, schema_of<AvatarMotionBlurState>},
    {0x10, schema_of<AvatarAfterimageState>},
    {0x12, schema_of<BoneDebrisState>},
    {0x13, schema_of<AnimatedLightState>},
    {0x14, schema_of<AnimDeleteState>},
    {0x18, schema_of<GhoulyKeyState>},
    {0x1B, schema_of<SpiderWebState>},
    {0x1C, schema_of<ScareActorState>},
    {0x1D, schema_of<ScareFigureState>},
    {0x1E, schema_of<BgBreakKeyState>},
    {0x20, schema_of<SssDoorState>},
    {0x21, schema_of<SssDropperState>},
    {0x22, schema_of<SuperScaryShockState>},
    {0x23, schema_of<TriggerState>},
    {0x24, schema_of<GhoulyBgBreakState>},
    {0x25, schema_of<GhoulyCutsceneState>},
    {0x26, schema_of<GhoulySwapState>},
    {0x27, schema_of<ParentPlayerBodyRecord>},
    {0x28, schema_of<CameraBodyRecord>},
    {0x2A, schema_of<PlayerBodyRecord>},
    {0x2E, schema_of<FxElectricState>},
    {0x30, schema_of<FxFadeState>},
    {0x31, schema_of<FxOutlineState>},
    {0x32, schema_of<FxSolidFadeInState>},
    {0x33, schema_of<FxFadeState>},
    {0x36, schema_of<CameraMindRecord>},
    {0x37, schema_of<BaronStrategyState>},
    {0x38, schema_of<BaronPlaneStrategyState>},
    {0x39, schema_of<DirectorStrategyState>},
    {0x3A, schema_of<ChickenStrategyState>},
    {0x3B, schema_of<ButlerStrategyState>},
    {0x3C, schema_of<GenericStrategyState>},
    {0x3D, schema_of<GhoulieGirlStrategyState>},
    {0x3E, schema_of<GiantHandStrategyState>},
    {0x3F, schema_of<GrimReaperStrategyState>},
    {0x40, schema_of<HauntedChairStrategyState>},
    {0x41, schema_of<HauntedCoatStrategyState>},
    {0x42, schema_of<HauntedDoorStrategyState>},
    {0x43, schema_of<HauntedPictureStrategyState>},
    {0x44, schema_of<HauntedTvStrategyState>},
    {0x45, schema_of<HunchbackStrategyState>},
    {0x46, schema_of<ImpFireStrategyState>},
    {0x47, schema_of<ImpFlamethrowerStrategyState>},
    {0x48, schema_of<ImpFlyingStrategyState>},
    {0x49, schema_of<ImpNinjaStrategyState>},
    {0x4A, schema_of<JessieClydeStrategyState>},
    {0x4B, schema_of<MedusaStrategyState>},
    {0x4C, schema_of<MummyStrategyState>},
    {0x4D, schema_of<MummyCursedStrategyState>},
    {0x4E, schema_of<PirateStrategyState>},
    {0x4F, schema_of<PlayerStrategyState>},
    {0x51, schema_of<SkeletonStrategyState>},
    {0x52, schema_of<SpiderStrategyState>},
    {0x53, schema_of<UglyGirlStrategyState>},
    {0x54, schema_of<VampireStrategyState>},
    {0x55, schema_of<VampireChickenStrategyState>},
    {0x56, schema_of<WarlockStrategyState>},
    {0x57, schema_of<WormStrategyState>},
    {0x58, schema_of<ZombieStrategyState>},
    {0x5E, schema_of<AnimState>},
    {0x5F, schema_of<ArcAttackState>},
    {0x60, schema_of<BandagedState>},
    {0x61, schema_of<BloodSuckState>},
    {0x62, schema_of<DieState>},
    {0x63, schema_of<DeathState>},
    {0x64, schema_of<DizzyState>},
    {0x65, schema_of<DroneAppearState>},
    {0x66, schema_of<ArcState>},
    {0x67, schema_of<DroneDanceState>},
    {0x68, schema_of<ExplodeState>},
    {0x69, schema_of<PlayerDeathState>},
    {0x6A, schema_of<FlamethrowerState>},
    {0x6B, schema_of<FlipState>},
    {0x6D, schema_of<FlyState>},
    {0x6E, schema_of<FlyAlertState>},
    {0x6F, schema_of<GenerateGhoulyState>},
    {0x70, schema_of<GrabbedState>},
    {0x71, schema_of<BodyStateBase>},
    {0x72, schema_of<GrabThrowState>},
    {0x73, schema_of<HitState>},
    {0x74, schema_of<HitFurnaceState>},
    {0x75, schema_of<FurnaceHitState>},
    {0x76, schema_of<HitStreamState>},
    {0x77, schema_of<HoverState>},
    {0x78, schema_of<SwingState>},
    {0x79, schema_of<JumpOnState>},
    {0x7A, schema_of<KnockdownState>},
    {0x7B, schema_of<LookForWeaponState>},
    {0x7C, schema_of<PickupState>},
    {0x7D, schema_of<LiftState>},
    {0x7E, schema_of<ScoopWhackState>},
    {0x7F, schema_of<ScrabbleState>},
    {0x80, schema_of<ShockedState>},
    {0x81, schema_of<ShootState>},
    {0x82, schema_of<SlamSmashState>},
    {0x83, schema_of<SlideState>},
    {0x84, schema_of<EndureState>},
    {0x85, schema_of<SpawnGhoulyState>},
    {0x86, schema_of<SpinDizzyState>},
    {0x87, schema_of<StandAlertState>},
    {0x88, schema_of<StruggleState>},
    {0x89, schema_of<StunnedState>},
    {0x8A, schema_of<HoldState>},
    {0x8B, schema_of<ThrowState>},
    {0x8C, schema_of<TransformState>},
    {0x8D, schema_of<WalkState>},
    {0x8F, schema_of<WalkTargetState>},
    {0x90, schema_of<YoyoSpinState>},
    {0x95, schema_of<Chapter2Scene4ControlState>},
    {0x97, schema_of<SceneFxControlState>},
    {0x99, schema_of<FinalScoreControlState>},
    {0xA0, schema_of<BobWeaponState>},
    {0xA1, schema_of<CandleWeaponState>},
    {0xA2, schema_of<ContainerWeaponState>},
    {0xA3, schema_of<ExtinguisherWeaponState>},
    {0xA4, schema_of<GunWeaponState>},
    {0xA5, schema_of<SingleShotWeaponState>},
    {0xA6, schema_of<WaterPistolWeaponState>},
    {0xA7, schema_of<ThrownProjectileState>},
    {0xA8, schema_of<HomingProjectileState>},
    {0xA9, schema_of<LargeProjectileState>},
    {0xAA, schema_of<ChairWeaponState>},
};
} // namespace

std::span<const ObjectSchema* const> object_schemas()
{
    static const auto list = []
    {
        std::array<const ObjectSchema*, std::size(schema_getters)> result{};
        for (size_t i = 0; i < result.size(); ++i)
        {
            result[i] = schema_getters[i]();
        }
        return result;
    }();
    return list;
}

const ObjectSchema* find_object_schema(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    const auto list = object_schemas();
    const auto at = std::lower_bound(list.begin(), list.end(), name, [](const ObjectSchema* schema, const char* key)
    {
        return std::strcmp(schema->name, key) < 0;
    });
    return at != list.end() && std::strcmp((*at)->name, name) == 0 ? *at : nullptr;
}

const ObjectSchema* object_schema_for_type(u32 type)
{
    for (const TypeSchema& entry : type_schemas)
    {
        if (entry.type == type)
        {
            return entry.schema();
        }
    }
    return nullptr;
}
} // namespace gbtg
#endif
