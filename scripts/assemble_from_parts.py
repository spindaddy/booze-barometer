#!/usr/bin/env python3
import hashlib, pathlib, sys
root = pathlib.Path(__file__).resolve().parents[1]
parts_dir = root / '.upload_parts'
data = ''.join((parts_dir / f'p{i}').read_text() for i in range(3))
blob = hashlib.sha1(f'blob {len(data)}\0'.encode() + data.encode()).hexdigest()
print(len(data), blob)
assert len(data) == 25000
assert blob == 'd96fca5fc8a92970f5398c4344c5a9998a1a0948'
(root / 'schematic.png.b64').write_text(data)
print('ok')
