/*
 * Copyright (C) 2016 Shahbaz Youssefi <ShabbyX@gmail.com>
 *
 * This file is part of Shabi's Vulkan Tutorials.
 *
 * Shabi's Vulkan Tutorials is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shabi's Vulkan Tutorials is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Shabi's Vulkan Tutorials.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include "tut3.h"

#define MAX_DEVICES 2

int main(int argc, char **argv)
{
	tut1_error res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_devs[MAX_DEVICES];
	struct tut2_device devs[MAX_DEVICES];
	uint32_t dev_count = MAX_DEVICES;
	VkShaderModule shaders[MAX_DEVICES] = {NULL};
	struct tut3_pipelines pipelines[MAX_DEVICES];

	if (argc < 2)
	{
		printf("Usage: %s shader_file\n\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Fire up Vulkan */
	res = tut1_init(&vk);
	if (!tut1_error_is_success(&res))
	{
		tut1_error_printf(&res, "Could not initialize Vulkan\n");
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not enumerate devices\n");
		goto exit_bad_enumerate;
	}

	/* Set up devices */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut2_setup(&phy_devs[i], &devs[i], VK_QUEUE_COMPUTE_BIT);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not setup logical device %u, command pools and queues\n", i);
			goto exit_bad_setup;
		}
	}

	/* Load our compute shader */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut3_load_shader(&devs[i], argv[1], &shaders[i]);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not load shader on device %u\n", i);
			goto exit_bad_shader;
		}
	}

	printf("Loaded the shader, awesome!\n");

	/*
	 * Create the pipelines.  There are as many pipelines created as command buffers (just for example).  If
	 * there are not actually enough resources for them, as many as possible are created.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
		tut3_make_compute_pipeline(&devs[i], &pipelines[i], shaders[i]);

	/*
	 * Like tutorial 2, we have covered a lot of ground in this tutorial.  Let's keep actual usage of our compute
	 * shader to the next tutorial, where we would see the effect of multiple threads on the processing speed.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		uint32_t count = 0;
		for (uint32_t j = 0; j < pipelines[i].pipeline_count; ++j)
			if (pipelines[i].pipelines[j].pipeline)
				++count;

		printf("Created %u pipeline%s on device %u\n", count, count == 1?"":"s", i);
	}

	retval = 0;

	/* Cleanup after yourself */

	for (uint32_t i = 0; i < dev_count; ++i)
		tut3_destroy_pipeline(&devs[i], &pipelines[i]);

exit_bad_shader:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut3_free_shader(&devs[i], shaders[i]);

exit_bad_setup:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut2_cleanup(&devs[i]);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
