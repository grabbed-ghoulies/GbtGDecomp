// Host side of contact_oracle.py. The heap boundary is malloc/realloc/free with the oracle's 0xCD fill.
#include "entities/body_contacts.h"
#include "entities/entity.h"
#include "entities/entity_collision.h"
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
void engine_heap_free(void*)
{
}
// 0x000F71F0 lives with the runtime; the oracle copies forward like the retail routine.
void engine_copy_bytes(void* dest, const void* src, u32 count)
{
    if (count != 0 && dest != src)
    {
        std::memmove(dest, src, count);
    }
}
void* g_occupancy_pool = nullptr; // DAT_00512344
i32 g_occupancy_mask = 0; // DAT_00512348
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
// Body action reloading shares body_resources.cpp with 0x0013EE80; the oracle never reaches it.
struct FactoryObject;
struct ObjectFactoryRegistry;
struct AnimationResourceRegistry;
void release_runtime_body_actions(std::span<u8>, FactoryObject&, FactoryObject&, std::span<const u8>&,
    ObjectFactoryRegistry&, AnimationResourceRegistry&)
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
std::string hex_bytes(const u8* data, size_t size)
{
    std::string text;
    char buffer[4];
    for (size_t index = 0; index < size; ++index)
    {
        std::snprintf(buffer, sizeof(buffer), "%02x", data[index]);
        text += buffer;
    }
    return text;
}
std::string words(const float* values, size_t count)
{
    std::string text;
    char buffer[16];
    for (size_t index = 0; index < count; ++index)
    {
        std::snprintf(buffer, sizeof(buffer), index == 0 ? "%08x" : " %08x", bits_of(values[index]));
        text += buffer;
    }
    return text;
}
std::string pool_text(const GhoulySpawnPool* pool)
{
    if (pool->stride != 0x54)
    {
        return "bad";
    }
    const size_t size = size_t(pool->cursor - pool->begin);
    return std::to_string(size / 0x54) + ":" + hex_bytes(pool->begin, size);
}

struct Object
{
    SegmentProbe probe;
    int id = 0;
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();
    ListHead node;
    u32 mask = 0;
    float a = 0.f;
    float h = 0.f;
    float h2 = 0.f;
    u32 clamp = 0;
    u32 reply = 0;
    std::array<u8, 0x54> record{};
    std::vector<std::array<u8, 0x54>> contacts;
    std::unique_ptr<RuntimeSegmentContactActor> view;
};
std::vector<std::string> g_log;
Actor* g_player = nullptr;
std::vector<Object>* g_objects = nullptr;

std::string name(const void* address)
{
    if (address == g_player)
    {
        return "A";
    }
    for (Object& object : *g_objects)
    {
        if (address == object.actor.get() || address == object.view.get() || address == &object.probe)
        {
            return "O" + std::to_string(object.id);
        }
    }
    return "?";
}

i32 point_probe(SegmentProbe& self, Vec3& candidate, Vec3& kept, float radius, u32 mask, u8* record)
{
    Object& p = reinterpret_cast<Object&>(self);
    const bool hit = (mask & p.mask) != 0 && kept.x * p.a + candidate.y < p.h;
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "P%d %08x %08x %08x %08x %08x %08x %08x", p.id, bits_of(candidate.x),
        bits_of(candidate.y), bits_of(candidate.z), bits_of(kept.x), bits_of(kept.y), bits_of(kept.z), bits_of(radius));
    g_log.push_back(buffer);
    if (hit)
    {
        std::memcpy(record, p.record.data(), 0x54);
        if (p.clamp != 0)
        {
            kept.y = p.h;
        }
    }
    return hit ? 1 : 0;
}
i32 contacts_probe(SegmentProbe& self, Vec3& origin, float radius, u32 mask, SegmentPool& pool)
{
    Object& p = reinterpret_cast<Object&>(self);
    const bool hit = (mask & p.mask) != 0 && origin.y * p.a + radius < p.h2;
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), "C%d %08x %08x %08x %08x %x", p.id, bits_of(origin.x), bits_of(origin.y),
        bits_of(origin.z), bits_of(radius), mask);
    g_log.push_back(buffer);
    if (hit)
    {
        for (const auto& contact : p.contacts)
        {
            std::memcpy(pool.cursor, contact.data(), 0x54);
            pool.cursor += 0x54;
        }
    }
    return hit ? 1 : 0;
}
}

int main()
{
    int cases = 0;
    std::cin >> cases;
    for (int c = 0; c < cases; ++c)
    {
        static float freeze, dt, scale;
        static u32 block = 1, index = 0;
        static std::array<u8, 4> enabled{};
        freeze = next_float();
        dt = next_float();
        scale = next_float();
        const RuntimeActorClock clock{freeze, dt, scale, block, enabled, index};
        auto player = std::make_unique<Actor>();
        g_player = player.get();
        std::vector<u8> actor_bytes(0xD00);
        player->animation_pause = next_float();
        player->animation_rate = next_float();
        player->model_flags = next();
        const u16 tag = u16(next());
        std::memcpy(actor_bytes.data() + 6, &tag, 2);
        for (float& value : player->xform)
        {
            value = next_float();
        }
        std::vector<u8> body(0x800);
        const u32 count = next();
        std::memcpy(body.data() + 0x2CC, &count, 4);
        RuntimeBodyCollisionState state{};
        state.forced = next();
        const u32 object_count = next();
        std::vector<Object> objects(object_count);
        g_objects = &objects;
        ListHead players;
        std::vector<ListHead*> player_nodes;
        for (u32 k = 0; k < object_count; ++k)
        {
            Object& object = objects[k];
            object.id = int(k + 1);
            object.actor->position = Vec3{next_float(), next_float(), next_float()};
            object.actor->model_flags = next();
            const u32 child = next();
            if (next() != 0)
            {
                player_nodes.push_back(&object.node);
            }
            object.mask = next();
            object.a = next_float();
            object.h = next_float();
            object.h2 = next_float();
            object.clamp = next();
            object.reply = next();
            for (u8& value : object.record)
            {
                value = u8(next());
            }
            object.contacts.resize(next());
            for (auto& contact : object.contacts)
            {
                for (u8& value : contact)
                {
                    value = u8(next());
                }
            }
            object.probe.point = point_probe;
            object.probe.contacts = contacts_probe;
            Object* self = &object;
            object.view = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{*object.actor, child,
                &object.node, [self](void* message)
                {
                    auto& notice = *static_cast<RuntimeContactNotice*>(message);
                    char buffer[200];
                    std::snprintf(buffer, sizeof(buffer), "M%d %x %x %s %s ", self->id, notice.id, notice.reply,
                        name(notice.sender).c_str(), words(&notice.velocity.x, 3).c_str());
                    g_log.push_back(buffer + pool_text(notice.contacts) + " " + words(&notice.point.x, 3));
                    notice.reply = self->reply;
                }});
        }
        for (u32 k = 0; k < object_count; ++k)
        {
            objects[k].probe.next = k + 1 < object_count ? &objects[k + 1].probe : nullptr;
        }
        g_world.segment = object_count != 0 ? &objects[0].probe : nullptr;
        players.next = player_nodes.empty() ? nullptr : player_nodes[0];
        for (size_t k = 0; k < player_nodes.size(); ++k)
        {
            player_nodes[k]->next = k + 1 < player_nodes.size() ? player_nodes[k + 1] : nullptr;
        }
        if (next() != 0)
        {
            const u32 rows = next();
            auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
                i32(sizeof(GhoulySpawnPool) + std::max<u32>(rows, 4) * sizeof(RuntimeBodyCollisionOverride))));
            u8* begin = reinterpret_cast<u8*>(pool + 1);
            pool->stride = sizeof(RuntimeBodyCollisionOverride);
            pool->grow = 4;
            pool->begin = begin;
            pool->cursor = begin + rows * sizeof(RuntimeBodyCollisionOverride);
            pool->limit = begin + std::max<u32>(rows, 4) * sizeof(RuntimeBodyCollisionOverride);
            for (u32 r = 0; r < rows; ++r)
            {
                auto& row = reinterpret_cast<RuntimeBodyCollisionOverride*>(begin)[r];
                row.actor = objects[next()].actor.get();
                row.saved = next();
                row.retained = next();
                row.active = next();
            }
            state.overrides = pool;
        }
        if (next() != 0)
        {
            const u32 picks = next();
            auto* pool = static_cast<GhoulySpawnPool*>(engine_heap_allocate(
                i32(sizeof(GhoulySpawnPool) + std::max<u32>(picks, 1) * sizeof(Actor*))));
            u8* begin = reinterpret_cast<u8*>(pool + 1);
            pool->stride = sizeof(Actor*);
            pool->grow = 4;
            pool->begin = begin;
            pool->cursor = begin + picks * sizeof(Actor*);
            pool->limit = pool->cursor;
            for (u32 r = 0; r < picks; ++r)
            {
                reinterpret_cast<Actor**>(begin)[r] = objects[next()].actor.get();
            }
            state.removed = pool;
        }
        const u32 shapes = next();
        std::vector<RuntimeBodyCollisionRecord> records(shapes);
        std::vector<std::array<u8, 0x24>> nodes(shapes);
        for (u32 s = 0; s < shapes; ++s)
        {
            auto& node = nodes[s];
            node.fill(0);
            const u16 word = u16(next());
            std::memcpy(node.data() + 2, &word, 2);
            node[8] = 0xFF;
            node[9] = 0xFF;
            for (int i = 0; i < 4; ++i)
            {
                const u32 value = next();
                std::memcpy(node.data() + 0x10 + 4 * i, &value, 4);
            }
            RuntimeBodyCollisionRecord& record = records[s];
            record.bytes.fill(0);
            for (int i = 0; i < 3; ++i)
            {
                const u32 value = next();
                std::memcpy(record.bytes.data() + 8 + 4 * i, &value, 4);
            }
            record.bytes[0x17] = u8(next());
            const u32 scale_bits = next();
            std::memcpy(record.bytes.data() + 0x18, &scale_bits, 4);
            record.shape = node.data();
        }
        for (u32 s = 0; s < shapes; ++s)
        {
            records[s].next = s + 1 < shapes ? &records[s + 1] : nullptr;
        }
        state.active = shapes != 0 ? &records[0] : nullptr;
        const u32 mode = next();
        std::array<std::pair<u32, u32>, 6> plan;
        for (auto& step : plan)
        {
            step.first = next();
            step.second = next();
        }
        for (SegmentSlot& slot : g_segment_slots)
        {
            slot = SegmentSlot{};
        }
        g_segment_count = 0;
        g_log.clear();
        static std::array<u32, 0x3D * 8> totals;
        static std::array<u32, 0x3D * 8> session;
        totals.fill(0);
        session.fill(0);
        RuntimePlayerContactReport report{body, actor_bytes, totals, session, [](void* message)
            {
                auto& r = *static_cast<RuntimeContactReport*>(message);
                char buffer[200];
                std::snprintf(buffer, sizeof(buffer), "S %x %d %s %s ", r.id, r.word, words(&r.velocity.x, 3).c_str(),
                    name(r.object).c_str());
                g_log.push_back(buffer + pool_text(r.contacts) + " " + words(&r.point.x, 3) + " " +
                    [&]
                    {
                        char tail[16];
                        std::snprintf(tail, sizeof(tail), "%x", r.reply);
                        return std::string(tail);
                    }());
            }};
        size_t calls = 0;
        RuntimeBodyContactWalk walk{*player, state, nullptr,
            [&objects](SegmentProbe& probe) -> RuntimeSegmentContactActor&
            {
                return *reinterpret_cast<Object&>(probe).view;
            },
            players, clock};
        if (mode != 0)
        {
            walk.callback = [&report](Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point,
                GhoulySpawnPool* contacts, const RuntimeSegmentContactRecord&, const Vec3& velocity, u16 word)
            {
                return report_runtime_player_contact(actor, object, point, contacts, velocity, word, report);
            };
        }
        else
        {
            walk.callback = [&](Actor& actor, RuntimeSegmentContactActor& object, const Vec3& point,
                GhoulySpawnPool* contacts, const RuntimeSegmentContactRecord& record, const Vec3& velocity, u16 word)
            {
                const auto& step = plan[calls % plan.size()];
                ++calls;
                char buffer[96];
                std::snprintf(buffer, sizeof(buffer), "%02x%02x%02x ", record.kind, record.size, record.flag);
                std::snprintf(buffer + 7, sizeof(buffer) - 7, "%s", "");
                std::string text = "K " + name(&actor) + " " + name(&object) + " " + words(&point.x, 3) + " " +
                    pool_text(contacts) + " " + buffer + words(&record.point.x, 3) + " " + words(&record.radius, 1) +
                    " " + words(&velocity.x, 3);
                char tail[8];
                std::snprintf(tail, sizeof(tail), " %04x", word);
                g_log.push_back(text + tail);
                if (step.second != 0)
                {
                    state.active = nullptr;
                }
                return step.first;
            };
        }
        update_runtime_body_contacts(walk);
        std::string out;
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%08x %08x %08x %08x", player->model_flags, state.active != nullptr ? 1u : 0u,
            state.walking, state.removed != nullptr ? 1u : 0u);
        out += buffer;
        out += " | rows";
        if (state.overrides != nullptr)
        {
            for (auto* row = reinterpret_cast<RuntimeBodyCollisionOverride*>(state.overrides->begin);
                row < reinterpret_cast<RuntimeBodyCollisionOverride*>(state.overrides->cursor); ++row)
            {
                std::snprintf(buffer, sizeof(buffer), " %s:%08x:%08x:%x", name(row->actor).c_str(), row->saved,
                    row->retained, row->active);
                out += buffer;
            }
        }
        else
        {
            out += " none";
        }
        out += " | prev";
        for (const auto& record : records)
        {
            out += " " + words(reinterpret_cast<const float*>(record.bytes.data() + 8), 3);
        }
        out += " | filters";
        for (const Object& object : objects)
        {
            std::snprintf(buffer, sizeof(buffer), " %08x", object.actor->model_flags);
            out += buffer;
        }
        u32 final_count;
        std::memcpy(&final_count, body.data() + 0x2CC, 4);
        out += " | body " + std::to_string(final_count);
        for (int t = 0; t < 6; ++t)
        {
            out += " " + std::to_string(totals[size_t(t) * 0x3D]);
        }
        for (int t = 0; t < 6; ++t)
        {
            out += " " + std::to_string(session[size_t(t) * 0x3D]);
        }
        out += " | log";
        for (size_t k = 0; k < g_log.size(); ++k)
        {
            out += (k == 0 ? " " : " ; ") + g_log[k];
        }
        std::printf("%s\n", out.c_str());
        std::fflush(stdout);
    }
}
