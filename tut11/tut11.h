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

#ifndef TUT11_H
#define TUT11_H

#include "../tut8/tut8.h"

enum tut11_render_pass_load_op
{
	TUT11_CLEAR = 0,
	TUT11_KEEP = 1,
};

enum tut11_make_depth_buffer
{
	TUT11_WITHOUT_DEPTH = 0,
	TUT11_WITH_DEPTH = 1,
};

struct tut11_offscreen_buffers
{
	/* inputs */

	VkExtent2D surface_size;

	/* outputs */

	struct tut7_image color;
	struct tut7_image depth;

	VkFramebuffer framebuffer;
};

tut1_error tut11_create_offscreen_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkFormat format,
		struct tut11_offscreen_buffers *offscreen_buffers, uint32_t offscreen_buffer_count, VkRenderPass *render_pass,
		enum tut11_render_pass_load_op keeps_contents, enum tut11_make_depth_buffer has_depth);
tut1_error tut11_create_graphics_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkFormat format,
		struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count, VkRenderPass *render_pass,
		enum tut11_render_pass_load_op keeps_contents, enum tut11_make_depth_buffer has_depth);

void tut11_free_offscreen_buffers(struct tut2_device *dev, struct tut11_offscreen_buffers *offscreen_buffers, uint32_t offscreen_buffer_count,
		VkRenderPass render_pass);
void tut11_free_graphics_buffers(struct tut2_device *dev, struct tut7_graphics_buffers *graphics_buffers,uint32_t graphics_buffer_count,
		VkRenderPass render_pass);

#endif
