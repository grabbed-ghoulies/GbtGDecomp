#pragma once
#include "spider_strategy.h"
#include "actor_attachment_events.h"
#include "body_attachment_cleanup.h"

namespace gbtg
{
struct RuntimeActorResourceReference;

// entityStrategyMummyCursed (factory type 0x4D, getter 0x000BDA20, vtable 0x00505F68; the factory table's row 0x4C
// carries the name) over the base strategy (type 8): the cursed mummy. Holding (0x401) it wanders ("randomwalk",
// "randomworriedwalk" once cursed) and evades ("evade") a target in reach; a target it may queue on is either cursed
// (0x3EF: "deathcurse", callout 0x42 and the effect named at +0xBC) or grabbed (0x3EE): a claimed rider it pulls in
// and slab-whacks ("slabwhackprepare", "slabwhack", "slabwhacksneaky") through the phases at +0x144. Its curse on the
// player (+0x188) shows the baddie meter (form 2 at +0x190, 0x0012E5E0) and, its body's bit 2 set, spawns the object
// named at +0x198 at model binding 4 (form 1).
// Strategy bytes: +0x1A attacking, +0x1C pause count, +0x20 target queue link.
namespace mummy_cursed_strategy_field
{
constexpr RetailField<u32, 0x38> parameters{}; // DAT_00428848, the block its tunables come from
constexpr RetailField<std::array<char, 0x80>, 0x3C> curse_effect_name{}; // Parameters +0x98, form 2's object
constexpr RetailField<std::array<char, 0x80>, 0xBC> deathcurse_name{}; // Parameters +0x18, 0x3EF's object (+0x2A0)
constexpr RetailField<u32, 0x13C> evading{}; // 0x401 wandering; 0x000BCB80 clears it
constexpr RetailField<u32, 0x140> victim{}; // 0x3EE's grabbed actor (mind +0x10 -> +0x10); message 0x7C forgets it
constexpr RetailField<u32, 0x144> phase{}; // 0x3EE's phase, 1..8 (0x000BD450)
constexpr RetailField<float, 0x148> phase_timer{}; // Phases 3 and 4: seconds, from the block's +0x18 / +0x20 ranges
constexpr RetailField<u32, 0x14C> unk_14C{}; // Message 0x6E from its mind target; phase 1 clears it
constexpr RetailField<u32, 0x150> pulling{}; // Phase 4: the victim is pulled in (0x000BD750)
constexpr RetailField<u32, 0x154> whacked{}; // Phase 5 sent the victim message 0x5B
constexpr RetailField<std::array<u8, 0x24>, 0x158> grab_events{}; // Callout 0x46's tokens (0x0013B7E0)
constexpr RetailField<u32, 0x17C> grab_callout{}; // Callout 0x46 requested
constexpr RetailField<u32, 0x180> riding{}; // The rider claim on +0x184 (0x00138140)
constexpr RetailField<u32, 0x184> mount{}; // The actor it claimed; its body +0x718 holds this mummy
constexpr RetailField<u32, 0x188> cursed{}; // The player the curse is on (0x3EF)
constexpr RetailField<u32, 0x18C> curse_landed{}; // Message 0x5F
constexpr RetailField<u32, 0x190> curse_form{}; // 0x000BC910: 1 the bound spawn, 2 the meter; 0 ended (0x000BC770)
constexpr RetailField<u32, 0x194> curse_step{}; // 0x3EF: 1 facing, 2 cursing, 3 done (0x000BD8B0)
constexpr RetailField<std::array<char, 0x80>, 0x198> spawn_name{}; // Parameters +0x198, form 1's object
constexpr RetailField<std::array<char, 0x80>, 0x218> burst_name{}; // Parameters +0x118, form 1's burst (0x0013B1F0)
constexpr RetailField<u32, 0x298> spawn{}; // Form 1's object
constexpr RetailField<u32, 0x29C> curse_effect{}; // Form 2's object
constexpr RetailField<u32, 0x2A0> deathcurse{}; // 0x3EF's object
constexpr RetailField<std::array<u8, 0x24>, 0x2A4> curse_events{}; // Callout 0x42's tokens
constexpr RetailField<std::array<u8, 0x24>, 0x2C8> spawn_events{}; // Callout 0x43's tokens
constexpr u32 kSize = 0x2EC;
static_assert(retail_fits(kSize, spawn_events));
} // namespace mummy_cursed_strategy_field
struct RuntimeMummyCursedStrategy
{
    // The instance's spider views over the same strategy: base goals, target checks, queue and actor views.
    RuntimeSpiderStrategy& shared;
    // DAT_00428848: +0 the reach it evades in, +8 / +0xC the grab's near and far reaches, +0x10 / +0x14 the curse's,
    // +0x18 / +0x1C and +0x20 / +0x24 phases 3 and 4's seconds, +0x28 the pull, +0x2C the reach that ends the pull,
    // +0x30 the reach that turns phase 6 sneaky, +0x34 the reach 0x000BD680 holds in, +0x38 the meter's value, +0x3C
    // set skips form 1's spawn.
    std::span<const u8> parameters;
    u32 parameters_address; // 0x00428848, kept at +0x38.
    std::function<u32(const char*, u32)> create_effect; // 0x00107720 / 0x00107520 with the actor at +8.
    // 0x00107720 / 0x00107520 with the clone patched first (form 1 writes its position and euler at +8 / +0x14).
    std::function<u32(const char*, const std::function<void(std::span<u8>)>&)> create_patched;
    std::function<void(u32, std::span<const u32>)> object_message; // An object's vtable +0xC.
    std::function<std::span<u8>(u32)> actor_bytes_at; // Writable actor bytes by retail address (+0xE50).
    std::function<EntityBitBlock&(u32)> body_bits_at; // An actor's body (+0x95C) +0x15C bits, by actor address.
    std::function<std::span<u8>(u32)> body_at; // An actor's body (+0x95C) bytes, by actor address.
    std::function<void(u32)> activate; // 0x00112CC0, ESI the actor.
    std::function<void(u32, float, const char*)> burst; // 0x0013B1F0 on the actor at an address, ESI name.
    std::function<void(float)> scare_near; // 0x0013B660 around this actor (ESI), with the radius.
    std::function<void(u32, u32, float)> start_meter; // 0x0012E5E0, EAX actor, ECX target.
    std::function<void()> stop_meter; // 0x0012E6C0.
    std::function<bool()> meter_shown; // DAT_00545A18.
    std::function<u32()> meter_actor; // DAT_00545A20.
    std::function<void(u32, u8)> model_nodes; // 0x00040D50: 0x000F96F0 on the +0x204 and +0x310 models.
    const RuntimeActorResourceReference& resource; // Actor +0x8C, the callouts' record.
    ActorAttachmentEventContext& callouts;
    RuntimeBodyAttachmentLists& lists;
    Game& game; // 0x00024A00's notices.
    const u32& runtime_flags; // DAT_0050EA9C.
    const char* goal_random_walk; // "randomwalk", 0x003216C4.
    const char* goal_random_worried_walk; // "randomworriedwalk", 0x003214C0.
    const char* goal_evade; // "evade", 0x003217D8.
    const char* goal_evade_knockdown; // "evadeknockdown", 0x003214E4.
    const char* goal_come_into_range_knockdown; // "comeintorangeknockdown", 0x0032150C.
    const char* goal_evade_special; // "evadespecial", 0x003214D4.
    const char* goal_come_into_range_special; // "comeintorangespecial", 0x003214F4.
    const char* goal_slab_whack_prepare; // "slabwhackprepare", 0x0031CDB4.
    const char* goal_slab_whack; // "slabwhack", 0x0031CDC8.
    const char* goal_slab_whack_sneaky; // "slabwhacksneaky", 0x0032149C.
    const char* goal_scared; // "scared", 0x00321234.
    const char* goal_death_curse; // "deathcurse", 0x0031CDE4.
};
void mummy_cursed_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000BDA00
bool initialize_runtime_mummy_cursed_strategy(std::span<const u8> parameters,
    RuntimeMummyCursedStrategy& context); // Xbox 0x000BDA70
void destroy_runtime_mummy_cursed_strategy(RuntimeMummyCursedStrategy& context); // Xbox 0x000BDB30
void update_runtime_mummy_cursed_strategy(RuntimeMummyCursedStrategy& context); // Xbox 0x000BDB80, vtable +0x10
void dispatch_runtime_mummy_cursed_message(std::span<const u32> message,
    RuntimeMummyCursedStrategy& context); // Xbox 0x000BDBE0, vtable +0xC
void enter_runtime_mummy_cursed_state(u16 state, RuntimeMummyCursedStrategy& context); // Xbox 0x000BDF90, vtable +0x14
void leave_runtime_mummy_cursed_state(u16 state, RuntimeMummyCursedStrategy& context); // Xbox 0x000BE010, vtable +0x18
// Each token of the block (+0 count, +4 tokens) names an actor +0x1B0 attachment whose timed effect (DAT_00506930)
// takes the distance squared at +0xBC, or -1 for a distance of -1.
void set_runtime_attachment_effect_reach(std::span<const u8> events, float distance,
    RuntimeBodyAttachmentLists& lists); // Xbox 0x0013B840, EDI block, EBX actor, XMM0 distance
}
