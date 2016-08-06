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

#ifndef TUT1_H
#define TUT1_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include "tut1_error.h"

tut1_error tut1_init(VkInstance *vk);
void tut1_exit(VkInstance vk);

#define TUT1_MAX_QUEUE_FAMILY 10

struct tut1_physical_device
{
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceMemoryProperties memories;

	VkQueueFamilyProperties queue_families[TUT1_MAX_QUEUE_FAMILY];
	uint32_t queue_family_count;
	bool queue_families_incomplete;
};

tut1_error tut1_enumerate_devices(VkInstance vk, struct tut1_physical_device *devs, uint32_t *count);

const char *tut1_VkPhysicalDeviceType_string(VkPhysicalDeviceType type);

#endif
