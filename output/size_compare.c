/* Size comparison: parse -> write GLB, print original vs output byte counts. */
#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../cgltf_write.h"
#include <stdio.h>
#include <sys/stat.h>

static long fsize(const char* p) { struct stat st; return stat(p, &st) ? -1 : (long)st.st_size; }

int main(int argc, char** argv)
{
	if (argc < 3) { printf("usage: %s in out.glb [load_buffers:0|1]\n", argv[0]); return 2; }
	cgltf_options opt = { 0 };
	cgltf_data* data = NULL;
	cgltf_result r = cgltf_parse_file(&opt, argv[1], &data);
	if (r) { printf("parse error %d for %s\n", r, argv[1]); return 1; }
	if (argc >= 4 && argv[3][0] == '1')
	{
		r = cgltf_load_buffers(&opt, data, argv[1]);
		if (r) { printf("load error %d for %s\n", r, argv[1]); return 1; }
	}
	opt.type = cgltf_file_type_glb;
	r = cgltf_write_file(&opt, argv[2], data);
	if (r) { printf("write error %d for %s\n", r, argv[1]); return 1; }
	long in = fsize(argv[1]), out = fsize(argv[2]);
	double pct = in ? (out - in) * 100.0 / in : 0;
	printf("%-58s in=%8ld out=%8ld delta=%+7ld (%+.2f%%)\n", argv[1], in, out, out - in, pct);
	cgltf_free(data);
	return 0;
}
