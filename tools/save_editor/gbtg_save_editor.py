#!/usr/bin/env python3
"""Standalone save editor for Grabbed by the Ghoulies saves (progress.xsv).

Each save is a folder under the save root (the port's ``saves`` beside gbtg.exe, or an Xbox ``UDATA\\4d530053``
dump) holding SaveMeta.xbx, SaveImage.xbx and progress.xsv. progress.xsv is written by 0x0010EB60:

    +0x000  the 0x268-byte save record (0x0010E9D0)
    +0x268  story bits, 8 bytes (kind 0x3E, 62 bits)
    +0x270  challenge books, 13 bytes (kind 100, books 1-100)
    +0x27D  unlock block, 1 byte used (13 written: the challenge block's size is reused)
    ...     zero padding up to the volume's sector size
    -0x14   XCalculateSignature (flags 0): HMAC-SHA1 over everything before it, keyed with the title's
            XboxSignatureKey (src/engine/runtime/xapi_runtime.cpp)

The game rejects a save whose signature does not match (0x000E9B70), so every write re-signs the file.
Unknown bytes are kept as they are.

Usage:
    python tools/save_editor/gbtg_save_editor.py [SAVE_ROOT | SAVE_FOLDER | progress.xsv]
    python tools/save_editor/gbtg_save_editor.py --dump PATH
    python tools/save_editor/gbtg_save_editor.py --set FIELD=VALUE [--set ...] PATH
    python tools/save_editor/gbtg_save_editor.py --resign PATH
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import shutil
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

SIGNATURE_KEY = bytes.fromhex("2618C508888E75B9363E23DA8FE02D37")  # XboxSignatureKey, title 0x4D530053
SIGNATURE_SIZE = 0x14
RECORD_SIZE = 0x268
PROGRESS_OFFSET, PROGRESS_SIZE = 0x268, 8
CHALLENGE_BITS_OFFSET, CHALLENGE_BITS_SIZE = 0x270, 13
UNLOCKS_OFFSET, UNLOCKS_SIZE = 0x27D, 1
PROGRESS_BITS = 0x3E
CHALLENGE_BITS = 100
SCORES_OFFSET = 0x1B4  # 21 best scores (-1 none), DAT_004E18F8
TROPHIES_OFFSET = 0x208  # 21 trophy tiers, DAT_004E194C
CHALLENGE_COUNT = 0x15
TROPHIES = ["Unplayed", "Failed", "None", "Bronze", "Silver", "Gold", "Platinum"]  # PTR_s__unplayed_004232d0
REPO_ROOT = Path(__file__).resolve().parents[2]

# The progress block is the game's story bits (*(DAT_00506C6C + 8)). Every reader and writer in the XBE (the bit
# helpers 0x000F29A0..0x000F2A20, the inlined byte tests) and every 0x9D / 0x9E node in the 157 scene scripts was
# checked; bits 0, 2 and 32 have none.
_INTROS = ["Baron", "Ghoulie girl", "Grim Reaper", "Haunted chair", "Haunted coat", "Haunted picture",
           "Haunted TV", "Hunchback", "Fire imp", "Flamethrower imp", "Flying imp", "Ninja imp", "Jessie Clyde",
           "Medusa", "Mummy", "Cursed mummy", "Pirate", "Skeleton", "Spider", "Vampire", "Vampire chicken",
           "Warlock", "Worm", "Zombie"]  # The ids each strategy passes to 0x0013A430 / 0x0013A4F0.
# The book map's quest items (0x002FD408): the marker shows once the first bit is set and goes once the second is,
# each bit set by the scene in brackets (script action 0x9E).
_MAP_ITEMS = [("Scroll 1", 39, "3a scene 3", 45, "3a scene 12"), ("Scroll 2", 40, "3b scene 1", 46, "3b scene 7"),
              ("Scroll 3", 41, "3b scene 7", 47, "3c scene 13"), ("Egg", 43, "2a scene 17", 49, "2b scene 5"),
              ("Glow worms", 42, "2a scene 1", 48, "2a scene 9"), ("Dung weed", 44, "2b scene 10", 50, "2c scene 5")]
PROGRESS_NAMES = {
    0: "(unused)",
    1: "Player boost used before",  # 0x00137A80: the first boost lasts 2.5 s, later ones 5 s
    2: "(unused)",
    3: "Imp hat intro seen",  # 0x0013FE60
    4: "Death curse intro seen",  # 0x0013FEB0
    5: "Super-scary shock explained",  # chapter 1 scene 2 (0x00084D64)
    6: "Health randomizer intro seen",  # 0x001301A0 / 0x001307D0
    7: "Powerup intro seen",  # 0x0010A590; a challenge sets it for its run and puts it back (0x0004A240)
    **{8 + index: f"{name} intro seen" for index, name in enumerate(_INTROS)},  # 0x000F4870 skips a set one
    32: "(unused)",
    33: "Butler's Brew offer seen",  # 0x0008F4D0, "intros_brewoffer"
    34: "Butler's Brew accept intro seen",  # 0x0013FF30, "intros_brewaccept"
    35: "Map in the pause menu",  # chapter 1 scene 1 and the trophy heads set it; 0x0011DD90 picks the map rows
    36: "First platinum notice seen",
    37: "Challenge 21 unlocked",  # 20 gold or platinum trophies; game select hides it without the bit
    38: "Challenge 21 platinum (gold bat)",  # the dialogue bat turns gold
    **{shown: f"Map: {item} shown ({at})" for item, shown, at, _, _ in _MAP_ITEMS},
    **{gone: f"Map: {item} removed ({at})" for item, _, _, gone, at in _MAP_ITEMS},
    # Chapter 5's scenes each set one once with script action 0x9D, which adds one to the +0x198 stat (the final
    # score's prisoners row); the countdown clears them all when it starts (0x000F27C0).
    **{0x33 + index: f"Chapter 5 scene {scene} prisoner"
       for index, scene in enumerate([2, 3, 4, 5, 6, 7, 8, 9, 10, 13])},
    61: "Game-select secret unlocked",  # 0x000DB380
}
# The unlock byte is the game options (0x000E0B60). A load keeps only 0x2, 0x4 and 0x8 (0x000F2AC0).
UNLOCK_NAMES = {
    0: "(unused, dropped on load)",
    1: "Camera inverted",
    2: "Rumble off",
    3: "Butler's Brew on",
    **{bit: "(unused, dropped on load)" for bit in range(4, 8)},
}


@dataclass(frozen=True)
class Field:
    key: str
    offset: int
    kind: str  # u8, u32, i32, f32, script
    label: str
    note: str = ""


FIELDS = [
    Field("script", 0x102, "script", "Resume scene", "+0x102: the scene a load starts (0x0010E220)"),
    Field("slot", 0x25C, "u8", "Slot", "+0x25C: the game-select slot (0x0010E270)"),
    Field("new_game", 0x184, "u32", "New-game flag", "+0x184: 1 on a new save, cleared by the first save"),
    Field("finished", 0x188, "u32", "Finished", "+0x188: set by the final score (0x0010E310)"),
    Field("play_seconds", 0x18C, "i32", "Play time (seconds)", "+0x18C: the final score's time row"),
    Field("kills", 0x190, "i32", "Kills", "+0x190: the final score's kills row"),
    Field("faints", 0x194, "i32", "Faints", "+0x194: the final score's faints row"),
    Field("prisoners", 0x198, "i32", "Prisoners", "+0x198: the final score's prisoners row"),
    Field("stat_19c", 0x19C, "i32", "Stat +0x19C", "+0x19C: the fifth profile time, kept by the final score"),
    Field("challenge_steps", 0x1A0, "i32", "Challenge 21 scene",
          "+0x1A0: scenes into challenge 21's order; it resumes there (0 = first)"),
    Field("challenge_time", 0x1A4, "f32", "Challenge 21 time (s)",
          "+0x1A4: challenge 21's seconds so far; the next scene's time adds to it"),
    Field("book_scene", 0x25D, "u8", "Chapter 5 book scene", "+0x25D: furthest chapter 5 book scene (0x0010E8A0)"),
    Field("gold_beaten", 0x260, "u32", "Countdown gold", "+0x260: chapter 5 countdown beaten with gold"),
    Field("final_book", 0x264, "u32", "Final book scene", "+0x264: final book scene reached"),
]
FIELD_BY_KEY = {field.key: field for field in FIELDS}


def signature(data: bytes) -> bytes:
    return hmac.new(SIGNATURE_KEY, data, hashlib.sha1).digest()


class SaveFile:
    def __init__(self, path: Path):
        self.path = Path(path)
        self.data = bytearray(self.path.read_bytes())
        minimum = UNLOCKS_OFFSET + UNLOCKS_SIZE + SIGNATURE_SIZE
        if len(self.data) < minimum:
            raise ValueError(f"{self.path}: {len(self.data)} bytes, expected at least {minimum}")
        self.signature_ok = signature(bytes(self.data[:-SIGNATURE_SIZE])) == bytes(self.data[-SIGNATURE_SIZE:])
        self.meta_name = read_meta_name(self.path.parent / "SaveMeta.xbx")

    # Fields
    def get(self, field: Field):
        offset = field.offset
        if field.kind == "u8":
            return self.data[offset]
        if field.kind == "u32":
            return struct.unpack_from("<I", self.data, offset)[0]
        if field.kind == "i32":
            return struct.unpack_from("<i", self.data, offset)[0]
        if field.kind == "f32":
            return struct.unpack_from("<f", self.data, offset)[0]
        if field.kind == "script":
            raw = bytes(self.data[offset:0x182])
            return raw.split(b"\0", 1)[0].decode("ascii", "replace")
        raise ValueError(field.kind)

    def set(self, field: Field, value) -> None:
        offset = field.offset
        if field.kind == "u8":
            value = int(value, 0) if isinstance(value, str) else int(value)
            if not 0 <= value <= 0xFF:
                raise ValueError(f"{field.label}: {value} is not a byte")
            self.data[offset] = value
        elif field.kind == "u32":
            value = int(value, 0) if isinstance(value, str) else int(value)
            struct.pack_into("<I", self.data, offset, value & 0xFFFFFFFF)
        elif field.kind == "i32":
            value = int(value, 0) if isinstance(value, str) else int(value)
            struct.pack_into("<i", self.data, offset, value)
        elif field.kind == "f32":
            struct.pack_into("<f", self.data, offset, float(value))
        elif field.kind == "script":
            encoded = str(value).encode("ascii")
            if len(encoded) >= 0x80:
                raise ValueError("script names are at most 127 characters")
            # strcpy: the name and its NUL; the bytes after it are left as they were.
            self.data[offset:offset + len(encoded) + 1] = encoded + b"\0"
        else:
            raise ValueError(field.kind)

    @property
    def record_name(self) -> str:
        raw = bytes(self.data[2:0x102])
        chars = []
        for index in range(0, len(raw), 2):
            code = raw[index] | (raw[index + 1] << 8)
            if code == 0:
                break
            chars.append(chr(code))
        return "".join(chars)

    def score(self, index: int) -> int:
        return struct.unpack_from("<i", self.data, SCORES_OFFSET + index * 4)[0]

    def set_score(self, index: int, value: int) -> None:
        struct.pack_into("<i", self.data, SCORES_OFFSET + index * 4, value)

    def trophy(self, index: int) -> int:
        return struct.unpack_from("<i", self.data, TROPHIES_OFFSET + index * 4)[0]

    def set_trophy(self, index: int, value: int) -> None:
        struct.pack_into("<i", self.data, TROPHIES_OFFSET + index * 4, value)

    def bit(self, base: int, index: int) -> bool:
        return bool(self.data[base + index // 8] & (1 << (index % 8)))

    def set_bit(self, base: int, index: int, on: bool) -> None:
        mask = 1 << (index % 8)
        if on:
            self.data[base + index // 8] |= mask
        else:
            self.data[base + index // 8] &= ~mask & 0xFF

    # Writing
    def resign(self) -> None:
        self.data[-SIGNATURE_SIZE:] = signature(bytes(self.data[:-SIGNATURE_SIZE]))
        self.signature_ok = True

    def write(self, path: Path | None = None) -> Path | None:
        """Re-signs and writes; the first write beside an untouched file keeps it as progress.xsv.bak."""
        target = Path(path) if path else self.path
        backup = None
        if target.exists():
            backup = target.with_name(target.name + ".bak")
            if not backup.exists():
                shutil.copy2(target, backup)
            else:
                backup = None
        self.resign()
        target.write_bytes(bytes(self.data))
        return backup


def read_meta_name(path: Path) -> str:
    try:
        text = path.read_bytes().decode("utf-16")
    except (OSError, UnicodeDecodeError):
        return ""
    for line in text.splitlines():
        if line.startswith("Name="):
            return line[5:]
    return ""


def find_saves(root: Path) -> list[Path]:
    root = Path(root)
    if root.is_file():
        return [root]
    if (root / "progress.xsv").is_file():
        return [root / "progress.xsv"]
    return sorted(root.glob("*/progress.xsv"))


def default_save_root() -> Path:
    for candidate in ("build/Release/saves", "build-opt/Release/saves", "build/RelWithDebInfo/saves",
                      "build/Debug/saves"):
        path = REPO_ROOT / candidate
        if path.is_dir():
            return path
    return Path.cwd()


def scene_scripts() -> list[str]:
    names = REPO_ROOT / "docs" / "generated" / "aid_names.txt"
    try:
        text = names.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    import re

    found = set(re.findall(r"aid_script_ghoulies_(?:chapter\d\w*_scene\w+|challenges_\w+|gameselect_\w+)", text))
    return sorted(found, key=natural_key)


def natural_key(text: str):
    import re

    return [int(part) if part.isdigit() else part for part in re.split(r"(\d+)", text)]


def bits_text(save: SaveFile, base: int, count: int) -> str:
    on = [str(index) for index in range(count) if save.bit(base, index)]
    return f"{len(on)}/{count} set" + (f": {', '.join(on)}" if on else "")


def dump(save: SaveFile) -> str:
    lines = [
        f"File:            {save.path}",
        f"Size:            {len(save.data)} bytes",
        f"Signature:       {'valid' if save.signature_ok else 'INVALID'}",
        f"SaveMeta name:   {save.meta_name}",
        f"Record name:     {save.record_name}",
        f"Record byte 0:   0x{save.data[0]:02X}",
    ]
    for field in FIELDS:
        value = save.get(field)
        if field.kind == "f32":
            value = f"{value:g}"
        lines.append(f"{field.key:16} {value}    ({field.note})")
    lines.append("Challenges (best score / trophy):")
    for index in range(CHALLENGE_COUNT):
        trophy = save.trophy(index)
        name = TROPHIES[trophy] if 0 <= trophy < len(TROPHIES) else f"? {trophy}"
        lines.append(f"  {index + 1:2}: {save.score(index):>8} / {name}")
    lines.append(f"Progress bits:   {bits_text(save, PROGRESS_OFFSET, PROGRESS_BITS)}")
    for bit, name in sorted(PROGRESS_NAMES.items()):
        lines.append(f"  {bit:2} {'x' if save.bit(PROGRESS_OFFSET, bit) else '-'} {name}")
    books = [str(index + 1) for index in range(CHALLENGE_BITS) if save.bit(CHALLENGE_BITS_OFFSET, index)]
    lines.append(f"Challenge books: {len(books)}/{CHALLENGE_BITS} found" + (f": {', '.join(books)}" if books else ""))
    lines.append(f"Unlock bits:     {bits_text(save, UNLOCKS_OFFSET, 8)}")
    for bit, name in UNLOCK_NAMES.items():
        lines.append(f"  {bit:2} {'x' if save.bit(UNLOCKS_OFFSET, bit) else '-'} {name}")
    return "\n".join(lines)


def apply_set(save: SaveFile, assignment: str) -> None:
    key, _, value = assignment.partition("=")
    key = key.strip()
    if key in FIELD_BY_KEY:
        save.set(FIELD_BY_KEY[key], value)
        return
    # score.N / trophy.N / book.N (1-based), progress.N / unlock.N (0-based bits).
    name, _, index_text = key.partition(".")
    index = int(index_text, 0)
    if name == "score":
        save.set_score(index - 1, int(value, 0))
    elif name == "trophy":
        tier = TROPHIES.index(value.capitalize()) if value.capitalize() in TROPHIES else int(value, 0)
        save.set_trophy(index - 1, tier)
    elif name == "progress":
        save.set_bit(PROGRESS_OFFSET, index, value not in ("0", "false", "off"))
    elif name == "book":
        save.set_bit(CHALLENGE_BITS_OFFSET, index - 1, value not in ("0", "false", "off"))
    elif name == "unlock":
        save.set_bit(UNLOCKS_OFFSET, index, value not in ("0", "false", "off"))
    else:
        raise SystemExit(f"unknown field {key!r}; fields: {', '.join(FIELD_BY_KEY)}, score.N, trophy.N, "
                         "progress.N, book.N, unlock.N")


# GUI ------------------------------------------------------------------------------------------------------------


def run_gui(start: Path) -> None:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    root = tk.Tk()
    root.title("Ghoulies Save Editor")
    root.geometry("1120x660")
    root.minsize(760, 520)

    state = {"saves": [], "save": None, "root": start}
    scripts = scene_scripts()

    top = ttk.Frame(root, padding=6)
    top.pack(fill="x")
    root_label = ttk.Label(top, text="")
    root_label.pack(side="left", fill="x", expand=True)
    status = ttk.Label(root, text="", padding=(6, 2), anchor="w", relief="sunken")
    status.pack(side="bottom", fill="x")

    body = ttk.PanedWindow(root, orient="horizontal")
    body.pack(fill="both", expand=True, padx=6, pady=(0, 6))
    left = ttk.Frame(body)
    saves_list = tk.Listbox(left, exportselection=False, width=30)
    saves_list.pack(fill="both", expand=True)
    body.add(left, weight=1)

    right = ttk.Frame(body)
    body.add(right, weight=4)
    notebook = ttk.Notebook(right)
    notebook.pack(fill="both", expand=True)
    buttons = ttk.Frame(right, padding=(0, 6, 0, 0))
    buttons.pack(fill="x")

    # General tab: header and the record's fields.
    general = ttk.Frame(notebook, padding=10)
    notebook.add(general, text="General")
    header = ttk.Label(general, text="", justify="left")
    header.grid(row=0, column=0, columnspan=3, sticky="w", pady=(0, 8))
    field_vars: dict[str, tk.StringVar] = {}
    shown: dict[str, str] = {}
    for row, field in enumerate(FIELDS, start=1):
        ttk.Label(general, text=field.label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
        var = tk.StringVar()
        field_vars[field.key] = var
        if field.kind == "script":
            widget = ttk.Combobox(general, textvariable=var, values=scripts, width=52)
        else:
            widget = ttk.Entry(general, textvariable=var, width=16)
        widget.grid(row=row, column=1, sticky="w", pady=2)
        ttk.Label(general, text=field.note, foreground="#666").grid(row=row, column=2, sticky="w", padx=8)
    general.columnconfigure(2, weight=1)

    # Challenges tab: 21 best scores and trophies.
    challenges = ttk.Frame(notebook, padding=10)
    notebook.add(challenges, text="Challenges")
    ttk.Label(challenges, text="Best score is -1 when the challenge has none. Challenge 21's trophy is the "
              "platinum run.", foreground="#666").grid(row=0, column=0, columnspan=4, sticky="w", pady=(0, 6))
    score_vars, trophy_vars = [], []
    for index in range(CHALLENGE_COUNT):
        column = (index // 11) * 4
        row = index % 11 + 1
        ttk.Label(challenges, text=f"Challenge {index + 1}").grid(row=row, column=column, sticky="w", padx=(0, 6))
        score = tk.StringVar()
        trophy = tk.StringVar()
        ttk.Entry(challenges, textvariable=score, width=10).grid(row=row, column=column + 1, pady=2)
        ttk.Combobox(challenges, textvariable=trophy, values=TROPHIES, width=10, state="readonly").grid(
            row=row, column=column + 2, padx=(4, 24), pady=2)
        score_vars.append(score)
        trophy_vars.append(trophy)
    tier_row = ttk.Frame(challenges)
    tier_row.grid(row=13, column=0, columnspan=8, sticky="w", pady=(10, 0))
    ttk.Label(tier_row, text="Set every trophy to:").pack(side="left")
    all_tier = tk.StringVar(value="Gold")
    ttk.Combobox(tier_row, textvariable=all_tier, values=TROPHIES, width=10, state="readonly").pack(
        side="left", padx=6)
    ttk.Button(tier_row, text="Apply", command=lambda: [var.set(all_tier.get()) for var in trophy_vars]).pack(
        side="left")

    # Bit tabs.
    def bit_tab(title: str, count: int, columns: int, note: str, label=lambda index: f"{index:3}",
                down=False):
        frame = ttk.Frame(notebook, padding=10)
        notebook.add(frame, text=title)
        ttk.Label(frame, text=note, foreground="#666").grid(row=0, column=0, columnspan=columns, sticky="w")
        rows = (count + columns - 1) // columns
        variables = []
        for index in range(count):
            var = tk.BooleanVar()
            row, column = (index % rows, index // rows) if down else (index // columns, index % columns)
            ttk.Checkbutton(frame, text=label(index), variable=var).grid(
                row=row + 1, column=column, sticky="w", padx=2)
            variables.append(var)
        row = rows + 1
        controls = ttk.Frame(frame)
        controls.grid(row=row, column=0, columnspan=columns, sticky="w", pady=(8, 0))
        ttk.Button(controls, text="All", command=lambda: [v.set(True) for v in variables]).pack(side="left")
        ttk.Button(controls, text="None", command=lambda: [v.set(False) for v in variables]).pack(
            side="left", padx=6)
        return variables

    progress_vars = bit_tab("Story bits", PROGRESS_BITS, 3, "Story bits (+0x268, DAT_00506C6C).",
                            lambda index: f"{index:2}  {PROGRESS_NAMES.get(index, '')}", down=True)
    challenge_bit_vars = bit_tab("Challenge books", CHALLENGE_BITS, 10,
                                 "Challenge books 1-100 (+0x270, DAT_00506C54).",
                                 lambda index: f"Book {index + 1}")
    unlock_vars = bit_tab("Options", 8, 1, "Game options (+0x27D, DAT_00506C70).",
                          lambda index: f"{index}  {UNLOCK_NAMES[index]}")

    def set_status(text: str) -> None:
        status.config(text=text)

    def show(save: SaveFile) -> None:
        state["save"] = save
        sig = "valid" if save.signature_ok else "INVALID (will be re-signed on save)"
        header.config(text=f"{save.path}\nSaveMeta name: {save.meta_name}    Record name: {save.record_name}"
                           f"    Size: {len(save.data)} bytes    Signature: {sig}")
        for field in FIELDS:
            value = save.get(field)
            shown[field.key] = f"{value:g}" if field.kind == "f32" else str(value)
            field_vars[field.key].set(shown[field.key])
        for index in range(CHALLENGE_COUNT):
            score_vars[index].set(str(save.score(index)))
            tier = save.trophy(index)
            trophy_vars[index].set(TROPHIES[tier] if 0 <= tier < len(TROPHIES) else str(tier))
        for index, var in enumerate(progress_vars):
            var.set(save.bit(PROGRESS_OFFSET, index))
        for index, var in enumerate(challenge_bit_vars):
            var.set(save.bit(CHALLENGE_BITS_OFFSET, index))
        for index, var in enumerate(unlock_vars):
            var.set(save.bit(UNLOCKS_OFFSET, index))
        set_status(f"Loaded {save.meta_name or save.path.parent.name}")

    def collect(save: SaveFile) -> None:
        for field in FIELDS:
            text = field_vars[field.key].get().strip()
            if text != shown.get(field.key):  # Untouched fields keep their exact bytes.
                save.set(field, text)
        for index in range(CHALLENGE_COUNT):
            save.set_score(index, int(score_vars[index].get().strip(), 0))
            text = trophy_vars[index].get()
            save.set_trophy(index, TROPHIES.index(text) if text in TROPHIES else int(text, 0))
        for index, var in enumerate(progress_vars):
            save.set_bit(PROGRESS_OFFSET, index, var.get())
        for index, var in enumerate(challenge_bit_vars):
            save.set_bit(CHALLENGE_BITS_OFFSET, index, var.get())
        for index, var in enumerate(unlock_vars):
            save.set_bit(UNLOCKS_OFFSET, index, var.get())

    def load_root(path: Path) -> None:
        state["root"] = path
        state["saves"] = find_saves(path)
        root_label.config(text=f"Save root: {path}")
        saves_list.delete(0, "end")
        for save_path in state["saves"]:
            name = read_meta_name(save_path.parent / "SaveMeta.xbx") or "(no name)"
            saves_list.insert("end", f"{name}  [{save_path.parent.name}]")
        if state["saves"]:
            saves_list.selection_set(0)
            select()
        else:
            set_status("No progress.xsv found here")

    def select(_event=None) -> None:
        picked = saves_list.curselection()
        if not picked:
            return
        try:
            show(SaveFile(state["saves"][picked[0]]))
        except (OSError, ValueError) as error:
            messagebox.showerror("Open failed", str(error))

    def save_current() -> None:
        save = state["save"]
        if save is None:
            return
        try:
            collect(save)
            backup = save.write()
        except (OSError, ValueError) as error:
            messagebox.showerror("Save failed", str(error))
            return
        show(save)
        set_status(f"Saved and re-signed {save.path}" + (f" (original kept as {backup.name})" if backup else ""))

    def revert() -> None:
        if state["save"] is not None:
            show(SaveFile(state["save"].path))

    def choose_root() -> None:
        folder = filedialog.askdirectory(initialdir=str(state["root"]), title="Save root (folder of save folders)")
        if folder:
            load_root(Path(folder))

    ttk.Button(top, text="Open folder...", command=choose_root).pack(side="right")
    ttk.Button(top, text="Reload", command=lambda: load_root(state["root"])).pack(side="right", padx=6)
    ttk.Button(buttons, text="Save", command=save_current).pack(side="right")
    ttk.Button(buttons, text="Revert", command=revert).pack(side="right", padx=6)
    ttk.Label(buttons, text="Close the game before saving: it keeps loaded saves in memory and writes them back.",
              foreground="#666").pack(side="left")
    saves_list.bind("<<ListboxSelect>>", select)
    root.bind("<Control-s>", lambda _e: save_current())

    load_root(start)
    root.mainloop()


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("path", nargs="?", type=Path, help="save root, save folder or progress.xsv")
    parser.add_argument("--dump", action="store_true", help="print every save's fields and exit")
    parser.add_argument("--set", action="append", default=[], metavar="FIELD=VALUE",
                        help="change a field and write the save (repeatable)")
    parser.add_argument("--resign", action="store_true", help="re-sign the save without changing it")
    args = parser.parse_args(argv)
    start = args.path or default_save_root()

    if args.dump or args.set or args.resign:
        saves = find_saves(start)
        if not saves:
            print(f"no progress.xsv under {start}", file=sys.stderr)
            return 1
        if (args.set or args.resign) and len(saves) != 1:
            print("--set and --resign take one save folder or progress.xsv", file=sys.stderr)
            return 1
        for path in saves:
            save = SaveFile(path)
            if args.set or args.resign:
                for assignment in args.set:
                    apply_set(save, assignment)
                backup = save.write()
                print(f"wrote {path}" + (f" (original kept as {backup.name})" if backup else ""))
            if args.dump:
                print(dump(save))
                print()
        return 0

    run_gui(start)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
