

#define CGLTF_IMPLEMENTATION
#include "../cgltf.h"

#include <stdio.h>

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("err\n");
		return -1;
	}

	cgltf_options options;
	memset(&options, 0, sizeof(cgltf_options));
	cgltf_data* data = NULL;
	cgltf_result result = cgltf_parse_file(&options, argv[1], &data);

	if (result == cgltf_result_success)
		result = cgltf_load_buffers(&options, data, argv[1]);

	if (result == cgltf_result_success)
		result = cgltf_validate(data);

	printf("Result: %d\n", result);

	if (result == cgltf_result_success)
	{
		printf("Type: %u\n", data->file_type);
		printf("Meshes: %u\n", (unsigned)data->meshes_count);

		for (cgltf_size i = 0; i < data->meshes_count; ++i)
		{
			const cgltf_mesh* mesh = &data->meshes[i];
			cgltf_size target_count = mesh->primitives_count ? mesh->primitives[0].targets_count : 0;

			if (target_count)
			{
				printf("Mesh %u morph targets: %u\n", (unsigned)i, (unsigned)target_count);
			}

			if (mesh->target_names_count)
			{
				printf("Mesh %u target names:", (unsigned)i);
				for (cgltf_size j = 0; j < mesh->target_names_count; ++j)
				{
					printf(" %s", mesh->target_names[j]);
				}
				printf("\n");
			}

			if (mesh->weights_count)
			{
				printf("Mesh %u weights:", (unsigned)i);
				for (cgltf_size j = 0; j < mesh->weights_count; ++j)
				{
					printf(" %g", (double)mesh->weights[j]);
				}
				printf("\n");
			}
		}

		for (cgltf_size i = 0; i < data->nodes_count; ++i)
		{
			const cgltf_node* node = &data->nodes[i];

			if (node->weights_count)
			{
				printf("Node %u weights:", (unsigned)i);
				for (cgltf_size j = 0; j < node->weights_count; ++j)
				{
					printf(" %g", (double)node->weights[j]);
				}
				printf("\n");
			}
		}

		for (cgltf_size i = 0; i < data->animations_count; ++i)
		{
			const cgltf_animation* anim = &data->animations[i];

			for (cgltf_size j = 0; j < anim->channels_count; ++j)
			{
				const cgltf_animation_channel* channel = &anim->channels[j];

				if (channel->target_path == cgltf_animation_path_type_weights && channel->target_node)
				{
					printf("Animation %u morph channel: node %u, %u keys, %u weights per key\n",
						(unsigned)i,
						(unsigned)(channel->target_node - data->nodes),
						(unsigned)channel->sampler->input->count,
						(unsigned)(channel->sampler->input->count ? channel->sampler->output->count / channel->sampler->input->count : 0));
				}
			}
		}
	}

	cgltf_free(data);

	return result;
}
