# Retail code oracle

These checks run a reconstructed function and the retail machine code for the
same address on the same inputs, then compare every output word. The retail
code runs in [Unicorn](https://www.unicorn-engine.org/) from bytes exported
from the user's own `default.xbe`; those bytes stay in `cache/oracle/` and are
never committed.

This complements the in-repo regression tests. The fixed vectors in
`tests/shader_validation_serializer_tests.cpp` were produced this way.

## Setup

1. Export the code range read-only (see `docs/GHIDRA.md`):

   ```text
   analyzeHeadless <ghidra_projects> GbtG -process default.xbe.original -readOnly -noanalysis
       -scriptPath tools/ghidra/ghidra_scripts -postScript DumpBytes.java cache/oracle 00185000-001A2000
   ```

2. `pip install unicorn`, then run `tools/oracle/run.sh [seed] [cases]`.

The harnesses compile only the reconstructed modules they test, so they build
with g++ on Linux. Each oracle reports `mismatches 0` on success.

## Coverage

| Script | Retail functions |
|--------|------------------|
| `serializer_oracle.py` | `0x0018F8B7`, `0x001914E8` and their callees, including the retained frame words |
| `accessor_oracle.py` | `0x0018E414`, `0x0018E595`, `0x0018E5C3`, `0x0018E67C`, `0x0018E6BE`, `0x0018E833`, `0x0018E84E`, `0x0018E869`, `0x0018E884`, `0x0018E8A5`, `0x0018E8E4`, `0x0018E8C6`, `0x0018E8F8`, `0x0018E76F`, `0x0018F4F7`, `0x0018E7AF`, `0x00190370` |
| `channel_ops_oracle.py` | `0x0018E5E2`, `0x0018E604`, `0x0018E63F`, `0x0018E661`, `0x0018E69C`, `0x0018E6DF`, `0x0018E700`, `0x0018E741`, `0x0018E548`, `0x0018F355`, `0x0018F394` |

An oracle is evidence for a translation, not a replacement for reading the
dump: inputs must reach every branch, and functions that call heap, D3D or
other host boundaries need those calls hooked rather than skipped.
