#include "shader_validation_serializer.h"
#include <cstdio>
#include <vector>
#include <iostream>
using namespace gbtg;
int main()
{
    long long mode, active_index, n;
    while (std::cin >> mode >> active_index)
    {
        std::cin >> n;
        std::vector<ShaderValidationOwner> owners(n);
        std::vector<long long> links(n);
        for (auto i = 0; i < n; ++i)
        {
            std::cin >> owners[i].kind >> owners[i].source >> links[i];
        }
        long long m;
        std::cin >> m;
        std::vector<ShaderValidationOperand> operands(m);
        ShaderValidationOwner dummy{};
        ShaderValidationOwner *active = active_index == -1 ? nullptr : active_index == -2 ? &dummy : &owners[active_index];
        for (auto i = 0; i < m; ++i)
        {
            long long parent;
            u32 marker;
            std::cin >> parent >> marker;
            operands[i].parent = parent == -1 ? nullptr : parent == -2 ? active : &owners[parent];
            operands[i].marker = static_cast<u8>(marker);
            for (auto &c : operands[i].channel_order.channels)
            {
                std::cin >> c;
            }
        }
        for (auto i = 0; i < n; ++i)
        {
            owners[i].operand = links[i] < 0 ? nullptr : &operands[links[i]];
        }
        long long k;
        std::cin >> k;
        std::vector<ShaderValidationInstruction> ins(k);
        auto owner_at = [&](long long v)
        {
            return v < 0 ? nullptr : &owners[v];
        };
        for (auto &x : ins)
        {
            long long a, b, c, o, vd, sd;
            u32 flag;
            std::cin >> a >> b >> c >> flag;
            x.operands = {&operands[a], &operands[b], &operands[c]};
            x.output_flag = static_cast<u8>(flag);
            for (auto *mask : {&x.output_mask, &x.vector_mask, &x.scalar_mask})
            {
                for (auto &ch : mask->channels)
                {
                    u32 v;
                    std::cin >> v;
                    ch = static_cast<u8>(v);
                }
            }
            std::cin >> x.vector_opcode >> x.scalar_opcode >> o >> vd >> sd;
            x.output_register = owner_at(o);
            x.vector_destination = owner_at(vd);
            x.scalar_destination = owner_at(sd);
        }
        ShaderValidationSerializerFrame frame;
        std::cin >> frame.word0 >> frame.word1 >> frame.word2;
        if (mode == 0)
        {
            for (auto &x : ins)
            {
                u32 out[3];
                u32 *r = serialize_shader_validation_instruction(x, out, frame, active);
                std::printf("%08X %08X %08X | %08X %08X %08X %d\n", out[0], out[1], out[2], frame.word0, frame.word1, frame.word2,
                            r == out);
            }
        }
        else
        {
            std::vector<ShaderValidationListNode> nodes(k);
            for (auto i = 0; i < k; ++i)
            {
                nodes[i].value = &ins[i];
                nodes[i].next = i + 1 < k ? &nodes[i + 1] : nullptr;
            }
            std::vector<u32> program(1 + 3 * k + 8, 0xCCCCCCCC);
            bool ok = serialize_shader_validation_program(k ? &nodes[0] : nullptr, program.data(), frame, active);
            std::printf("%d", ok);
            for (auto i = 0; i < 1 + 3 * k; ++i)
            {
                std::printf(" %08X", program[i]);
            }
            std::printf("\n");
        }
        for (auto &op : operands)
        {
            std::printf("%08X %08X %08X %08X\n", op.channel_order.channels[0], op.channel_order.channels[1], op.channel_order.channels[2],
                        op.channel_order.channels[3]);
        }
        std::printf("--\n");
    }
}
