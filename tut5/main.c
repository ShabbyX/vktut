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
#include "tut5.h"

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
	res = tut5_init(&vk);
	if (res)
	{
		printf("Could not initialize Vulkan: %s\n", tut1_VkResult_string(res));
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (res < 0)
	{
		printf("Could not enumerate devices: %s\n", tut1_VkResult_string(res));
		goto exit_bad_enumerate;
	}

	/*
	 * This step is similar to Tutorial 2, except that instead of tut2_setup we use tut5_setup.  The difference is
	 * that tut5_setup also enables layers and extensions.  From this Tutorial on, we will always use tut5_setup
	 * instead of tut2_setup.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut5_setup(&phy_devs[i], &devs[i], VK_QUEUE_COMPUTE_BIT);
		if (res < 0)
		{
			printf("Could not setup logical device %u, command pools and queues: %s\n", i, tut1_VkResult_string(res));
			goto exit_bad_setup;
		}
	}

	/* Let's print layers and extensions that we detected and are enabled */
	tut5_print_layers_and_extensions();
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		printf("\n");
		tut5_print_device_layers_and_extensions(&phy_devs[i]);
	}

	/*
	 * While mass-enabling all layers and extensions has been fun, it's not really what you should be doing.
	 * Neither during your own development, and especially nor when releasing your software/game.  Instead, you
	 * normally look for a specific set of layers or extensions that serve your need.
	 *
	 * For example, VK_LAYER_LUNARG_param_checker is useful to make sure you got all the parameters to Vulkan
	 * functions correctly.  There are many useful layers already and you can just list them in an array and
	 * provide that to Vulkan, assuming you can tolerate some of them not existing on another computer (if you
	 * release the software).
	 *
	 * Vulkan has a nice way of setting itself up.  Normally, when you use Vulkan functions, they go through a
	 * Vulkan "loader" that call the actual Vulkan functions.  You could actually retrieve those functions yourself
	 * and circumvent the loader, but the loader is nice!  One nice thing it does for you is to manage layers and
	 * extensions.  What's even nicer is that it can be affected by environment variables.
	 *
	 * Before Tutorial 5, we hadn't had any layers enabled.  What if we want to enable the param_checker layer to
	 * see if there were any mistakes in previous Tutorials?  One way is to change their code to use `tut5_setup()`
	 * instead of `tut2_setup()` and get all layers and extensions enabled.  Another way is to use environment
	 * variables to have the loader enable any layers we want, without the rebuilding the code.  The nice thing
	 * about this is that you actually never need to think about layers in the code itself.  You can do without
	 * them, and during testing just enable those layers you need from the outside.  This is not true (as far as I
	 * can tell) about extensions; likely because the code needs to know what extensions are enabled to use them,
	 * so it must enable them itself (if I'm wrong, please correct me).
	 *
	 * To enable layers using environment variables, you can do the following:
	 *
	 *     $ export VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_param_checker
	 *     $ export VK_DEVICE_LAYERS=VK_LAYER_LUNARG_param_checker
	 *
	 * and then run your program.  Needless to say, the VK_INSTANCE_LAYERS enables layers that apply to the Vulkan
	 * instance and VK_DEVICE_LAYERS enables those that are per-device.  The variables can actually take a list of
	 * layers that are separated by : (or ; on windows, because windows).
	 *
	 * You can go ahead and set the above environment variables and run any of the previous tutorials.  I found a
	 * bug in Tutorial 2 with it actually myself, which resulted in this:
	 *
	 *     https://github.com/LunarG/VulkanTools/issues/16
	 *
	 * Feel free to experiment with other layers as well.  There are very useful layers available, which I will not
	 * get into (at least not any time soon), but I'm sure you can benefit from.
	 */

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
