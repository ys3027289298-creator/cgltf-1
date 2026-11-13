# Sparse Accessor Fix — Verification Bundle

## Root cause

glTF 2.0 stores sparse accessor replacement **values** in a tightly packed
buffer view (no `byteStride`; element stride equals the element size). The
loader instead advanced through that view using the *base* accessor's
`accessor->stride`. Whenever the base view used a stride larger than the
element size and the sparse set had more than one entry, the second and later
replacement values were read from the wrong offsets — producing wrong decode
output and, at the end of the buffer, out-of-bounds reads (caught by ASan).

Two read paths had the same defect in `cgltf.h`:

- `cgltf_find_sparse_index` (used by `cgltf_accessor_read_float`,
  `cgltf_accessor_read_uint`, `cgltf_accessor_read_index`).
- `cgltf_accessor_unpack_floats`.

Both now walk the sparse values view with
`cgltf_calc_size(type, component_type)` instead of `accessor->stride`.
The change is confined to sparse branches, so non-sparse behavior is untouched.

## Acceptance criteria mapping

- **Decode matches dense reference** — `sparse_driver.c` decodes tight,
  strided, pure-sparse (no base view), and integer sparse accessors both via
  `cgltf_accessor_unpack_floats` and per-element `cgltf_accessor_read_*`,
  comparing against hand-computed dense references.
- **Illegal indices rejected** — `sparse_bad_index*` files contain a sparse
  index `>= accessor.count`; `cgltf_validate` returns
  `cgltf_result_data_too_short` (error code `1`), including when the indices
  sit behind a non-zero `byteOffset`.
- **Sparse structure preserved on write** — the driver round-trips sparse
  files through both `cgltf_write_file` (.gltf) and GLB output and asserts the
  sparse flag/count survive and decoded data still matches the reference.
- **Non-sparse files unchanged** — the dense asset round-trips with identical
  decoded values and stays non-sparse; the full repository regression
  (726 sample-model files) reports zero errors.

## Layout

- `gen_assets.py` — generates the tiny glTF/.bin fixtures under `assets/`.
- `sparse_driver.c` — self-contained decode / reject / round-trip checks.
- `run_checks.sh` — non-interactive one-shot verification (driver + repo
  entry points + full `test_all.py` regression).
- `demo.sh` — segmented, non-interactive demo script used for the recording.
- `demo.cast` — asciinema recording, four titled segments with pauses;
  replay with `asciinema play demo.cast`.
- `assets/` — generated small fixtures (regenerate via `gen_assets.py`).

## Reproduce

```
cd output
python3 gen_assets.py
cc -std=c99 -Wall -Wextra -Werror -g -fsanitize=address -I.. sparse_driver.c -o sparse_driver -lm
./sparse_driver
```

Full regression through the repository's own entry points:

```
./run_checks.sh
```

Last observed result: `PASSED: 0 failure(s)` for the driver, and
`Tested files: 726 / Errors: 0` for the repository suite.
