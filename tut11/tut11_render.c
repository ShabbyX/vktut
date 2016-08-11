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

#include "tut11_render.h"

int tut11_render_start(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index)
{
	/* Nothing different here */
	return tut7_render_start(essentials, dev, swapchain, to_layout, image_index);
}

int tut11_render_finish(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index,
		VkSemaphore wait_sem, VkSemaphore signal_sem)
{
	/* This is all the same as in tut7_render_finish, except the added semaphores */
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/* Transition image to PRESENT_SRC */
	VkImageMemoryBarrier image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = from_layout,
		.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = essentials->images[image_index],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	/* All WRITEs anywhere must be done before all READs after the pipeline. */
	vkCmdPipelineBarrier(essentials->cmd_buffer,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */

	vkEndCommandBuffer(essentials->cmd_buffer);

	res = vkResetFences(dev->device, 1, &essentials->exec_fence);
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Failed to reset fence\n");
		return res;
	}

	/*
	 * Submit to queue.
	 *
	 * Wait on wait_sem (if provided) in addition to sem_post_acquire.  Signal signal_sem (if provided) in addition
	 * to sem_pre_submit.
	 */
	VkSemaphore wait_sems[2] = {essentials->sem_post_acquire, wait_sem};
	VkSemaphore signal_sems[2] = {essentials->sem_pre_submit, signal_sem};
	VkPipelineStageFlags wait_sem_stages[2] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = wait_sem?2:1,
		.pWaitSemaphores = wait_sems,
		.pWaitDstStageMask = wait_sem_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &essentials->cmd_buffer,
		.signalSemaphoreCount = signal_sem?2:1,
		.pSignalSemaphores = signal_sems,
	};
	vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);

	/* Present image */
	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &essentials->sem_pre_submit,
		.swapchainCount = 1,
		.pSwapchains = &swapchain->swapchain,
		.pImageIndices = &image_index,
	};
	res = vkQueuePresentKHR(essentials->present_queue, &present_info);
	tut1_error_set_vkresult(&retval, res);
	if (res < 0)
	{
		tut1_error_printf(&retval, "Failed to queue image for presentation\n");
		return -1;
	}

	return 0;
}
