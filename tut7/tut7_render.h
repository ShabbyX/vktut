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

#ifndef TUT7_RENDER_H
#define TUT7_RENDER_H

#include "tut7.h"

struct tut7_render_essentials
{
	VkImage *images;		/* Images from the swapchain */
	VkQueue present_queue;		/* The queue to present to */
	VkCommandBuffer cmd_buffer;	/* The command buffer to render to */

	VkSemaphore sem_post_acquire;	/* The post-acquire semaphore */
	VkSemaphore sem_pre_submit;	/* The pre-submit semaphore */

	VkFence exec_fence;		/* The fence indicating when a command buffer is finished executing */
	bool first_render;		/* Whether this is the first render */
};

int tut7_render_get_essentials(struct tut7_render_essentials *essentials, struct tut1_physical_device *phy_dev,
		struct tut2_device *dev, struct tut6_swapchain *swapchain);
void tut7_render_cleanup_essentials(struct tut7_render_essentials *essentials, struct tut2_device *dev);

/*
 * Acquire an image from the swapchain, reset the command buffer, start recording, perform layout transition from
 * undefined to to_layout.
 */
int tut7_render_start(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index);
/*
 * Perform layout transition from from_layout to present src, stop recording, submit to queue for rendering, submit to
 * presentation engine for presentation.
 */
int tut7_render_finish(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index);

#endif
