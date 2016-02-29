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

#ifndef TUT4_H
#define TUT4_H

#include <pthread.h>
#include "../tut3/tut3.h"

struct tut4_per_cmd_buffer_data
{
	VkBufferView buffer_view;
	VkDescriptorSet set;
	VkFence fence;

	size_t start_index, end_index;

	/* worker thread data */
	VkDevice device;
	VkQueue queue;
	VkCommandBuffer cmd_buffer;
	VkPipeline pipeline;
	VkPipelineLayout pipeline_layout;
	uint64_t busy_time_ns;

	int success;
	VkResult error;
};

struct tut4_data
{
	VkBuffer buffer;
	VkDeviceMemory buffer_mem;
	VkDescriptorPool set_pool;
	size_t buffer_size;

	struct tut4_per_cmd_buffer_data *per_cmd_buffer;
	uint32_t per_cmd_buffer_count;

	/* test thread data */
	struct tut2_device *dev;
	struct tut3_pipelines *pipelines;
	bool busy_threads;
	pthread_t test_thread;

	int success;
	VkResult error;
};

VkResult tut4_prepare_test(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut3_pipelines *pipelines,
		struct tut4_data *test_data, size_t buffer_size, size_t thread_count);
void tut4_free_test(struct tut2_device *dev, struct tut4_data *test_data);

uint32_t tut4_find_suitable_memory(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkMemoryRequirements *mem_req, VkMemoryPropertyFlags properties);

int tut4_start_test(struct tut4_data *test_data, bool busy_threads);
void tut4_wait_test_end(struct tut4_data *test_data);

#endif
