#!/usr/bin/env python3
"""Address-named globals: fail on any `g_<hex address>` name outside the list below.

Globals are named for what they hold, with the retail address in a `// DAT_........` comment beside the declaration
(AGENTS.md, "Source organization and names"). The names below keep their address for a recorded reason; remove a
name here when it is renamed. Exits 1 and lists the offenders when a new address name appears.

    python tools/layout/address_names.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Another host global already holds the same retail address. These are separate host variables for one retail
# variable, so merging them changes behavior; each needs its own fix before it can be renamed.
DUPLICATES = {
    # The play bounds: background_geometry.cpp writes g_background_collision_bounds_*, the bodies read these.
    "g_50ea01", "g_50ea7c", "g_50ea80", "g_50ea84", "g_50ea88", "g_50ea8c", "g_50ea90", "g_50ea94",
    # The pause's pad-shake hold: the shakes read g_pad_shakes_held (actor_camera_effects.cpp).
    "g_507df4",
    # The light table slots: lighting.cpp's g_light_table_slots.
    "g_50d770", "g_50d774", "g_50d778", "g_50d77c", "g_50d780",
    # The background regions' objects: shadow_volume_pass.cpp's regions.
    "g_50eaa0", "g_50eacc", "g_50eaf8", "g_50eb24", "g_50eb50", "g_50eb7c", "g_50eba8", "g_50ebd4",
    # The dialogue sprite count: the queue counts in scene_actor_services.cpp's sprite_count; the teardown clears this.
    "g_511a10",
}

# No dump settles what these hold yet.
UNSETTLED = {
    "g_504330",
    "g_4deda4", "g_4dedac",  # the pending record pool's sweep flag and 0x000341E0's result
    "g_510f7c", "g_510f80",  # scene record +0x124 and +0x128
}

ALLOWED = DUPLICATES | UNSETTLED
NAME = re.compile(r"\bg_[0-9a-f]{5,8}\b")


def main():
    found = {}
    for folder in ("src", "include", "tests", "tools"):
        for path in (ROOT / folder).rglob("*"):
            if path.suffix not in (".c", ".cpp", ".h", ".inl"):
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                for name in NAME.findall(line):
                    if name not in ALLOWED:
                        found.setdefault(name, f"{path.relative_to(ROOT)}:{number}")
    for name, where in sorted(found.items()):
        print(f"{where}: {name} is named for its address; name it for what it holds")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
