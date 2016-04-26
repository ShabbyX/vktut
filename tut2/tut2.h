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

#ifndef TUT2_H
#define TUT2_H

#include "../tut1/tut1.h"

struct tut2_commands
{
	VkQueueFlags qflags;

	VkCommandPool pool;
	VkQueue *queues;
	uint32_t queue_count;
	VkCommandBuffer *buffers;
	uint32_t buffer_count;
};

struct tut2_device
{
	VkDevice device;
	struct tut2_commands *command_pools;
	uint32_t command_pool_count;
};

VkResult tut2_get_dev(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count);
VkResult tut2_get_commands(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkDeviceQueueCreateInfo queue_info[], uint32_t queue_info_count);

static inline VkResult tut2_setup(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags)
{
	VkDeviceQueueCreateInfo queue_info[phy_dev->queue_family_count];
	uint32_t queue_info_count = phy_dev->queue_family_count;

	VkResult res = tut2_get_dev(phy_dev, dev, qflags, queue_info, &queue_info_count);
	if (res == 0)
		res = tut2_get_commands(phy_dev, dev, queue_info, queue_info_count);
	return res;
}
void tut2_cleanup(struct tut2_device *dev);

#endif
