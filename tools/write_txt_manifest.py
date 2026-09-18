#!/usr/bin/env python3
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
json_path = root / "manifests" / "retail_files.json"
doc = json.loads(json_path.read_text(encoding="utf-8"))
for e in doc["files"]:
    e["path"] = e["path"].replace("\\", "/")
json_path.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
out = root / "manifests" / "retail_files.txt"
lines = ["# sha256 size relative_path", f"# file_count {doc['file_count']} total_size {doc['total_size']}"]
for e in doc["files"]:
    lines.append(f"{e['sha256']} {e['size']} {e['path']}")
out.write_text("\n".join(lines) + "\n", encoding="utf-8")
print("wrote", json_path, "and", out, "files", len(doc["files"]))
