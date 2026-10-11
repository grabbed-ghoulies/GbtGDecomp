#include "entities/trigger_actor.h"
#include "entities/cinema_mind_lifecycle.h"
#include "entities/cinema_mind_requests.h"
#include "entities/cinema_mind_control.h"
#include "entities/body_orientation.h"
#include "entities/mind_target_policy.h"
#include "navigation/mind_escape_position.h"
#include "navigation/mind_navigation_angles.h"
#include "entities/body_damage_motion.h"
#include "entities/cinema_mind_request_update.h"
#include <cstring>
#include <iostream>

using namespace gbtg;

int main()
{
    u32 count;
    std::cin >> count;
    for (u32 index = 0; index < count; ++index)
    {
        u32 mode, random, slot_word;
        std::cin >> mode >> random >> slot_word;
        std::array<u8, 0x1000> parameters;
        FactoryObject object;
        object.size = 0xC6C;
        object.bytes = std::make_unique<u8[]>(object.size);
        for (u8& byte : parameters)
        {
            u32 value;
            std::cin >> value;
            byte = u8(value);
        }
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            u32 value;
            std::cin >> value;
            object.bytes[offset] = u8(value);
        }
        i32 slot = i32(slot_word);
        bool result;
        if (mode == 0)
        {
            result = initialize_runtime_area_actor(object, parameters);
        }
        else if (mode == 1)
        {
            Actor actor;
            GhoulyController controller;
            GhoulySpawnRecord* load = nullptr;
            const i32 blocked = -1;
            RuntimeTriggerActorContext context{{object.bytes.get(), object.size}, actor,
                controller, random, 0.f, {}, load, blocked, {}};
            result = initialize_runtime_trigger_actor(context, parameters);
        }
        else if (mode == 2)
        {
            result = find_runtime_trigger_spawn_slot({object.bytes.get(), object.size}, slot);
        }
        else if (mode == 3)
        {
            std::span<const u8> resource;
            AnimationResourceRegistry registry;
            ResourceErrorState error{};
            RuntimeCinemaMindLifecycle context{{object.bytes.get(), object.size}, parameters, resource,
                registry, error, random, [](const void*) -> u32
                {
                    throw Error("cinema-mind fixture unexpectedly acquired a resource");
                }};
            result = initialize_runtime_cinema_mind(context, parameters);
        }
        else if (mode == 4)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler = {read_f32(parameters, 0x14), read_f32(parameters, 0x18), read_f32(parameters, 0x1C)};
            const auto target = read_u32(parameters, 0) == 0 ? std::span<const u8>{} :
                std::span<const u8>{parameters}.subspan(0x400);
            result = initialize_runtime_cinema_mind_request(actor, target,
                {object.bytes.get(), object.size}, random) != 0;
        }
        else if (mode == 5)
        {
            refresh_runtime_cinema_mind_action_permission({object.bytes.get(), object.size}, parameters,
                i32(read_u32(parameters, 0)), std::span<const u8>{parameters}.subspan(0x400));
            result = true;
        }
        else if (mode == 6)
        {
            Actor actor;
            actor.animation_rate = read_f32(parameters, 0x92C);
            actor.animation_pause = read_f32(parameters, 0x930);
            const float freeze = read_f32(parameters, 8);
            const float dt = read_f32(parameters, 0xC);
            const float scale = read_f32(parameters, 0x10);
            const u32 zero = 0;
            RuntimeActorClock clock{freeze, dt, scale, zero, {}, zero};
            RuntimeActorSoundQueues sounds{};
            update_runtime_cinema_mind_sound({object.bytes.get(), object.size},
                std::span<const u8>{parameters}.subspan(0x100),
                std::span<const u8>{parameters}.subspan(0x400), actor, clock, random, sounds);
            result = true;
        }
        else if (mode == 7 || mode == 8)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            if (mode == 7)
            {
                const Vec3 point{read_f32(parameters, 0x420), read_f32(parameters, 0x424), read_f32(parameters, 0x428)};
                face_runtime_body_point({object.bytes.get(), object.size}, actor, point);
                result = true;
            }
            else
            {
                result = face_runtime_cinema_mind_target(std::span<const u8>{parameters}.subspan(0x600),
                    {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400)) != 0;
            }
        }
        else if (mode == 9)
        {
            Actor actor, target_actor;
            RuntimeScriptMindQuery query{};
            RuntimeMindTarget target{nullptr, std::span<const u8>{parameters}.subspan(0x600),
                0x10002600, target_actor, parameters, query, nullptr};
            result = runtime_mind_target_is_eligible({object.bytes.get(), object.size}, parameters,
                actor, 0x10002000, target, read_u32(parameters, 0) != 0);
        }
        else if (mode == 10)
        {
            std::array<RuntimeInputAxisWriteDevice, 4> children;
            for (size_t index = 0; index < children.size(); ++index)
            {
                children[index].record = {object.bytes.get() + 0x100 + index * 0x80, 0x80};
            }
            RuntimeInputAxisWriteDevice device{{object.bytes.get(), 0x80},
                {&children[0], &children[1], &children[2], &children[3]}};
            set_runtime_input_vertical_axes(device,
                {read_f32(parameters, 0), read_f32(parameters, 4)}, {object.bytes.get() + 0x800, 0x80});
            result = true;
        }
        else if (mode == 15)
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            actor.euler.y = read_f32(parameters, 0x18);
            result = update_runtime_cinema_mind_facing_request(std::span<const u8>{parameters}.subspan(0x600),
                {object.bytes.get(), object.size}, actor, std::span<const u8>{parameters}.subspan(0x400),
                std::span<const u8>{parameters}.subspan(0x800)) != 0;
        }
        else if (mode == 14)
        {
            std::array<float, 2> direction{read_f32(parameters, 0), read_f32(parameters, 4)};
            scale_runtime_planar_direction(direction, read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, direction.data(), 8);
            result = true;
        }
        else if (mode == 12)
        {
            const float angle = interpolate_runtime_mind_angle(read_f32(parameters, 0),
                read_f32(parameters, 4), read_f32(parameters, 8));
            std::memcpy(object.bytes.get() + 0x70, &angle, 4);
            result = true;
        }
        else if (mode == 13)
        {
            const Vec3 source{read_f32(parameters, 0), read_f32(parameters, 4), read_f32(parameters, 8)};
            Vec3 output;
            offset_runtime_horizontal_point(output, source, read_f32(parameters, 0xC), read_f32(parameters, 0x10));
            std::memcpy(object.bytes.get() + 0x70, &output, sizeof(output));
            result = true;
        }
        else
        {
            Actor actor;
            actor.position = {read_f32(parameters, 8), read_f32(parameters, 0xC), read_f32(parameters, 0x10)};
            RuntimeMindEscapePosition context{actor, parameters, std::span<const u8>{parameters}.subspan(0x400),
                parameters.data() + 0x800, nullptr, random, 0.f};
            Vec3 point;
            result = select_runtime_mind_escape_position(context, read_f32(parameters, 0x3C0),
                read_f32(parameters, 0x3C4), read_f32(parameters, 0x3C8), point);
            if (result)
            {
                std::memcpy(object.bytes.get() + 0x70, &point, sizeof(point));
            }
        }
        std::cout << result << ' ' << random << ' ' << u32(slot) << ' ';
        for (u32 offset = 0; offset < object.size; ++offset)
        {
            std::cout << u32(object.bytes[offset]) << ' ';
        }
        std::cout << '\n';
    }
}
