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
#include "tut1.h"

#define MAX_DEVICES 2

static void print_surprise(const char *indent, const char *who, const char *what, const char *how)
{
	static bool already_asked = false;
	static char choice = 0;;
	if (!already_asked)
	{
		printf("Are you a woman or a man? ");
		if (scanf(" %c%*s", &choice) != 1)
			choice = 0;
		already_asked = true;
	}

	printf("%s", indent);
	if (choice == 'w' || choice == 'W')
		printf("Damn girl, ");
	else if (choice == 'm' || choice == 'M')
		printf("Whoa dude, ");
	else
		printf("Wow neither-woman-nor-man, ");

	printf("%s more %s than I could %s.\n", who, what, how);
}

int main(int argc, char **argv)
{
	VkResult res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device devs[MAX_DEVICES];
	uint32_t dev_count = MAX_DEVICES;

	/* Fire up Vulkan */
	res = tut1_init(&vk);
	if (res)
	{
		printf("Could not initialize Vulkan: %s\n", tut1_VkResult_string(res));
		goto exit_bad_init;
	}

	printf("Vulkan is in the house.\n");

	/* Take a look at what devices there are */
	res = tut1_enumerate_devices(vk, devs, &dev_count);
	if (res < 0)
	{
		printf("Could not enumerate devices: %s\n", tut1_VkResult_string(res));
		goto exit_bad_enumerate;
	}
	else if (res == VK_INCOMPLETE)
	{
		print_surprise("", "you've got", "devices", "dream of");
		printf("I have information on only %u of them:\n", dev_count);
	}
	else
		printf("I detected the following %u device%s:\n", dev_count, dev_count == 1?"":"s");

	/*
	 * Print out some of the information taken when enumerating physical devices.  This is by no means an
	 * exhaustive printout, but to give you the idea.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		struct tut1_physical_device *dev = &devs[i];
		VkPhysicalDeviceProperties *pr = &dev->properties;

		printf("  - %s: %s (id: 0x%04X) from vendor 0x%04X [driver version: 0x%04X, API version: 0x%04X]\n",
				tut1_VkPhysicalDeviceType_string(pr->deviceType), pr->deviceName,
				pr->deviceID, pr->vendorID, pr->driverVersion, pr->apiVersion);
		if (dev->queue_families_incomplete)
		{
			print_surprise("    ", "your device", "queue families", "imagine");
			printf("    I have information on only %u of them:\n", dev->queue_family_count);
		}
		else
			printf("    The device supports the following %u queue famil%s:\n", dev->queue_family_count, dev->queue_family_count == 1?"y":"ies");

		for (uint32_t j = 0; j < dev->queue_family_count; ++j)
		{
			VkQueueFamilyProperties *qf = &dev->queue_families[j];

			printf("    * %u queue%s with the following capabilit%s:\n", qf->queueCount, qf->queueCount == 1?"":"s",
					qf->queueFlags && (qf->queueFlags & (qf->queueFlags - 1)) == 0?"y":"ies");
			if (qf->queueFlags == 0)
				printf("          None\n");
			if ((qf->queueFlags & VK_QUEUE_GRAPHICS_BIT))
				printf("          Graphics\n");
			if ((qf->queueFlags & VK_QUEUE_COMPUTE_BIT))
				printf("          Compute\n");
			if ((qf->queueFlags & VK_QUEUE_TRANSFER_BIT))
				printf("          Transfer\n");
			if ((qf->queueFlags & VK_QUEUE_SPARSE_BINDING_BIT))
				printf("          Sparse binding\n");
		}
	}

	/* Congratulations, you can now duplicate the `vulkaninfo` program. */

	retval = 0;

	/* Cleanup after yourself */

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
