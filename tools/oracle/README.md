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
       -scriptPath tools/ghidra/ghidra_scripts -postScript DumpBytes.java cache/oracle 00160000-00185000 00185000-001A2000 00200000-00203000 00203000-00210000
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
| `containers_oracle.py` | `0x001902D9`, `0x001903E1`, `0x0019042A`, `0x00190473`, `0x0019051C`, `0x00190337`, `0x00190B12`, `0x00190CA6`, `0x001909B1`, `0x0018F314`, `0x0018F1E5`, `0x001929B7` (XMemAlloc and the deleting destructor are hooked) |
| `registers_oracle.py` | `0x00190554`, `0x0019060B`, `0x00190656`, `0x00191E8F`, `0x00191F00`, `0x0018F4A9`, `0x0018F595`, `0x0018F86B`, `0x0018F436`, `0x0019010E`, `0x00190BDD`, `0x00192629`, `0x0018E4EB`, `0x0018E48C`, `0x0018E988`, `0x0018ED95`, `0x0018FF4B`, `0x0018E9DC`, `0x00191990`, `0x001919BD`, `0x0018F848`, `0x0018EB4C`, `0x0018EB96`, `0x00192469`, `0x0018FFC9`, `0x001906BB`, `0x0018FC2A`, `0x0018FD7D`, `0x001936B5` (via `0x00192469`), `0x0018EBEA`, `0x0018EDEE`, `0x001907E9`, `0x0018F815`, `0x00190D14`, `0x00190858`, `0x0018FED0`, `0x001908AE` |
| `trees_oracle.py` | Tree template instances `0x0019164F`, `0x00191714`, `0x00191806`, `0x001918CB`, `0x00191B1C` (join), `0x0019204A`, `0x0019214A`, `0x0019224A`, `0x0019234A`, `0x00192529` (erase), `0x001926FB`, `0x001927B9` (unregister), `0x001922BB`, `0x0019259A` (insert) and pass insertion `0x00191A82` |
| `sets_oracle.py` | `0x0018E78B`, `0x0018F56F`, `0x0018F549`, `0x0018F2AA`, `0x0018F2D3`, `0x0018F1BC`, `0x00190322`, `0x00190163`, `0x00190AA0`, `0x00190AC6`, `0x00192772`, `0x0019287F`, `0x001924DA`, `0x0018E94C`, `0x0018F174`, `0x001923BB`, `0x001927D1`, `0x00192830`, `0x001929E7`, `0x00192A2B`, `0x00190774`, `0x00190813`, `0x00190FFE` |
| `ctor_oracle.py` | SEH-framed `0x00192DF1`, `0x001935E6`, `0x001934A5`, `0x00190E27`, `0x00192B84`, `0x00194311` (with `0x00193C2A`), `0x00193EE2`, and `0x0019149A` (normal path, against SEH-shimmed host copies built by `run.sh`; needs `cache/oracle/mem_00160000.bin`) |

An oracle is evidence for a translation, not a replacement for reading the
dump: inputs must reach every branch, and functions that call heap, D3D or
other host boundaries need those calls hooked rather than skipped.
