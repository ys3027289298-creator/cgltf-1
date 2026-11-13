#!/usr/bin/env python3
"""Generate tiny glTF 2.0 assets for sparse accessor testing."""

import json
import os
import struct


HERE = os.path.dirname(os.path.abspath(__file__))


def write_gltf(name, json_dict, bin_blob):
    out_dir = os.path.join(HERE, "assets")
    os.makedirs(out_dir, exist_ok=True)
    bin_path = os.path.join(out_dir, name + ".bin")
    gltf_path = os.path.join(out_dir, name + ".gltf")
    json_dict["buffers"][0]["uri"] = name + ".bin"
    with open(bin_path, "wb") as f:
        f.write(bin_blob)
    with open(gltf_path, "w") as f:
        json.dump(json_dict, f, indent=1)
    return gltf_path


def base_doc(views, accessors, blob):
    return {
        "asset": {"version": "2.0"},
        "buffers": [{"uri": "sparse.bin", "byteLength": len(blob)}],
        "bufferViews": views,
        "accessors": accessors,
        "meshes": [],
    }, blob


def gen_tight_sparse():
    """Scalar float accessor, count=4, sparse replaces indices 1 and 3.

    Base values: [10, 20, 30, 40]; sparse values at 1 -> 21, 3 -> 41.
    Dense reference after applying sparse: [10, 21, 30, 41].
    Sparse values buffer view is tightly packed (element stride == 4).
    """
    base = struct.pack("<4f", 10.0, 20.0, 30.0, 40.0)
    indices = struct.pack("<2H", 1, 3)
    values = struct.pack("<2f", 21.0, 41.0)
    blob = base + indices + values
    off_base, off_idx, off_val = 0, len(base), len(base) + len(indices)
    views = [
        {"buffer": 0, "byteOffset": off_base, "byteLength": len(base)},
        {"buffer": 0, "byteOffset": off_idx, "byteLength": len(indices)},
        {"buffer": 0, "byteOffset": off_val, "byteLength": len(values)},
    ]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 4,
            "type": "SCALAR",
            "min": [10.0],
            "max": [41.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 1, "byteOffset": 0, "componentType": 5123},
                "values": {"bufferView": 2, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_tight", doc, blob)


def gen_strided_sparse():
    """VEC3 float accessor with a strided BASE view (stride 16, element 12).

    Sparse values view stays tightly packed per glTF 2.0 spec (stride 12),
    while the base view uses a 16-byte stride. Two sparse replacements make
    a wrong stride on the values view observable.
    Base count=3, sparse replaces element indices 0 and 2.
    """
    element = struct.pack("<3f", 1.0, 2.0, 3.0)
    pad = struct.pack("<f", 0.0)
    base = (element + pad) * 2 + element
    indices = struct.pack("<2H", 0, 2)
    values = struct.pack("<6f", 9.0, 8.0, 7.0, 6.0, 5.0, 4.0)
    blob = base + indices + values
    off_idx = len(base)
    off_val = off_idx + len(indices)
    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(base), "byteStride": 16, "target": 34962},
        {"buffer": 0, "byteOffset": off_idx, "byteLength": len(indices)},
        {"buffer": 0, "byteOffset": off_val, "byteLength": len(values)},
    ]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 3,
                "type": "VEC3",
            "min": [1.0, 2.0, 3.0],
            "max": [9.0, 8.0, 7.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 1, "byteOffset": 0, "componentType": 5123},
                "values": {"bufferView": 2, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_strided", doc, blob)


def gen_pure_sparse():
    """Sparse accessor without a base bufferView: missing elements read as zero."""
    indices = struct.pack("<2B", 0, 2)
    values = struct.pack("<2f", 5.0, 7.0)
    blob = indices + values
    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(indices)},
        {"buffer": 0, "byteOffset": len(indices), "byteLength": len(values)},
    ]
    accessors = [
        {
            "componentType": 5126,
            "count": 3,
            "type": "SCALAR",
            "min": [0.0],
            "max": [7.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 0, "byteOffset": 0, "componentType": 5121},
                "values": {"bufferView": 1, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_pure", doc, blob)


def gen_bad_index():
    """Sparse index 5 is out of range for accessor count 4; must be rejected."""
    base = struct.pack("<4f", 1.0, 2.0, 3.0, 4.0)
    indices = struct.pack("<2H", 1, 5)
    values = struct.pack("<2f", 21.0, 99.0)
    blob = base + indices + values
    off_idx = len(base)
    off_val = off_idx + len(indices)
    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(base)},
        {"buffer": 0, "byteOffset": off_idx, "byteLength": len(indices)},
        {"buffer": 0, "byteOffset": off_val, "byteLength": len(values)},
    ]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 4,
            "type": "SCALAR",
            "min": [1.0],
            "max": [99.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 1, "byteOffset": 0, "componentType": 5123},
                "values": {"bufferView": 2, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_bad_index", doc, blob)


def gen_bad_index_offset():
    """Out-of-range sparse index hidden behind a nonzero indices byteOffset.

    Exercises that validation reads indices at the correct offset rather than
    the start of the buffer view.
    """
    base = struct.pack("<4f", 1.0, 2.0, 3.0, 4.0)
    pad = struct.pack("<2H", 0, 0)
    indices = struct.pack("<2H", 2, 4)  # 4 >= accessor count 4 -> illegal
    values = struct.pack("<2f", 33.0, 88.0)
    blob = base + pad + indices + values
    off_pad = len(base)
    off_idx = off_pad + len(pad)
    off_val = off_idx + len(indices)
    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(base)},
        {"buffer": 0, "byteOffset": off_pad, "byteLength": len(pad) + len(indices)},
        {"buffer": 0, "byteOffset": off_val, "byteLength": len(values)},
    ]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 4,
            "type": "SCALAR",
            "min": [1.0],
            "max": [88.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 1, "byteOffset": 4, "componentType": 5123},
                "values": {"bufferView": 2, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_bad_index_offset", doc, blob)


def gen_dense():
    """Plain non-sparse accessor; behavior must remain byte-identical."""
    base = struct.pack("<4f", 1.0, 2.0, 3.0, 4.0)
    views = [{"buffer": 0, "byteOffset": 0, "byteLength": len(base)}]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": 4,
            "type": "SCALAR",
            "min": [1.0],
            "max": [4.0],
        }
    ]
    doc, blob = base_doc(views, accessors, base)
    write_gltf("dense", doc, blob)


def gen_uint_sparse():
    """VEC2 unsigned-short sparse accessor over a strided base view.

    Base element is 4 bytes (two ushort); base view stride is 8 bytes.
    Sparse values are tightly packed at 4 bytes/element. Two replacements
    exercise cgltf_accessor_read_uint on sparse data.
    Dense reference: (7,8) (1,2) (5,6).
    """
    base_elem = struct.pack("<2H", 1, 2)
    base = (base_elem + base_elem) + (base_elem + base_elem) + base_elem
    indices = struct.pack("<2B", 0, 2)
    values = struct.pack("<4H", 7, 8, 5, 6)
    blob = base + indices + values
    off_idx = len(base)
    off_val = off_idx + len(indices)
    views = [
        {"buffer": 0, "byteOffset": 0, "byteLength": len(base), "byteStride": 8, "target": 34962},
        {"buffer": 0, "byteOffset": off_idx, "byteLength": len(indices)},
        {"buffer": 0, "byteOffset": off_val, "byteLength": len(values)},
    ]
    accessors = [
        {
            "bufferView": 0,
            "componentType": 5123,
            "count": 3,
            "type": "VEC2",
            "min": [3.0, 2.0],
            "max": [7.0, 8.0],
            "sparse": {
                "count": 2,
                "indices": {"bufferView": 1, "byteOffset": 0, "componentType": 5121},
                "values": {"bufferView": 2, "byteOffset": 0},
            },
        }
    ]
    doc, blob = base_doc(views, accessors, blob)
    write_gltf("sparse_uint", doc, blob)


if __name__ == "__main__":
    gen_tight_sparse()
    gen_strided_sparse()
    gen_pure_sparse()
    gen_bad_index()
    gen_bad_index_offset()
    gen_dense()
    gen_uint_sparse()
    print("generated:", sorted(os.listdir(os.path.join(HERE, "assets"))))
