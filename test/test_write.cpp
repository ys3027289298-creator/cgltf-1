#define CGLTF_IMPLEMENTATION
#define CGLTF_WRITE_IMPLEMENTATION
#include "../cgltf_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

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

	result = cgltf_write_file(&options, "out.gltf", data0);
	if (result != cgltf_result_success)
	{
		return result;
	}
	cgltf_data* data1 = NULL;
	result = cgltf_parse_file(&options, "out.gltf", &data1);
	if (result != cgltf_result_success)
	{
		return result;
	}
	if (data0->meshes_count != data1->meshes_count) {
		return -1;
	}

	// Morph targets and weights must survive the round trip.
	for (cgltf_size i = 0; i < data0->meshes_count; ++i)
	{
		const cgltf_mesh* mesh0 = &data0->meshes[i];
		const cgltf_mesh* mesh1 = &data1->meshes[i];

		if (mesh0->primitives_count != mesh1->primitives_count) {
			return -1;
		}
		for (cgltf_size j = 0; j < mesh0->primitives_count; ++j)
		{
			if (mesh0->primitives[j].targets_count != mesh1->primitives[j].targets_count) {
				return -1;
			}
		}
		if (mesh0->weights_count != mesh1->weights_count || mesh0->target_names_count != mesh1->target_names_count) {
			return -1;
		}
		for (cgltf_size j = 0; j < mesh0->weights_count; ++j)
		{
			if (mesh0->weights[j] != mesh1->weights[j]) {
				return -1;
			}
		}
	}

	if (data0->nodes_count != data1->nodes_count) {
		return -1;
	}
	for (cgltf_size i = 0; i < data0->nodes_count; ++i)
	{
		const cgltf_node* node0 = &data0->nodes[i];
		const cgltf_node* node1 = &data1->nodes[i];

		if (node0->weights_count != node1->weights_count) {
			return -1;
		}
		for (cgltf_size j = 0; j < node0->weights_count; ++j)
		{
			if (node0->weights[j] != node1->weights[j]) {
				return -1;
			}
		}
	}

	// Morph animation channels must be restorable.
	if (data0->animations_count != data1->animations_count) {
		return -1;
	}
	for (cgltf_size i = 0; i < data0->animations_count; ++i)
	{
		const cgltf_animation* anim0 = &data0->animations[i];
		const cgltf_animation* anim1 = &data1->animations[i];

		if (anim0->channels_count != anim1->channels_count) {
			return -1;
		}
		for (cgltf_size j = 0; j < anim0->channels_count; ++j)
		{
			const cgltf_animation_channel* channel0 = &anim0->channels[j];
			const cgltf_animation_channel* channel1 = &anim1->channels[j];

			if (channel0->target_path != channel1->target_path) {
				return -1;
			}
			if (channel0->target_path == cgltf_animation_path_type_weights &&
				channel0->sampler->output->count != channel1->sampler->output->count) {
				return -1;
			}
		}
	}

	cgltf_free(data1);
	cgltf_free(data0);
	return cgltf_result_success;
}
