#!/usr/bin/env python3
"""Generate small self-made glTF assets for morph-target acceptance checks."""
import base64
import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))


def f32(*values):
    return struct.pack("<%df" % len(values), *values)


def data_uri(blob):
    return "data:application/octet-stream;base64," + base64.b64encode(blob).decode("ascii")


def make_morph_face(path, mesh_weights):
    positions = f32(0, 0, 0, 1, 0, 0, 0, 1, 0)
    smile = f32(0, 0.1, 0, 0, 0.2, 0, 0, 0.1, 0)
    frown = f32(0, -0.1, 0, 0, -0.2, 0, 0, -0.1, 0)
    times = f32(0.0, 1.0)
    weights = f32(0.0, 0.0, 1.0, 1.0)

    blob = positions + smile + frown + times + weights
    views = []
    offset = 0
    for chunk in (positions, smile, frown, times, weights):
        views.append({"buffer": 0, "byteOffset": offset, "byteLength": len(chunk)})
        offset += len(chunk)

    doc = {
        "asset": {"version": "2.0", "generator": "cgltf morph acceptance"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Character", "mesh": 0, "weights": [0.25, 0.75]}],
        "meshes": [{
            "name": "Face",
            "primitives": [{
                "attributes": {"POSITION": 0},
                "targets": [{"POSITION": 1}, {"POSITION": 2}],
            }],
            "weights": mesh_weights,
            "extras": {"targetNames": ["Smile", "Frown"]},
        }],
        "animations": [{
            "name": "Expressions",
            "channels": [{"sampler": 0, "target": {"node": 0, "path": "weights"}}],
            "samplers": [{"input": 3, "interpolation": "LINEAR", "output": 4}],
        }],
        "buffers": [{"uri": data_uri(blob), "byteLength": len(blob)}],
        "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0, 0, 0], "max": [1, 1, 0]},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 3, "componentType": 5126, "count": 2, "type": "SCALAR",
             "min": [0.0], "max": [1.0]},
            {"bufferView": 4, "componentType": 5126, "count": 4, "type": "SCALAR"},
        ],
    }
    with open(path, "w") as fh:
        json.dump(doc, fh, indent=2)


def make_plain_triangle(path):
    positions = f32(0, 0, 0, 1, 0, 0, 0, 1, 0)
    doc = {
        "asset": {"version": "2.0", "generator": "cgltf morph acceptance"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": "Character", "mesh": 0}],
        "meshes": [{
            "name": "Face",
            "primitives": [{"attributes": {"POSITION": 0}}],
        }],
        "buffers": [{"uri": data_uri(positions), "byteLength": len(positions)}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": len(positions)}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [0, 0, 0], "max": [1, 1, 0]},
        ],
    }
    with open(path, "w") as fh:
        json.dump(doc, fh, indent=2)


if __name__ == "__main__":
    make_morph_face(os.path.join(HERE, "assets", "morph_face.gltf"), [0.25, 0.75])
    # 3 weights for 2 morph targets: must be rejected by cgltf_validate.
    make_morph_face(os.path.join(HERE, "assets", "morph_bad_weights.gltf"), [0.25, 0.75, 0.5])
    make_plain_triangle(os.path.join(HERE, "assets", "plain_triangle.gltf"))
    for name in sorted(os.listdir(os.path.join(HERE, "assets"))):
        full = os.path.join(HERE, "assets", name)
        print("%s: %d bytes" % (name, os.path.getsize(full)))
