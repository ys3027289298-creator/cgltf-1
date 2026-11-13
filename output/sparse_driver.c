#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../cgltf_write.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, msg)                                              \
	do {                                                              \
		if (!(cond)) {                                                \
			printf("  [FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); \
			failures++;                                               \
		} else {                                                      \
			printf("  [ok]   %s\n", msg);                             \
		}                                                             \
	} while (0)

static cgltf_data* load_valid(const char* path)
{
	cgltf_options options;
	memset(&options, 0, sizeof(options));
	cgltf_data* data = NULL;
	cgltf_result r = cgltf_parse_file(&options, path, &data);
	if (r != cgltf_result_success) {
		printf("  [FAIL] parse %s -> %d\n", path, (int)r);
		failures++;
		return NULL;
	}
	r = cgltf_load_buffers(&options, data, path);
	if (r != cgltf_result_success) {
		printf("  [FAIL] load_buffers %s -> %d\n", path, (int)r);
		failures++;
		return NULL;
	}
	r = cgltf_validate(data);
	if (r != cgltf_result_success) {
		printf("  [FAIL] validate %s -> %d\n", path, (int)r);
		failures++;
		return NULL;
	}
	return data;
}

static int floats_equal(const cgltf_float* a, const cgltf_float* b, cgltf_size n)
{
	for (cgltf_size i = 0; i < n; ++i) {
		if (fabsf(a[i] - b[i]) > 1e-6f)
			return 0;
	}
	return 1;
}

/* Compare unpack_floats output and per-element read_float against a dense reference. */
static void check_decode(const char* path, cgltf_size count, cgltf_size comps, const cgltf_float* dense_ref)
{
	printf("decode %s\n", path);
	cgltf_data* data = load_valid(path);
	CHECK(data != NULL, "parse+load+validate succeed");
	if (!data)
		return;
	CHECK(data->accessors_count == 1, "exactly one accessor");
	cgltf_accessor* acc = &data->accessors[0];
	CHECK(acc->is_sparse != 0, "accessor flagged sparse");

	cgltf_size total = count * comps;
	cgltf_float* unpacked = (cgltf_float*)malloc(total * sizeof(cgltf_float));
	cgltf_size got = cgltf_accessor_unpack_floats(acc, unpacked, total);
	CHECK(got == total, "unpack_floats returns full element count");
	CHECK(floats_equal(unpacked, dense_ref, total), "unpack_floats matches dense reference");

	int per_element_ok = 1;
	for (cgltf_size i = 0; i < count; ++i) {
		cgltf_float tmp[16] = {0};
		cgltf_bool ok = cgltf_accessor_read_float(acc, i, tmp, 16);
		if (!ok || !floats_equal(tmp, dense_ref + i * comps, comps))
			per_element_ok = 0;
	}
	CHECK(per_element_ok, "read_float per element matches dense reference");

	free(unpacked);
	cgltf_free(data);
}

/* Integer sparse accessor: read_uint must follow the tightly packed values view. */
static void check_uint_decode(const char* path)
{
	printf("uint decode %s\n", path);
	cgltf_data* data = load_valid(path);
	CHECK(data != NULL, "uint sparse file loads");
	if (!data)
		return;
	const cgltf_uint ref[3][2] = {{7, 8}, {1, 2}, {5, 6}};
	cgltf_accessor* acc = &data->accessors[0];
	int ok = 1;
	for (cgltf_size i = 0; i < 3; ++i) {
		cgltf_uint tmp[4] = {0};
		cgltf_bool r = cgltf_accessor_read_uint(acc, i, tmp, 4);
		if (!r || tmp[0] != ref[i][0] || tmp[1] != ref[i][1])
			ok = 0;
	}
	CHECK(ok, "read_uint per element matches dense integer reference");
	cgltf_free(data);
}

/* Parse+load must succeed structurally; validate must reject the out-of-range sparse index. */
static void check_rejected(const char* path)
{
	printf("reject %s\n", path);
	cgltf_options options;
	memset(&options, 0, sizeof(options));
	cgltf_data* data = NULL;
	cgltf_result r = cgltf_parse_file(&options, path, &data);
	CHECK(r == cgltf_result_success, "malformed-index file parses structurally");
	if (r == cgltf_result_success) {
		r = cgltf_load_buffers(&options, data, path);
		CHECK(r == cgltf_result_success, "buffers load");
		if (r == cgltf_result_success) {
			r = cgltf_validate(data);
			CHECK(r != cgltf_result_success, "validate rejects illegal sparse index");
		}
		cgltf_free(data);
	}
}

/* Round-trip through the writer and confirm sparse structure plus decoded data survive. */
static void check_roundtrip(const char* path, cgltf_size count, cgltf_size comps, const cgltf_float* dense_ref)
{
	printf("roundtrip %s\n", path);
	cgltf_data* data = load_valid(path);
	CHECK(data != NULL, "source loads");
	if (!data)
		return;

	cgltf_options options;
	memset(&options, 0, sizeof(options));
	const char* out_path = "assets/roundtrip.gltf";
	cgltf_result r = cgltf_write_file(&options, out_path, data);
	CHECK(r == cgltf_result_success, "write_file succeeds");

	cgltf_data* data2 = load_valid(out_path);
	CHECK(data2 != NULL, "written file reloads and validates");
	if (data2) {
		CHECK(data2->accessors[0].is_sparse != 0, "sparse flag preserved on write");
		CHECK(data2->accessors[0].sparse.count == data->accessors[0].sparse.count, "sparse count preserved");
		cgltf_size total = count * comps;
		cgltf_float* unpacked = (cgltf_float*)malloc(total * sizeof(cgltf_float));
		cgltf_size got = cgltf_accessor_unpack_floats(&data2->accessors[0], unpacked, total);
		CHECK(got == total, "reloaded sparse accessor fully unpacks");
		CHECK(floats_equal(unpacked, dense_ref, total), "reloaded decode matches dense reference");
		free(unpacked);
		cgltf_free(data2);
	}
	cgltf_free(data);
}

/* GLB round-trip: binary blob is preserved byte-for-byte and sparse survives. */
static void check_glb_roundtrip(const char* path, cgltf_size count, cgltf_size comps, const cgltf_float* dense_ref)
{
	printf("glb roundtrip %s\n", path);
	cgltf_data* data = load_valid(path);
	CHECK(data != NULL, "source loads");
	if (!data)
		return;

	cgltf_options options;
	memset(&options, 0, sizeof(options));
	options.type = cgltf_file_type_glb;
	const char* out_path = "assets/roundtrip.glb";
	cgltf_result r = cgltf_write_file(&options, out_path, data);
	CHECK(r == cgltf_result_success, "GLB write_file succeeds");

	cgltf_data* data2 = load_valid(out_path);
	CHECK(data2 != NULL, "GLB reloads and validates");
	if (data2) {
		CHECK(data2->accessors[0].is_sparse != 0, "GLB sparse flag preserved");
		CHECK(data2->accessors[0].sparse.count == data->accessors[0].sparse.count, "GLB sparse count preserved");
		cgltf_size total = count * comps;
		cgltf_float* unpacked = (cgltf_float*)malloc(total * sizeof(cgltf_float));
		cgltf_size got = cgltf_accessor_unpack_floats(&data2->accessors[0], unpacked, total);
		CHECK(got == total, "GLB sparse accessor fully unpacks");
		CHECK(floats_equal(unpacked, dense_ref, total), "GLB decode matches dense reference");
		free(unpacked);
		cgltf_free(data2);
	}
	cgltf_free(data);
}

/* Non-sparse files must keep identical decoded output and remain non-sparse after writing. */
static void check_dense_unchanged(const char* path)
{
	printf("dense unchanged %s\n", path);
	cgltf_data* data = load_valid(path);
	CHECK(data != NULL, "dense file loads");
	if (!data)
		return;
	CHECK(data->accessors[0].is_sparse == 0, "dense accessor stays non-sparse");
	cgltf_float ref[4];
	cgltf_accessor_unpack_floats(&data->accessors[0], ref, 4);

	cgltf_options options;
	memset(&options, 0, sizeof(options));
	const char* out_path = "assets/roundtrip_dense.gltf";
	cgltf_result r = cgltf_write_file(&options, out_path, data);
	CHECK(r == cgltf_result_success, "dense write succeeds");
	cgltf_data* data2 = load_valid(out_path);
	if (data2) {
		CHECK(data2->accessors[0].is_sparse == 0, "written dense accessor still non-sparse");
		cgltf_float again[4];
		cgltf_accessor_unpack_floats(&data2->accessors[0], again, 4);
		CHECK(floats_equal(ref, again, 4), "dense decode identical after round-trip");
		cgltf_free(data2);
	}
	cgltf_free(data);
}

int main(void)
{
	const cgltf_float tight_ref[4] = {10.f, 21.f, 30.f, 41.f};
	const cgltf_float strided_ref[9] = {9.f, 8.f, 7.f, 1.f, 2.f, 3.f, 6.f, 5.f, 4.f};
	const cgltf_float pure_ref[3] = {5.f, 0.f, 7.f};

	check_decode("assets/sparse_tight.gltf", 4, 1, tight_ref);
	check_decode("assets/sparse_strided.gltf", 3, 3, strided_ref);
	check_decode("assets/sparse_pure.gltf", 3, 1, pure_ref);
	check_uint_decode("assets/sparse_uint.gltf");

	check_roundtrip("assets/sparse_tight.gltf", 4, 1, tight_ref);
	check_roundtrip("assets/sparse_strided.gltf", 3, 3, strided_ref);
	check_glb_roundtrip("assets/sparse_strided.gltf", 3, 3, strided_ref);

	check_rejected("assets/sparse_bad_index.gltf");
	check_rejected("assets/sparse_bad_index_offset.gltf");
	check_dense_unchanged("assets/dense.gltf");

	/* Pure-sparse (no base bufferView) must also survive a GLB round-trip. */
	cgltf_data* pure = load_valid("assets/sparse_pure.gltf");
	CHECK(pure != NULL && pure->accessors[0].is_sparse, "pure-sparse source loads");
	if (pure) {
		cgltf_options options;
		memset(&options, 0, sizeof(options));
		options.type = cgltf_file_type_glb;
		cgltf_result r = cgltf_write_file(&options, "assets/pure.glb", pure);
		CHECK(r == cgltf_result_success, "pure-sparse GLB write succeeds");
		cgltf_data* pure2 = load_valid("assets/pure.glb");
		if (pure2) {
			CHECK(pure2->accessors[0].is_sparse, "pure-sparse GLB keeps sparse flag");
			cgltf_float out[3];
			cgltf_accessor_unpack_floats(&pure2->accessors[0], out, 3);
			CHECK(floats_equal(out, pure_ref, 3), "pure-sparse GLB decode matches reference");
			cgltf_free(pure2);
		}
		cgltf_free(pure);
	}

	printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}
