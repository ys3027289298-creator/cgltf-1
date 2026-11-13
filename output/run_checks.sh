#!/usr/bin/env bash
# One-shot, non-interactive verification for the sparse accessor fix.
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/output"

echo "== Regenerating small glTF assets =="
python3 gen_assets.py

echo "== Building and running sparse verification driver (ASan) =="
cc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address -I.. sparse_driver.c -o sparse_driver -lm
./sparse_driver

echo "== Building repository test entry points =="
cmake -S "$ROOT/test" -B "$ROOT/test/build" >/dev/null
cmake --build "$ROOT/test/build" >/dev/null

echo "== Repo entry points on generated assets =="
for f in assets/sparse_tight.gltf assets/sparse_strided.gltf assets/sparse_pure.gltf assets/dense.gltf; do
	"$ROOT/test/build/cgltf_test" "$f" | tail -1
	"$ROOT/test/build/test_conversion" "$f"
	"$ROOT/test/build/test_write" "$f"
done
"$ROOT/test/build/cgltf_test" assets/sparse_bad_index.gltf | tail -1

echo "== Full repository regression (sample models) =="
( cd "$ROOT/test" && python3 test_all.py )

find . -maxdepth 1 -name 'sparse_driver' -delete
find assets -maxdepth 1 -type f \( -name 'roundtrip*' -o -name 'pure.glb' \) -delete
echo "All checks finished."
