#!/usr/bin/env python3
"""Tiny handmade glTF 2.0 assets (<1 KB) for the GLB writer verification."""
import os, struct, json

ASSET = os.path.join(os.path.dirname(__file__), "assets")
BAD = os.path.join(ASSET, "bad")
os.makedirs(BAD, exist_ok=True)

# 3 vertices * VEC3 float (36 B) + 3 uint16 indices (6 B) = 42 B -> forces 2 B BIN padding.
verts = [(-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (0.0, 1.0, 0.0)]
blob = b"".join(struct.pack("<3f", *v) for v in verts) + struct.pack("<3H", 0, 1, 2)
assert len(blob) == 42
with open(os.path.join(ASSET, "triangle.bin"), "wb") as f:
    f.write(blob)

gltf = {
    "asset": {"version": "2.0", "generator": "cgltf-glb-verify"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "TriangleNode"}],
    "meshes": [{"name": "TriangleMesh",
                "primitives": [{"attributes": {"POSITION": 0}, "indices": 1, "mode": 4}]}],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [-1.0, -1.0, 0.0], "max": [1.0, 1.0, 0.0]},
        {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
        {"buffer": 0, "byteOffset": 36, "byteLength": 6, "target": 34963},
    ],
    "buffers": [{"uri": "triangle.bin", "byteLength": 42}],
}
with open(os.path.join(ASSET, "triangle.gltf"), "w") as f:
    json.dump(gltf, f, indent=2)

# A minimal valid GLB used as the base for malformed variants.
def pad4(b): return b + b" " * ((4 - len(b) % 4) % 4)
j = pad4(json.dumps({"asset": {"version": "2.0"}, "scene": 0,
                     "scenes": [{"nodes": []}]}).encode())
b = pad4(blob)
def glb(total, jlen, jmag, bdata, bmag):
    out = struct.pack("<III", 0x46546C67, 2, total)
    out += struct.pack("<II", jlen, jmag) + j
    if bdata is not None:
        out += struct.pack("<II", len(bdata), bmag) + bdata
    return out

valid_total = 12 + 8 + len(j) + 8 + len(b)
valid = glb(valid_total, len(j), 0x4E4F534A, b, 0x004E4942)
with open(os.path.join(ASSET, "triangle.glb"), "wb") as f:
    f.write(valid)

# 1) bad magic
with open(os.path.join(BAD, "bad_magic.glb"), "wb") as f:
    f.write(b"XXXX" + valid[4:])
# 2) bad JSON chunk magic
with open(os.path.join(BAD, "bad_jsonmagic.glb"), "wb") as f:
    f.write(valid[:16] + struct.pack("<I", 0xDEADBEEF) + valid[20:])
# 3) truncated: header claims full size but file is cut in half
with open(os.path.join(BAD, "truncated.glb"), "wb") as f:
    f.write(valid[:len(valid) // 2])
# 4) header total length lies (larger than actual file)
with open(os.path.join(BAD, "bad_total.glb"), "wb") as f:
    f.write(valid[:8] + struct.pack("<I", valid_total + 100) + valid[12:])

for name in sorted(os.listdir(ASSET)) + ["bad/" + n for n in sorted(os.listdir(BAD))]:
    p = os.path.join(ASSET, name)
    if os.path.isfile(p):
        print(name, os.path.getsize(p), "bytes")
