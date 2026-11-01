#pragma once
#include "render_optimize.h"
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
namespace gbtg::render_profile
{
using Clock = std::chrono::steady_clock;
enum Phase
{
    Tick, Presenter, Declaration, Vertex, Topology, Texture, Submission, Present, Compilation, Recording, HostRendering, PhaseCount
};
struct Frame
{
    std::array<double, PhaseCount> ms{};
    unsigned long long draws = 0, vertices = 0, output_vertices = 0, uploaded_bytes = 0;
    unsigned long long buffer_allocations = 0, state_misses = 0, declaration_misses = 0, texture_uploads = 0, shader_instructions = 0, pixel_misses = 0, gpu_vertex_draws = 0, cpu_vertex_draws = 0, all_draws = 0, allocations = 0;
};
inline bool enabled = false;
inline bool reference = false;
inline bool cpu_vertex = false;
inline bool validate_vertex = false;
inline bool capture_replay = false;
inline std::function<void()> replay;
#if GBTG_OPTIMIZE
// GBTG_OPTIMIZE step 11: the render thread runs backend calls, so each thread counts its own frame. The render thread's
// totals are added to the main thread's with merge when it finishes presenting (gfx_render_thread.cpp).
inline thread_local Frame frame;
inline void merge(Frame& into, const Frame& from)
{
    for (size_t n = 0; n < into.ms.size(); ++n)
    {
        into.ms[n] += from.ms[n];
    }
    into.draws += from.draws;
    into.vertices += from.vertices;
    into.output_vertices += from.output_vertices;
    into.uploaded_bytes += from.uploaded_bytes;
    into.buffer_allocations += from.buffer_allocations;
    into.state_misses += from.state_misses;
    into.declaration_misses += from.declaration_misses;
    into.texture_uploads += from.texture_uploads;
    into.shader_instructions += from.shader_instructions;
    into.pixel_misses += from.pixel_misses;
    into.gpu_vertex_draws += from.gpu_vertex_draws;
    into.cpu_vertex_draws += from.cpu_vertex_draws;
    into.all_draws += from.all_draws;
    into.allocations += from.allocations;
}
#else
inline Frame frame;
#endif
inline thread_local bool count_allocations = false;
inline unsigned long long number = 0;
inline std::ofstream output, gpu_output;
inline void start(const std::string& path)
{
    output.open(path);
    gpu_output.open(path + ".gpu.csv");
    if (!output || !gpu_output)
    {
        throw std::runtime_error("cannot open renderer profile output");
    }
    enabled = true;
    output << "frame,frame_ms,tick_ms,presenter_ms,declaration_ms,vertex_ms,topology_ms,texture_ms,submission_ms,present_ms,compilation_ms,recording_ms,host_rendering_ms,draws,unique_vertices,output_vertices,uploaded_bytes,buffer_allocations,state_misses,declaration_misses,texture_uploads,shader_instructions,pixel_misses,gpu_vertex_draws,cpu_vertex_draws,all_draws,allocations,optimize\n";
    gpu_output << "frame,gpu_ms,gpu_draw_ms\n";
}
struct Scope
{
    Phase phase;
    Clock::time_point begin;
    explicit Scope(Phase value) : phase(value), begin(enabled ? Clock::now() : Clock::time_point{})
    {
    }
    ~Scope()
    {
        if (enabled)
        {
            frame.ms[phase] += std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        }
    }
};
inline void finish(double duration)
{
    if (!enabled)
    {
        return;
    }
    output << number << ',' << duration;
    for (double value : frame.ms)
    {
        output << ',' << value;
    }
    output << ',' << frame.draws << ',' << frame.vertices << ',' << frame.output_vertices << ','
        << frame.uploaded_bytes << ',' << frame.buffer_allocations << ',' << frame.state_misses << ','
        << frame.declaration_misses << ',' << frame.texture_uploads << ',' << frame.shader_instructions << ',' << frame.pixel_misses << ',' << frame.gpu_vertex_draws << ',' << frame.cpu_vertex_draws << ',' << frame.all_draws << ',' << frame.allocations << ',' << GBTG_OPTIMIZE << '\n';
    ++number;
    frame = {};
}
}
