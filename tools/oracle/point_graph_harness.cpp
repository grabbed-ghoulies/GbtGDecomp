// Native side of point_graph_oracle.py: reads cases from stdin and writes the resulting memory image.
#include "engine/graphics/point_graph_solver.h"
#include "engine/graphics/d3dx_matrix.h"
#include "engine/scene/background_update.h"
#include <cstdio>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
#include <cstring>
#include <vector>

using namespace gbtg;

namespace
{
constexpr u32 kBase = 0x40000000;
constexpr size_t kSize = 0x8000;
constexpr size_t kFields = 0;
constexpr size_t kPoints = 0x1000;
constexpr size_t kEdges = 0x2000;
constexpr size_t kCoordinates = 0x3000;
constexpr size_t kFlags = 0x3800;
constexpr size_t kContacts = 0x4000;
constexpr size_t kVertices = 0x5000;
constexpr size_t kScratch = 0x6000;

bool read_exact(void* output, size_t size)
{
    return std::fread(output, 1, size, stdin) == size;
}

RuntimePointGraph graph_view(std::vector<u8>& memory)
{
    RuntimePointGraph graph{};
    graph.fields = {memory.data() + kFields, 0x1E0};
    graph.allocation = memory.data() + kEdges;
    graph.points = {memory.data() + kPoints, 0x1000};
    graph.edges = {memory.data() + kEdges, 0x1000};
    graph.coordinates = {memory.data() + kCoordinates, 0x800};
    graph.edge_flags = {memory.data() + kFlags, 0x100};
    graph.points_address = kBase + kPoints;
    return graph;
}

const Vec3* vertex_at(std::vector<u8>& memory, u32 address)
{
    return reinterpret_cast<const Vec3*>(memory.data() + (address - kBase));
}
}

int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::vector<u8> memory(kSize);
    u32 header[2];
    while (read_exact(header, sizeof(header)))
    {
        const u32 mode = header[0];
        u32 words[16];
        if (!read_exact(memory.data(), kSize) || !read_exact(words, sizeof(words)))
        {
            return 1;
        }
        float reals[16];
        std::memcpy(reals, words, sizeof(reals));
        RuntimePointGraph graph = graph_view(memory);
        u32 result = 0;
        switch (mode)
        {
        case 0:
            step_runtime_point_graph(graph, reals[0]);
            break;
        case 1:
            relax_runtime_point_graph_edge(graph, words[1], words[2], reals[3]);
            break;
        case 2:
            relax_runtime_point_graph_edge_squared(graph, words[1], words[2], reals[3]);
            break;
        case 3:
            hold_runtime_point_graph_edge(graph, words[1], words[2], reals[3]);
            break;
        case 4:
            relax_runtime_point_graph(graph);
            break;
        case 5:
        {
            Vec3 point{reals[4], reals[5], reals[6]};
            const u8* found = find_runtime_sphere_record(point, {memory.data() + kScratch, 0x800});
            result = found == nullptr ? 0 : kBase + u32(found - memory.data());
            break;
        }
        case 6:
        {
            Vec3 from{reals[4], reals[5], reals[6]};
            Vec3 to{reals[7], reals[8], reals[9]};
            result = runtime_rotation_between(reinterpret_cast<float*>(memory.data() + kScratch), from, to);
            break;
        }
        case 7:
        {
            const i32 count = i32(words[1]);
            std::vector<RuntimeGraphContact> contacts(size_t(count > 0 ? count : 0));
            for (i32 index = 0; index < count; ++index)
            {
                const u8* row = memory.data() + kContacts + size_t(index) * 0x28;
                u32 corners[3];
                std::memcpy(corners, row, 12);
                RuntimeGraphContact& contact = contacts[size_t(index)];
                contact.first = vertex_at(memory, corners[0]);
                contact.second = vertex_at(memory, corners[1]);
                contact.third = vertex_at(memory, corners[2]);
                std::memcpy(&contact.normal, row + 0xC, 12);
                std::memcpy(&contact.plane, row + 0x18, 12);
                std::memcpy(&contact.offset, row + 0x24, 4);
            }
            const RuntimeGraphContact* hit = project_runtime_point_graph_edges(graph, contacts, count);
            result = hit == nullptr ? 0 : kBase + u32(kContacts + size_t(hit - contacts.data()) * 0x28);
            break;
        }
        case 8:
            recover_runtime_point_graph_transform(graph);
            break;
        case 9:
        {
            float* out = reinterpret_cast<float*>(memory.data() + kScratch);
            multiply_runtime_graphics_matrices(out, out + 16, out + 32);
            runtime_graphics_translation_matrix(out + 48, reals[4], reals[5], reals[6]);
            Vec3 normal{reals[7], reals[8], reals[9]};
            Vec3 turned{};
            transform_runtime_graphics_normal(turned, normal, out + 16);
            std::memcpy(out + 64, &turned, 12);
            break;
        }
        case 11:
        {
            Actor actor;
            actor.animation_rate = reals[7];
            actor.animation_pause = reals[8];
            ActorMatrixSetup matrices{};
            matrices.matrices.count = 1;
            matrices.matrices.node = {memory.data() + 0x6200, 0x40};
            matrices.matrices.instance = {memory.data() + 0x6300, 0x100};
            RuntimeBackgroundLifetime lifetime{};
            lifetime.animation = memory.data();
            RuntimeBackgroundAnimation animation{};
            animation.rows = {&graph, 1};
            animation.animation_block = {memory.data() + 0x6880, 0x48};
            animation.header = {memory.data() + 0x67F0, 0x10};
            animation.matrix_count = 1;
            animation.matrix_node = matrices.matrices.node;
            RuntimeBackgroundConstruction background{{memory.data() + 0x4000, 0x9E8}, actor, matrices,
                lifetime, animation, {}, {}};
            const u32 zero = 0;
            const RuntimeActorClock clock{reals[4], reals[5], reals[6], zero, {}, zero};
            const std::function<std::span<const u8>(u32, size_t)> resolve = [&memory](u32 address, size_t size)
            {
                return std::span<const u8>{memory}.subspan(address - kBase, size);
            };
            RuntimeShaderMatrixBoundaries boundaries{};
            RuntimeBackgroundUpdate context{&background, clock, resolve, nullptr, boundaries};
            update_background_animation(background, context);
            break;
        }
        case 10:
        {
            const auto resolve = [&memory](u32 address, size_t size) -> std::span<const u8>
            {
                return {memory.data() + (address - kBase), size};
            };
            step_runtime_point_graph_collision(graph, reals[0], resolve(kBase + 0x6900, 0x48), resolve);
            break;
        }
        default:
            return 2;
        }
        std::fwrite(memory.data(), 1, kSize, stdout);
        std::fwrite(&result, 1, 4, stdout);
    }
    return 0;
}
