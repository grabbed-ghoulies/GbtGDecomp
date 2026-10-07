#include "gbtg/model.h"
#include "gbtg/cutscene.h"

#include <cstring>
#include <iostream>

namespace
{

void require(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}

void word(std::vector<gbtg::u8>& bytes, size_t offset, gbtg::u32 value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

struct Fixture
{
    std::vector<gbtg::u8> cpu = std::vector<gbtg::u8>(2000);
    std::vector<gbtg::u8> gpu = std::vector<gbtg::u8>(48);

    explicit Fixture(gbtg::u32 mode)
    {
        auto node = [&](size_t rel, gbtg::u32 type, gbtg::u32 child, gbtg::u32 data, gbtg::u32 count)
        {
            word(cpu, rel + 64 + 4, type);
            word(cpu, rel + 64 + 20, child);
            word(cpu, rel + 64 + 32, data);
            word(cpu, rel + 64 + 36, count);
        };
        node(52, 19, 128, 1000, 2);
        node(128, 17, 176, 1100, 0);
        node(176, 21, 320, 1500, 1);
        node(320, 20, 0, 1, 0);
        word(cpu, 320 + 64, 1600);
        word(cpu, 1000 + 64, 12 | (9 << 8));
        word(cpu, 1000 + 64 + 20, 36);
        word(cpu, 1024 + 64, 4 | (11 << 8));
        word(cpu, 1024 + 64 + 16, 36);
        word(cpu, 1024 + 64 + 20, 12);
        word(cpu, 1100 + 64, mode);
        word(cpu, 1100 + 64 + 4, 1200);
        word(cpu, 1100 + 64 + 8, 240);
        word(cpu, 176 + 64 + 80, 1530);
        word(cpu, 176 + 64 + 84, 1560);
        word(cpu, 1500 + 64, 1);
        word(cpu, 1530 + 64, 0x20000000);
        word(cpu, 1530 + 64 + 4, 0x40000003);
        word(cpu, 1530 + 64 + 8, 0xffffffff);
        word(cpu, 1560 + 64, 0x00012078);
        word(cpu, 1560 + 64 + 8, 0x0020061b); // MOV v3.xyzw
        word(cpu, 1560 + 64 + 12, 0x08000000);
        word(cpu, 1560 + 64 + 16, 0x0000f819); // oD0.xyzw, output register bank
        word(cpu, 1600 + 64 + 16, 3);
        word(cpu, 1600 + 64 + 20, 1630);
        word(cpu, 1600 + 64 + 24, 5);
        word(cpu, 1630 + 64, 0x00010000);
        word(cpu, 1630 + 64 + 4, 2);
        const float positions[9] = {0, 0, 0, 0.001f, 0, 0, 0, 0.001f, 0};
        std::memcpy(gpu.data(), positions, sizeof(positions));
        word(gpu, 36, 0x7f123456);
        word(gpu, 40, 0xffabcdef);
        word(gpu, 44, 0xff001122);
    }

    gbtg::Mesh parse() const
    {
        gbtg::BundleEntry entry;
        entry.type = gbtg::ResourceType::Model;
        entry.cpu = cpu;
        entry.gpu = gpu;
        return gbtg::parse_model(entry);
    }
};

}

int main()
{
    try
    {
        gbtg::u32 event_phase = 99;
        require(!gbtg::cutscene_event_phase(0.03f, 0.f, 0.03f, 0.f, event_phase),
                "cutscene cue fired at equality instead of after its authored time");
        require(gbtg::cutscene_event_phase(0.03f, 0.f, 0.04f, 0.02f, event_phase) && event_phase == 0,
                "cutscene cue missed a frame crossing its authored time");
        require(!gbtg::cutscene_event_phase(0.03f, 0.f, 0.05f, 0.04f, event_phase),
                "one-shot cutscene cue retriggered after its time crossing");
        require(gbtg::cutscene_event_phase(0.f, 4.f, 2.f, 1.f, event_phase) && event_phase == 2,
                "active sound window was mistaken for a start or stop");
        require(!gbtg::cutscene_event_phase(0.f, 4.f, 4.f, 3.f, event_phase),
                "sound window stopped at equality instead of after its end");
        require(gbtg::cutscene_event_phase(0.f, 4.f, 4.1f, 3.9f, event_phase) && event_phase == 1,
                "sound window did not stop when its end was crossed");
        for (gbtg::u32 mode = 0; mode != 4; ++mode)
        {
            const auto mesh = Fixture(mode).parse();
            require(mesh.indices.size() == 3, "small valid triangle was discarded");
            require(mesh.diffuse.size() == 3 && mesh.diffuse[0] == 0x7f123456, "D3DCOLOR lost channels or alpha");
            require(mesh.submeshes.size() == 1, "declared draw section missing");
            require(mesh.submeshes[0].render_mode == int(mode), "material state category changed");
            require(mesh.submeshes[0].colour_source[0] == 0, "declared diffuse MOV not bound to colour descriptor");
        }
        // Draw metadata belongs to the node, not its separate index-array payload.
        // These alternate surfaces must follow changes to the live instance state.
        Fixture gated(0);
        gated.cpu[320 + 64 + 0x3D] = 5;
        gated.cpu[320 + 64 + 0x3E] = 2;
        auto gated_mesh = gated.parse();
        const auto& gated_sub = gated_mesh.submeshes[0];
        require(gated_sub.node_state_slot == 5 && gated_sub.node_state_value == 2,
                "push-buffer visibility metadata read from index payload instead of node");
        require(!gbtg::model_submesh_visible(gated_mesh, gated_sub.node_state_slot, gated_sub.node_state_value),
                "alternate surface drawn with default instance state");
        gated_mesh.node_flags[1] = 0xe4; // slots 4..7: 0,1,2,3.
        require(!gbtg::model_submesh_visible(gated_mesh, gated_sub.node_state_slot, gated_sub.node_state_value),
                "neighboring two-bit state selected the alternate surface");
        gated_mesh.node_flags[1] = 0xe8; // slot 5 changes from 1 to 2.
        require(gbtg::model_submesh_visible(gated_mesh, gated_sub.node_state_slot, gated_sub.node_state_value),
                "live instance state did not select its alternate surface");
        gated.cpu[320 + 64 + 0x3D] = 255;
        gated.cpu[320 + 64 + 0x3E] = 255;
        const auto unconditional = gated.parse();
        require(gbtg::model_submesh_visible(unconditional, unconditional.submeshes[0].node_state_slot,
                    unconditional.submeshes[0].node_state_value),
                "unconditional push-buffer sentinel was treated as an instance state");
        Fixture invalid(0);
        Fixture rigid(0);
        rigid.gpu.resize(72);
        word(rigid.cpu, 52 + 64 + 36, 4);
        word(rigid.cpu, 1048 + 64, 4); // palette, FLOAT1
        word(rigid.cpu, 1048 + 64 + 16, 48);
        word(rigid.cpu, 1048 + 64 + 20, 12);
        word(rigid.cpu, 1072 + 64, 4 | (8 << 8)); // weights, FLOAT1
        word(rigid.cpu, 1072 + 64 + 16, 60);
        word(rigid.cpu, 1072 + 64 + 20, 12);
        const float slots[3] = {24.f, 27.f, 30.f};
        const float weights[3] = {1.f, 1.f, 1.f};
        std::memcpy(rigid.gpu.data() + 48, slots, sizeof(slots));
        std::memcpy(rigid.gpu.data() + 60, weights, sizeof(weights));
        const auto rigid_mesh = rigid.parse();
        require(rigid_mesh.bones.size() == 3 && rigid_mesh.bones[2] == 2, "FLOAT1 palette not decoded");
        require(rigid_mesh.bone2[2] == 0 && rigid_mesh.weight0[2] == 1.f && rigid_mesh.weight1[2] == 0.f,
                "FLOAT1 declaration defaults for second skin influence changed");
        require(!rigid_mesh.submeshes[0].uses_skin_palette, "palette storage alone enabled skinning for a static shader");
        Fixture weighted(0);
        weighted.gpu.resize(144);
        word(weighted.cpu, 52 + 64 + 36, 4);
        word(weighted.cpu, 1048 + 64, 16);
        word(weighted.cpu, 1048 + 64 + 16, 48);
        word(weighted.cpu, 1048 + 64 + 20, 48);
        word(weighted.cpu, 1072 + 64, 16 | (8 << 8));
        word(weighted.cpu, 1072 + 64 + 16, 96);
        word(weighted.cpu, 1072 + 64 + 20, 48);
        word(weighted.cpu, 1500 + 64, 3); // v3 reads the weight descriptor
        const float four_slots[4] = {24.f, 27.f, 30.f, 33.f};
        const float four_weights[4] = {0.1f, 0.2f, 0.3f, 0.4f};
        for (size_t vertex = 0; vertex < 3; ++vertex)
        {
            std::memcpy(weighted.gpu.data() + 48 + vertex * 16, four_slots, sizeof(four_slots));
            std::memcpy(weighted.gpu.data() + 96 + vertex * 16, four_weights, sizeof(four_weights));
        }
        const auto weighted_mesh = weighted.parse();
        require(weighted_mesh.bone3[1] == 2 && weighted_mesh.bone4[1] == 3 &&
                weighted_mesh.weight2[1] == 0.3f && weighted_mesh.weight3[1] == 0.4f,
                "FLOAT4 skin data lost the third/fourth influences");
        require(weighted_mesh.submeshes[0].skin_weight_mask == 15, "vertex-program weight-component selection lost lanes");
        Fixture fourth_uv(0);
        fourth_uv.gpu.resize(72);
        word(fourth_uv.cpu, 52 + 64 + 36, 3);
        word(fourth_uv.cpu, 1048 + 64, 8 | (16 << 8));
        word(fourth_uv.cpu, 1048 + 64 + 16, 48);
        word(fourth_uv.cpu, 1048 + 64 + 20, 24);
        word(fourth_uv.cpu, 1500 + 64, 2);
        word(fourth_uv.cpu, 1560 + 64 + 16, 0x0000f849); // MOV v3 to oT0
        word(fourth_uv.cpu, 1200 + 64 + 54 * 4, 1); // stage 0 PROJECT2D
        const float coords[6] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
        std::memcpy(fourth_uv.gpu.data() + 48, coords, sizeof(coords));
        const auto fourth_uv_mesh = fourth_uv.parse();
        require(fourth_uv_mesh.uv3[2].v == 0.6f && fourth_uv_mesh.submeshes[0].stage_uv[0] == 3 &&
                fourth_uv_mesh.submeshes[0].direct_uv, "fourth UV descriptor was not bound to the declared texture output");
        // Retail corridor: MAC DP3 and ILU MOV v3 share the output port.
        word(fourth_uv.cpu, 1560 + 64 + 8, (5u << 21) | (1u << 25) | (3u << 9));
        word(fourth_uv.cpu, 1560 + 64 + 12, 0x1bu << 2);
        word(fourth_uv.cpu, 1560 + 64 + 16, 0x2000f84d);
        require(fourth_uv.parse().submeshes[0].direct_uv && fourth_uv.parse().submeshes[0].stage_uv[0] == 3,
                "paired ILU MOV texture output was mistaken for the MAC result");
        word(fourth_uv.cpu, 1560 + 64 + 12, (0x1bu << 2) | 0x400);
        require(!fourth_uv.parse().submeshes[0].direct_uv, "negated ILU UV output was accepted as a direct binding");
        Fixture constant_write(0);
        word(constant_write.cpu, 1560 + 64 + 16, 0x0000f019);
        require(constant_write.parse().submeshes[0].colour_source[0] == 255,
                "constant-register write was interpreted as a colour output");
        word(invalid.cpu, 1630 + 64 + 4, 7);
        bool rejected = false;
        try
        {
            invalid.parse();
        }
        catch (const gbtg::Error&)
        {
            rejected = true;
        }
        require(rejected, "invalid index was silently discarded");
        std::cout << "model render inputs passed\n";
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
