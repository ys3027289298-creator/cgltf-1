#!/usr/bin/env bash
# Automated, non-interactive demo for the sparse accessor fix.
# Each section prints a title banner, pauses, then runs its commands.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/output"
cd "$OUT"

pause() { sleep 2; }

title() {
	pause
	echo
	printf '\033[1;36m============================================================\033[0m\n'
	printf '\033[1;36m %s\033[0m\n' "$1"
	printf '\033[1;36m============================================================\033[0m\n'
	echo
	sleep 1
}

title "Segment 1/4: Generate tiny sparse glTF test assets"
python3 gen_assets.py
echo
ls -1 assets

title "Segment 2/4: Show the root-cause fix in cgltf.h"
git -c color.ui=always diff -- cgltf.h

title "Segment 3/4: Build and run the sparse verification driver"
cc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address -I.. sparse_driver.c -o sparse_driver -lm
./sparse_driver

title "Segment 4/4: Repo test entry on assets + full sample-model regression"
cd "$ROOT/test/build"
for f in "$OUT"/assets/sparse_strided.gltf "$OUT"/assets/sparse_bad_index.gltf "$OUT"/assets/dense.gltf; do
	echo "\$ cgltf_test $(basename "$f")"
	./cgltf_test "$f" | tail -1
done
echo
echo "Full repository test suite (726 sample files):"
cd "$ROOT/test"
python3 test_all.py 2>&1 | tail -2

pause
title "Demo complete: decode matches dense refs, bad indices rejected, dense unchanged"
sleep 1
