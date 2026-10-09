#include "../src/engine/graphics/shader_schedule_init.h"
#include "../src/engine/graphics/shader_schedule_update.h"
#include "../src/engine/graphics/shader_schedule_record.h"
#include "../src/engine/graphics/shader_schedule_cost_inputs.h"
#include "../src/engine/graphics/shader_schedule_hazards.h"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}
}

void check_shader_cost_dependencies()
{
    using namespace gbtg;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    require(runtime_shader_issue_value_near(1, 1, 0.25f)
        && !runtime_shader_issue_value_near(1.25f, 1, 0.25f)
        && !runtime_shader_issue_value_near(0.75f, 1, 0.25f)
        && !runtime_shader_issue_value_near(nan, 1, 0.25f),
        "issue near comparison uses strict bounds and rejects unordered values");
    std::array<u32, runtime_shader_schedule_storage_size / 4> words{};
    auto* state = reinterpret_cast<u8*>(words.data());
    auto* issue = state + 0x1C0;
    configure_runtime_shader_schedule(state, 1, 10);
    update_runtime_shader_issue_schedule(issue, 0, 13, 0, 3, 0x43);
    require(words[0x290 / 4] == 0x3E2E147B && words[0x370 / 4] == 0x3E2E147B
        && words[0x610 / 4] == 0x48 && words[0x6F0 / 4] == 0x48,
        "special issue register ignores mask and uses option-ten duration");
    configure_runtime_shader_schedule(state, 1, 11);
    update_runtime_shader_issue_schedule(issue, 0, 13, 0, 3, 0x43);
    require(words[0x290 / 4] == 0x3EA8F5C3, "special issue register changes duration only beyond option ten");
    configure_runtime_shader_schedule(state, 1, 10);
    words[0x540 / 4] = 0x42;
    update_runtime_shader_issue_schedule(issue, 1, 0, 1, 2, 0x43);
    require(words[0x1C0 / 4] == 0x3E4CCCCD && words[0x2A0 / 4] == 0x3E4CCCCD
        && words[0x540 / 4] == 0x46 && words[0x620 / 4] == 0x46
        && words[0x380 / 4] == 0x3E4CCCCD && words[0x460 / 4] == 0x3E4CCCCD,
        "unit-latency issue update preserves float and status banks");
    const auto before_mask = words;
    update_runtime_shader_issue_schedule(issue, 2, 0, 0x80, 5, 0x46);
    require(words == before_mask, "issue update ignores mask bits outside four channels");
    configure_runtime_shader_schedule(state, 0, 2);
    words[0x14 / 4] = std::bit_cast<u32>(7.75f);
    const std::array<u32, 3> write{0x0F200000, 0, 0x02000000};
    require(record_runtime_shader_register_writes(state, write.data()) == 0
        && words[0x40 / 4] == 13 && words[0x4C / 4] == 13 && words[0x110 / 4] == 1,
        "register writes truncate issued time and store vector latency for selected destination");
    const std::array<u32, 3> invalid_write{0x01C00000, 0, 0x02000000};
    require(record_runtime_shader_register_writes(state, invalid_write.data()) == 0x80004005
        && words[0xE0 / 4] == 13, "invalid register write retains native write before returning failure");
    const std::array<u32, 3> constant_write{0x1020, 0, 0};
    require(record_runtime_shader_register_writes(state, constant_write.data()) == 0 && words[7] == 1,
        "constant write selects actual readiness bank");
    configure_runtime_shader_schedule(state, 0, 2);
    const std::array<u32, 3> pending_write{0x1880, 0, 0};
    require(record_runtime_shader_issue_writes(state, pending_write.data(), 0x20) == 0 && state[0x10] == 1,
        "issue pending-byte write preserves native alias with scheduling mode");
    configure_runtime_shader_schedule(state, 1, 2);
    std::array<u32, 3> no_op{};
    u32 reason = 99;
    require(runtime_shader_operand_hazard_cost(state, no_op.data(), 2, 0, 0, 0, 0, 0, 0, 0, 15, 0, reason) == 0
        && reason == 0x20, "vertex input has no register hazard cost");
    words[0x1C0 / 4] = std::bit_cast<u32>(0.f);
    words[(0x1C0 + 4) / 4] = std::bit_cast<u32>(0.f);
    words[0x540 / 4] = 0x42;
    words[(0x540 + 4) / 4] = 0x47;
    require(runtime_shader_masked_hazard_cost(state, no_op.data(), 0, 0, 3, 0, reason) == 0 && reason == 0x47,
        "equal hazard costs select the last channel reason");
    words[(0x1C0 + 4) / 4] = std::bit_cast<u32>(nan);
    require(runtime_shader_masked_hazard_cost(state, no_op.data(), 0, 0, 3, 0, reason) == 0 && reason == 0x42,
        "unordered hazard candidate preserves earlier channel reason");
    words[0x20 / 4] = 8;
    words[0xF0 / 4] = 1;
    require(runtime_shader_masked_hazard_ready(state, no_op.data(), 0, 0, 1, 5)
        && !runtime_shader_masked_hazard_ready(state, no_op.data(), 0, 0, 1, 4),
        "register bypass accepts exact three-cycle distance and rejects adjacent cycle");
    words[0x18 / 4] = 2;
    require(runtime_shader_operand_hazard_ready(state, no_op.data(), 3, 0, 0, 0, 0, 0, 0, 0, 15, 8)
        && !runtime_shader_operand_hazard_ready(state, no_op.data(), 3, 0, 0, 0, 0, 0, 0, 0, 15, 7),
        "constant readiness preserves six-cycle boundary");
    const std::array<u32, 7> program{2, 0, 0, 0, 0, 0, 0};
    float cost = -99;
    require(estimate_runtime_shader_program_cost(state, 0, program.data(), &cost) == 0 && cost == 20,
        "disabled scheduling estimates instruction cycles plus real eighteen-cycle tail");
    require(estimate_runtime_shader_program_cost(state, 1, program.data(), &cost) == 0
        && std::fabs(cost - 2.2f) < 0.000001f,
        "enabled scheduling joins instruction issue with minimum tail cost");
    const std::array<u32, 4> invalid_program{1, 0x01C00000, 0, 0x02000000};
    cost = -99;
    require(estimate_runtime_shader_program_cost(state, 0, invalid_program.data(), &cost) == 0x80004005 && cost == -99,
        "program estimation preserves caller cost on native recording failure");
    require(estimate_runtime_shader_program_cost(state, 1, invalid_program.data(), &cost) == 0x80004005 && cost == -99,
        "enabled issue recording propagates failure without overwriting program cost");
    std::array<u32, 4> multiply_program{1, 0, 0, 0x06000000};
    const auto original_multiply = multiply_program;
    require(optimize_runtime_shader_multiply_order(state, 1, multiply_program.data()) == 0
        && multiply_program == original_multiply, "equal scheduling cost retains multiply order and actual result");
    std::array<u32, 4> rejected_order{1, 0x01C00000, 0, 0x02000000};
    require(optimize_runtime_shader_multiply_order(state, 0, rejected_order.data()) == 0x80004005,
        "multiply-order pass forwards actual issue failure to parent optimizer");
}
