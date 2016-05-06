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

#ifndef TUT7_H
#define TUT7_H

#include "../tut6/tut6.h"
#include "../tut4/tut4.h"

struct tut7_image
{
	/* inputs */

	/* information for creating an image, and which stage to bind it to */
	VkFormat format;
	VkExtent2D extent;
	VkImageUsageFlagBits usage;
	VkShaderStageFlagBits stage;
	bool will_be_initialized;
	bool multisample;
	uint32_t *sharing_queues;
	uint32_t sharing_queue_count;

	/* outputs */

	/* Vulkan image object */
	VkImage image;
	VkDeviceMemory image_mem;
	VkImageView view;

	VkSampler sampler;
};

struct tut7_buffer
{
	/* inputs */

	/* information for creating a buffer, and which stage to bind it to */
	VkFormat format;
	uint32_t size;
	VkBufferUsageFlagBits usage;
	VkShaderStageFlagBits stage;
	uint32_t *sharing_queues;
	uint32_t sharing_queue_count;

	/* outputs */

	/* Vulkan buffer object */
	VkBuffer buffer;
	VkDeviceMemory buffer_mem;
	VkBufferView view;
};

struct tut7_shader
{
	/* inputs */

	const char *spirv_file;
	VkShaderStageFlagBits stage;

	/* outputs */

	VkShaderModule shader;
};

struct tut7_graphics_buffers
{
	/* inputs */

	VkExtent2D surface_size;
	VkImage swapchain_image;

	/* outputs */

	VkImageView color_view;
	struct tut7_image depth;

	VkFramebuffer framebuffer;
};

VkResult tut7_create_images(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut7_image *images, uint32_t image_count);
VkResult tut7_create_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut7_buffer *buffers, uint32_t buffer_count);
VkResult tut7_load_shaders(struct tut2_device *dev,
		struct tut7_shader *shaders, uint32_t shader_count);
VkResult tut7_create_graphics_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkExtent2D surface_size, VkSurfaceFormatKHR surface_format,
		struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count, VkRenderPass *render_pass);
VkResult tut7_get_presentable_queues(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkSurfaceKHR surface, uint32_t **presentable_queues, uint32_t *presentable_queue_count);

void tut7_free_images(struct tut2_device *dev, struct tut7_image *images, uint32_t image_count);
void tut7_free_buffers(struct tut2_device *dev, struct tut7_buffer *buffers, uint32_t buffer_count);
void tut7_free_shaders(struct tut2_device *dev, struct tut7_shader *shaders, uint32_t shader_count);
void tut7_free_graphics_buffers(struct tut2_device *dev, struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count,
		VkRenderPass render_pass);

#endif
