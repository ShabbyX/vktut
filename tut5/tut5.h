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

#ifndef TUT5_H
#define TUT5_H

#include "../tut1/tut1.h"
#include "../tut2/tut2.h"

/* This tutorial replaces tut1_init and tut2_get_dev with versions that enable layers and extensions */
tut1_error tut5_init(VkInstance *vk);
tut1_error tut5_get_dev(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count);

void tut5_print_layers_and_extensions(void);
void tut5_print_device_layers_and_extensions(struct tut1_physical_device *phy_dev);

static inline tut1_error tut5_setup(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags)
{
	VkDeviceQueueCreateInfo queue_info[phy_dev->queue_family_count];
	uint32_t queue_info_count = phy_dev->queue_family_count;

	tut1_error res = tut5_get_dev(phy_dev, dev, qflags, queue_info, &queue_info_count);
	if (tut1_error_is_success(&res))
		res = tut2_get_commands(phy_dev, dev, queue_info, queue_info_count);
	return res;
}

#endif
