#!/usr/bin/env python3
"""Concatenate schematic.png.b64.c* into schematic.png.b64 and verify blob."""
import hashlib, pathlib, sys
root = pathlib.Path(__file__).resolve().parents[1]
parts = []
i = 0
while True:
    p = root / f"schematic.png.b64.c{i:02d}"
    if not p.exists():
        break
    parts.append(p.read_text())
    i += 1
data = "".join(parts)
blob = hashlib.sha1(f"blob {len(data)}\0".encode() + data.encode()).hexdigest()
expect = "79ed027f355028f0b2a5185feff7c2f73cff79f6"
if blob != expect:
    print("blob mismatch", blob, "len", len(data), file=sys.stderr)
    sys.exit(1)
(root / "schematic.png.b64").write_text(data)
print("wrote", len(data), blob)
