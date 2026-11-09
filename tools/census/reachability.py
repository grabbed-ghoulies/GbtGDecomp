#!/usr/bin/env python3
"""Reachability census: which untranslated game functions the running game can reach, and from where.

Inputs:
  - cache/function_list.txt (tools/ghidra_scripts/ListFunctions.java) and the translated addresses, as for
    tools/census/completion.py.
  - cache/reference_list.txt (tools/ghidra_scripts/ListReferences.java): every call, tail jump and data reference
    into game code.

Edges are direct calls and tail jumps, and data references made from game code (a function pointer pushed,
stored or compared). A function whose address sits in a .data or .rdata table (vtables, handler and factory
tables), or that the linked libraries reference (the C runtime's initializer table, callbacks), is a table root:
static analysis cannot tell whether its table is used, so those are reported apart. Data references made from
the libraries' own data sections are left out; they are constants that happen to land in game code.

Each untranslated function gets one class:
  frontier   a translated function references it, so native code raises the moment it gets there
  reachable  WinMain (0x00032390) reaches it through untranslated functions only
  table      reached only from a table root
  unreached  nothing above reaches it: dead code, or reached through a computed call Ghidra did not resolve
  untagged   translated and named with a bare `// 0x` comment after its declaration, not the `// Xbox 0x` tag
  fragment   not 16-byte aligned and just past a translated function: Ghidra split that function's body

and a source note: `raises` when src names it on a line that throws (a bound callee that is missing, or a guard
for a state retail never reaches), `inline` when src names it only in comments (often a helper folded into the
expression that calls it), `unnamed` when src never names it.

    python tools/census/reachability.py            # summary and the frontier, ranked
    python tools/census/reachability.py --all      # every untranslated function with its class and path

Writes cache/census/reachability.txt.
"""
import argparse
import bisect
import re
import sys
from collections import defaultdict, deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from completion import GAME_BEGIN, GAME_END, ROOT, ghidra_functions, translated_addresses  # noqa: E402

WINMAIN = 0x00032390
CODE_BLOCKS = {".text"}
TABLE_BLOCKS = {".data", ".rdata"}
MENTION = re.compile(r"0x([0-9A-Fa-f]{8})")
RAISES = re.compile(r"throw|Error\(|untranslated|not bound|not translated", re.IGNORECASE)
# A declaration or definition named with a bare `// 0x` instead of the `// Xbox 0x` tag: `...); // 0x0010F690`.
DECLARED = re.compile(r"\)\s*(?:const\s*)?(?:override\s*)?;?\s*//\s*0x([0-9A-Fa-f]{8})")


def source_mentions():
    """Addresses named in src outside the address maps: on lines that raise, on other lines, and after a
    declaration's closing parenthesis."""
    inline, raising, declared = set(), set(), set()
    for path in (ROOT / "src").rglob("*"):
        if path.suffix not in (".cpp", ".h", ".inl") or "address_map" in path.name:
            continue
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
            hits = {int(hit, 16) for hit in MENTION.findall(line)}
            if not hits:
                continue
            (raising if RAISES.search(line) else inline).update(hits)
            declared.update(int(hit, 16) for hit in DECLARED.findall(line))
    return inline, raising, declared


def load_references():
    path = ROOT / "cache" / "reference_list.txt"
    if not path.is_file():
        raise SystemExit(f"{path} is missing: run tools/ghidra_scripts/ListReferences.java first")
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        source, target, kind, owner, block = line.split()
        rows.append((int(source, 16), int(target, 16), kind, None if owner == "-" else int(owner, 16), block))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--all", action="store_true", help="print every untranslated function")
    parser.add_argument("--top", type=int, default=40, help="frontier rows to print (default 40)")
    args = parser.parse_args()

    functions = ghidra_functions()
    game = {va: row for va, row in functions.items() if GAME_BEGIN <= va < GAME_END and row[1] == ".text"}
    translated = {va for va in translated_addresses() if GAME_BEGIN <= va < GAME_END}
    known = sorted(set(game) | translated)
    untranslated = {va for va in game if va not in translated}

    def owner_of(address):
        index = bisect.bisect_right(known, address) - 1
        return known[index] if index >= 0 else None

    edges = defaultdict(set)
    referrers = defaultdict(set)
    table_roots = defaultdict(set)
    for source, target, kind, owner, block in load_references():
        if target not in game and target not in translated:
            continue  # A reference into a function's body (a switch case, a loop), not to an entry.
        if block in CODE_BLOCKS and not GAME_BEGIN <= source < GAME_END:
            table_roots[target].add(source)  # The linked libraries: the C runtime's initializer table, callbacks.
        elif block in CODE_BLOCKS:
            caller = owner if owner is not None and owner in game else owner_of(source)
            if caller is None or caller == target:
                continue
            edges[caller].add(target)
            referrers[target].add(caller)
        elif block in TABLE_BLOCKS and kind == "data":
            table_roots[target].add(source)

    def search(roots):
        parent = {root: None for root in roots}
        queue = deque(roots)
        while queue:
            node = queue.popleft()
            for target in edges.get(node, ()):
                if target not in parent:
                    parent[target] = node
                    queue.append(target)
        return parent

    from_main = search([WINMAIN])
    from_tables = search(sorted(table_roots))

    def path(parent, node):
        chain = []
        while node is not None:
            chain.append(node)
            node = parent[node]
        return " <- ".join(f"{va:08X}" for va in chain)

    inline, raising, declared = source_mentions()
    classes = {}
    for va in untranslated:
        if va % 16 and owner_of(va - 1) in translated:
            classes[va] = "fragment"
        elif va in declared:
            classes[va] = "untagged"
        elif any(caller in translated for caller in referrers.get(va, ())):
            classes[va] = "frontier"
        elif va in from_main:
            classes[va] = "reachable"
        elif va in from_tables:
            classes[va] = "table"
        else:
            classes[va] = "unreached"

    def source(va):
        if va in raising:
            return "raises"
        return "inline" if va in inline else "unnamed"

    # What each frontier function opens up: untranslated functions reached through it before any other frontier.
    def opens(root):
        seen = {root}
        queue = deque([root])
        while queue:
            node = queue.popleft()
            for target in edges.get(node, ()):
                if target in untranslated and target not in seen and classes[target] not in (
                        "frontier", "untagged", "fragment"):
                    seen.add(target)
                    queue.append(target)
        return seen

    order = ("frontier", "reachable", "table", "unreached", "untagged", "fragment")
    grid = defaultdict(lambda: [0, 0])
    for va, cls in classes.items():
        for key in ((cls, None), (cls, source(va))):
            grid[key][0] += 1
            grid[key][1] += game[va][0]
    print(f"Untranslated game functions: {len(untranslated)}, {sum(game[va][0] for va in untranslated)} bytes")
    print("| Class | Functions | Bytes | raises | inline | unnamed |")
    print("|---|---:|---:|---:|---:|---:|")
    for cls in order:
        cells = " | ".join(f"{grid[cls, kind][0]} ({grid[cls, kind][1]} B)" for kind in ("raises", "inline", "unnamed"))
        print(f"| {cls} | {grid[cls, None][0]} | {grid[cls, None][1]} | {cells} |")
    print(f"Translated functions WinMain reaches by direct reference: {len(translated & set(from_main))}"
          f" of {len(translated)}")

    rank = {"raises": 0, "unnamed": 1, "inline": 2}
    frontier = []
    for va in (va for va, cls in classes.items() if cls == "frontier"):
        callers = sorted(caller for caller in referrers[va] if caller in translated)
        frontier.append((va, callers, opens(va)))
    frontier.sort(key=lambda row: (rank[source(row[0])], -len(row[1]), -sum(game[v][0] for v in row[2])))

    lines = ["# va size class/source name | referrers | opens (count, bytes) | path from WinMain or a table root"]
    for va, callers, opened in frontier:
        lines.append(f"{va:08x} {game[va][0]} frontier/{source(va)} {game[va][3]} | "
                     f"{' '.join(f'{c:08x}' for c in callers)} | {len(opened)} {sum(game[v][0] for v in opened)} | "
                     f"{path(from_main, va) if va in from_main else '-'}")
    for cls in order[1:]:
        for va in sorted(v for v, c in classes.items() if c == cls):
            parent = from_main if va in from_main else from_tables
            lines.append(f"{va:08x} {game[va][0]} {cls}/{source(va)} {game[va][3]} | "
                         f"{' '.join(f'{c:08x}' for c in sorted(referrers.get(va, ())))} | - | "
                         f"{path(parent, va) if va in parent else '-'}")
    out = ROOT / "cache" / "census"
    out.mkdir(parents=True, exist_ok=True)
    (out / "reachability.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    print(f"\nFrontier, raising and unnamed first, then most translated referrers (top {args.top}):")
    print("| Function | Bytes | Source | Translated referrers | Opens (functions, bytes) |")
    print("|---|---:|---|---|---:|")
    for va, callers, opened in frontier[:args.top]:
        shown = ", ".join(f"0x{c:08X}" for c in callers[:4]) + (" ..." if len(callers) > 4 else "")
        print(f"| 0x{va:08X} | {game[va][0]} | {source(va)} | {shown} |"
              f" {len(opened)}, {sum(game[v][0] for v in opened)} |")
    if args.all:
        print()
        print("\n".join(lines[1 + len(frontier):]))


if __name__ == "__main__":
    main()
