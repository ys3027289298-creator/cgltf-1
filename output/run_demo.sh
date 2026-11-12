#!/usr/bin/env bash
# Non-interactive demo: cgltf GLB single-file writer hardening.
# Stages print titled banners and sleep between them; no keyboard input needed.
set -euo pipefail
cd "$(dirname "$0")/.."
B=/tmp/cgltf_glb_demo
mkdir -p "$B"

title() { printf '\n========== %s ==========\n' "$1"; sleep 2; }

title "1/7 defect: GLB writer lost external buffers and kept uri"
echo "changed files:"; ls -1 cgltf_write.h test/test_write_glb.cpp
echo "old behavior: buffers[0].data was not embedded and JSON kept external uri -> broken GLB"
sleep 2

title "2/7 build tiny assets (42 B BIN, not 4-aligned)"
python3 output/make_assets.py
sleep 2

title "3/7 compile verifier (zero warnings, -Werror)"
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O1 -I. output/glb_verify.c -o "$B/glb_verify"
echo "compile OK (zero warnings)"
sleep 2

title "4/7 roundtrip: structure, BIN bytes and container layout"
"$B/glb_verify" output/assets/triangle.gltf output/assets/bad
sleep 2

title "5/7 invalid input returns error codes, no broken file"
echo "NULL args / unloaded buffer covered in stage 4 section F; malformed GLBs via cgltf_test:"
test/build/cgltf_test output/assets/bad/bad_magic.glb     || true
test/build/cgltf_test output/assets/bad/bad_jsonmagic.glb || true
test/build/cgltf_test output/assets/bad/truncated.glb     || true
test/build/cgltf_test output/assets/bad/bad_total.glb     || true
test/build/cgltf_test output/assets/triangle.glb
sleep 2

title "6/7 repo entry regression: 726 sample models, ASan"
echo "full run saved in output/regression.log"
grep -E "Tested files|Errors" output/regression.log
sleep 2

title "7/7 size comparison: BIN chunk never grows"
./output/size_report.sh
sleep 2
echo
echo "ALL STAGES DONE"
