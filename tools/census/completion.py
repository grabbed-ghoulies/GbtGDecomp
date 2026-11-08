#!/usr/bin/env python3
"""Completion census: how much of the retail game code has a translation.

Inputs:
  - cache/function_list.txt, every function Ghidra knows (address, size, block, thunk, name). Write it with the
    read-only headless run of tools/ghidra_scripts/ListFunctions.java (see AGENTS.md for the command form).
  - src/, scanned for `// Xbox 0x........` tags, the address of each translated function.
  - OriginalFiles/default.xbe (or --xbe), for the object factory table at 0x0030D41C.

Game code is `.text` below GAME_END, where the statically linked XAPI and C runtime begin (`IsBadReadPtr` is the
first). Those libraries, D3D8, XGRAPHICS, DirectSound, XACT and Bink are boundaries: they are translated only
where the game calls them, so they are left out of the totals.

Ghidra's analysis does not find every function: about a fifth of the translated ones are missing from its list
(the export creates them on the fly). Known functions are therefore Ghidra's plus every translated address, and
the untranslated count is a lower bound.

    python tools/census/completion.py                # summary, per-area table and factory types
    python tools/census/completion.py --list         # also every known untranslated game function

Writes cache/census/untranslated.txt.
"""
import argparse
import bisect
import re
import struct
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME_BEGIN = 0x00011000
GAME_END = 0x0014A840  # IsBadReadPtr, the first XAPI function in .text.
FACTORY_TABLE = 0x0030D41C
FACTORY_TYPES = 0xAB
TAG = re.compile(r"// Xbox 0x([0-9A-Fa-f]{8})")


def translated_addresses():
    """Each tagged address with the source area (src/engine/<area> or src/<area>) that holds it first."""
    areas = {}
    for path in sorted([*(ROOT / "src").rglob("*"), *(ROOT / "include").rglob("*")]):
        if path.suffix not in (".cpp", ".h", ".inl"):
            continue
        parts = path.relative_to(ROOT).parts[1:]
        if path.is_relative_to(ROOT / "include"):
            parts = ("include",) + parts
        area = parts[1] if parts[0] == "engine" and len(parts) > 2 else parts[0]
        for hit in TAG.findall(path.read_text(encoding="utf-8", errors="ignore")):
            areas.setdefault(int(hit, 16), area)
    return areas


def ghidra_functions():
    path = ROOT / "cache" / "function_list.txt"
    if not path.is_file():
        raise SystemExit(f"{path} is missing: run tools/ghidra_scripts/ListFunctions.java first")
    functions = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        va, size, block, thunk, name = line.split(None, 4)
        functions[int(va, 16)] = (int(size), block, thunk == "1", name)
    return functions


class Xbe:
    def __init__(self, path):
        self.data = path.read_bytes()
        base, = struct.unpack_from("<I", self.data, 0x104)
        count, = struct.unpack_from("<I", self.data, 0x11C)
        headers, = struct.unpack_from("<I", self.data, 0x120)
        self.sections = []
        for index in range(count):
            _, va, _, raw, raw_size = struct.unpack_from("<5I", self.data, headers - base + index * 0x38)
            self.sections.append((va, raw, raw_size))

    def u32(self, va):
        for start, raw, size in self.sections:
            if start <= va < start + size:
                return struct.unpack_from("<I", self.data, raw + va - start)[0]
        raise ValueError(f"0x{va:08X} is not in a loaded section")

    def cstr(self, va):
        for start, raw, size in self.sections:
            if start <= va < start + size:
                offset = raw + va - start
                return self.data[offset:self.data.index(b"\0", offset)].decode("latin1")
        return ""


def factory_types(xbe_path):
    """Factory types whose retail getter no source file names. A type's name is on the row before it.

    The regional address maps (`*address_map*`) list every retail address, so they are left out of the search."""
    if not xbe_path.is_file():
        return None
    xbe = Xbe(xbe_path)
    source = "\n".join(p.read_text(encoding="utf-8", errors="ignore").upper()
                       for p in (ROOT / "src").rglob("*")
                       if p.suffix in (".cpp", ".h", ".inl") and "address_map" not in p.name)
    rows = []
    for index in range(FACTORY_TYPES):
        getter = xbe.u32(FACTORY_TABLE + index * 12 + 4)
        name = xbe.u32(FACTORY_TABLE + index * 12 + 8)
        rows.append((index, getter, xbe.cstr(name) if name else ""))
    total = 0
    missing = []
    for index, getter, _ in rows:
        if getter == 0 or index <= 2:  # Type 0 has none; 1 and 2 are the base classes.
            continue
        total += 1
        if f"0X{getter:08X}" not in source:
            missing.append((index, getter, rows[index - 1][2]))
    return total, missing


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--list", action="store_true", help="print every known untranslated game function")
    parser.add_argument("--xbe", type=Path, default=ROOT / "OriginalFiles" / "default.xbe")
    args = parser.parse_args()

    areas = translated_addresses()
    functions = ghidra_functions()
    game = {va: row for va, row in functions.items() if GAME_BEGIN <= va < GAME_END and row[1] == ".text"}
    translated = {va for va in areas if GAME_BEGIN <= va < GAME_END}
    known = set(game) | translated
    untranslated = sorted(va for va in game if va not in translated)
    missing_bytes = sum(game[va][0] for va in untranslated)

    print(f"Game functions known: {len(known)} (Ghidra {len(game)}, plus {len(known) - len(game)} found by translation)")
    print(f"Translated: {len(translated)} ({100.0 * len(translated) / len(known):.1f}%)")
    print(f"Untranslated (lower bound): {len(untranslated)}, {missing_bytes} bytes")

    keys = sorted(areas)
    by_area = Counter()
    by_area_bytes = Counter()
    for va in untranslated:
        index = bisect.bisect_right(keys, va) - 1
        area = areas[keys[index]] if index >= 0 else "?"
        by_area[area] += 1
        by_area_bytes[area] += game[va][0]
    done_by_area = Counter(areas[va] for va in translated)
    print("\n| Area | Translated | Untranslated (nearest area) | Untranslated bytes |")
    print("|---|---:|---:|---:|")
    for area in sorted(set(done_by_area) | set(by_area), key=lambda a: -(done_by_area[a] + by_area[a])):
        print(f"| {area} | {done_by_area[area]} | {by_area[area]} | {by_area_bytes[area]} |")

    factory = factory_types(args.xbe)
    if factory is not None:
        total, missing = factory
        print(f"\nFactory types: {total - len(missing)} of {total} have their getter named in src")
        for index, getter, name in missing:
            print(f"  type 0x{index:02X} getter 0x{getter:08X} {name}")

    out = ROOT / "cache" / "census"
    out.mkdir(parents=True, exist_ok=True)
    (out / "untranslated.txt").write_text(
        "".join(f"{va:08x} {game[va][0]} {game[va][3]}\n" for va in untranslated), encoding="utf-8")
    if args.list:
        for va in untranslated:
            print(f"0x{va:08X} {game[va][0]:6d} {game[va][3]}")


if __name__ == "__main__":
    main()
