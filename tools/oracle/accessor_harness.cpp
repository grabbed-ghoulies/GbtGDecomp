#include "shader_validation_accessors.h"
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
using namespace gbtg;
int main()
{
    std::string fn;
    while (std::cin >> fn)
    {
        if (fn == "mask")
        {
            u32 a, b, c, d;
            std::cin >> a >> b >> c >> d;
            ShaderValidationChannelMask m{0x11111111, {9, 9, 9, 9}};
            auto *r = construct_shader_validation_channel_mask(m, (u8)a, (u8)b, (u8)c, (u8)d);
            std::printf("%08X %02X%02X%02X%02X %d\n", m.vtable_address, m.channels[0], m.channels[1], m.channels[2], m.channels[3],
                        r == &m);
        }
        else if (fn == "order")
        {
            u32 a, b, c, d;
            std::cin >> a >> b >> c >> d;
            ShaderValidationChannelOrder o{};
            auto *r = construct_shader_validation_channel_order(o, a, b, c, d);
            std::printf("%08X %08X %08X %08X %08X %d\n", o.vtable_address, o.channels[0], o.channels[1], o.channels[2], o.channels[3],
                        r == &o);
        }
        else if (fn == "ord")
        {
            ShaderValidationChannelOrder o{0x203794, {}};
            for (auto &c : o.channels)
            {
                std::cin >> c;
            }
            u32 v, idx;
            std::cin >> v >> idx;
            std::printf("%d %u %u\n", shader_validation_channel_order_contains(o, v) ? 1 : 0, shader_validation_channel_order_keeps(o, idx),
                        find_shader_validation_channel_order_index(o, v));
        }
        else if (fn == "ins")
        {
            ShaderValidationInstruction x{};
            u32 flag;
            std::cin >> flag;
            x.output_flag = (u8)flag;
            for (auto *m : {&x.output_mask, &x.vector_mask, &x.scalar_mask})
            {
                for (auto &c : m->channels)
                {
                    u32 v;
                    std::cin >> v;
                    c = (u8)v;
                }
            }
            std::cin >> x.vector_opcode >> x.scalar_opcode;
            ShaderValidationChannelMask r[3];
            copy_shader_validation_output_mask(x, r[0]);
            copy_shader_validation_vector_mask(x, r[1]);
            auto *p = copy_shader_validation_scalar_mask(x, r[2]);
            for (auto &m : r)
            {
                std::printf("%08X %02X%02X%02X%02X ", m.vtable_address, m.channels[0], m.channels[1], m.channels[2], m.channels[3]);
            }
            std::printf("%d |", p == &r[2]);
            for (u32 i = 0; i < 4; ++i)
            {
                std::printf(" %u%u", shader_validation_vector_writes_channel(x, i), shader_validation_scalar_writes_channel(x, i));
            }
            std::printf(" | %u %u %u\n", shader_validation_vector_opcode_is_add_or_mad(x),
                        shader_validation_vector_opcode_is_add_through_dp4(x), shader_validation_instruction_is_single_move(x));
        }
        else if (fn == "own")
        {
            int n;
            std::cin >> n;
            std::vector<ShaderValidationOwner> o(n);
            auto at = [&](int v)
            {
                return v < 0 ? nullptr : &o[v];
            };
            for (auto &w : o)
            {
                int d, c;
                for (auto &ch : w.channels)
                {
                    std::cin >> ch;
                }
                std::cin >> d >> c;
                w.derived_owner = at(d);
                w.channel_owner = at(c);
            }
            int ops;
            std::cin >> ops;
            for (int i = 0; i < ops; ++i)
            {
                int kind, a, b;
                std::cin >> kind >> a >> b;
                if (kind == 0)
                {
                    set_shader_validation_owner_derived(o[a], at(b));
                }
                else if (kind == 1)
                {
                    set_shader_validation_owner_channel_owner(o[a], at(b));
                }
                else
                {
                    std::printf("%08X ", find_shader_validation_owner_channel(&o[a], (u32)b));
                }
            }
            for (auto &w : o)
            {
                std::printf("%d %d ", w.derived_owner ? (int)(w.derived_owner - o.data()) : -1,
                            w.channel_owner ? (int)(w.channel_owner - o.data()) : -1);
            }
            std::printf("\n");
        }
        else if (fn == "tree")
        {
            int n, root;
            std::cin >> n >> root;
            std::vector<ShaderValidationPassNode> nodes(n);
            for (auto &x : nodes)
            {
                int f, s;
                std::cin >> x.value >> f >> s;
                x.first = f < 0 ? nullptr : &nodes[f];
                x.second = s < 0 ? nullptr : &nodes[s];
            }
            ShaderValidationPassTree tree{root < 0 ? nullptr : &nodes[root], 0};
            int q;
            std::cin >> q;
            for (int i = 0; i < q; ++i)
            {
                u32 key;
                std::cin >> key;
                auto *r = find_shader_validation_pass_node_signed(tree, key);
                std::printf("%d ", r ? (int)(r - nodes.data()) : -1);
            }
            std::printf("\n");
        }
    }
}
