#!/usr/bin/env bash
# Builds the host harnesses and compares them with retail code under Unicorn.
# Requires: g++ (C++20), python3, pip install unicorn, and cache/oracle/mem_00185000.bin.
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
root="$(cd "$(dirname "$0")/../.." && pwd)"
graphics="$root/src/engine/graphics"
out="$root/cache/oracle"
mkdir -p "$out"
common=("$graphics/shader_validation_operand_order.cpp" "$graphics/shader_validation_channels.cpp"
    "$graphics/shader_validation_storage.cpp" "$graphics/shader_validation_reserved_register.cpp")
g++ -std=c++20 -O1 -I"$root/include" -I"$graphics" "$root/tools/oracle/serializer_harness.cpp" \
    "$graphics/shader_validation_serializer.cpp" "${common[@]}" -o "$out/serializer_harness"
g++ -std=c++20 -O1 -I"$root/include" -I"$graphics" "$root/tools/oracle/accessor_harness.cpp" \
    "$graphics/shader_validation_accessors.cpp" -o "$out/accessor_harness"
g++ -std=c++20 -O1 -I"$root/include" -I"$graphics" "$root/tools/oracle/channel_ops_harness.cpp" \
    "$graphics/shader_validation_channel_ops.cpp" "$graphics/shader_validation_accessors.cpp" \
    "$graphics/shader_validation_storage.cpp" -o "$out/channel_ops_harness"
cd "$root/tools/oracle"
python3 serializer_oracle.py "${1:-1}" "${2:-500}"
python3 accessor_oracle.py
python3 channel_ops_oracle.py "${1:-1}"
