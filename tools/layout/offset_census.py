#!/usr/bin/env python3
"""Offset census: every retail-offset access in src/, grouped by file, buffer and offset.

Reads the source only. For each accessor call with a hex offset (word(w, 0xB2C), put_real(w, p + 0x434, ...),
w.bytes[0xB4F], ...) it records the enclosing `// Xbox 0x........` function, the buffer expression, the offset
expression, the access width and whether it reads or writes. The output is the evidence a naming pass needs for one
object type: which functions touch each offset, with the path to each function's Ghidra dump.

    python tools/layout/offset_census.py                       # whole tree
    python tools/layout/offset_census.py --file gameselect_slots.cpp

Writes cache/layout/census.json and cache/layout/<file>.md.
"""
import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Accessor name -> (width in bytes, 'R' or 'W'). 'P' widths are pointers into the buffer (the retail code passes
# the field by address).
ACCESSORS = {
    "word": (4, "R"),
    "integer": (4, "R"),
    "real": (4, "R"),
    "number": (4, "R"),
    "vector": (12, "R"),
    "half": (2, "R"),
    "read_u32": (4, "R"),
    "read_f32": (4, "R"),
    "read_u16": (2, "R"),
    "put_word": (4, "W"),
    "put_real": (4, "W"),
    "put_float": (4, "W"),
    "put_vector": (12, "W"),
    "put_half": (2, "W"),
    "write_u32": (4, "W"),
    "reals": ("P", "R"),
}

XBOX = re.compile(r"//\s*Xbox\s+0x([0-9A-Fa-f]{8})")
CALL = re.compile(r"\b(" + "|".join(sorted(ACCESSORS, key=len, reverse=True)) + r")\(")
BYTES = re.compile(r"([A-Za-z_][\w.\->]*)\.bytes\[([^\]]*0x[0-9A-Fa-f]+[^\]]*)\]\s*(=(?!=))?")
HEX = re.compile(r"0x([0-9A-Fa-f]+)")
DEFINITION = re.compile(r"^[A-Za-z_][\w:<>,\s\*&]*\b[A-Za-z_]\w*\s*\(")


def split_args(text, start):
    """The arguments of the call whose '(' is at text[start - 1], and the index after its ')'."""
    depth = 0
    args = []
    current = []
    i = start
    while i < len(text):
        c = text[i]
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                args.append("".join(current).strip())
                return args, i + 1
            depth -= 1
        elif c == "," and depth == 0:
            args.append("".join(current).strip())
            current = []
            i += 1
            continue
        current.append(c)
        i += 1
    return None, i


def literal_offset(expression):
    """The hex literal an offset expression ends with, and what is added to it (a record base such as `p`)."""
    literals = list(HEX.finditer(expression))
    if not literals:
        return None, expression
    last = literals[-1]
    base = (expression[: last.start()] + expression[last.end():]).strip(" +")
    return int(last.group(1), 16), base


def scan_file(path):
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    entries = []
    pending = None
    current = None
    for number, line in enumerate(lines, 1):
        note = XBOX.search(line)
        top_level = not line.startswith((" ", "\t"))
        if note and top_level and line.startswith("//"):
            # A note above a definition names the function that follows.
            pending = note.group(1).upper()
        elif top_level and DEFINITION.match(line) and not line.rstrip().endswith(";"):
            # A definition without a note above it is a host helper.
            current = pending
            pending = None
        for match in CALL.finditer(line):
            name = match.group(1)
            args, _ = split_args(line, match.end())
            if not args or len(args) < 2:
                continue
            offset, base = literal_offset(args[1])
            if offset is None:
                continue
            width, kind = ACCESSORS[name]
            entries.append(
                {
                    "file": path.relative_to(ROOT).as_posix(),
                    "line": number,
                    "function": current,
                    "accessor": name,
                    "buffer": args[0],
                    "expression": args[1],
                    "base": base,
                    "offset": offset,
                    "width": width,
                    "kind": kind,
                }
            )
        for match in BYTES.finditer(line):
            offset, base = literal_offset(match.group(2))
            entries.append(
                {
                    "file": path.relative_to(ROOT).as_posix(),
                    "line": number,
                    "function": current,
                    "accessor": "bytes[]",
                    "buffer": match.group(1),
                    "expression": match.group(2),
                    "base": base,
                    "offset": offset,
                    "width": 1,
                    "kind": "W" if match.group(3) else "R",
                }
            )
    return entries


def dump_path(va):
    if va is None:
        return None
    path = ROOT / "cache" / "ghidra_frontier" / f"{va}.txt"
    return path.relative_to(ROOT).as_posix() if path.exists() else None


def write_markdown(path, entries):
    groups = defaultdict(lambda: defaultdict(list))
    for e in entries:
        key = e["buffer"] + (f"  (record base `{e['base']}`)" if e["base"] else "")
        groups[key][e["offset"]].append(e)
    out = [f"# Offset census: {entries[0]['file']}", ""]
    functions = sorted({e["function"] for e in entries if e["function"]})
    for key in sorted(groups):
        out += [f"## `{key}`", "", "| Offset | Width | R/W | Functions | Lines |", "|---|---|---|---|---|"]
        for offset in sorted(groups[key]):
            hits = groups[key][offset]
            widths = ",".join(sorted({str(h["width"]) for h in hits}))
            kinds = "".join(sorted({h["kind"] for h in hits}))
            funcs = " ".join(sorted({h["function"] or "?" for h in hits}))
            line_numbers = " ".join(str(h["line"]) for h in hits[:8]) + (" ..." if len(hits) > 8 else "")
            out.append(f"| 0x{offset:X} | {widths} | {kinds} | {funcs} | {line_numbers} |")
        out.append("")
    out += ["## Dumps", ""]
    for va in functions:
        dump = dump_path(va)
        out.append(f"- 0x{va}: {dump if dump else '(no dump in cache/ghidra_frontier)'}")
    path.write_text("\n".join(out) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--file", action="append", default=[], help="only files whose name contains this")
    parser.add_argument("--out", default=str(ROOT / "cache" / "layout"))
    options = parser.parse_args()

    sources = sorted(p for p in (ROOT / "src").rglob("*") if p.suffix in (".cpp", ".h", ".inl"))
    if options.file:
        sources = [p for p in sources if any(f in p.as_posix() for f in options.file)]
    out = Path(options.out)
    out.mkdir(parents=True, exist_ok=True)

    entries = []
    per_file = defaultdict(list)
    for source in sources:
        found = scan_file(source)
        entries += found
        if found:
            per_file[source].extend(found)
    for source, found in per_file.items():
        write_markdown(out / f"{source.stem}.md", found)
    (out / "census.json").write_text(json.dumps(entries, indent=1) + "\n", encoding="utf-8")
    print(f"{len(entries)} accesses in {len(per_file)} files -> {out}")
    for source, found in sorted(per_file.items(), key=lambda item: -len(item[1]))[:15]:
        print(f"  {len(found):5d}  {source.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
