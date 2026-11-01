#include "../src/engine/graphics/scene_render_helpers.h"
#include "../src/engine/graphics/scene_ambient_sampling.h"
#include "../src/engine/graphics/scene_environment_matrices.h"
#include "../src/engine/graphics/scene_light_upload.h"
#include "../src/engine/graphics/scene_environment_setup.h"
#include "../src/engine/graphics/model_tree_dispatch.h"
#include "../src/engine/graphics/model_push_buffer_draw.h"
#include "../src/engine/graphics/model_render_nodes.h"
#include "../src/engine/graphics/model_shader_fragments.h"
#include "../src/engine/graphics/vertex_shader_code.h"
#include "../src/engine/graphics/xg_shader_assembly.h"
#include "../src/engine/graphics/xg_pixel_shader.h"
#include "../src/engine/graphics/xbox_vertex_program.h"
#include "../src/engine/graphics/xbox_vertex_hlsl.h"
#include "../src/engine/graphics/scene_d3d_device.h"
#include "../src/engine/graphics/door_shadow_draw.h"
#include "../src/engine/graphics/shader_schedule_cost.h"
#include "../src/engine/camera/display_view.h"
#include "../src/engine/camera/display_setup_math.h"
#include "../src/engine/graphics/color_blend.h"
#include "../src/engine/entities/actor_frame_render.h"
#include <bit>
#include <limits>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cmath>

void check_shader_compiler_leaves();

namespace
{
using namespace gbtg;
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
void check_color_helpers()
{
    require(equal_runtime_packed_colors(0x12345678, 0x12345678), "packed color equality");
    for (u32 byte = 0; byte != 4; ++byte)
    {
        require(!equal_runtime_packed_colors(0x12345678, 0x12345678 ^ (1u << (byte * 8))),
            "packed color channel comparison");
    }
    RuntimeModelRenderContext renderer{};
    std::array<float, 4> retained{};
    set_runtime_scene_color(.5f, 1.5f, -.25f, 1.f, retained, renderer);
    require(retained == std::array<float, 4>{.5f, 1.5f, -.25f, 1.f}
        && renderer.state.color == std::array<u8, 4>{127, 126, 193, 255},
        "scene color must truncate and retain unclamped floats");
    const float nan = std::bit_cast<float>(0x7FC01234u);
    set_runtime_scene_color(nan, 0.f, 0.f, 0.f, retained, renderer);
    require(std::bit_cast<u32>(retained[0]) == 0x7FC01234u && renderer.state.color[0] == 0,
        "scene color invalid conversion and retained NaN bits");
}
void check_vertex_gpu_translation()
{
    const auto program = assemble_xg_vertex_shader("xvs.1.1\nm4x4 oPos, v0, c0\nmov oD0, v3\n");
    const auto source = xbox_vertex_hlsl(program);
    require(!source.empty() && source.find("precise") != std::string::npos && source.find("16777215.0") != std::string::npos,
        "GPU translator preserves precise arithmetic and retail depth conversion");
    const std::vector<u32> addressed
    {
        (2u << 16) | 0x2078, 0, (13u << 21) | (1u << 9), 2u << 26, 0,
        0, (1u << 21) | (96u << 13) | 0x1B, 3u << 26, 0xF803
    };
    require(!xbox_vertex_hlsl(addressed).empty(), "direct address-register loads translate");
    const auto validations = xbox_vertex_address_validation(addressed);
    std::array<XboxVector, 16> inputs{};
    std::array<XboxVector, 192> constants{};
    inputs[1][0] = 2.9f;
    validate_xbox_vertex_address(validations, inputs, constants);
    bool raised = false;
    inputs[1][0] = 1000.f;
    try
    {
        validate_xbox_vertex_address(validations, inputs, constants);
    }
    catch (const Error&)
    {
        raised = true;
    }
    require(raised, "GPU relative reads outside the register file must raise before submission");
    auto constant_write = assemble_xg_vertex_shader("vs.1.0\n#pragma screenspace\nmov oPos, v0\n");
    constant_write[4] &= ~(1u << 11);
    constant_write[4] |= 96u << 3;
    require(xbox_vertex_hlsl(constant_write).empty(), "constant writes remain explicit CPU fallbacks");
    auto transcendental = assemble_xg_vertex_shader("vs.1.0\n#pragma screenspace\nrcc oPos, v0.x\n");
    transcendental[2] = (transcendental[2] & ~(7u << 25)) | (5u << 25);
    require(xbox_vertex_hlsl(transcendental).empty(), "unvalidated transcendental arithmetic retains the interpreter");
    std::map<u32, u32> states{{SceneD3DDevice::kPixelShader, 17}, {SceneD3DDevice::kCullMode, 0x900}};
    const SceneD3DStateSnapshot snapshot(states);
    states[SceneD3DDevice::kPixelShader] = 19;
    require(snapshot.at(SceneD3DDevice::kPixelShader) == 17 && snapshot.find(999) == snapshot.end(),
        "contiguous state snapshots retain draw-time values after mutation");
}

void check_boot_shader_boundary()
{
    const auto shader = assemble_xg_vertex_shader("xvs.1.1\nm4x4 oPos, v0, c0\nmov oD0, v3\n");
    XboxVertexProgramState state{};
    state.inputs[0] = {2.f, 3.f, .25f, 2.f};
    state.inputs[3] = {.1f, .2f, .3f, .4f};
    for (u32 index = 0; index < 4; ++index)
    {
        state.constants[96 + index][index] = 1.f;
    }
    state.constants[58] = {320.f, -240.f, SceneD3DDevice::kDepthScale, 0.f};
    state.constants[59] = {320.f, 240.f, 0.f, 0.f};
    run_xbox_vertex_program(shader, state);
    require(state.outputs[0][0] == 640.f && state.outputs[0][1] == -120.f && state.outputs[0][3] == 2.f,
        "boot assembly applies the reciprocal viewport transform and keeps homogeneous w");
    require(state.outputs[3] == state.inputs[3], "boot assembly preserves the door vertex colour");
    const auto screen = assemble_xg_vertex_shader("vs.1.0\n#pragma screenspace\nadd oPos, v0, c0\n");
    state.constants[96] = {10.f, 20.f, 30.f, 0.f};
    run_xbox_vertex_program(screen, state);
    require(state.outputs[0] == XboxVector{12.f, 23.f, 30.25f, 2.f},
        "screenspace pragma suppresses the viewport epilogue");
    bool raised = false;
    try
    {
        (void)assemble_xg_vertex_shader("xvs.1.1\nunknown oPos, v0\n");
    }
    catch (const Error&)
    {
        raised = true;
    }
    require(raised, "unsupported shader instructions raise");
    const std::array<i32, 32> declaration{0x20000000, 0x40320000, 0x40400003, -1};
    SceneD3DDevice device;
    u32 handle;
    device.create_vertex_shader(declaration.data(), shader.data(), handle, 0);
    device.dialogue().vertex_shader(handle);
    u32* push = device.dialogue().begin_push(21);
    const std::array<u32, 21> words{0x417FC, 8, 0x40401818,
        0, 0, 0, 0xFF000000, 0, 0, 0, 0xFF000000,
        0, 0, 0, 0xFF000000, 0, 0, 0, 0xFF000000, 0x417FC, 0};
    std::memcpy(push, words.data(), sizeof(words));
    device.dialogue().end_push(push + words.size());
    device.dialogue().vertex_shader(0);
    const auto commands = device.take_commands();
    const auto* draw = std::get_if<SceneD3DInlineDraw>(&commands.at(0));
    require(draw != nullptr && draw->vertices.size() == 16 && draw->vertices[3] == 0xFF000000 &&
        draw->state.states.at(SceneD3DDevice::kVertexShader) == handle,
        "inline arrays preserve packed colour bits and draw-time shader state");
    std::array<u8, 0x154> door{};
    const auto store = [&door](u32 offset, float value)
    {
        std::memcpy(door.data() + offset, &value, 4);
    };
    store(0x70, 1.f);
    store(0x84, 1.f);
    store(0x98, 1.f);
    store(0xA0, -1.f);
    store(0xA4, -2.f);
    store(0xA8, -3.f);
    store(0xAC, 1.f);
    store(0xB0, 2.f);
    store(0xB4, 3.f);
    std::array<u32, 3> dimensions{std::bit_cast<u32>(10.f), std::bit_cast<u32>(255.f), std::bit_cast<u32>(0.f)};
    u32 margin = std::bit_cast<u32>(2.f);
    RuntimeRelocationImage image{{}, 0, [&dimensions, &margin](u32 address) -> u8*
    {
        if (address == 0x0032B214)
        {
            return reinterpret_cast<u8*>(&margin);
        }
        require(address >= 0x00424998 && address < 0x004249A4, "door bound table address");
        return reinterpret_cast<u8*>(dimensions.data()) + address - 0x00424998;
    }};
    RuntimeDoorShadowBounds bounds{91.f, 92.f, 93.f, 94.f};
    require(select_runtime_door_shadow_bounds(door, {-1.f, 2.f, 3.f}, bounds, image) == 0 &&
        bounds.depth_start == 91.f && bounds.opacity_start == 93.f,
        "camera on the door volume boundary suppresses drawing without changing outputs");
    require(select_runtime_door_shadow_bounds(door, {0.f, 0.f, -5.f}, bounds, image) == 1 &&
        bounds.depth_start == 12.f && bounds.depth_end == 8.f && bounds.opacity_start == 255.f
        && bounds.opacity_end == 0.f,
        "near first door plane selects the forward fade depth and opacity");
    require(select_runtime_door_shadow_bounds(door, {0.f, 0.f, 5.f}, bounds, image) == 1 &&
        bounds.depth_start == 8.f && bounds.depth_end == 12.f && bounds.opacity_start == 0.f
        && bounds.opacity_end == 255.f,
        "near second door plane reverses the fade depth and opacity");
    require(select_runtime_door_shadow_bounds(door,
        {std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f}, bounds, image) == 1,
        "unordered camera coordinates follow the outside-volume branch");
}

void check_boot_pixel_shaders()
{
    const auto pass = assemble_xg_pixel_shader("xps.1.1\nmov r0, v0\n");
    require((pass[53] & 255) == 1 && (pass[45] & 0xFFF) == 0xC0 && (pass[26] & 0xFFF) == 0xC0,
        "pass-colour shader writes both RGB and alpha to r0");
    require((pass[34] >> 24 & 15) == 4 && (pass[0] >> 24 & 31) == 20 &&
        pass[8] == 0xC && pass[9] == 0x1C00,
        "pass-colour shader selects diffuse RGB/alpha and passes r0 through the final combiner");
    const auto texture = assemble_xg_pixel_shader("xps.1.1\ntex t0\nmul r0, t0, v0\n");
    require((texture[54] & 31) == 1 && (texture[34] >> 24 & 15) == 8 &&
        (texture[34] >> 16 & 15) == 4 && (texture[0] >> 24 & 31) == 24,
        "textured diffuse shader samples t0 and multiplies both channels by v0");
    const auto paired = assemble_xg_pixel_shader(
        "xps.1.1\ntex t0\nmad r0.rgb, t0, c0, c1 + mov_x4 r0.a, t0.b\n");
    require((paired[53] & 255) == 1 && (paired[45] & 0xFFF) == 0xC00 &&
        (paired[26] & 0x20000) != 0 && (paired[0] >> 24 & 31) == 8 &&
        (paired[57] & 15) == 0 && (paired[58] & 15) == 1,
        "paired RGB/alpha share a stage and constants, retaining blue-source and x4 output flags");
    const auto fixed = assemble_xg_pixel_shader("xps.1.1\ndef c0, 1, 0.5, 0, 1\nmov r0, c0\n");
    require(fixed[10] == 0xFFFF8000 && (fixed[57] & 15) == 15,
        "def constants are packed and excluded from dynamic constant mapping");
    const auto dot = assemble_xg_pixel_shader(
        "xps.1.1\ntex t0\ntexm3x2pad t1, t0_bx2\ntexm3x2tex t2, t0_bx2\nmov r0, t2\n");
    require((dot[54] >> 5 & 31) == 0x11 && (dot[54] >> 10 & 31) == 9 && dot[55] == 0x11,
        "texture matrix instructions preserve dependent-read modes and D3D dot mapping");
    bool raised = false;
    try
    {
        (void)assemble_xg_pixel_shader("xps.1.1\nunsupported r0, v0\n");
    }
    catch (const Error&)
    {
        raised = true;
    }
    require(raised, "unsupported pixel instructions raise instead of fabricating a shader");

    std::array<u8, 0x148> table{};
    const std::string source = "xps.1.1\nmov r0, v0\n";
    RuntimeRelocationImage image{table, 0x00425DE0,
        [&table, &source](u32 address) -> u8*
        {
            if (address == 0x10000)
            {
                return reinterpret_cast<u8*>(const_cast<char*>(source.data()));
            }
            require(address >= 0x00425DE0 && address < 0x00425DE0 + table.size(), "pixel boot table address");
            return table.data() + address - 0x00425DE0;
        }};
    for (u32 index = 0; index < 20; ++index)
    {
        const u32 row = 0x00425DE8 + index * 0x10;
        image.set_word(row - 8, 0, 0x10000);
        image.set_word(row - 4, 0, u32(source.size()));
        image.set_word(row, 0, index == 0 ? 0x00063F40 : 0);
    }
    u32 assembly_calls = 0, creation_calls = 0, factory_calls = 0;
    RuntimeBootPixelShaders context{image,
        [&assembly_calls](std::string_view text, XboxPixelShaderDefinition& definition) -> i32
        {
            ++assembly_calls;
            if (assembly_calls == 3)
            {
                return -1;
            }
            definition = assemble_xg_pixel_shader(text);
            return 0;
        },
        [&creation_calls](const XboxPixelShaderDefinition& definition, u32& handle) -> i32
        {
            ++creation_calls;
            if (creation_calls == 1)
            {
                require(definition[8] == 0x20080000 && definition[9] == 0x1800 &&
                    definition[53] == 1 && definition[54] == 0x54421 &&
                    definition[55] == 0x440 && definition[56] == 0x110000,
                    "0x63F40 factory retains its exact six nonzero definition words");
            }
            handle = creation_calls + 100;
            return -1; // The retail pixel boot functions do not test CreatePixelShader's HRESULT.
        },
        [&context, &factory_calls](u32 address) -> u32
        {
            require(address == 0x00063F40, "pixel boot invokes the table's factory address");
            ++factory_calls;
            return create_runtime_boot_texture_pixel_shader(context);
        }};
    create_runtime_boot_pixel_shaders(context);
    require(factory_calls == 1 && assembly_calls == 19 && creation_calls == 19 &&
        image.word(0x00425DEC) == 101 && image.word(0x00425DEC + 3 * 0x10) == 0 &&
        image.word(0x00425F1C) == 119,
        "pixel boot walks twenty rows, writes zero after assembly failure, and preserves creation output handles");
}
void check_matrix_helpers()
{
    std::array<u32, 16> input{};
    for (u32 index = 0; index != input.size(); ++index)
    {
        input[index] = 0x7F800001u + index;
    }
    std::array<u32, 16> projection{}, view{};
    copy_runtime_primary_environment_matrix(input.data(), projection);
    copy_runtime_secondary_environment_matrix(input.data(), view);
    require(projection == input && view == input, "matrix copies must preserve all raw words");
    copy_runtime_primary_environment_matrix(projection.data(), projection);
    copy_runtime_secondary_environment_matrix(view.data(), view);
    require(projection == input && view == input, "matrix self-copy must preserve words");
}
void check_attachment_draw_walk()
{
    Actor actor{};
    ActorAnimationState animation{};
    ModelAnimationState model{};
    ActorAttachmentList attachments{};
    ActorAttachmentNode pending{};
    attachments.pending = &pending; // Actor +0x1B4.
    entity_detail::ListHead anchors{};
    ActorFrameState state{actor, animation, model, attachments, pending, nullptr, anchors};
    struct DrawRow : ActorFrameAttachment
    {
        i32 id;
        std::vector<i32>* visits;
        ActorFrameState* frame;
    };
    std::vector<i32> visits;
    DrawRow first{}, second{};
    first.next = &second;
    first.id = 1;
    second.id = 2;
    first.visits = second.visits = &visits;
    first.frame = second.frame = &state;
    const ActorFrameDrawCallback draw = [](ActorFrameAttachment& attachment, Actor& owner, i32 kind)
    {
        auto& row = static_cast<DrawRow&>(attachment);
        require(&owner == &row.frame->actor && kind == 3, "attachment draw owner and kind arguments");
        row.visits->push_back(row.id);
        row.next = nullptr;
        row.frame->updates = nullptr;
    };
    const std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback> draws
    {
        {&first, draw}, {&second, draw}
    };
    const ActorFrameDrawContext context{draws};
    state.updates = &first;
    draw_actor_frame_attachments(state, 3, context);
    require(visits == std::vector<i32>{1, 2}, "attachment draw must cache next before callback mutation");
    draw_actor_frame_attachments(state, 3, context);
    require(visits.size() == 2, "empty attachment draw list");
}
void check_ambient_sampling()
{
    std::array<u8, 0x38> grid{};
    const auto put = [&grid](u32 offset, const auto& value)
    {
        std::memcpy(grid.data() + offset, &value, sizeof(value));
    };
    put(0, 10.f);
    put(8, 20.f);
    put(0x18, 2.f);
    put(0x20, 4.f);
    put(0x24, u16{2});
    put(0x28, u16{2});
    const std::array<u8, 12> pixels{10, 20, 30, 110, 40, 50, 30, 120, 70, 130, 140, 90};
    std::memcpy(grid.data() + 0x2C, pixels.data(), pixels.size());
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Vec3 center{11.f, nan, 22.f};
    require(sample_runtime_environment_grid(grid.data(), center) == 0xFF3C5046u,
        "environment bilinear sample must use X/Z axes and RGB stride");
    require(sample_runtime_environment_grid(grid.data(), Vec3{14.f, 0.f, 28.f}) == 0xFF5A8C82u,
        "environment sample must clamp both grid indices");
    require(sample_runtime_environment_grid(grid.data(), Vec3{7.f, 0.f, 22.f}) == 0,
        "negative grid truncation must retain the retail unsigned fraction conversion");
    require(add_saturated_runtime_colors(0xFFFA0301u, 0x01FF0402u) == 0xFFFF0703u,
        "packed environment addition must saturate each channel independently");
    require(menu_color(0xFFFFFFFFu, 0, nan) == 0,
        "packed color interpolation must use retail invalid truncation");
    std::array<SceneAmbientPoint, 32> points{};
    const u8* grids[]{grid.data(), grid.data()};
    u32 override_count = 0;
    SceneAmbientContext context{grids, 0, override_count, {.1f, .2f, .3f, .4f},
        {2.f, 3.f, 4.f}, 0, points};
    std::array<float, 4> color{};
    sample_runtime_environment_color(center, color.data(), context);
    require(color == std::array<float, 4>{1.f, 1.f, 1.f, 1.f}, "zero environment grid count default");
    context.grid_count = -1;
    sample_runtime_environment_color(center, color.data(), context);
    require(color == std::array<float, 4>{0.f, 0.f, 0.f, 1.f}, "negative environment grid count preserves black");
    context.grid_count = 2;
    sample_runtime_environment_color(center, color.data(), context);
    require(std::fabs(color[0] - 140.f / 255.f) < .00001f
        && std::fabs(color[1] - 160.f / 255.f) < .00001f
        && std::fabs(color[2] - 120.f / 255.f) < .00001f,
        "environment grids must combine with packed saturation before float conversion");
    override_count = 1;
    context.points_enabled = 1;
    points[31].position = Vec3{10.f, 0.f, 0.f};
    points[31].active = 1;
    sample_runtime_scene_ambient(Vec3{}, color.data(), context);
    require(color == context.override_color, "ambient override bypasses grids, scaling and point contributions");
    override_count = 0;
    context.grid_count = 0;
    sample_runtime_scene_ambient(Vec3{}, color.data(), context);
    require(color == std::array<float, 4>{52.f, 53.f, 54.f, 51.f},
        "last ambient point must affect all four scaled channels");
    points[31].position = Vec3{};
    sample_runtime_scene_ambient(Vec3{}, color.data(), context);
    require(color == std::array<float, 4>{2.f, 3.f, 4.f, 1.f}, "coincident point must skip reciprocal distance");
    points[31].position.x = nan;
    sample_runtime_scene_ambient(Vec3{}, color.data(), context);
    require(color == std::array<float, 4>{2.f, 3.f, 4.f, 1.f}, "unordered point distance must skip contribution");
    points[31].active = 0;
    sample_runtime_scene_ambient(Vec3{}, color.data(), context);
    require(color == std::array<float, 4>{2.f, 3.f, 4.f, 1.f}, "empty enabled point scan must stop at slot 32");
    u8 retained = 0;
    set_runtime_environment_world_space(0xA5, retained);
    require(retained == 0xA5, "environment world-space mode preserves the actual byte");
}
void check_environment_matrices()
{
    Quat rotation;
    make_environment_direction_rotation(Vec3{1.f, 0.f, 0.f}, Vec3{-1.f, 0.f, 0.f}, rotation);
    require(rotation.x == 0.f && rotation.y == 0.f && rotation.z == -1.f && rotation.w == 0.f,
        "opposite X direction fallback axis");
    make_environment_direction_rotation(Vec3{0.f, 0.f, 1.f}, Vec3{0.f, 0.f, -1.f}, rotation);
    require(rotation.x == 0.f && rotation.y == -1.f && rotation.z == 0.f && rotation.w == 0.f,
        "opposite Z direction fallback axis");
    make_environment_direction_rotation(Vec3{1.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, rotation);
    std::array<float, 12> matrix;
    make_display_quaternion_matrix(rotation, matrix.data());
    require(std::fabs(matrix[0]) < .0001f && std::fabs(matrix[4] - 1.f) < .0001f,
        "direction rotation must carry X onto Y");
    const float nan = std::numeric_limits<float>::quiet_NaN();
    make_environment_direction_rotation(Vec3{nan, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, rotation);
    require(std::isnan(rotation.w), "unordered direction dot must enter reciprocal-sqrt branch");
    using namespace entity_detail;
    struct RestoreDisplay
    {
        DisplayView* selected = g_display_view;
        ~RestoreDisplay()
        {
            g_display_view = selected;
        }
    } restore;
    DisplayView display{};
    g_display_view = &display;
    display.forward = Vec3{-1.f, 0.f, 0.f};
    RuntimeSceneLightDirection lighting{};
    std::array<float, 16> primary, secondary;
    // Camera-relative normalization is not called on this real branch.
    build_runtime_environment_matrices(primary.data(), secondary.data(), lighting, {});
    const std::array<float, 16> aligned
    {
        .5f, 0.f, 0.f, .5f,
        0.f, -.5f, 0.f, .5f,
        0.f, 0.f, 0.f, 1.f,
        0.f, 0.f, 0.f, 1.f
    };
    require(secondary == aligned, "aligned reference environment scale and texture bias");
    for (float value : primary)
    {
        require(std::isfinite(value), "primary light environment matrix must be finite");
    }
    display.forward = Vec3{1.f, 0.f, 0.f};
    build_runtime_environment_matrices(primary.data(), secondary.data(), lighting, {});
    const std::array<float, 16> opposite
    {
        -.5f, 0.f, 0.f, .5f,
        0.f, .5f, 0.f, .5f,
        0.f, 0.f, 0.f, 1.f,
        0.f, 0.f, 0.f, 1.f
    };
    require(secondary == opposite, "opposite reference fallback preserves environment orientation");
    RuntimeModelRenderContext renderer;
    RuntimeSceneFog fog;
    fog.enabled = 1;
    fog.start = 10.f;
    fog.end = 110.f;
    std::array<SceneAmbientPoint, 32> points{};
    u32 override_count = 0;
    SceneAmbientContext ambient{{}, 0, override_count, {.7f, .8f, .9f, 1.f}, {.2f, .3f, .4f}, 0, points};
    std::array<u32, 20> records{};
    SceneLightUploadBoundaries boundaries;
    int constants = 0;
    std::array<u32, 4> shader_scene_color{};
    boundaries.set_constant = [&constants, &shader_scene_color](u32 slot, const u32* data)
    {
        ++constants;
        if (slot == 190)
        {
            std::memcpy(shader_scene_color.data(), data, sizeof(shader_scene_color));
        }
    };
    u8 world_space = 1;
    std::array<float, 4> retained{1.f, 1.f, 1.f, 1.f};
    SceneLightUploadContext upload{renderer, world_space, retained, records, boundaries};
    const std::array<float, 4> base{.1f, .2f, .3f, .4f};
    const std::array<float, 4> override_base{.5f, .6f, .7f, .8f};
    std::array<u32, 16> primary_output{}, secondary_output{};
    SceneEnvironmentSetupContext context{upload, fog, ambient, lighting,
        base, override_base, primary_output, secondary_output};
    configure_runtime_scene_environment(Vec3{}, 1, context);
    require(constants == 2 && world_space == 0 && renderer.state.fog_enabled == 1
        && renderer.state.base_color == base
        && renderer.state.ambient_color == std::array<float, 4>{.2f, .3f, .4f, 1.f}
        && secondary_output == std::bit_cast<std::array<u32, 16>>(secondary),
        "scene environment joins fog, lights, base, ambient and camera matrices");
    override_count = 1;
    configure_runtime_scene_environment(Vec3{}, 0, context);
    require(renderer.state.base_color == override_base
        && renderer.state.ambient_color == ambient.override_color,
        "scene override selects both retail base and ambient colors");
    std::array<RuntimeRenderLight, 2> temporary;
    RuntimeRenderLight directional;
    std::array<u8*, 40> local_lights{};
    const std::array<float, 3> scene_color{.1f, .2f, .3f};
    SceneLightConstructionContext construction{context, scene_color, retained,
        .25f, .75f, directional, local_lights, temporary};
    boundaries.transform_normal = [](Vec3& output, const Vec3& input, const float*)
    {
        const Vec3 copy = input;
        output = copy;
    };
    boundaries.normalize = [](Vec3& output, const Vec3& input)
    {
        const Vec3 copy = input;
        const float length = std::sqrt(copy.x * copy.x + copy.y * copy.y + copy.z * copy.z);
        output = {copy.x / length, copy.y / length, copy.z / length};
    };
    boundaries.transform_coord = [](Vec3& output, const Vec3& input, const float*)
    {
        output = input;
    };
    build_runtime_scene_lights(Vec3{}, construction);
    require(renderer.lights.next == &directional && directional.next == nullptr
        && directional.color[0] == .75f && retained == std::array<float, 4>{.1f, .2f, .3f, 1.f}
        && shader_scene_color == std::bit_cast<std::array<u32, 4>>(retained),
        "scene light construction scans empty forty-slot list and selects override intensity");
    // These words are the actual trailing fields of the temporary light views.
    temporary[0].spot = {0x12345678, 0, 0, 0};
    temporary[1].spot = {0x87654321, 0, 0, 0};
    // 0x98-byte light records as 0x000FDF60 registers them: position +0x40, radius +0x70, color +0x78.
    auto light_record = [](const Vec3& position, float radius, u32 color)
    {
        std::array<u8, 0x98> record{};
        std::memcpy(record.data() + 0x40, &position, sizeof(position));
        std::memcpy(record.data() + 0x70, &radius, sizeof(radius));
        std::memcpy(record.data() + 0x78, &color, sizeof(color));
        return record;
    };
    std::array<u8, 0x98> first = light_record({1.f, 2.f, 3.f}, 10.f, 0x80402010);
    std::array<u8, 0x98> second = light_record({4.f, 5.f, 6.f}, 20.f, 0xFF80FFFF);
    std::array<u8, 0x98> third = light_record({7.f, 8.f, 9.f}, 30.f, 0);
    local_lights[0] = first.data();
    local_lights[38] = second.data();
    local_lights[39] = third.data();
    override_count = 0;
    build_runtime_scene_lights(Vec3{}, construction);
    require(directional.color[0] == .25f && temporary[0].type == 0xE
        && temporary[0].position.x == 1.f && temporary[0].range == 10.f
        && temporary[0].range_reciprocal == .2f && temporary[0].reserved == 0
        && temporary[1].position.x == 4.f && temporary[1].range == 20.f,
        "scene light construction skips holes, builds two point lights and excludes third");
    local_lights.fill(nullptr);
    local_lights[39] = third.data();
    build_runtime_scene_lights(Vec3{}, construction);
    require(renderer.lights.next == &directional && directional.next == &temporary[0]
        && temporary[0].next == nullptr && temporary[0].position.x == 7.f,
        "scene light construction retains final local slot and clears stale second light");
    std::array<u8, 0x200> model_bytes{};
    RuntimeRelocationImage image{model_bytes, 0x1000};
    image.set_word(0x1000, 8, 0x18);
    image.set_word(0x1000, 0x10, 0x1040);
    image.set_word(0x1040, 0x24, 0x1080);
    image.set_word(0x1070, 0, 0xFEED);
    RuntimeModelInstance instance{};
    RuntimeShaderMatrixBoundaries matrix_boundaries;
    SceneModelDrawContext model_context{image, construction, matrix_boundaries};
    std::array<RuntimeModelRenderFrame, 1> render_frames{};
    renderer.frames = render_frames;
    Actor actor;
    ActorAnimationState actor_animation{};
    ModelAnimationState model_animation{};
    ActorAttachmentList attachments{};
    ActorAttachmentNode pending{};
    attachments.pending = &pending; // Actor +0x1B4.
    entity_detail::ListHead anchors{};
    ActorFrameState frame{actor, actor_animation, model_animation, attachments, pending, nullptr, anchors};
    RuntimeActorMatrixState actor_matrices;
    u32 override_second = 0x11223344;
    ActorDrawModelView model_view{instance, 0x1000, instance.state_1c0, instance.state_1c4, override_second};
    ActorDrawState draw_state{frame, &model_view, 1, 0, 0x12345678, 0x87654321, {}, {}, nullptr, actor_matrices};
    std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback> attachment_callbacks;
    struct CountDrawAttachment : ActorFrameAttachment
    {
        int calls = 0;
    } attachment{};
    attachment_callbacks[&attachment] = [](ActorFrameAttachment& value, Actor&, i32)
    {
        ++static_cast<CountDrawAttachment&>(value).calls;
    };
    frame.updates = &attachment;
    ActorFrameDrawContext attachment_context{attachment_callbacks};
    int draws = 0;
    std::vector<i32> order;
    renderer.state.draw = [&](u32 node, u32 first_parameter, u32 draw_kind, void* storage)
    {
        require(node == 0x1080 && first_parameter == 0 && draw_kind == 0
            && storage == &instance.tree_storage && renderer.state.node_descriptor == 0xFEED
            && render_frames[0].world[12] == 11.f && render_frames[0].world[13] == 12.f
            && render_frames[0].world[14] == 13.f,
            "actor model wrapper preserves node, draw kind, tree storage and callback-updated world");
        ++draws;
        order.push_back(2);
    };
    require(draw_runtime_actor(draw_state, 0, attachment_context, model_context) == 0
        && draws == 0 && attachment.calls == 0,
        "hidden actor bypasses rendering");
    draw_state.hidden = 0;
    draw_state.model = nullptr;
    require(draw_runtime_actor(draw_state, 0, attachment_context, model_context) == 0
        && attachment.calls == 1,
        "actor without model returns zero");
    draw_state.model = &model_view;
    image.set_word(0x1000, 8, 0);
    require(draw_runtime_actor(draw_state, 0, attachment_context, model_context) == 0
        && draw_runtime_actor(draw_state, 2, attachment_context, model_context) == 0
        && draws == 0 && attachment.calls == 3,
        "actor draw respects both model header visibility flags");
    image.set_word(0x1000, 8, 0x18);
    draw_state.before = [&](Actor& owner, i32 kind, void* payload)
    {
        require(&owner == &actor && kind == 0 && payload == &draw_state
            && instance.state_1c0 == 1 && instance.state_1c4 == 0x12345678
            && override_second == 0x87654321,
            "actor pre-draw receives payload after material override writes");
        actor.xform[3] = 11.f;
        actor.xform[7] = 12.f;
        actor.xform[11] = 13.f;
        order.push_back(1);
    };
    draw_state.after = [&](Actor& owner, i32 kind, void* payload)
    {
        require(&owner == &actor && kind == 0 && payload == &draw_state && renderer.rendering == 0,
            "actor post-draw follows native model rendering cleanup");
        order.push_back(3);
    };
    draw_state.payload = &draw_state;
    require(draw_runtime_actor(draw_state, 0, attachment_context, model_context) == 1
        && draws == 1 && order == std::vector<i32>{1, 2, 3}
        && (renderer.lighting_flags & 0x80000000) != 0,
        "lit actor drawing joins callbacks, real light construction and model tree draw");
    draw_state.before = {};
    draw_state.after = {};
    draw_state.material_second = 0xFF000000;
    draw_state.matrices_ready = 1;
    image.set_word(0x1000, 8, 0x1A);
    require(draw_runtime_actor(draw_state, 0, attachment_context, model_context) == 1
        && instance.state_1c0 == 0 && instance.state_1c4 == 0x12345678
        && override_second == 0x87654321 && renderer.lights.next == nullptr
        && (renderer.lighting_flags & 0x80000000) == 0
        && retained == std::array<float, 4>{1.f, 1.f, 1.f, 1.f},
        "unlit actor clears lighting and white color while retaining disabled material fields");
    image.set_word(0x1080, 8, 0x1090);
    image.set_word(0x1090, 0, 0x10B0);
    image.set_half(0x10B0, 6, 1);
    image.set_word(0x10B0, 0x10, 0x10A0);
    image.set_word(0x10A0, 0x50, 0x10FC);
    image.set_word(0x10FC, 0, 0xF00D);
    std::array<ModelTreeStackEntry, 16> stack{};
    i32 stack_index = -1;
    std::array<RuntimeShaderMatrix, 1> combined{};
    const RuntimeShaderMatrix camera_matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<const RuntimeShaderMatrix*, 1> camera_matrices{&camera_matrix};
    u32 retained_204 = 123;
    u16 mask = 0xFFFF;
    u32 fog_pixel = 123;
    const u32 cull_mode = 2;
    int node_calls = 0;
    std::unordered_map<u32, std::function<void(u32, void*)>> node_callbacks;
    node_callbacks[0xF00D] = [&](u32 node, void* storage)
    {
        require(node == 0x10B0 && storage == &instance.tree_storage,
            "installed retail tree callback preserves node and runtime instance storage");
        ++node_calls;
    };
    ModelTreeDispatchContext dispatcher{renderer, image, stack, stack_index, combined, camera_matrices,
        retained_204, mask, fog_pixel, cull_mode, node_callbacks};
    int matrix_multiplies = 0;
    dispatcher.multiply = [&](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& left,
        const RuntimeShaderMatrix& right)
    {
        multiply_runtime_matrix(output, left, right);
        ++matrix_multiplies;
    };
    dispatcher.scaling = [](RuntimeShaderMatrix& output, float x, float y, float z)
    {
        output = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1};
    };
    dispatcher.set_cull_mode = [](u32 mode)
    {
        require(mode == 2, "tree callback uploads retained cull mode");
    };
    dispatcher.set_fog_color = [&](u32 color)
    {
        require(color == renderer.state.fog_color, "tree callback uploads native fog color");
    };
    install_runtime_model_tree_dispatch(dispatcher);
    require(draw_runtime_actor(draw_state, 1, attachment_context, model_context) == 1
        && node_calls == 1 && matrix_multiplies == 2 && stack_index == -1
        && retained_204 == 0 && mask == 1 && fog_pixel == 0
        && combined[0] == render_frames[0].world,
        "actor draw reaches installed retail tree traversal and unwinds sentinel stack");
    image.set_half(0x10B0, 6, 0);
    image.set_word(0x10B0, 0x18, 0x10C0);
    image.set_half(0x10C0, 6, 1);
    image.set_word(0x10C0, 0x10, 0x10A0);
    node_callbacks[0xF00D] = [&](u32 node, void* storage)
    {
        require(node == 0x10C0 && storage == &instance.tree_storage,
            "masked tree root traverses actual sibling and forwards storage");
        ++node_calls;
    };
    require(draw_runtime_actor(draw_state, 1, attachment_context, model_context) == 1
        && node_calls == 2 && stack_index == -1,
        "installed dispatch skips masked root and dispatches its sibling");
    image.set_word(0x10B0, 0x14, 0x1140);
    image.set_word(0x10B0, 0x18, 0x1180);
    image.set_half(0x1140, 6, 1);
    image.set_word(0x1140, 0x10, 0x10A0);
    image.set_half(0x1180, 6, 1);
    image.set_word(0x1180, 0x10, 0x10A0);
    std::vector<u32> group_nodes;
    install_runtime_model_group_dispatch(dispatcher, node_callbacks);
    node_callbacks[0xF00D] = [&](u32 node, void* storage)
    {
        require(storage == &instance.tree_storage, "group descendants preserve instance storage");
        group_nodes.push_back(node);
    };
    push_runtime_model_node(0, 0, dispatcher);
    node_callbacks.at(0x00158960)(0x10B0, &instance.tree_storage);
    dispatch_runtime_model_nodes(1, &instance.tree_storage, dispatcher);
    require(group_nodes == std::vector<u32>{0x1140, 0x1180} && stack_index == -1,
        "registered group callback queues sibling then child for depth-first traversal");
    std::array<u8, 64> flag_storage{};
    flag_storage[32] = 0xC0;
    flag_storage[31] = 0x0C;
    const std::unordered_map<RuntimeModelInstance*, const u8*> flag_origins
    {
        {&instance, flag_storage.data() + 32}
    };
    std::vector<std::array<u32, 3>> indexed;
    ModelPushBufferDrawContext push_context{dispatcher, flag_origins,
        [&](u32 primitive, u32 count, u32 indices)
        {
            indexed.push_back({primitive, count, indices});
        }};
    image.set_byte(0x1140, 0x3D, 3);
    image.set_byte(0x1140, 0x3E, 3);
    require(runtime_push_buffer_visible(0x1140, push_context), "push visibility selects packed two-bit flag");
    image.set_byte(0x1140, 0x3E, 2);
    require(!runtime_push_buffer_visible(0x1140, push_context), "push visibility rejects mismatched flag");
    image.set_byte(0x1140, 0x3D, 0xFB); // -5: offset -1, shift 30, result zero.
    image.set_byte(0x1140, 0x3E, 0);
    require(runtime_push_buffer_visible(0x1140, push_context), "negative visibility index preserves signed offset and masked shift");
    image.set_byte(0x1140, 0x3D, 0xFF);
    image.set_byte(0x1140, 0x3E, 0xFF);
    image.set_byte(0x1140, 0x3C, 1);
    image.set_word(0x1140, 0x20, 2);
    image.set_word(0x1140, 0x30, 0x11D0);
    image.set_word(0x1140, 0x34, 0x11D8);
    image.set_word(0x1140, 0x38, 0x11E0);
    image.set_word(0x11D0, 0, 0x12340000);
    image.set_word(0x11D0, 4, 0x56780000);
    image.set_word(0x11D8, 0, 4);
    image.set_word(0x11D8, 4, 5);
    image.set_word(0x11E0, 0, 6);
    image.set_word(0x11E0, 4, 9);
    std::vector<u32> culls;
    dispatcher.set_cull_mode = [&](u32 mode)
    {
        culls.push_back(mode);
    };
    install_runtime_push_buffer_dispatch(push_context, node_callbacks);
    node_callbacks.at(0x001582A0)(0x1140, &instance.tree_storage);
    require(indexed == std::vector<std::array<u32, 3>>{{4, 6, 0x12340000}, {5, 9, 0x56780000}}
        && culls == std::vector<u32>{0, 2},
        "installed push handler issues retail indexed arguments and restores culling");
    indexed.clear();
    culls.clear();
    image.set_byte(0x1140, 0x3D, 3);
    image.set_byte(0x1140, 0x3E, 2);
    image.set_word(0x1140, 0x18, 0x1180);
    push_runtime_model_node(0, 0, dispatcher);
    node_callbacks.at(0x001582A0)(0x1140, &instance.tree_storage);
    dispatch_runtime_model_nodes(1, &instance.tree_storage, dispatcher);
    require(indexed.empty() && culls.empty() && group_nodes.back() == 0x1180 && stack_index == -1,
        "invisible push node still traverses sibling without D3D calls");
    std::array<u32, 16> streams;
    streams.fill(0xDEADBEEF);
    const u32 shader = 0x11223344;
    u32 depth_function = 0xFF;
    u32 blend_enabled = 0xFF;
    std::vector<std::array<u32, 3>> state_calls;
    ModelRenderNodeContext render_nodes{dispatcher, streams, shader, depth_function, blend_enabled,
        [&](u32 value)
        {
            state_calls.push_back({1, value, 0});
        },
        [&](u32 command, u32 value)
        {
            state_calls.push_back({2, command, value});
        },
        [&](u32 value)
        {
            require(depth_function == 0x203, "pixel handler retains depth function before enabling depth");
            state_calls.push_back({3, value, 0});
        }};
    install_runtime_render_node_dispatch(render_nodes, node_callbacks);
    image.set_word(0x1140, 0x20, 0x90000000);
    image.set_word(0x1140, 0x24, 2);
    image.set_word(0x1140, 0x18, 0);
    node_callbacks.at(0x00158480)(0x1140, &instance.tree_storage);
    require(streams[0] == 0x90000000 && streams[1] == 0x90000018 && streams[2] == 0xDEADBEEF,
        "vertex stream handler uses retail descriptor stride and preserves unused slots");
    image.set_word(0x1140, 0x24, 0);
    node_callbacks.at(0x00158480)(0x1140, &instance.tree_storage);
    require(streams[1] == 0x90000018, "zero stream count preserves retained descriptors");
    node_callbacks.at(0x001586D0)(0x1140, &instance.tree_storage);
    require(state_calls == std::vector<std::array<u32, 3>>
        {{1, shader, 0}, {2, 0x40354, 0x203}, {3, 1, 0}, {2, 0x40304, 0}}
        && depth_function == 0x203 && blend_enabled == 0,
        "pixel node uploads original shader and depth/blend commands in retail order");
    image.set_word(0x1140, 0x18, 0x1180);
    push_runtime_model_node(0, 0, dispatcher);
    node_callbacks.at(0x00158090)(0x1140, &instance.tree_storage);
    dispatch_runtime_model_nodes(1, &instance.tree_storage, dispatcher);
    require(stack_index == -1 && group_nodes.back() == 0x1180,
        "registered visibility node continues genuine sibling traversal");
}
}
void check_scene_render()
{
    check_shader_compiler_leaves();
    {
        ShaderScheduleCostState state{};
        u32 reason = 0;
        require(runtime_shader_schedule_tail_cost(state, reason) == 18.L && reason == 0x4B,
            "disabled shader scheduling preserves fixed native tail penalty");
        state.enabled = 1;
        require(runtime_shader_schedule_tail_cost(state, reason) == static_cast<long double>(2.2f)
            && reason == 0x4C, "shader scheduling rounds minimum shortfall to float");
        for (u32 index = 0; index < 5; ++index)
        {
            state.pending[index] = 0xFF;
        }
        state.issued = 2.f;
        require(runtime_shader_schedule_tail_cost(state, reason) == 3.L && reason == 0x4A,
            "pending shader slots override smaller minimum shortfall");
        state.issued = 7.f;
        require(runtime_shader_schedule_tail_cost(state, reason) == 0.L && reason == 0x20,
            "sufficient shader schedule retains no-penalty reason");
        state.issued = std::numeric_limits<float>::quiet_NaN();
        require(runtime_shader_schedule_tail_cost(state, reason) == 0.L && reason == 0x20,
            "unordered scheduling comparisons preserve zero cost and reason");
    }
    {
        u16 signature = 0x2078;
        require(runtime_vertex_shader_code_type(&signature) == 1, "ordinary Xbox shader signature");
        signature = 0x7378;
        require(runtime_vertex_shader_code_type(&signature) == 3, "state Xbox shader signature");
        signature = 0x7778;
        require(runtime_vertex_shader_code_type(&signature) == 2, "read-write Xbox shader signature");
        signature = 0;
        require(runtime_vertex_shader_code_type(&signature) == 0xFF, "unknown shader signature retains native invalid byte");
        const std::array<u32, 4> packed{0xDEADBEEF, 0x12345678, 0x9ABCDEF0, 0xFEDCBA98};
        std::array<u32, 3> decoded{};
        unpack_runtime_vertex_instruction(packed.data(), decoded.data());
        require(decoded == std::array<u32, 3>{0x3EDCBA98, 0x2AF37BC3, 0x23456789},
            "shader unpack preserves bit fields across word boundaries");
        auto aliased = packed;
        unpack_runtime_vertex_instruction(aliased.data(), aliased.data());
        require(aliased[0] == 0x3EDCBA98 && aliased[1] == 3 && aliased[2] == 0,
            "shader unpack preserves output-clear ordering with aliased input");
    }
    {
        RuntimeModelRenderContext renderer;
        std::array<RuntimeRenderLight, 239> buckets{};
        RuntimeRenderLight collision{}, matching{}, zero{};
        std::unordered_map<RuntimeRenderLight*, u32> programs
        {
            {&collision, 0xDEADBEEF}, {&matching, 0x12345678}, {&zero, 0x87654321}
        };
        RuntimeShaderFragmentContext context{renderer, buckets, programs};
        renderer.lighting_flags = 0x8000000F;
        u32 selected = 0xCAFEBABE;
        find_runtime_shader_fragment(selected, 1, context);
        require(selected == 0xCAFEBABE, "missing shader bucket retains fragment output");
        collision.type = renderer.lighting_flags - 239;
        matching.type = renderer.lighting_flags;
        RuntimeRenderLight& bucket = buckets[renderer.lighting_flags % 239];
        append_runtime_light(bucket, collision);
        find_runtime_shader_fragment(selected, 1, context);
        require(selected == 0xCAFEBABE, "shader hash collision exhaustion retains output");
        append_runtime_light(bucket, matching);
        find_runtime_shader_fragment(selected, 0x80, context);
        require(selected == 0x12345678 && runtime_scene_lighting_flags(renderer) == 0x8000000F,
            "shader fragment selects complete lighting key after collision");
        zero.type = 0;
        append_runtime_light(buckets[0], zero);
        find_runtime_shader_fragment(selected, 0, context);
        require(selected == 0x87654321 && renderer.lighting_flags == 0x8000000F,
            "unlit shader fragment uses zero key without changing global lighting flags");
    }
    {
        RuntimeModelRenderContext renderer;
        u8 world_space = 0xCC;
        std::array<u32, 20> records;
        records.fill(0xAABBCCDD);
        std::vector<std::pair<u32, std::array<u32, 4>>> uploads;
        SceneLightUploadBoundaries boundaries;
        boundaries.set_constant = [&uploads](u32 slot, const u32* data)
        {
            std::array<u32, 4> copy;
            std::memcpy(copy.data(), data, sizeof(copy));
            uploads.emplace_back(slot, copy);
        };
        const std::array<float, 4> retained_color{1.f, 1.f, 1.f, 1.f};
        SceneLightUploadContext context{renderer, world_space, retained_color, records, boundaries};
        upload_runtime_scene_lights(nullptr, context);
        require(uploads.empty() && world_space == 0xCC,
            "null matrix with local lights bypasses all shader and state writes");
        renderer.lights_in_world_space = 0xA5;
        upload_runtime_scene_lights(nullptr, context);
        require(uploads.size() == 2 && uploads[0].first == 191 && uploads[1].first == 190
            && uploads[0].second == std::array<u32, 4>{0, 0, 0, 0x3F800000}
            && world_space == 0xA5 && renderer.state.directional_light_count == 0,
            "world-space empty list writes shader constants and clears directional count");
        uploads.clear();
        RuntimeRenderLight directional;
        directional.type = 1;
        directional.color = {.5f, 1.5f, -.25f, 1.f};
        directional.direction = {1.f, 2.f, 3.f};
        append_runtime_light(renderer.lights, directional);
        RuntimeRenderLight ignored;
        ignored.type = 0;
        append_runtime_light(renderer.lights, ignored);
        RuntimeRenderLight point;
        point.type = 2 | 4;
        point.position = {4.f, 5.f, 6.f};
        point.range = 7.f;
        point.range_reciprocal = 8.f;
        point.reserved = 0x12345678;
        point.spot = {0xCAFEBABE, 0, 0, 0};
        append_runtime_light(renderer.lights, point);
        RuntimeRenderLight cone;
        cone.type = 3;
        cone.direction = {9.f, 10.f, 11.f};
        cone.position = {12.f, 13.f, 14.f};
        cone.range = 15.f;
        cone.spot = {1, 2, 3, 4};
        append_runtime_light(renderer.lights, cone);
        upload_runtime_scene_lights(nullptr, context);
        require(uploads.size() == 11 && uploads[2].first == 189 && uploads[10].first == 181,
            "shader slots descend with directional, ranged point and spot payloads");
        require(uploads[3].second == std::array<u32, 4>{0xBF800000, 0xC0000000, 0xC0400000, 0}
            && uploads[5].second[3] == std::bit_cast<u32>(7.f)
            && uploads[6].second[2] == 0x12345678 && uploads[6].second[3] == 0xCAFEBABE
            && uploads[10].second == cone.spot,
            "shader uploads preserve negation, adjoining fields and spot raw words");
        require(renderer.state.directional_light_count == 1
            && renderer.state.directional_lights[0] == 0xFFC17E7F
            && renderer.state.directional_lights[1] == std::bit_cast<u32>(1.f)
            && renderer.state.directional_lights[4] == 0xAABB0101,
            "directional records retain upper flag bytes and wrapping channel conversion");
        uploads.clear();
        renderer.lights_in_world_space = 0;
        int transforms = 0;
        boundaries.transform_normal = [&transforms](Vec3& output, const Vec3& input, const float*)
        {
            const Vec3 saved = input;
            output = saved;
            ++transforms;
        };
        boundaries.normalize = [&transforms](Vec3& output, const Vec3& input)
        {
            const Vec3 saved = input;
            const float length = std::sqrt(saved.x * saved.x + saved.y * saved.y + saved.z * saved.z);
            output = {saved.x / length, saved.y / length, saved.z / length};
            ++transforms;
        };
        boundaries.transform_coord = [&transforms](Vec3& output, const Vec3& input, const float*)
        {
            output = {input.x + 100.f, input.y, input.z};
            ++transforms;
        };
        std::array<float, 16> matrix{};
        upload_runtime_scene_lights(matrix.data(), context);
        require(transforms == 6 && uploads[5].second[0] == std::bit_cast<u32>(104.f)
            && uploads[5].second[3] == 0 && world_space == 0,
            "local lighting calls transforms and normalization and keeps scratch fourth lane zero");
        renderer.lights.next = nullptr;
        std::array<RuntimeRenderLight, 5> five;
        for (RuntimeRenderLight& light : five)
        {
            light.type = 1;
            append_runtime_light(renderer.lights, light);
        }
        renderer.lights_in_world_space = 1;
        upload_runtime_scene_lights(nullptr, context);
        require(renderer.state.directional_light_count == 4,
            "directional record copy caps at four while shader list remains complete");
    }
    gbtg::RuntimeModelRenderContext light_renderer;
    std::array<gbtg::u32, 20> records;
    for (std::size_t index = 0; index != records.size(); ++index)
    {
        records[index] = 0x7FC00000U + static_cast<gbtg::u32>(index);
    }
    gbtg::copy_runtime_directional_lights(records.data(), 4, light_renderer);
    require(light_renderer.state.directional_light_count == 4
        && light_renderer.state.directional_lights == records,
        "directional light records preserve all raw bits");
    gbtg::copy_runtime_directional_lights(nullptr, 0, light_renderer);
    require(light_renderer.state.directional_light_count == 0
        && light_renderer.state.directional_lights == records,
        "zero lights update count without touching retained records");
    records[0] = 0x12345678U;
    gbtg::copy_runtime_directional_lights(records.data(), 1, light_renderer);
    require(light_renderer.state.directional_lights[0] == records[0]
        && light_renderer.state.directional_lights[5] == records[5],
        "partial light upload retains records beyond count");
    check_color_helpers();
    check_boot_shader_boundary();
    check_vertex_gpu_translation();
    check_boot_pixel_shaders();
    check_matrix_helpers();
    check_attachment_draw_walk();
    check_ambient_sampling();
    check_environment_matrices();
}
