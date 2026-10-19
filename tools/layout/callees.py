#!/usr/bin/env python3
"""Callees of reconstructed-function dumps, and which of them are already translated.

    python tools/layout/callees.py 000D7A60 000D8840        # direct callees
    python tools/layout/callees.py --depth 3 000D7A60         # and their callees, through untranslated ones

Reads cache/ghidra_frontier/<VA>.txt (the instruction listing's CALL targets) and the `// Xbox 0x........` notes
under src/. A callee with no dump in the cache is listed as "no dump"; export it with the read-only Ghidra command
in AGENTS.md.
"""
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DUMPS = ROOT / "cache" / "ghidra_frontier"
CALL = re.compile(r"^[0-9a-f]{8} CALL 0x([0-9a-f]{8})\s*$", re.M)
NOTE = re.compile(r"Xbox 0x([0-9A-Fa-f]{8})")


def translated():
    found = set()
    for path in (ROOT / "src").rglob("*"):
        if path.suffix in (".cpp", ".h", ".inl"):
            for m in NOTE.finditer(path.read_text(encoding="utf-8", errors="replace")):
                found.add(m.group(1).upper())
    return found


def callees(va):
    path = DUMPS / f"{va}.txt"
    if not path.exists():
        return None
    return sorted({m.group(1).upper() for m in CALL.finditer(path.read_text(encoding="utf-8", errors="replace"))})


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("va", nargs="+")
    parser.add_argument("--depth", type=int, default=1)
    options = parser.parse_args()
    done = translated()
    seen = set()
    missing_dumps = set()
    frontier = [(va.upper().removeprefix("0X").zfill(8), 0) for va in options.va]
    while frontier:
        va, depth = frontier.pop(0)
        if va in seen:
            continue
        seen.add(va)
        calls = callees(va)
        if calls is None:
            missing_dumps.add(va)
            continue
        marks = []
        for callee in calls:
            mark = "ok" if callee in done else ("no dump" if callees(callee) is None else "TODO")
            marks.append(f"{callee}:{mark}")
            if callee not in done and depth + 1 < options.depth:
                frontier.append((callee, depth + 1))
        print(f"{va} ({'translated' if va in done else 'untranslated'}): " + " ".join(marks))
    todo = sorted(v for v in seen if v not in done)
    print(f"\nuntranslated reached: {len(todo)}; without a dump: {' '.join(sorted(missing_dumps)) or 'none'}")


if __name__ == "__main__":
    main()
