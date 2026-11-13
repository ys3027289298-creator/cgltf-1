/* Emits concrete parse->write artifacts (glTF + GLB) for manual inspection. */
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../../cgltf_write.h"

#include <stdio.h>
#include <stdlib.h>

static char* read_file(const char* path, size_t* size_out)
{
	FILE* f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* buf = (char*)malloc((size_t)size + 1);
	size_t read = fread(buf, 1, (size_t)size, f);
	fclose(f);
	buf[read] = 0;
	*size_out = (size_t)read;
	return buf;
}

int main(int argc, char** argv)
{
	if (argc < 4)
	{
		printf("usage: %s in.gltf out.gltf out.glb\n", argv[0]);
		return 2;
	}

	size_t size = 0;
	char* buf = read_file(argv[1], &size);

	cgltf_options options = {0};
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse(&options, buf, size, &data);
	free(buf);
	if (result != cgltf_result_success)
	{
		printf("parse failed %d\n", (int)result);
		return 1;
	}

	options.type = cgltf_file_type_gltf;
	result = cgltf_write_file(&options, argv[2], data);
	if (result != cgltf_result_success)
	{
		printf("gltf write failed %d\n", (int)result);
		return 1;
	}

	options.type = cgltf_file_type_glb;
	result = cgltf_write_file(&options, argv[3], data);
	if (result != cgltf_result_success)
	{
		printf("glb write failed %d\n", (int)result);
		return 1;
	}

	cgltf_free(data);
	printf("wrote %s and %s\n", argv[2], argv[3]);
	return 0;
}
