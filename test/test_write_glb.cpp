#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../cgltf_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string.h>

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("err\n");
		return -1;
	}

	cgltf_options options = {};
	cgltf_data* data0 = NULL;
	cgltf_result result = cgltf_parse_file(&options, argv[1], &data0);

	// Silently skip over files that are unreadable since this is a writing test.
	if (result != cgltf_result_success)
	{
		return cgltf_result_success;
	}

	// External .gltf files keep their bytes in buffers[0].data; load them so
	// the GLB writer can embed the binary chunk in a single self-contained file.
	result = cgltf_load_buffers(&options, data0, argv[1]);
	if (result != cgltf_result_success)
	{
		return result;
	}

	options.type = cgltf_file_type_glb;  // Write back in a GLB format
	result = cgltf_write_file(&options, "out.glb", data0);
	if (result != cgltf_result_success)
	{
		return result;
	}

	// Invalid arguments must be rejected with an error code rather than
	// producing a partial or unreadable file.
	if (cgltf_write_file(&options, NULL, data0) != cgltf_result_invalid_options)
	{
		return -1;
	}
	if (cgltf_write_file(&options, "out.glb", NULL) != cgltf_result_invalid_options)
	{
		return -1;
	}

	cgltf_data* data1 = NULL;
	result = cgltf_parse_file(&options, "out.glb", &data1);
	if (result != cgltf_result_success)
	{
		return result;
	}

	if (data0->meshes_count != data1->meshes_count) {
		return -1;
	}

	// Compare structural collections.
	if (data0->accessors_count != data1->accessors_count
		|| data0->buffer_views_count != data1->buffer_views_count
		|| data0->buffers_count != data1->buffers_count
		|| data0->nodes_count != data1->nodes_count
		|| data0->materials_count != data1->materials_count) {
		return -1;
	}

	// A self-contained GLB must not point at an external buffer URI.
	if (data1->buffers_count && data1->buffers[0].uri != NULL) {
		return -1;
	}

	// Compare the logical binary payload. Sources parsed from .gltf have no
	// BIN chunk (data->bin is null); the bytes live in buffers[0].data. The
	// GLB BIN chunk may carry up to 3 spec-mandated alignment padding bytes.
	const void* bin0 = data0->bin ? data0->bin : (data0->buffers_count ? data0->buffers[0].data : NULL);
	cgltf_size logical_size = data0->buffers_count ? data0->buffers[0].size : 0;
	if (data1->bin_size < logical_size || bin0 == NULL) {
		return -1;
	}
	if (memcmp(bin0, data1->bin, logical_size) != 0) {
		return -1;
	}

	cgltf_free(data1);
	cgltf_free(data0);
	return cgltf_result_success;
}
