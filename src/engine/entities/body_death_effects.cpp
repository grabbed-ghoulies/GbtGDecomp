#include "body_death_effects.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00068770
void initialize_runtime_body_death_effect(std::span<u8, 0x55C> parameters,
    const Actor& actor, u32 actor_address, float first, float lifetime,
    const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name);
    const u16 size = 0x55C;
    const u16 type = 0x42;
    const u16 flags = 0x12;
    std::memcpy(parameters.data(), &size, 2);
    std::memcpy(parameters.data() + 2, &type, 2);
    std::memcpy(parameters.data() + 4, &flags, 2);
    const Vec3 position = actor_world_position(actor);
    const Vec3 euler = actor_world_euler(actor);
    std::memcpy(parameters.data() + 8, &position, 12);
    std::memcpy(parameters.data() + 0x14, &euler, 12);
    std::memcpy(parameters.data() + 0x4BC, &first, 4);
    std::memcpy(parameters.data() + 0x334, &actor_address, 4);
    std::memcpy(parameters.data() + 0x4C0, &lifetime, 4);
    const u32 zero = 0;
    parameters[0x3B8] = 0;
    std::memcpy(parameters.data() + 0x558, &zero, 4);
    parameters[0x438] = 0;
    std::memcpy(parameters.data() + 0x4B8, &zero, 4);
    std::memcpy(parameters.data() + 0x544, &zero, 4);
    parameters[0x4C4] = 0;
    const char* name = "aid_fxemitter_ghoulies_weapon_bits_explodeall";
    char* destination = reinterpret_cast<char*>(parameters.data() + 0x338);
    char character;
    do
    {
        character = *name++;
        *destination++ = character;
    } while (character != '\0');
}
}
