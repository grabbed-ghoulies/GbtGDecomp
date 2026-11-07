// Host side of update_oracle.py. The heap boundary is malloc/free with the oracle's 0xCD fill.
#include "entities/body_collision.h"
#include "entities/entity.h"
#include "entities/entity_collision.h"
#include "entities/player_update.h"
#include "navigation/floor_tracking.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace gbtg;
using namespace gbtg::entity_detail;
namespace gbtg::entity_detail
{
extern i32 g_segment_count; // DAT_00511A18, defined in entity_collision.cpp.
}

namespace
{
int g_freed = 0;
std::vector<std::string> g_calls;
}

namespace gbtg
{
void* engine_heap_allocate(i32 size)
{
    void* block = std::malloc(size_t(size));
    std::memset(block, 0xCD, size_t(size));
    return block;
}
void* engine_heap_reallocate(void* block, i32 size)
{
    return std::realloc(block, size_t(size));
}
void engine_heap_free(void* block)
{
    ++g_freed;
}
void* g_occupancy_pool = nullptr; // DAT_00512344.
i32 g_occupancy_mask = 0; // DAT_00512348.
// Bound-actor and resource paths are unreachable here: the oracle never supplies a bind.
struct ModelPoseBuffer;
struct Joint;
struct ModelBoneBinding;
struct ResourceErrorState;
const std::array<float, 12>& model_joint_world_matrix(const ModelPoseBuffer&, int, const Joint&,
    const std::array<float, 12>&)
{
    std::abort();
}
void model_bone_world_matrix(const ModelBoneBinding&, const ModelPoseBuffer*, const std::array<float, 12>&,
    std::array<float, 12>&)
{
    std::abort();
}
Actor* actor_bound_parent(Actor&)
{
    std::abort();
}
void resource_error(ResourceErrorState&, int)
{
    std::abort();
}
// player_update.cpp also holds the 0x0002C2B0 orchestrator; these callees stay outside the oracle.
struct RuntimeModelShaderAnimations;
struct RuntimeShaderMatrixBoundaries;
struct ActorEffectStreamContext;
void tick_runtime_model_shader_animations(RuntimeModelShaderAnimations&, float, const RuntimeShaderMatrixBoundaries&)
{
    std::abort();
}
void update_actor_attachment_effects(Actor&, ActorAttachmentList&, ActorAttachmentNode&, ActorEffectStreamContext&)
{
    std::abort();
}
void reset_runtime_analog_input(std::span<u8>)
{
    std::abort();
}
void reset_runtime_input_device(std::span<u8>, const RuntimeInputReleaseContext&)
{
    std::abort();
}
void refresh_runtime_input_device(u32, u32, RuntimeInputActivationContext&, const RuntimeInputReleaseContext&)
{
    std::abort();
}
const u8* find_runtime_script_table_row(const RuntimeScriptLookupTable*, u16, u32&)
{
    std::abort();
}
bool select_runtime_body_action_row(std::span<u8>, const u8*, const RuntimeBodyActionTemplates*,
    RuntimeBodyActionSelection&)
{
    std::abort();
}
void update_runtime_body_contacts(RuntimeBodyContactWalk&)
{
    std::abort();
}
void register_actor_shape_probe(const Actor&, entity_detail::ShapeProbe&)
{
    std::abort();
}
}

namespace
{
u32 next()
{
    u32 value;
    std::cin >> value;
    return value;
}
float next_float()
{
    const u32 bits = next();
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}
u32 bits_of(float value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}
void put_float(std::vector<u8>& bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void put_u16(std::vector<u8>& bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}
void put_u32(std::vector<u8>& bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

struct Clock
{
    float freeze = 0.f;
    float dt = 0.f;
    float scale = 0.f;
    u32 block = 0;
    u32 index = 0;
    std::array<u8, 4> enabled{};
    RuntimeActorClock view{freeze, dt, scale, block, enabled, index};
};
void read_clock(Clock& clock)
{
    clock.freeze = next_float();
    clock.dt = next_float();
    clock.scale = next_float();
    clock.block = next();
    clock.index = next();
    for (u8& value : clock.enabled)
    {
        value = u8(next());
    }
}

struct Player
{
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();
    std::vector<u8> bytes = std::vector<u8>(0xD00);
    std::vector<u32> messages;
    RuntimeBodyAttachedActor self{bytes, 0, [this](std::span<const u32> message)
        {
            messages.push_back(message[0]);
        }};
};
void read_actor(Player& player)
{
    player.self.actor = player.actor.get();
    player.actor->position = Vec3{next_float(), next_float(), next_float()};
    player.actor->animation_pause = next_float();
    player.actor->animation_rate = next_float();
}
std::string messages_text(const Player& player)
{
    std::string text;
    char buffer[16];
    for (size_t index = 0; index < player.messages.size(); ++index)
    {
        std::snprintf(buffer, sizeof(buffer), index == 0 ? "%x" : " %x", player.messages[index]);
        text += buffer;
    }
    return text;
}

struct Grids
{
    std::array<NavGrid, 4> grids;
};
void read_grids(Grids& storage)
{
    g_nav_origin_x = i32(next());
    g_nav_origin_z = i32(next());
    g_nav_count = i32(next());
    for (i32 band = 0; band < 2; ++band)
    {
        g_nav_min_y[band] = next_float();
        g_nav_max_y[band] = next_float();
    }
    for (i32 g = 0; g < 4; ++g)
    {
        g_nav_grids[g] = nullptr;
        if (next() != 0)
        {
            NavGrid& grid = storage.grids[g];
            grid.width = i32(next());
            grid.depth = i32(next());
            grid.cell = i32(next());
            grid.cells.assign(size_t(grid.width * grid.depth), NavCell{});
            for (NavCell& cell : grid.cells)
            {
                const u32 word = next();
                cell.value_6 = u8(word);
                cell.flags_7 = u8(word >> 8);
            }
            g_nav_grids[g] = &grid;
        }
    }
}
std::string grid_words()
{
    std::string text;
    char buffer[16];
    for (i32 g = 0; g < 4; ++g)
    {
        if (g_nav_grids[g] != nullptr)
        {
            for (const NavCell& cell : g_nav_grids[g]->cells)
            {
                std::snprintf(buffer, sizeof(buffer), " %u", u32(cell.value_6) | (u32(cell.flags_7) << 8));
                text += buffer;
            }
        }
    }
    return text;
}

void run_motion()
{
    Clock clock;
    read_clock(clock);
    Player owner;
    read_actor(owner);
    std::vector<u8> history(4 + 0x50 * 12);
    const u32 index = next();
    put_u32(history, 0, index);
    const Vec3 delta{next_float(), next_float(), next_float()};
    const i32 result = record_runtime_body_motion(history, delta, *owner.actor, clock.view);
    const size_t sample = 4 + size_t(index) * 12;
    std::printf("%d %u %08x %08x %08x\n", result, read_u32(history, 0), read_u32(history, sample),
        read_u32(history, sample + 4), read_u32(history, sample + 8));
}

void run_floor()
{
    Grids storage;
    read_grids(storage);
    const Vec3 point{next_float(), next_float(), next_float()};
    i32 floor = i32(next());
    const i32 preferred = i32(next());
    const i32 check = i32(next());
    NavigationCellRegistration* records = nullptr;
    std::vector<u8> rows;
    if (next() != 0)
    {
        const u32 count = next();
        rows.resize(0xC * std::max<u32>(count, 1));
        for (u32 row = 0; row < count; ++row)
        {
            put_u32(rows, 0xC * row, next());
            put_u32(rows, 0xC * row + 4, next());
        }
        records = new NavigationCellRegistration{rows.data(), rows.data() + 0xC * count};
    }
    NavigationPathSearch search_storage;
    NavigationPathSearch* search = nullptr;
    std::vector<u8> nodes(0x14 * 40);
    int scratch = 0;
    if (next() != 0)
    {
        scratch = int(next());
        u32 words[11];
        for (u32& word : words)
        {
            word = next();
        }
        search_storage.word_0 = words[0];
        search_storage.word_4 = words[1];
        search_storage.count = words[2];
        search_storage.nodes = nodes.data();
        search_storage.grid = nullptr;
        search_storage.scratch = scratch != 0 ? &search_storage : nullptr;
        search_storage.first = i32(words[4]);
        search_storage.last = i32(words[5]);
        search_storage.word_20 = words[6];
        search_storage.word_24 = words[7];
        search_storage.word_28 = words[8];
        search_storage.word_2c = words[9];
        search_storage.word_30 = words[10];
        search = &search_storage;
    }
    g_freed = 0;
    const u32 result = update_navigation_floor(point, floor, records, search, preferred, check);
    std::string text = std::to_string(result) + " " + std::to_string(floor) + (records == nullptr ? " null |" : " kept |");
    text += grid_words() + " |";
    if (search != nullptr)
    {
        i32 grid_index = -1;
        for (i32 g = 0; g < 4; ++g)
        {
            if (search->grid != nullptr && search->grid == g_nav_grids[g])
            {
                grid_index = g;
            }
        }
        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), " %u %d %u %d %d %u %u %u %u %u", search->count, grid_index,
            search->scratch != nullptr ? 1u : 0u, search->first, search->last, search->word_0, search->word_4,
            search->word_20, search->word_24, search->word_28);
        text += buffer;
        for (u32 index = 0; index < std::min<u32>(search->count, 40); ++index)
        {
            text += " " + std::to_string(read_u16(nodes, 0x14 * index + 0xA));
        }
        text += " freed " + std::to_string(g_freed);
    }
    std::printf("%s\n", text.c_str());
}

void run_timers()
{
    Clock clock;
    read_clock(clock);
    Player player;
    read_actor(player);
    put_float(player.bytes, 0x974, next_float());
    put_float(player.bytes, 0x978, next_float());
    tick_runtime_player_status_timers(player.self, clock.view);
    std::printf("%08x %08x | %s\n", read_u32(player.bytes, 0x974), read_u32(player.bytes, 0x978),
        messages_text(player).c_str());
}

void run_bounds()
{
    Grids storage;
    read_grids(storage);
    Player player;
    read_actor(player);
    std::vector<u8> rows(0x40);
    RuntimePlayerGoalRing ring_storage{8, 4, 0, 1, rows};
    const RuntimePlayerGoalRing* ring = nullptr;
    if (next() != 0)
    {
        ring_storage.head = u16(next());
        ring_storage.tail = next() != 0 ? 4 : 1;
        put_u16(rows, 8 * ring_storage.head, u16(next()));
        ring = &ring_storage;
    }
    u8 enabled = u8(next());
    float values[6];
    for (float& value : values)
    {
        value = next_float();
    }
    const RuntimeWorldBounds bounds{enabled, values[0], values[1], values[2], values[3], values[4], values[5]};
    put_u16(player.bytes, 0xCDE, u16(next()));
    put_u32(player.bytes, 0xCEC, next());
    std::array<u8, 8> bit_bytes{};
    bit_bytes[4] = u8(next());
    EntityBitBlock bits{bit_bytes.data()};
    const std::array<u8, 4> zero{};
    RuntimePlayerBoundsCheck context{player.self, bounds, &bits, ring, zero};
    check_runtime_player_bounds(context);
    std::printf("%s\n", messages_text(player).c_str());
}

void run_room()
{
    Player player;
    read_actor(player);
    const u32 present = next();
    g_occupancy_mask = i32(next());
    const u32 count = next();
    std::vector<OccupancyRecord> records(std::max<u32>(count, 1));
    for (u32 index = 0; index < count; ++index)
    {
        OccupancyRecord& record = records[index];
        record.mask = next();
        record.group = i16(next());
        record.center = Vec3{next_float(), next_float(), next_float()};
        record.extent_x = next_float();
    }
    OccupancyPool pool{0x38, 4, records.data(), records.data() + count, records.data() + count};
    g_occupancy_pool = present != 0 ? &pool : nullptr;
    put_u16(player.bytes, 6, u16(next()));
    const OccupancyRecord* allowed = next() != 0 ? records.data() : nullptr;
    const OccupancyRecord* current = nullptr;
    put_u32(player.bytes, 0xCB8, next());
    std::vector<u8> body(0x40);
    put_u16(body, 0x30, u16(next()));
    put_float(body, 0x34, next_float());
    const bool strategy = next() != 0;
    put_u16(player.bytes, 0xCDC, u16(next()));
    put_u32(player.bytes, 0x93C, next());
    RuntimePlayerRoomTracking context{player.self, body, allowed, current, strategy};
    track_runtime_player_room(context);
    const int index = current == nullptr ? -1 : int(current - records.data());
    std::printf("%d %u %08x %08x %08x | %s\n", index, read_u32(player.bytes, 0xCB8), read_u32(player.bytes, 0xCAC),
        read_u32(player.bytes, 0xCB0), read_u32(player.bytes, 0xCB4), messages_text(player).c_str());
}

struct HarnessProbe
{
    SegmentProbe probe;
    int id = 0;
    u32 mask = 0;
    float a = 0.f;
    float h = 0.f;
    float normal[3]{};
    u8 flag = 0;
    u8 clamp = 0;
};
i32 probe_call(SegmentProbe& self, Vec3& candidate, Vec3& kept, i32 kind, u32 mask, u8* record, int which)
{
    HarnessProbe& p = reinterpret_cast<HarnessProbe&>(self);
    const bool hit = (mask & p.mask) != 0 && kept.x * p.a + candidate.y < p.h;
    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "%d %d %08x %08x %08x %08x %08x %08x %d", p.id, which, bits_of(candidate.x),
        bits_of(candidate.y), bits_of(candidate.z), bits_of(kept.x), bits_of(kept.y), bits_of(kept.z), kind);
    g_calls.push_back(buffer);
    if (hit)
    {
        std::memcpy(record + 0xC, p.normal, 12);
        record[0x26] = p.flag;
        record[0x27] = u8(p.id);
        if (p.clamp != 0)
        {
            kept.y = p.h;
        }
    }
    return hit ? 1 : 0;
}
i32 probe_query(SegmentProbe& self, Vec3& candidate, Vec3& kept, u32, i32 kind, u32 mask, u8* record)
{
    return probe_call(self, candidate, kept, kind, mask, record, 1);
}
i32 probe_sweep(SegmentProbe& self, Vec3& candidate, Vec3& kept, i32 kind, u32 mask, u8* record)
{
    return probe_call(self, candidate, kept, kind, mask, record, 0);
}

void run_spheres()
{
    Clock clock;
    read_clock(clock);
    Player owner;
    read_actor(owner);
    Actor& actor = *owner.actor;
    actor.euler = Vec3{next_float(), next_float(), next_float()};
    actor.scale_x = next_float();
    actor.radius_y = next_float();
    actor.radius_xz = next_float();
    actor.scale_200 = next_float();
    EntityMaskHost mask;
    mask.flag = i32(next());
    mask.mode = i16(next());
    std::array<u8, 8> bit_bytes{};
    bit_bytes[5] = u8(next());
    EntityBitBlock bits{bit_bytes.data()};
    std::vector<u8> body(0x800);
    put_float(body, 0x18, next_float());
    put_float(body, 0x1C, next_float());
    put_float(body, 0x20, next_float());
    const u32 count = next();
    body[0x714] = u8(count);
    body[0x715] = u8(next());
    body[0x716] = 0x77;
    body[0x717] = u8(next());
    const bool present = next() != 0;
    std::vector<RuntimeBodyCollisionSphere> spheres(std::max<u32>(count, 1));
    std::vector<EntityShapeNode> nodes(spheres.size());
    for (u32 index = 0; present && index < count; ++index)
    {
        EntityShapeNode& node = nodes[index];
        node.bone = 0xFFFF;
        node.along_x = next_float();
        node.along_y = next_float();
        node.point.x = next_float();
        node.point.y = next_float();
        spheres[index].node = &node;
    }
    const u32 probes = next();
    std::vector<HarnessProbe> objects(probes);
    for (u32 index = 0; index < probes; ++index)
    {
        HarnessProbe& p = objects[index];
        p.id = int(index + 1);
        p.mask = next();
        p.a = next_float();
        p.h = next_float();
        for (float& n : p.normal)
        {
            n = next_float();
        }
        p.flag = u8(next());
        p.clamp = u8(next());
        p.probe.query = probe_query;
        p.probe.sweep = probe_sweep;
    }
    for (u32 index = 0; index < probes; ++index)
    {
        objects[index].probe.next = index + 1 < probes ? &objects[index + 1].probe : nullptr;
    }
    g_world.segment = probes != 0 ? &objects[0].probe : nullptr;
    for (SegmentSlot& slot : g_segment_slots)
    {
        slot = SegmentSlot{};
    }
    g_segment_count = 0;
    g_calls.clear();
    RuntimeBodyCollision context{body, actor, &bits, present ? spheres.data() : nullptr, mask, clock.view};
    push_runtime_body_spheres(context);
    std::string text;
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%08x %08x %08x %u |", read_u32(body, 0x18), read_u32(body, 0x1C),
        read_u32(body, 0x20), u32(body[0x716]));
    text += buffer;
    for (u32 index = 0; present && index < count; ++index)
    {
        text += ' ';
        for (u8 value : spheres[index].contact)
        {
            std::snprintf(buffer, sizeof(buffer), "%02x", value);
            text += buffer;
        }
    }
    text += " |";
    for (size_t index = 0; index < g_calls.size(); ++index)
    {
        text += (index == 0 ? " " : " ; ") + g_calls[index];
    }
    std::printf("%s\n", text.c_str());
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "motion")
        {
            run_motion();
        }
        else if (kind == "floor")
        {
            run_floor();
        }
        else if (kind == "timers")
        {
            run_timers();
        }
        else if (kind == "bounds")
        {
            run_bounds();
        }
        else if (kind == "room")
        {
            run_room();
        }
        else
        {
            run_spheres();
        }
        std::fflush(stdout);
    }
}
