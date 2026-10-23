"""Verify the normalized text sources saved in this Git revision. Offline only."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'provenance/SNAPSHOT.sha256.json').read_text(encoding='utf-8'))
bad = []
for name, expected in manifest.items():
    path = root / name
    if not path.is_file():
        bad.append(name + ': missing')
        continue
    data = path.read_bytes()
    if path.suffix not in {'.gz', '.zip', '.tar'}:
        data = data.replace(b'\r\n', b'\n')
    actual = hashlib.sha256(data).hexdigest()
    if actual != expected:
        bad.append(name + ': hash mismatch')
if bad:
    raise SystemExit('\n'.join(bad))
print('PASS normalized source snapshot:', len(manifest), 'files')
