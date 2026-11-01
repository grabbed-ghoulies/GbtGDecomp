#!/usr/bin/env python3
"""PAL -> NTSC-J address map for the XBE data the port reads.

The port reads retail tables and constants from default.xbe at PAL addresses (SceneExecutableResources). NTSC-J is
a later, re-linked build, so those addresses moved. This tool builds the map from the user's own two dumps and
writes addresses only, never game bytes.

Two sources of evidence:

1. Data blocks. `.rdata` and `.data` are aligned dword by dword, with any dword that looks like an image address
   masked (pointers differ between builds even when the data is the same). An ordered pass anchors on unique
   6-dword runs and keeps them in order; a gap of the same size on both sides between two runs is data changed
   in place ("replaced"); a last pass relocates the leftover PAL dwords anywhere in NTSC-J (tables the linker
   moved). A block says "this is the same data", nothing about which code uses it.
2. Code references. Each PAL instruction that holds a literal address is found in NTSC-J `.text` by its
   surrounding bytes (addresses wildcarded), and the NTSC-J operand is read. This says which NTSC-J address the
   same instruction uses, so it wins over the data blocks. A literal the code reaches through a nearby base (a
   table indexed from base - stride, say) is mapped through that base when the bases agree.

Literal addresses come from the source: `memory(0x...)`, `string(0x...)` and `read_pool(..., 0x...)` calls.

    python tools/regions/xbe_address_map.py --pal Dumps/PAL/default.xbe --ntscj Dumps/NTSC-J/default.xbe

Writes src/engine/scene/ntsc_j_address_map.inl (what SceneExecutableResources needs to read NTSC-J in the PAL
layout) and cache/address_maps/ntsc_j.json (every literal with its evidence and source uses), and prints the
literals that need a decision.
"""
import argparse
import bisect
import json
import re
import struct
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IMAGE_LO, IMAGE_HI = 0x11000, 0x580000
DATA_SECTIONS = (".rdata", ".data")
ANCHOR = 6
NEAR = 0x200


class Xbe:
    def __init__(self, path):
        self.bytes = Path(path).read_bytes()
        d = self.bytes
        base = struct.unpack_from("<I", d, 0x104)[0]
        count, table = struct.unpack_from("<II", d, 0x11C)
        self.sections = {}
        for i in range(count):
            _, va, vs, ra, rs, name = struct.unpack_from("<IIIIII", d, table - base + i * 0x38)
            label = d[name - base:name - base + 16].split(b"\0")[0].decode()
            self.sections.setdefault(label, (va, vs, ra, rs))
        self.timestamp = struct.unpack_from("<I", d, 0x114)[0]

    def raw(self, name):
        va, _, ra, rs = self.sections[name]
        return va, self.bytes[ra:ra + rs]

    def read(self, va, size):
        for sva, _, ra, rs in self.sections.values():
            if sva <= va and va + size <= sva + rs:
                return self.bytes[ra + va - sva:ra + va - sva + size]
        return None


def tokens(data):
    words = struct.unpack("<%dI" % (len(data) // 4), data[:len(data) // 4 * 4])
    return [-1 if IMAGE_LO <= w < IMAGE_HI else w for w in words]


def ordered_runs(a, b):
    """Unique anchors present once in each side, longest increasing chain, grown into equal runs."""
    count_a = Counter(tuple(a[i:i + ANCHOR]) for i in range(len(a) - ANCHOR))
    count_b, where_b = Counter(), {}
    for j in range(len(b) - ANCHOR):
        key = tuple(b[j:j + ANCHOR])
        count_b[key] += 1
        where_b[key] = j
    pairs = []
    for i in range(len(a) - ANCHOR):
        key = tuple(a[i:i + ANCHOR])
        if count_a[key] == 1 and count_b.get(key) == 1 and len(set(key)) >= 3:
            pairs.append((i, where_b[key]))
    tails, tail_index, previous = [], [], [None] * len(pairs)
    for n, (_, j) in enumerate(pairs):
        k = bisect.bisect_left(tails, j)
        if k == len(tails):
            tails.append(j)
            tail_index.append(n)
        else:
            tails[k] = j
            tail_index[k] = n
        previous[n] = tail_index[k - 1] if k else None
    chain, n = [], tail_index[-1] if tail_index else None
    while n is not None:
        chain.append(pairs[n])
        n = previous[n]
    runs, floor_a, floor_b = [], 0, 0
    for i, j in reversed(chain):
        if i < floor_a or j < floor_b:
            continue
        back = 0
        while i - back - 1 >= floor_a and j - back - 1 >= floor_b and a[i - back - 1] == b[j - back - 1]:
            back += 1
        ahead = 0
        while i + ahead < len(a) and j + ahead < len(b) and a[i + ahead] == b[j + ahead]:
            ahead += 1
        runs.append((i - back, j - back, back + ahead))
        floor_a, floor_b = i + ahead, j + ahead
    return runs


def refine_gap(a, b):
    """Blocks inside one gap between aligned runs, as (i, j, n, pass) in gap-local dwords. A longest common
    subsequence of the masked tokens finds data that only shifted by a few dwords (a field moved ahead of a
    table); leftover dwords whose value is unique on both sides pair up wherever they sit; what is left between
    two matches with the same size on both sides changed in place."""
    out = []
    if not a or not b:
        return out
    n, m = len(a), len(b)
    if n * m <= 0x40000:
        table = [[0] * (m + 1) for _ in range(n + 1)]
        for i in range(n - 1, -1, -1):
            row, below = table[i], table[i + 1]
            for j in range(m - 1, -1, -1):
                row[j] = below[j + 1] + 1 if a[i] == b[j] else max(below[j], row[j + 1])
        pairs, i, j = [], 0, 0
        while i < n and j < m:
            if a[i] == b[j]:
                pairs.append((i, j))
                i += 1
                j += 1
            elif table[i + 1][j] >= table[i][j + 1]:
                i += 1
            else:
                j += 1
        # Lone matches of a common value (0, a pointer) prove nothing; keep runs of two or more.
        k = 0
        while k < len(pairs):
            end = k
            while end + 1 < len(pairs) and pairs[end + 1] == (pairs[end][0] + 1, pairs[end][1] + 1):
                end += 1
            if end > k:
                out.append((pairs[k][0], pairs[k][1], end - k + 1, "gap"))
            k = end + 1
    used_a = {i + d for i, _, size, _ in out for d in range(size)}
    used_b = {j + d for _, j, size, _ in out for d in range(size)}
    left_a = Counter(a[i] for i in range(n) if i not in used_a)
    left_b = Counter(b[j] for j in range(m) if j not in used_b)
    for i in range(n):
        if i in used_a or a[i] in (0, -1) or left_a[a[i]] != 1 or left_b.get(a[i]) != 1:
            continue
        j = next(j for j in range(m) if j not in used_b and b[j] == a[i])
        out.append((i, j, 1, "gap"))
        used_a.add(i)
        used_b.add(j)
    # Equal-size leftovers between consecutive in-order matches.
    anchors = sorted((i, j, size) for i, j, size, _ in out)
    floor_a = floor_b = 0
    for i, j, size in anchors + [(n, m, 0)]:
        if i >= floor_a and j >= floor_b:
            span_a = [x for x in range(floor_a, i) if x not in used_a]
            span_b = [y for y in range(floor_b, j) if y not in used_b]
            if span_a and len(span_a) == i - floor_a and len(span_b) == j - floor_b and i - floor_a == j - floor_b:
                out.append((floor_a, floor_b, i - floor_a, "replaced"))
            floor_a, floor_b = i + size, j + size
    return out


def data_blocks(pal, ntscj):
    blocks, covered = [], set()
    pal_tokens = {s: (pal.raw(s)[0], tokens(pal.raw(s)[1])) for s in DATA_SECTIONS}
    ntscj_tokens = {s: (ntscj.raw(s)[0], tokens(ntscj.raw(s)[1])) for s in DATA_SECTIONS}
    for s in DATA_SECTIONS:
        pva, a = pal_tokens[s]
        jva, b = ntscj_tokens[s]
        floor_a = floor_b = 0
        for i, j, n in ordered_runs(a, b) + [(len(a), len(b), 0)]:
            for gi, gj, size, kind in refine_gap(a[floor_a:i], b[floor_b:j]):
                blocks.append({"pal": pva + (floor_a + gi) * 4, "ntscj": jva + (floor_b + gj) * 4, "size": size * 4,
                               "pass": kind})
                covered.update(range(floor_a + gi + (pva >> 2), floor_a + gi + size + (pva >> 2)))
            if n:
                blocks.append({"pal": pva + i * 4, "ntscj": jva + j * 4, "size": n * 4, "pass": "ordered"})
                covered.update(range(i + (pva >> 2), i + n + (pva >> 2)))
            floor_a, floor_b = i + n, j + n
    index = defaultdict(list)
    for jva, b in ntscj_tokens.values():
        for j in range(len(b) - ANCHOR):
            index[tuple(b[j:j + ANCHOR])].append((jva, j))
    for s in DATA_SECTIONS:
        pva, a = pal_tokens[s]
        i = 0
        while i < len(a) - ANCHOR:
            if (pva >> 2) + i in covered:
                i += 1
                continue
            key = tuple(a[i:i + ANCHOR])
            hits = index.get(key, [])
            if len(set(key)) < 3 or len(hits) != 1:
                i += 1
                continue
            jva, j = hits[0]
            b = ntscj_tokens[".rdata" if jva == ntscj_tokens[".rdata"][0] else ".data"][1]
            back = 0
            while (i - back - 1 >= 0 and (pva >> 2) + i - back - 1 not in covered and j - back - 1 >= 0
                   and a[i - back - 1] == b[j - back - 1]):
                back += 1
            ahead = 0
            while (i + ahead < len(a) and (pva >> 2) + i + ahead not in covered and j + ahead < len(b)
                   and a[i + ahead] == b[j + ahead]):
                ahead += 1
            blocks.append({"pal": pva + (i - back) * 4, "ntscj": jva + (j - back) * 4, "size": (back + ahead) * 4,
                           "pass": "moved"})
            covered.update(range((pva >> 2) + i - back, (pva >> 2) + i + ahead))
            i += ahead
    blocks.sort(key=lambda block: block["pal"])
    total = sum(len(a) for _, a in pal_tokens.values())
    return blocks, len(covered) / total


def through_blocks(blocks, address):
    starts = [block["pal"] for block in blocks]
    k = bisect.bisect_right(starts, address) - 1
    if k >= 0 and address < blocks[k]["pal"] + blocks[k]["size"]:
        return blocks[k]["ntscj"] + address - blocks[k]["pal"]
    return None


def code_pattern(data):
    """Bytes as a regex with image addresses and the rel32 of call/jmp/jcc wildcarded: both move when the
    linker reorders functions."""
    out, i = b"", 0
    while i < len(data):
        if i + 4 <= len(data) and IMAGE_LO <= struct.unpack_from("<I", data, i)[0] < IMAGE_HI:
            out += b"...."
            i += 4
        elif data[i] in (0xE8, 0xE9) or (data[i] == 0x0F and i + 1 < len(data) and 0x80 <= data[i + 1] <= 0x8F):
            width = 1 if data[i] != 0x0F else 2
            out += re.escape(data[i:i + width]) + b"." * min(4, len(data) - i - width)
            i += width + 4
        else:
            out += re.escape(data[i:i + 1])
            i += 1
    return out


def code_operands(pal_text, ntscj_text, operand):
    """NTSC-J operands of the instructions that hold `operand` in PAL, by vote."""
    votes, key = Counter(), struct.pack("<I", operand)
    at = pal_text.find(key)
    while at >= 0:
        for width in (10, 16, 24, 32):
            if at < width:
                break
            rx = re.compile(code_pattern(pal_text[at - width:at]) + b"(....)" +
                            code_pattern(pal_text[at + 4:at + 4 + width]), re.S)
            hits = [m.group(1) for m in rx.finditer(ntscj_text)]
            if len(hits) == 1:
                votes[struct.unpack("<I", hits[0])[0]] += 1
                break
            # No hit can mean the window edge cut an address dword in half; a wider window may still match.
        at = pal_text.find(key, at + 1)
    return votes


def source_literals(pal):
    """Every hex literal outside comments in src/engine that lands in PAL's file-backed .rdata/.data.

    The port reads these through many helpers (memory, string, exe_real, real, word, data, ...), so the scan takes
    the address range rather than the call name. A literal used only as an identifier maps harmlessly."""
    ranges = [(pal.sections[s][0], pal.sections[s][0] + pal.sections[s][3]) for s in DATA_SECTIONS]
    found = defaultdict(set)
    literal = re.compile(r"\b0x([0-9A-Fa-f]{6,8})u?\b")
    for path in sorted((ROOT / "src" / "engine").rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), 1):
            for match in literal.finditer(line.split("//")[0]):
                value = int(match.group(1), 16)
                if any(lo <= value < hi for lo, hi in ranges):
                    found[value].add("%s:%d" % (path.relative_to(ROOT).as_posix(), line_number))
    return found


def gap_operands(pal, blocks, pal_text, known):
    """Addresses PAL code names inside data the blocks don't cover. The port can reach these by arithmetic
    rather than a literal, so the source scan misses them; the code still says where NTSC-J keeps them."""
    gaps = []
    for name in DATA_SECTIONS:
        va, _, _, rs = pal.sections[name]
        cursor = va
        for block in blocks:
            if va <= block["pal"] < va + rs:
                if block["pal"] > cursor:
                    gaps.append((cursor, block["pal"]))
                cursor = max(cursor, block["pal"] + block["size"])
        if cursor < va + rs:
            gaps.append((cursor, va + rs))
    starts = [g[0] for g in gaps]
    found = set()
    for at in range(len(pal_text) - 3):
        value = struct.unpack_from("<I", pal_text, at)[0]
        k = bisect.bisect_right(starts, value) - 1
        if k >= 0 and value < gaps[k][1] and value not in known:
            found.add(value)
    return sorted(found)


def resolve(literal, blocks, pal, ntscj, pal_text, ntscj_text):
    by_data = through_blocks(blocks, literal)
    direct = code_operands(pal_text, ntscj_text, literal)
    by_code, method = None, None
    if direct:
        by_code, method = direct.most_common(1)[0][0], "code"
    elif by_data is not None:
        # A base the code names near the literal, when the data blocks agree on where that base went.
        near = Counter()
        for word in range(literal - NEAR, literal + NEAR + 1, 4):
            if word == literal:
                continue
            for target, votes in code_operands(pal_text, ntscj_text, word).items():
                near[target + literal - word] += votes
        if near:
            by_code, method = near.most_common(1)[0][0], "code-near"
    entry = {"pal": literal, "ntscj": None, "method": None, "data": by_data, "code": by_code}
    if method == "code" and by_data is not None and by_data != by_code:
        # The instruction match can land on a neighbouring load (a constant 4 bytes off in a pool), and the
        # blocks can hold a base that only the code indexes past (a biased table base inside a string). The
        # item itself decides; failing that, the window around it, by a clear margin.
        code_same, data_same = same_item(pal, ntscj, literal, by_code), same_item(pal, ntscj, literal, by_data)
        if data_same:
            entry.update(ntscj=by_data, method="data (code disagrees)")
        elif code_same:
            entry.update(ntscj=by_code, method="code (data disagrees)")
        else:
            by_window = window_score(pal, ntscj, literal, by_code), window_score(pal, ntscj, literal, by_data)
            if by_window[0] >= 8 and by_window[0] >= 2 * by_window[1]:
                entry.update(ntscj=by_code, method="code (window)")
            elif by_window[1] >= 8 and by_window[1] >= 2 * by_window[0]:
                entry.update(ntscj=by_data, method="data (window)")
            else:
                entry.update(method="unresolved (code and data disagree)")
    elif method == "code":
        entry.update(ntscj=by_code, method="code+data" if by_data == by_code else "code")
    elif by_data is not None:
        # A nearby base only confirms the data blocks; on its own it guesses.
        entry.update(ntscj=by_data, method="data+code-near" if by_code == by_data else "data")
    elif (by_value := unique_value(pal, ntscj, literal)) is not None:
        # Weakest evidence: the item is the only one of its kind in both builds' data. The code reading it
        # changed, so this keeps the PAL reading working; it says nothing about what NTSC-J's code does with it.
        entry.update(ntscj=by_value, method="value")
    else:
        entry.update(method="unresolved")
    # Different bytes at the NTSC-J end mean the content changed between builds, so the translated PAL code
    # may not hold for NTSC-J.
    if entry["ntscj"] is not None:
        entry["same_bytes"] = same_item(pal, ntscj, literal, entry["ntscj"])
    return entry


def same_item(pal, ntscj, p, j):
    """The item the port reads: a string up to its terminator, otherwise one dword (pointers masked)."""
    head = pal.read(p, 0x80) or b""
    text = head.split(b"\0")[0]
    size = len(text) + 1 if len(text) >= 3 and all(32 <= c < 127 for c in text) else 4
    a, b = pal.read(p, size), ntscj.read(j, size)
    return a is not None and b is not None and (a == b or (size == 4 and tokens(a) == tokens(b)))


def item_bytes(xbe, p):
    head = xbe.read(p, 0x80) or b""
    text = head.split(b"\0")[0]
    if len(text) >= 3 and all(32 <= c < 127 for c in text):
        return text + b"\0"
    return xbe.read(p, 4)


def unique_value(pal, ntscj, p):
    """The NTSC-J address of the PAL item when its bytes occur exactly once in each build's data."""
    item = item_bytes(pal, p)
    if item is None or (len(item) == 4 and IMAGE_LO <= struct.unpack("<I", item)[0] < IMAGE_HI):
        return None

    def places(xbe):
        found = []
        for name in DATA_SECTIONS:
            va, raw = xbe.raw(name)
            at = raw.find(item)
            while at >= 0 and len(found) < 2:
                if len(item) > 4 or at % 4 == 0:
                    found.append(va + at)
                at = raw.find(item, at + 1)
        return found

    here, there = places(pal), places(ntscj)
    return there[0] if here == [p] and len(there) == 1 else None


def window_score(pal, ntscj, p, j, size=0x200):
    a, b = pal.read(p, size), ntscj.read(j, size)
    if a is None or b is None:
        return 0
    return sum(1 for x, y in zip(tokens(a), tokens(b)) if x == y and x != 0)


def section_of(xbe, address):
    for name, (va, vs, _, _) in xbe.sections.items():
        if va <= address < va + vs:
            return name
    return None


def item_extent(pal, ntscj, p, j, limit):
    """How far a code-proven item keeps the same shape in both builds: a string to its terminator, otherwise
    dwords that are addresses on both sides or equal, up to `limit` (the next PAL data the blocks hold)."""
    head = pal.read(p, 0x80) or b""
    text = head.split(b"\0")[0]
    if len(text) >= 3 and all(32 <= c < 127 for c in text):
        size = (len(text) + 4) & ~3
        a, b = pal.read(p, size), ntscj.read(j, size)
        return size if a is not None and a == b else len(text) + 1
    size = 0
    while size < min(limit, 0x400):
        a, b = pal.read(p + size, 4), ntscj.read(j + size, 4)
        if a is None or b is None:
            break
        x, y = struct.unpack("<I", a)[0], struct.unpack("<I", b)[0]
        if not (x == y or (IMAGE_LO <= x < IMAGE_HI and IMAGE_LO <= y < IMAGE_HI)):
            break
        size += 4
    return max(size, 4)


def code_blocks(pal, ntscj, blocks, entries):
    """Code-proven items whose PAL place no block holds become blocks of their own, so the view relocates the
    pointers inside them. An item whose PAL place a block already holds stays a redirect."""
    starts = [b["pal"] for b in blocks]
    added = []
    for e in sorted(entries, key=lambda e: e["pal"]):
        if e["ntscj"] is None or not e["method"].startswith("code") or "data disagrees" in e["method"]:
            continue
        if through_blocks(blocks, e["pal"]) is not None:
            continue
        k = bisect.bisect_right(starts, e["pal"])
        limit = (starts[k] if k < len(starts) else e["pal"] + 0x400) - e["pal"]
        if added and added[-1]["pal"] + added[-1]["size"] > e["pal"]:
            continue
        size = min(item_extent(pal, ntscj, e["pal"], e["ntscj"], limit), limit)
        added.append({"pal": e["pal"], "ntscj": e["ntscj"], "size": size, "pass": "code"})
        e["block"] = True
    return added


def relocations(pal, ntscj, blocks):
    """Aligned dwords whose value differs between the builds and that hold an image address in NTSC-J: the
    pointers the PAL-layout view has to rewrite. A pointer into data goes through the blocks at run time. A
    pointer into code (not .rdata/.data) takes the PAL value at the same position: the same slot of the same
    structure names the same function, and identical stubs folded together in one build stay exact."""
    positions, code = [], []
    for block in blocks:
        # Replaced and code-proven data wasn't aligned with pointers masked, so only a dword that is an address
        # on both sides counts as a pointer there. Replaced content changed, so its code pointers stay as they are.
        loose = block["pass"] in ("replaced", "code")
        for k in range(0, block["size"] & ~3, 4):
            p = struct.unpack("<I", pal.read(block["pal"] + k, 4))[0]
            j = struct.unpack("<I", ntscj.read(block["ntscj"] + k, 4))[0]
            if p != j and IMAGE_LO <= j < IMAGE_HI and (not loose or IMAGE_LO <= p < IMAGE_HI):
                if section_of(ntscj, j) in DATA_SECTIONS:
                    positions.append(block["pal"] + k)
                elif block["pass"] != "replaced":
                    code.append((block["pal"] + k, p))
    return sorted(positions), sorted(code)


def redirects(entries, blocks):
    """Literals the code moved somewhere the blocks don't send them: exact-address redirects. A redirect reads
    raw NTSC-J bytes (no pointer relocation), so an address the view already maps the same way gets none."""
    out = []
    for e in entries:
        if e["ntscj"] is not None and e["ntscj"] != e["pal"] and through_blocks(blocks, e["pal"]) != e["ntscj"]:
            out.append((e["pal"], e["ntscj"]))
    return out


def missing(entries):
    """Literals with no NTSC-J answer. Some sit inside a data block, where the view would otherwise serve the
    block's bytes; the runtime refuses them instead."""
    return [e["pal"] for e in entries if e["ntscj"] is None]


def write_inl(path, pal, ntscj, blocks, positions, code, moved, absent):
    def rows(values, per_line, fmt):
        lines = []
        for i in range(0, len(values), per_line):
            lines.append("    " + " ".join(fmt(v) for v in values[i:i + per_line]))
        return "\n".join(lines)
    sections = [(pal.sections[s][0], pal.sections[s][3]) for s in DATA_SECTIONS]
    text = [
        "// Generated by tools/regions/xbe_address_map.py from the user's PAL and NTSC-J default.xbe. Addresses",
        "// only, no game bytes. Do not edit; rerun the tool.",
        "// PAL timestamp 0x%08X, NTSC-J timestamp 0x%08X, %d blocks, %d relocations, %d code pointers, %d redirects," % (
            pal.timestamp, ntscj.timestamp, len(blocks), len(positions), len(code), len(moved)),
        "// %d missing." % len(absent),
        "",
        "constexpr u32 kPalTimestamp = 0x%08X;" % pal.timestamp,
        "constexpr u32 kNtscjTimestamp = 0x%08X;" % ntscj.timestamp,
        "",
        "// PAL file-backed data sections the view rebuilds: virtual address, raw size.",
        "constexpr NtscjSection kPalDataSections[] = {",
        rows(sections, 2, lambda s: "{0x%08X, 0x%X}," % s),
        "};",
        "",
        "// The same data at both ends: PAL address, NTSC-J address, size.",
        "constexpr NtscjBlock kBlocks[] = {",
        rows([(b["pal"], b["ntscj"], b["size"]) for b in blocks], 3, lambda b: "{0x%08X, 0x%08X, 0x%X}," % b),
        "};",
        "",
        "// PAL addresses of the dwords in the blocks that hold a pointer into data the view rewrites.",
        "constexpr u32 kRelocations[] = {",
        rows(positions, 8, lambda v: "0x%08X," % v),
        "};",
        "",
        "// Dwords in the blocks that point into code and libraries: PAL address, PAL target.",
        "constexpr NtscjPair kCodePointers[] = {",
        rows(code, 4, lambda c: "{0x%08X, 0x%08X}," % c),
        "};",
        "",
        "// Literals whose NTSC-J item the code found away from the blocks: PAL address, NTSC-J address.",
        "constexpr NtscjPair kRedirects[] = {",
        rows(moved, 4, lambda c: "{0x%08X, 0x%08X}," % c),
        "};",
        "",
        "// Literals with no NTSC-J answer: the code reading them differs between the builds.",
        "constexpr u32 kMissing[] = {",
        rows(absent, 8, lambda v: "0x%08X," % v),
        "};",
        "",
    ]
    path.write_text("\n".join(text), encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pal", required=True, type=Path)
    parser.add_argument("--ntscj", required=True, type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "cache" / "address_maps" / "ntsc_j.json")
    parser.add_argument("--inl", type=Path, default=ROOT / "src" / "engine" / "scene" / "ntsc_j_address_map.inl")
    args = parser.parse_args()
    pal, ntscj = Xbe(args.pal), Xbe(args.ntscj)
    blocks, coverage = data_blocks(pal, ntscj)
    pal_text, ntscj_text = pal.raw(".text")[1], ntscj.raw(".text")[1]
    literals = source_literals(pal)
    entries = []
    for literal in sorted(literals):
        entry = resolve(literal, blocks, pal, ntscj, pal_text, ntscj_text)
        entry["uses"] = sorted(literals[literal])
        entries.append(entry)
    # Gap operands only add redirects the code proves (same bytes at both ends); the rest stay gaps.
    for operand in gap_operands(pal, blocks, pal_text, set(literals)):
        entry = resolve(operand, blocks, pal, ntscj, pal_text, ntscj_text)
        if entry["method"] == "code" and entry.get("same_bytes"):
            entry["uses"] = ["PAL code operand in a data gap"]
            entries.append(entry)
    blocks = sorted(blocks + code_blocks(pal, ntscj, blocks, entries), key=lambda block: block["pal"])
    positions, code = relocations(pal, ntscj, blocks)
    hexed = lambda v: None if v is None else "0x%08X" % v
    out = {
        "pal_timestamp": "0x%08X" % pal.timestamp,
        "ntscj_timestamp": "0x%08X" % ntscj.timestamp,
        "data_coverage": round(coverage, 5),
        "literals": [dict(e, pal=hexed(e["pal"]), ntscj=hexed(e["ntscj"]), data=hexed(e["data"]),
                          code=hexed(e["code"])) for e in entries],
        "blocks": [dict(b, pal=hexed(b["pal"]), ntscj=hexed(b["ntscj"]), size="0x%X" % b["size"]) for b in blocks],
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(out, indent=1) + "\n", encoding="utf-8")
    write_inl(args.inl, pal, ntscj, blocks, positions, code, redirects(entries, blocks), missing(entries))
    counts = Counter(e["method"] for e in entries)
    print("data coverage %.2f%%, %d blocks; %d literals: %s" % (
        100 * coverage, len(blocks), len(entries), ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
    for e in entries:
        if e["method"].startswith("unresolved") or "disagrees" in e["method"] or "window" in e["method"]:
            print("  0x%08X -> %s  %s%s  %s" % (e["pal"], hexed(e["ntscj"]), e["method"],
                                               "" if e.get("same_bytes", True) else ", bytes differ", e["uses"][0]))
    print("%d relocations, %d code pointers" % (len(positions), len(code)))
    for path in (args.out, args.inl):
        print("wrote", path.relative_to(ROOT) if path.is_relative_to(ROOT) else path)


if __name__ == "__main__":
    main()
