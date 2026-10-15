"""Verify checked-in third-party bytes without network access."""

import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "third_party"
manifest = json.loads((root / "MANIFEST.json").read_text(encoding="utf-8"))
for artifact in manifest["artifacts"]:
    path = (root / artifact["path"]).resolve()
    if not path.is_relative_to(root):
        raise RuntimeError("Manifest path escapes third_party")
    data = path.read_bytes()
    if (
        len(data) != artifact["bytes"]
        or hashlib.sha256(data).hexdigest() != artifact["sha256"]
    ):
        raise RuntimeError(f"Vendored bytes differ: {artifact['path']}")
print(f"Verified {len(manifest['artifacts'])} vendored files")
