#pragma once
#include "body_damage_processors.h"
#include "body_timer_requests.h"

namespace gbtg
{
struct RuntimeBodyDamageEntry
{
    Game& game;
    RuntimeBodyDamageProcessor& processor;
    const entity_detail::SegmentPool* contacts; // Damage +0x30.
    RuntimeBodyTimerCreation& timer;
};
void receive_runtime_body_damage(std::span<u8> body, std::span<u8> damage,
    std::span<u32, 4> result, RuntimeBodyDamageEntry& context); // Xbox 0x00020DC0
void dispatch_runtime_body_damage(std::span<u8> body, std::span<const u8> damage,
    std::span<u32, 4> result, u32 phase, u32& timer,
    RuntimeBodyDamageProcessor& context); // Xbox 0x00021220
void dispatch_runtime_body_classified_damage(std::span<u8> body,
    std::span<const u8> damage, std::span<u32, 4> result, u32 phase,
    u32& timer, RuntimeBodyDamageProcessor& context); // Xbox 0x00021450
}
