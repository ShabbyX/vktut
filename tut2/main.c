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
#include "tut2.h"

#define MAX_DEVICES 2

int main(int argc, char **argv)
{
	VkResult res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_devs[MAX_DEVICES];
	struct tut2_device devs[MAX_DEVICES];
	uint32_t dev_count = MAX_DEVICES;

	/* Fire up Vulkan */
	res = tut1_init(&vk);
	if (res)
	{
		printf("Could not initialize Vulkan: %s\n", tut1_VkResult_string(res));
		goto exit_bad_init;
	}

	/* Take a look at what devices there are */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (res < 0)
	{
		printf("Could not enumerate devices: %s\n", tut1_VkResult_string(res));
		goto exit_bad_enumerate;
	}

	/*
	 * Set up devices.  In the early tutorials, we will use the simpler compute queues rather than graphics.  This
	 * may not be as cool, but directly moving on to graphics is a large step.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut2_setup(&phy_devs[i], &devs[i], VK_QUEUE_COMPUTE_BIT);
		if (res)
		{
			printf("Could not setup logical device, command pools and queues: %s\n", tut1_VkResult_string(res));
			goto exit_bad_setup;
		}
	}

	/*
	 * Let's take a break at this point.  There is already a lot of information in this tutorial and we need a few
	 * more things to set up before we can do anything.  You probably had heard that Vulkan is a verbose API.
	 *
	 * [Drops mic]
	 */

	printf("Got queues and command buffers, it was nice.\n");

	retval = 0;

	/* Cleanup after yourself */

exit_bad_setup:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut2_cleanup(&devs[i]);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
