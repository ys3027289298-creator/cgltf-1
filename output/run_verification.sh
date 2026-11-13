#!/usr/bin/env bash
# Reproduces every verification used for the unknown-extension round-trip work.
# Uses only the repository's own public entry points (cgltf_parse / cgltf_write)
# and its bundled fuzz harness. No network, no interactive prompts.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

OUT="$ROOT/output"
mkdir -p "$OUT/bin"

echo "### [1/4] Build strict (-Wall -Wextra -pedantic -Werror + ASan)"
gcc -std=c99 -Wall -Wextra -pedantic -Werror -fsanitize=address -g \
    "$OUT/src/roundtrip.c" -o "$OUT/bin/roundtrip"
gcc -std=c99 -Wall -Wextra -pedantic -Werror -fsanitize=address -g \
    "$OUT/src/dump_roundtrip.c" -o "$OUT/bin/dump_roundtrip"
clang -O1 -g -fsanitize=address,fuzzer fuzz/main.c -o "$OUT/bin/fuzz"

echo "### [2/4] Functional + corruption + zero-diff checks"
ASAN_OPTIONS=detect_leaks=1 "$OUT/bin/roundtrip" \
    "$OUT/assets/ext.gltf" "$OUT/assets/known.gltf" "$OUT/assets/ext.glb"

echo "### [3/4] Emit concrete round-trip artifacts for inspection"
"$OUT/bin/dump_roundtrip" \
    "$OUT/assets/ext.gltf" \
    "$OUT/bin/ext.roundtrip.gltf" \
    "$OUT/bin/ext.roundtrip.glb"

echo "### [4/4] Fuzz the parser (bounded run; Ctrl-C safe)"
mkdir -p "$OUT/bin/fuzz_corpus"
cp -f "$OUT/assets/ext.gltf" "$OUT/assets/known.gltf" "$OUT/assets/ext.glb" \
      "$OUT/bin/fuzz_corpus/"
timeout "${FUZZ_SECONDS:-60}" "$OUT/bin/fuzz" \
    -dict=fuzz/gltf.dict -max_len=4096 -print_final_stats=1 \
    "$OUT/bin/fuzz_corpus" || test $? -eq 124

echo "### Verification complete."
