/* GLB roundtrip + invalid-input verifier for the cgltf GLB writer fix.
 * Build: see output/run_demo.sh
 */
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../cgltf_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } else { printf("  ok:   %s\n", msg); } } while (0)

/* Recursively compare the structural parts that survive a write/read cycle. */
static void compare(const cgltf_data* a, const cgltf_data* b)
{
	CHECK(a->meshes_count == b->meshes_count, "meshes_count equal");
	CHECK(a->materials_count == b->materials_count, "materials_count equal");
	CHECK(a->nodes_count == b->nodes_count, "nodes_count equal");
	CHECK(a->accessors_count == b->accessors_count, "accessors_count equal");
	CHECK(a->buffer_views_count == b->buffer_views_count, "buffer_views_count equal");
	CHECK(a->buffers_count == b->buffers_count, "buffers_count equal");
	CHECK(a->scenes_count == b->scenes_count, "scenes_count equal");
	CHECK(a->images_count == b->images_count, "images_count equal");
	CHECK(a->textures_count == b->textures_count, "textures_count equal");

	/* Binary blob must be byte-identical. Source .gltf keeps the bytes in
	 * buffers[0].data (no BIN chunk), while a GLB reports its padded BIN
	 * chunk, so compare against the logical buffer byteLength. */
	cgltf_size logical = a->buffers_count ? a->buffers[0].size : 0;
	CHECK(b->bin_size >= logical, "GLB BIN chunk holds full logical payload (plus <=3 pad bytes)");
	const void* a_bin = a->bin ? a->bin : (a->buffers_count ? a->buffers[0].data : NULL);
	if (a_bin && b->bin)
		CHECK(memcmp(a_bin, b->bin, logical) == 0, "bin payload byte-identical (logical length)");

	if (a->buffers_count && b->buffers_count)
	{
		CHECK(a->buffers[0].size == b->buffers[0].size, "buffers[0].byteLength equal");
		CHECK(b->buffers[0].uri == NULL, "GLB buffers[0] has no uri");
	}
	if (a->meshes_count && b->meshes_count)
	{
		CHECK(a->meshes[0].primitives_count == b->meshes[0].primitives_count, "primitive count equal");
		CHECK(strcmp(a->meshes[0].name, b->meshes[0].name) == 0, "mesh name preserved");
	}
	if (a->nodes_count && b->nodes_count)
		CHECK(strcmp(a->nodes[0].name, b->nodes[0].name) == 0, "node name preserved");
	if (a->accessors_count && b->accessors_count)
	{
		CHECK(a->accessors[0].count == b->accessors[0].count, "accessor[0].count equal");
		CHECK(a->accessors[0].component_type == b->accessors[0].component_type, "accessor[0].component_type equal");
		CHECK(a->accessors[1].component_type == b->accessors[1].component_type, "index accessor component_type equal");
	}
	if (a->buffer_views_count && b->buffer_views_count)
	{
		CHECK(a->buffer_views[0].size == b->buffer_views[0].size, "bufferView[0].byteLength equal");
		CHECK(a->buffer_views[1].offset == b->buffer_views[1].offset, "bufferView[1].byteOffset equal");
	}
}

/* Parse the raw container and validate GLB chunk layout/alignment/self-consistency. */
static void check_container(const char* path, cgltf_size expect_json, cgltf_size expect_bin)
{
	FILE* f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	long file_len = ftell(f);
	fseek(f, 0, SEEK_SET);
	unsigned char* bytes = (unsigned char*)malloc((size_t)file_len);
	if (fread(bytes, 1, (size_t)file_len, f) != (size_t)file_len) { printf("  FAIL: short read\n"); failures++; }
	fclose(f);

	CHECK(file_len >= 12, "file >= 12-byte header");
	CHECK(memcmp(bytes, "glTF", 4) == 0, "magic glTF");
	uint32_t version, total;
	memcpy(&version, bytes + 4, 4);
	memcpy(&total, bytes + 8, 4);
	CHECK(version == 2, "version == 2");
	CHECK((uint32_t)file_len == total, "header total length == actual file size");

	uint32_t jlen; uint32_t jmagic;
	memcpy(&jlen, bytes + 12, 4);
	memcpy(&jmagic, bytes + 16, 4);
	CHECK(jmagic == 0x4E4F534Au, "JSON chunk magic");
	CHECK(jlen % 4 == 0, "JSON chunk length 4-byte aligned");
	CHECK(jlen == (expect_json + 3) / 4 * 4, "JSON chunk length matches payload + padding");
	CHECK(jlen >= expect_json, "JSON padding does not shrink payload");

	if (expect_bin > 0)
	{
		size_t off = 12 + 8 + jlen;
		uint32_t blen, bmagic;
		memcpy(&blen, bytes + off, 4);
		memcpy(&bmagic, bytes + off + 4, 4);
		CHECK(bmagic == 0x004E4942u, "BIN chunk magic");
		CHECK(blen % 4 == 0, "BIN chunk length 4-byte aligned");
		CHECK(blen == (expect_bin + 3) / 4 * 4, "BIN chunk length matches payload + padding");
		CHECK((size_t)file_len == off + 8 + blen, "BIN chunk is last and fills file");
	}
	else
	{
		CHECK((size_t)file_len == 12 + 8 + jlen, "no BIN chunk: file ends after JSON");
	}
	free(bytes);
}

int main(int argc, char** argv)
{
	if (argc < 2) { printf("usage: %s <asset.gltf> [baddir]\n", argv[0]); return 2; }

	/* ---------- Stage A: roundtrip ---------- */
	printf("== A. parse external .gltf + load buffers ==\n");
	cgltf_options opt = { 0 };
	cgltf_data* src = NULL;
	cgltf_result r = cgltf_parse_file(&opt, argv[1], &src);
	CHECK(r == cgltf_result_success, "parse source gltf");
	r = cgltf_load_buffers(&opt, src, argv[1]);
	CHECK(r == cgltf_result_success, "load external .bin");

	printf("== B. write single-file GLB ==\n");
	opt.type = cgltf_file_type_glb;
	const char* out_path = "output/roundtrip.glb";
	r = cgltf_write_file(&opt, out_path, src);
	CHECK(r == cgltf_result_success, "cgltf_write_file GLB returns success");

	/* JSON size inside the GLB for container checks. */
	cgltf_size json_len = cgltf_write(&opt, NULL, 0, src) - 1;

	printf("== C. read GLB back and compare structure ==\n");
	cgltf_data* dst = NULL;
	r = cgltf_parse_file(&opt, out_path, &dst);
	CHECK(r == cgltf_result_success, "re-parse written GLB");
	CHECK(dst->file_type == cgltf_file_type_glb, "detected as GLB");
	compare(src, dst);

	printf("== D. container layout: alignment + self-consistency ==\n");
	check_container(out_path, json_len, src->buffers[0].size);

	printf("== E. second write from GLB data is stable ==\n");
	const char* out2 = "output/roundtrip2.glb";
	r = cgltf_write_file(&opt, out2, dst);
	CHECK(r == cgltf_result_success, "rewrite parsed GLB");
	FILE* f1 = fopen(out_path, "rb"); FILE* f2 = fopen(out2, "rb");
	fseek(f1, 0, SEEK_END); fseek(f2, 0, SEEK_END);
	long s1 = ftell(f1), s2 = ftell(f2);
	CHECK(s1 == s2, "GLB->GLB output size stable");
	unsigned char *p1 = malloc((size_t)s1), *p2 = malloc((size_t)s2);
	fseek(f1, 0, SEEK_SET); fseek(f2, 0, SEEK_SET);
	size_t n1 = fread(p1, 1, (size_t)s1, f1); size_t n2 = fread(p2, 1, (size_t)s2, f2);
	CHECK(n1 == (size_t)s1 && n2 == (size_t)s2, "rewritten files fully read");
	CHECK(memcmp(p1, p2, (size_t)s1) == 0, "GLB->GLB bytes identical");
	fclose(f1); fclose(f2); free(p1); free(p2);

	cgltf_free(dst);
	cgltf_free(src);

	/* ---------- Stage F: invalid inputs must return errors, no bad files ---------- */
	printf("== F. invalid inputs rejected with error codes ==\n");
	cgltf_data* dummy = NULL;
	cgltf_parse_file(&(cgltf_options){0}, argv[1], &dummy);
	cgltf_load_buffers(&(cgltf_options){0}, dummy, argv[1]);

	remove("output/bad_null_path.glb");
	r = cgltf_write_file(&opt, NULL, dummy);
	CHECK(r == cgltf_result_invalid_options, "NULL path -> invalid_options");
	FILE* probe = fopen("output/bad_null_path.glb", "rb");
	CHECK(probe == NULL, "no file created for NULL path");
	if (probe) fclose(probe);

	remove("output/bad_null_data.glb");
	r = cgltf_write_file(&opt, "output/bad_null_data.glb", NULL);
	CHECK(r == cgltf_result_invalid_options, "NULL data -> invalid_options");
	probe = fopen("output/bad_null_data.glb", "rb");
	CHECK(probe == NULL, "no file created for NULL data");
	if (probe) fclose(probe);

	cgltf_options bad_type = opt;
	bad_type.type = (cgltf_file_type)99;
	remove("output/bad_type.glb");
	r = cgltf_write_file(&bad_type, "output/bad_type.glb", dummy);
	CHECK(r == cgltf_result_invalid_options, "bogus options.type -> invalid_options");
	probe = fopen("output/bad_type.glb", "rb");
	CHECK(probe == NULL, "no file created for bogus type");
	if (probe) fclose(probe);

	/* Unloaded buffer (parse without load_buffers): GLB cannot be self-contained. */
	cgltf_data* unloaded = NULL;
	cgltf_parse_file(&(cgltf_options){0}, argv[1], &unloaded);
	remove("output/bad_unloaded.glb");
	r = cgltf_write_file(&opt, "output/bad_unloaded.glb", unloaded);
	CHECK(r != cgltf_result_success, "unloaded external buffer -> error, no broken GLB");
	probe = fopen("output/bad_unloaded.glb", "rb");
	CHECK(probe == NULL, "failed write leaves no partial file");
	if (probe) fclose(probe);
	cgltf_free(unloaded);

	/* Malformed GLB files on disk must be refused by the parser. */
	if (argc >= 3)
	{
		const char* bads[] = { "bad_magic.glb", "bad_jsonmagic.glb", "truncated.glb", "bad_total.glb" };
		for (size_t i = 0; i < sizeof(bads)/sizeof(bads[0]); ++i)
		{
			char p[256];
			snprintf(p, sizeof(p), "%s/%s", argv[2], bads[i]);
			cgltf_data* junk = NULL;
			cgltf_result pr = cgltf_parse_file(&opt, p, &junk);
			printf("  %s -> %d\n", bads[i], (int)pr);
			CHECK(pr != cgltf_result_success, "malformed GLB rejected");
			if (junk) cgltf_free(junk);
		}
	}

	cgltf_free(dummy);
	printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
	return failures ? 1 : 0;
}
