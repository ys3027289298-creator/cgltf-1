# Morph-target (expression) acceptance artifacts

Code changes (small, two places):
- `test/main.c` — `cgltf_test` now reports morph targets, target names,
  mesh/node weights and weights-animation channels when present.
  Files without morph data produce byte-identical output to before.
- `test/test_write.cpp` — the write round-trip check now also compares
  morph target counts, mesh/node weights, target-name counts and
  weights-animation channels between the parsed and re-written data.

The parsing/validation/writing support itself lives in `cgltf.h` /
`cgltf_write.h` (targets, mesh/node weights, `weights` animation path,
and `cgltf_validate` rejecting weight/target count mismatches).

Contents:
- `assets/` — self-made minimal glTF assets (< 3 KB each):
  - `morph_face.gltf` — 2 morph targets, mesh+node weights, targetNames,
    one `weights` animation channel.
  - `morph_bad_weights.gltf` — 3 weights for 2 targets (must be rejected).
  - `plain_triangle.gltf` — no morph data (behavior must be unchanged).
- `make_assets.py` — regenerates the assets.
- `bin/` — built test entry points (`cgltf_test`, `test_write`) plus
  pre-change builds (`*_orig`) used for the zero-diff check.
- `build/` — cmake build tree (`cmake -S test -B output/build`).
- `out.gltf` — result of the write round-trip.
- `verify.log` — captured acceptance run.
- `orig.txt` / `new.txt` — pre-/post-change output on the no-morph asset.
- `demo.sh` — segmented, non-interactive demo (titles + sleeps).
- `demo.cast` — screen recording of the demo (`asciinema play demo.cast`).

Acceptance results:
- Morph data round-trips identically: `test_write` exit 0, re-parsed
  `out.gltf` keeps targets, weights, targetNames and the weights channel.
- Bad input rejected: `morph_bad_weights.gltf` fails validation with
  `cgltf_result_invalid_gltf` (exit 4).
- No-morph files: zero diff between pre- and post-change `cgltf_test`.
