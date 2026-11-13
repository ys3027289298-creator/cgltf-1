/*
 * Round-trip verifier for preserved (unknown) vendor extensions.
 *
 * It uses the repository's own public entry points (cgltf_parse / cgltf_write)
 * and checks three properties:
 *   1. Every unknown extension's raw JSON payload is byte-identical after a
 *      parse -> write -> parse round trip.
 *   2. A document whose individual extension entry is corrupt still parses:
 *      the corrupt entry is skipped while the main structure and the sibling
 *      healthy extension survive.
 *   3. A document that contains no unknown extensions is written with zero
 *      diff across a second round trip (old-file stability).
 */
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../../cgltf_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, ...) \
	do { \
		if (!(cond)) { \
			++failures; \
			printf("  [FAIL] "); \
			printf(__VA_ARGS__); \
			printf("\n"); \
		} \
	} while (0)

static char* read_file(const char* path, cgltf_size* size_out)
{
	FILE* f = fopen(path, "rb");
	if (!f)
	{
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc((size_t)size + 1);
	size_t read = fread(buf, 1, (size_t)size, f);
	fclose(f);
	buf[read] = 0;
	*size_out = (cgltf_size)read;
	return buf;
}

static cgltf_data* parse_path(const char* path)
{
	cgltf_size size = 0;
	char* buf = read_file(path, &size);
	if (!buf)
	{
		printf("  [FAIL] cannot read %s\n", path);
		++failures;
		return NULL;
	}

	cgltf_options options = {0};
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse(&options, buf, size, &data);
	free(buf);
	if (result != cgltf_result_success)
	{
		printf("  [FAIL] parse %s returned %d\n", path, (int)result);
		++failures;
		return NULL;
	}
	return data;
}

static cgltf_data* parse_bytes_keep_buffer(const char* path, char** kept_buffer)
{
	cgltf_size size = 0;
	char* buf = read_file(path, &size);
	if (!buf)
	{
		*kept_buffer = NULL;
		printf("  [FAIL] cannot read %s\n", path);
		++failures;
		return NULL;
	}

	cgltf_options options = {0};
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse(&options, buf, size, &data);
	if (result != cgltf_result_success)
	{
		printf("  [FAIL] parse %s returned %d\n", path, (int)result);
		++failures;
		free(buf);
		*kept_buffer = NULL;
		return NULL;
	}

	*kept_buffer = buf; /* GLB bin chunk aliases this buffer. */
	return data;
}

static cgltf_data* roundtrip(const cgltf_data* data, cgltf_file_type type, char** json_out)
{
	cgltf_options options = {0};
	options.type = type;

	if (type == cgltf_file_type_glb)
	{
		/* GLB container packaging is performed by cgltf_write_file, not cgltf_write. */
		const char* tmp_path = "output/bin/roundtrip_tmp.glb";
		cgltf_result w = cgltf_write_file(&options, tmp_path, data);
		CHECK(w == cgltf_result_success, "glb write failed %d", (int)w);
		cgltf_data* again = parse_path(tmp_path);
		if (json_out)
		{
			*json_out = NULL;
		}
		return again;
	}

	cgltf_size needed = cgltf_write(&options, NULL, 0, data);
	char* written = (char*)malloc((size_t)needed);
	cgltf_size actual = cgltf_write(&options, written, needed, data);
	CHECK(actual == needed, "write size mismatch %zu != %zu", (size_t)actual, (size_t)needed);

	cgltf_data* again = NULL;
	cgltf_result result = cgltf_parse(&options, written, actual - 1, &again);
	CHECK(result == cgltf_result_success, "re-parse failed with %d", (int)result);

	if (json_out)
	{
		*json_out = written;
	}
	else
	{
		free(written);
	}
	return again;
}

static void compare_extensions(const char* label, const cgltf_extension* a, cgltf_size na,
                               const cgltf_extension* b, cgltf_size nb)
{
	CHECK(na == nb, "%s: extension count %zu != %zu", label, (size_t)na, (size_t)nb);
	cgltf_size common = na < nb ? na : nb;
	for (cgltf_size i = 0; i < common; ++i)
	{
		CHECK(a[i].name && b[i].name && strcmp(a[i].name, b[i].name) == 0,
		      "%s[%zu]: name mismatch", label, (size_t)i);
		CHECK(a[i].data && b[i].data && strcmp(a[i].data, b[i].data) == 0,
		      "%s[%zu] (%s): payload not byte-identical", label, (size_t)i,
		      a[i].name ? a[i].name : "?");
	}
}

static void test_unknown_extensions_preserved(const char* path, cgltf_file_type type)
{
	printf("== unknown extension preservation (%s) ==\n",
	       type == cgltf_file_type_glb ? "GLB" : "glTF");

	cgltf_data* d0 = parse_path(path);
	CHECK(d0 != NULL, "initial parse");
	if (!d0)
	{
		return;
	}

	CHECK(d0->data_extensions_count == 1, "root extension count");
	CHECK(d0->asset.extensions_count == 1, "asset extension count");
	CHECK(d0->meshes[0].extensions_count == 1, "mesh extension count");
	CHECK(d0->meshes[0].primitives[0].extensions_count == 1, "primitive extension count");
	CHECK(d0->materials[0].extensions_count == 1, "material extension count");
	CHECK(d0->nodes[0].extensions_count == 1, "node extension count");
	CHECK(d0->scenes[0].extensions_count == 1, "scene extension count");
	CHECK(d0->accessors[0].extensions_count == 1, "accessor extension count");
	CHECK(d0->buffer_views[0].extensions_count == 1, "bufferView extension count");
	CHECK(d0->buffers[0].extensions_count == 1, "buffer extension count");

	cgltf_data* d1 = roundtrip(d0, type, NULL);
	CHECK(d1 != NULL, "round-trip parse");
	if (!d1)
	{
		cgltf_free(d0);
		return;
	}

	compare_extensions("root", d0->data_extensions, d0->data_extensions_count,
	                   d1->data_extensions, d1->data_extensions_count);
	compare_extensions("asset", d0->asset.extensions, d0->asset.extensions_count,
	                   d1->asset.extensions, d1->asset.extensions_count);
	compare_extensions("mesh", d0->meshes[0].extensions, d0->meshes[0].extensions_count,
	                   d1->meshes[0].extensions, d1->meshes[0].extensions_count);
	compare_extensions("primitive", d0->meshes[0].primitives[0].extensions,
	                   d0->meshes[0].primitives[0].extensions_count,
	                   d1->meshes[0].primitives[0].extensions,
	                   d1->meshes[0].primitives[0].extensions_count);
	compare_extensions("material", d0->materials[0].extensions, d0->materials[0].extensions_count,
	                   d1->materials[0].extensions, d1->materials[0].extensions_count);
	compare_extensions("node", d0->nodes[0].extensions, d0->nodes[0].extensions_count,
	                   d1->nodes[0].extensions, d1->nodes[0].extensions_count);
	compare_extensions("scene", d0->scenes[0].extensions, d0->scenes[0].extensions_count,
	                   d1->scenes[0].extensions, d1->scenes[0].extensions_count);
	compare_extensions("accessor", d0->accessors[0].extensions, d0->accessors[0].extensions_count,
	                   d1->accessors[0].extensions, d1->accessors[0].extensions_count);
	compare_extensions("bufferView", d0->buffer_views[0].extensions,
	                   d0->buffer_views[0].extensions_count,
	                   d1->buffer_views[0].extensions, d1->buffer_views[0].extensions_count);
	compare_extensions("buffer", d0->buffers[0].extensions, d0->buffers[0].extensions_count,
	                   d1->buffers[0].extensions, d1->buffers[0].extensions_count);

	/* Every unknown extension name must be advertised in extensionsUsed. */
	const char* want[] = {
		"ACME_root_flags", "ACME_asset_meta", "ACME_mesh_extra", "ACME_primitive_hint",
		"ACME_material_tint", "ACME_node_tag", "ACME_scene_note", "ACME_accessor_unit",
		"ACME_view_range", "ACME_buffer_origin"
	};
	for (size_t i = 0; i < sizeof(want) / sizeof(want[0]); ++i)
	{
		int found = 0;
		for (cgltf_size j = 0; j < d1->extensions_used_count; ++j)
		{
			if (d1->extensions_used[j] && strcmp(d1->extensions_used[j], want[i]) == 0)
			{
				found = 1;
			}
		}
		CHECK(found, "extensionsUsed missing %s", want[i]);
	}

	/* Main structure is untouched by the extension work. */
	CHECK(d1->meshes_count == d0->meshes_count, "mesh count stable");
	CHECK(d1->nodes_count == d0->nodes_count, "node count stable");
	CHECK(d1->accessors_count == d0->accessors_count, "accessor count stable");

	cgltf_free(d1);
	cgltf_free(d0);
}

static void test_corrupt_extension_skipped(void)
{
	printf("== corrupt extension entry is skipped ==\n");

	/*
	 * The extensions object holds one healthy entry followed by entries whose
	 * value type is wrong (a string, a number and an array instead of an
	 * object). The parser must drop only the corrupt entries and keep the
	 * document intact. A structurally broken JSON object (for example a
	 * valueless key) is rejected wholesale by the tokenizer, which is expected;
	 * that path is covered separately by the fuzz harness without leaking.
	 */
	static const char* doc =
		"{"
		"  \"asset\": { \"version\": \"2.0\" },"
		"  \"meshes\": [ {"
		"    \"name\": \"mesh0\","
		"    \"primitives\": [ { \"attributes\": { \"POSITION\": 0 } } ],"
		"    \"extensions\": {"
		"      \"ACME_good\": { \"ok\": true },"
		"      \"ACME_bad_str\": \"not-an-object\","
		"      \"ACME_bad_num\": 5,"
		"      \"ACME_bad_arr\": [1, 2]"
		"    }"
		"  } ],"
		"  \"accessors\": [ { \"componentType\": 5126, \"count\": 1, \"type\": \"VEC3\" } ]"
		"}";

	cgltf_options options = {0};
	cgltf_data* d0 = NULL;
	cgltf_result result = cgltf_parse(&options, doc, strlen(doc), &d0);
	CHECK(result == cgltf_result_success, "corrupt-doc parse returned %d", (int)result);
	if (result == cgltf_result_success)
	{
		CHECK(d0->meshes_count == 1, "main mesh survived");
		CHECK(d0->meshes[0].extensions_count == 1, "only healthy extension kept");
		if (d0->meshes[0].extensions_count == 1)
		{
			CHECK(strcmp(d0->meshes[0].extensions[0].name, "ACME_good") == 0,
			      "healthy extension name kept");
			CHECK(strcmp(d0->meshes[0].extensions[0].data, "{ \"ok\": true }") == 0,
			      "healthy extension payload kept byte-identical");
		}

		/* The cleaned document must still serialize and re-parse cleanly. */
		cgltf_data* d1 = roundtrip(d0, cgltf_file_type_gltf, NULL);
		CHECK(d1 != NULL, "corrupt-doc round trip");
		if (d1)
		{
			CHECK(d1->meshes[0].extensions_count == 1, "re-parsed extension count");
			cgltf_free(d1);
		}
		cgltf_free(d0);
	}
}

static void test_old_file_zero_diff(const char* path)
{
	printf("== old file (no unknown extensions) zero diff ==\n");

	cgltf_data* d0 = parse_path(path);
	CHECK(d0 != NULL, "known-ext parse");
	if (!d0)
	{
		return;
	}

	char* json1 = NULL;
	cgltf_data* d1 = roundtrip(d0, cgltf_file_type_gltf, &json1);
	CHECK(d1 != NULL, "first round trip");

	char* json2 = NULL;
	cgltf_data* d2 = roundtrip(d1, cgltf_file_type_gltf, &json2);
	CHECK(d2 != NULL, "second round trip");

	if (d1 && d2)
	{
		CHECK(strcmp(json1, json2) == 0, "serialized JSON differs across round trips");
		/* Known extension behavior is unchanged and still recognized. */
		CHECK(d2->materials[0].unlit == 1, "KHR_materials_unlit still parsed as known");
		CHECK(d2->materials[0].extensions_count == 0, "known extension not duplicated as unknown");
	}

	free(json1);
	free(json2);
	if (d2)
	{
		cgltf_free(d2);
	}
	if (d1)
	{
		cgltf_free(d1);
	}
	cgltf_free(d0);
}

int main(int argc, char** argv)
{
	const char* ext_path = argc > 1 ? argv[1] : "output/assets/ext.gltf";
	const char* known_path = argc > 2 ? argv[2] : "output/assets/known.gltf";
	const char* glb_path = argc > 3 ? argv[3] : "output/assets/ext.glb";

	printf("== structurally corrupt documents fail cleanly ==\n");
	{
		const char* corpus[] = {
			"output/corpus/crash_truncated_extensions.gltf",
			"output/corpus/leak_valueless_key.gltf"
		};
		for (size_t c = 0; c < sizeof(corpus) / sizeof(corpus[0]); ++c)
		{
			cgltf_size size = 0;
			char* bytes = read_file(corpus[c], &size);
			if (!bytes)
			{
				++failures;
				printf("  [FAIL] missing corpus file %s\n", corpus[c]);
				continue;
			}
			cgltf_options options = {0};
			cgltf_data* data = NULL;
			cgltf_result result = cgltf_parse(&options, bytes, size, &data);
			CHECK(result != cgltf_result_success, "corrupt file %s unexpectedly parsed", corpus[c]);
			/* On failure cgltf must not hand back a partially allocated object. */
			CHECK(data == NULL, "corrupt file %s produced a non-null cgltf_data", corpus[c]);
			free(bytes);
		}
	}

	printf("== GLB round trip (extensions + embedded BIN) ==\n");
	{
		char* g0_buffer = NULL;
		cgltf_data* g0 = parse_bytes_keep_buffer(glb_path, &g0_buffer);
		CHECK(g0 != NULL, "embedded-glb parse");
		if (g0)
		{
			CHECK(g0->bin_size >= 42, "embedded bin size %zu", (size_t)g0->bin_size);
			const char* tmp_path = "output/bin/glb_roundtrip_tmp.glb";
			cgltf_options gopts = {0};
			gopts.type = cgltf_file_type_glb;
			cgltf_result gw = cgltf_write_file(&gopts, tmp_path, g0);
			CHECK(gw == cgltf_result_success, "glb rewrite %d", (int)gw);

			char* g1_buffer = NULL;
			cgltf_data* g1 = parse_bytes_keep_buffer(tmp_path, &g1_buffer);
			CHECK(g1 != NULL, "rewritten glb parse");
			if (g1)
			{
				CHECK(g1->bin_size == g0->bin_size, "bin chunk size stable");
				const cgltf_size payload_size = 42; /* positions(36) + indices(6), no padding */
				CHECK(memcmp(g0->bin, g1->bin, payload_size) == 0,
				      "embedded BIN payload not byte-identical");
				compare_extensions("glb-asset", g0->asset.extensions, g0->asset.extensions_count,
				                   g1->asset.extensions, g1->asset.extensions_count);
				compare_extensions("glb-node", g0->nodes[0].extensions, g0->nodes[0].extensions_count,
				                   g1->nodes[0].extensions, g1->nodes[0].extensions_count);
				compare_extensions("glb-root", g0->data_extensions, g0->data_extensions_count,
				                   g1->data_extensions, g1->data_extensions_count);
				cgltf_free(g1);
				free(g1_buffer);
			}
			cgltf_free(g0);
			free(g0_buffer);
		}
	}

	test_unknown_extensions_preserved(ext_path, cgltf_file_type_gltf);
	test_unknown_extensions_preserved(ext_path, cgltf_file_type_glb);
	test_corrupt_extension_skipped();
	test_old_file_zero_diff(known_path);

	if (failures == 0)
	{
		printf("\nALL CHECKS PASSED\n");
		return 0;
	}

	printf("\n%d CHECK(S) FAILED\n", failures);
	return 1;
}
