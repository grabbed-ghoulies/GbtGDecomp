#!/usr/bin/env bash
# Builds the host harnesses and compares them with retail code under Unicorn.
# Requires: g++ (C++20), python3, pip install unicorn, and cache/oracle/mem_00185000.bin.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
root="$(cd "$(dirname "$0")/../.." && pwd)"
graphics="$root/src/engine/graphics"
out="$root/cache/oracle"
mkdir -p "$out"
# Portable validator modules; constructors need the Windows SEH backend and are excluded.
validator=("$graphics"/shader_validation_{instruction_ops,registers,channel_ops,containers,accessors,serializer,operand_order,channels,storage,reserved_register,arena,registration,insert,node_init,balance,pass_traversal,erase,tree_instances,sets,operand_tree}.cpp)
for name in serializer accessor channel_ops containers registers trees sets; do
    g++ -std=c++20 -O1 -I"$root/include" -I"$graphics" "$root/tools/oracle/${name}_harness.cpp" "${validator[@]}" -o "$out/${name}_harness"
done
# SEH-framed translations: compile shimmed copies so the normal path can be compared on Linux.
shim="$out/shim"
mkdir -p "$shim"
: > "$shim/excpt.h"
for name in shader_validation_constructors shader_validation_instruction_constructor shader_validation_pass_lifecycle shader_validation_dependencies shader_validation_instruction_lifecycle shader_validation_packed_program shader_validation_optimizer shader_validation_scheduler shader_validation_program shader_validation_owner_sets shader_validation_destinations; do
    sed -e 's/__try/if (true)/' -e 's/__finally/else/' -e 's/AbnormalTermination()/false/' -e 's/__except (.*)/else if (false)/' "$graphics/$name.cpp" > "$shim/$name.cpp"
done
g++ -std=c++20 -O1 -D_WIN32 -I"$shim" -I"$root/include" -I"$graphics" "$root/tools/oracle/ctor_harness.cpp" \
    "$shim/shader_validation_constructors.cpp" "$shim/shader_validation_instruction_constructor.cpp" "$shim/shader_validation_pass_lifecycle.cpp" "$shim/shader_validation_dependencies.cpp" "$shim/shader_validation_instruction_lifecycle.cpp" "$shim/shader_validation_packed_program.cpp" "$shim/shader_validation_optimizer.cpp" "$shim/shader_validation_scheduler.cpp" "$shim/shader_validation_program.cpp" "$shim/shader_validation_owner_sets.cpp" "$shim/shader_validation_destinations.cpp" "${validator[@]}" \
    "$graphics/shader_validation_cleanup.cpp" -o "$out/ctor_harness"
cd "$root/tools/oracle"
python3 serializer_oracle.py "${1:-1}" "${2:-500}"
python3 accessor_oracle.py
python3 channel_ops_oracle.py "${1:-1}"
python3 containers_oracle.py "${1:-1}"
python3 registers_oracle.py "${1:-1}"
python3 trees_oracle.py "${1:-1}"
python3 sets_oracle.py "${1:-1}"
python3 ctor_oracle.py "${1:-1}" 600
