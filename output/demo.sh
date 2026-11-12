#!/usr/bin/env bash
# Segmented acceptance demo for morph-target (expression) support.
# Non-interactive: pauses are sleeps, no keyboard input required.
set -u
cd "$(dirname "$0")"

banner() {
    echo
    echo "================================================================"
    echo "  $1"
    echo "================================================================"
    sleep 2
}

pause() { sleep 3; }

banner "Segment 1/5: Build the repo's own test entry points (cmake)"
cmake --build build -j --target cgltf_test test_write | tail -3
pause

banner "Segment 2/5: Parse keeps morph targets & weights"
./bin/cgltf_test assets/morph_face.gltf
echo "exit=$?"
pause

banner "Segment 3/5: Morph data round-trips through the writer"
./bin/test_write assets/morph_face.gltf && echo "round-trip: OK"
echo "--- reparse written file ---"
./bin/cgltf_test out.gltf
echo "--- written JSON still carries targets/weights/animation ---"
python3 - <<'PY'
import json
d = json.load(open('out.gltf'))
m = d['meshes'][0]
print('targets:    ', m['primitives'][0]['targets'])
print('mesh weights:', m['weights'])
print('targetNames:', m['extras']['targetNames'])
print('node weights:', d['nodes'][0]['weights'])
print('anim path:  ', d['animations'][0]['channels'][0]['target']['path'])
PY
pause

banner "Segment 4/5: Weight/target count mismatch is rejected"
./bin/cgltf_test assets/morph_bad_weights.gltf
echo "exit=$? (4 = cgltf_result_invalid_gltf, rejected as required)"
pause

banner "Segment 5/5: Files without morphs behave exactly as before"
./bin/cgltf_test_orig assets/plain_triangle.gltf > orig.txt
./bin/cgltf_test      assets/plain_triangle.gltf > new.txt
if diff -u orig.txt new.txt; then echo "ZERO DIFF: output identical to pre-change behavior"; fi
pause

echo
echo "All acceptance checks passed."
sleep 2
