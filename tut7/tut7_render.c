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

#include "tut7_render.h"

int tut7_render_get_essentials(struct tut7_render_essentials *essentials, struct tut1_physical_device *phy_dev,
		struct tut2_device *dev, struct tut6_swapchain *swapchain)
{
	VkResult res;

	/* Like in Tutorial 6, take the list of swapchain images for future */
	essentials->images = tut6_get_swapchain_images(dev, swapchain, &essentials->image_count);
	if (essentials->images == NULL)
		return -1;

	/*
	 * Take the first queue out of the first presentable queue family (and command buffer on it) to use for
	 * presentation (for now)
	 */
	uint32_t *presentable_queues = NULL;
	uint32_t presentable_queue_count = 0;

	if (tut7_get_presentable_queues(phy_dev, dev, swapchain->surface,
				&presentable_queues, &presentable_queue_count) || presentable_queue_count == 0)
	{
		printf("No presentable queue families!  What kind of graphics card is this!\n");
		return -1;
	}

	essentials->present_queue = dev->command_pools[presentable_queues[0]].queues[0];
	essentials->cmd_buffer = dev->command_pools[presentable_queues[0]].buffers[0];
	free(presentable_queues);

	/* Create semaphores for synchronization (details in tut7_render_start) */
	VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};

	res = vkCreateSemaphore(dev->device, &sem_info, NULL, &essentials->sem_post_acquire);
	if (res)
	{
		printf("Failed to create post-acquire semaphore: %s\n", tut1_VkResult_string(res));
		return -1;
	}

	res = vkCreateSemaphore(dev->device, &sem_info, NULL, &essentials->sem_pre_submit);
	if (res)
	{
		printf("Failed to create pre-submit semaphore: %s\n", tut1_VkResult_string(res));
		return -1;
	}

	/* Create fence for throttling the rendering (details in tut7_render_start) */
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};

	res = vkCreateFence(dev->device, &fence_info, NULL, &essentials->exec_fence);
	if (res)
	{
		printf("Failed to create fence: %s\n", tut1_VkResult_string(res));
		return -1;
	}

	essentials->first_render = true;

	return 0;
}

void tut7_render_cleanup_essentials(struct tut7_render_essentials *essentials, struct tut2_device *dev)
{
	vkDeviceWaitIdle(dev->device);

	vkDestroySemaphore(dev->device, essentials->sem_post_acquire, NULL);
	vkDestroySemaphore(dev->device, essentials->sem_pre_submit, NULL);
	vkDestroyFence(dev->device, essentials->exec_fence, NULL);
	free(essentials->images);
}

int tut7_render_start(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index)
{
	VkResult res;

	/* Use `vkAcquireNextImageKHR` to get an image to render to */

	res = vkAcquireNextImageKHR(dev->device, swapchain->swapchain, 1000000000, essentials->sem_post_acquire, NULL, image_index);
	if (res == VK_TIMEOUT)
	{
		printf("A whole second and no image.  I give up.\n");
		return -1;
	}
	else if (res == VK_SUBOPTIMAL_KHR)
		printf("Did you change the window size?  I didn't recreate the swapchains,\n"
				"so the presentation is now suboptimal.\n");
	else if (res < 0)
	{
		printf("Couldn't acquire image: %s\n", tut1_VkResult_string(res));
		return -1;
	}

	/*
	 * Unless the first time we are rendering, wait for the last frame to finish rendering.  Let's wait up to a
	 * second, and if the fence is still not signalled, we'll assume something went horribly wrong and quit.
	 *
	 * If no data was sent to the GPU to be used for rendering, this wait could have been delayed all the way to
	 * just before the next submission.
	 */
	if (!essentials->first_render)
	{
		res = vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
		if (res)
		{
			printf("Wait for fence failed: %s\n", tut1_VkResult_string(res));
			return -1;
		}
		essentials->first_render = false;
	}

	/*
	 * We have seen many of the command buffer functions in Tutorial 4.  Here is a short recap:
	 *
	 * - reset: remove all previous recordings from the command buffer
	 * - begin: start recording
	 * - bind pipeline: specify the pipeline the commands run on (unused here)
	 * - bind descriptor set: specify resources to use for rendering (unused here)
	 * - end: stop recording
	 */
	vkResetCommandBuffer(essentials->cmd_buffer, 0);
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	res = vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
	if (res)
	{
		printf("Couldn't even begin recording a command buffer: %s\n", tut1_VkResult_string(res));
		return -1;
	}

	/*
	 * To transition an image to a new layout, an image barrier is used.  Before we see how that is done, let's see
	 * what it even means.
	 *
	 * In Vulkan, there are barriers on different kinds of resources (images, buffers and memory) and other means
	 * to specify execution dependency.  In each case, you want to make sure some actions A are all executed before
	 * some actions B.  In the specific case of barriers, A could be actions that do something to the resource and
	 * B could be actions that need the result of those actions.
	 *
	 * In our specific case, we want to change the layout of a swapchain image.  For the transition from present
	 * src, we want to make sure that all writes to the image are done after the transition is done.  For the
	 * transition to present src, we want to make sure that all writes to the image are done before the transition
	 * is done.  Note: if we had a graphics pipeline, we would be talking about "color attachment writes" instead
	 * of just "writes".  Keep that in mind.
	 *
	 * Using a VkImageMemoryBarrier, we are not only specifying how the image layout should change (if changed at
	 * all), but also defining the actions A and B where an execution dependency would be created.  In the first
	 * barrier (transition from present src), all reads of the image (for previous presentation) must happen before
	 * the barrier (A is the set of READ operations), and all writes must be done after the barrier (B is the set
	 * of WRITE operations).  The situation is reversed with the second barrier (transition to present src).
	 *
	 * In Vulkan, actions A are referred to as `src` and actions B are referred to as `dst`.
	 *
	 * Using an image barrier, it's also possible to transfer one image from a queue family to another, in which
	 * case A is the set of actions accessing the image in the first queue family and B is the set of actions
	 * accessing the image in the second queue family.  We are not moving between queue families, so we'll specify
	 * this intention as well.
	 *
	 * In our layout transition, we are transitioning from present src to to_layout and back.  However, the first
	 * time the transition happens, the swapchain image layout is actually UNDEFINED.  Either way, since we are not
	 * interested in what was previously in the image when we are just about to render into it, we can set the
	 * `oldLayout` (the layout transitioning from) to UNDEFINED.  This makes the transition more efficient because
	 * Vulkan knows it can just throw away the contents of the image.  Note: in Tutorial 7, we are transition to
	 * "general", but if we had a graphics pipeline, we would be transition to the "color attachment optimal"
	 * layout instead.
	 *
	 * Finally, we need to specify which part of the image (subresource) is being transitioned.  We want to
	 * transition COLOR parts of the image (which in this case, all of the image is COLOR), and all mip levels and
	 * arrays (which are both in this case single).
	 */
	VkImageMemoryBarrier image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = to_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = essentials->images[*image_index],
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	/*
	 * The image barrier structure above defines the execution dependency of sets of actions A and B.  When
	 * applying the barrier, we also need to specify which pipeline stages these sets of actions are taken from.
	 *
	 * In our barrier, first we want to make sure all READs from the image (by the previous presentation) is done
	 * before the barrier.  These reads are not part of our rendering.  In fact, they are really done before the
	 * graphics pipeline even begins.  So the pipeline stage we specify for `src` would be the top of the pipeline,
	 * which means before the pipeline begins.  Second, we want to make sure all writes to the image (for
	 * rendering) is done after the barrier.  The writes to the image are likely to happen at later stages of the
	 * graphics pipeline, so we can specify those stages as `dst` stages of the barrier.  We have already specified
	 * that the barrier works on WRITEs, so we can also be a bit lazy and say that the `dst` stage is all graphics
	 * pipeline stages.
	 *
	 * Let's rephrase the above to make sure it's clear.  The vkCmdPipelineBarrier takes a src and dst stage mask.
	 * The arguments are called srcStageMask and dstStageMask.  They can contain more than one pipeline stage.
	 * Take the combinations (srcAccessMask, srcStageMask) and (dstAccessMask, dstStageMask).  Say we make a
	 * barrier from (A, Sa) to (B, Sb) as src and dst parts of the barrier respectively.  The barrier then means
	 * that all actions A in stages Sa are done before all actions B in stages Sb.  So, if Sb is all graphics
	 * stages, it means that all actions A in stages Sa are done before all actions B anywhere.  If Sa is top of
	 * the pipeline, it means that all actions A before the pipeline are done before all actions B anywhere.
	 *
	 * All READs before the pipeline must be done before all WRITEs anywhere.
	 */
	vkCmdPipelineBarrier(essentials->cmd_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */

	return 0;
}

int tut7_render_finish(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index)
{
	VkResult res;

	/* The second memory barrier is similar to the first (in tut7_render_start), but inverted */
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

	/*
	 * For the second barrier, we want the opposite of the first barrier.  We want to make sure image reads by the
	 * presentation engine are done after the image is written to during rendering.  Just as top of the pipeline
	 * means before the pipeline begins, bottom of the pipeline means after it ends.
	 *
	 * With the explanation on the previous barrier, this should already makes sense to you:
	 *
	 * All WRITEs anywhere must be done before all READs after the pipeline.
	 */
	vkCmdPipelineBarrier(essentials->cmd_buffer,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */

	vkEndCommandBuffer(essentials->cmd_buffer);

	/*
	 * Having built the command buffer, we are ready to submit it to a queue for presentation.  We wanted our
	 * submission to wait for the image acquisition semaphore and subsequently signal the presentation semaphore,
	 * so we'll simply specify exactly that.  The semaphore wait can be done at different stages of the pipeline,
	 * so that the pipeline could go ahead with its calculations before the stage where it really needs to wait
	 * for the semaphore.  Since we don't have a pipeline yet, we'll ask it to wait at the top of the pipeline.
	 *
	 * Side note: we didn't need to submit the command buffer to the same queue we use for presentation, or even a
	 * queue in the same family, but we do that now for simplicity.
	 */
	VkPipelineStageFlags wait_sem_stages[1] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &essentials->sem_post_acquire,
		.pWaitDstStageMask = wait_sem_stages,
		.commandBufferCount = 1,
		.pCommandBuffers = &essentials->cmd_buffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &essentials->sem_pre_submit,
	};
	vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);

	/* Use `vkQueuePresentKHR` to give the image back for presentation */
	VkPresentInfoKHR present_info = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &essentials->sem_pre_submit,
		.swapchainCount = 1,
		.pSwapchains = &swapchain->swapchain,
		.pImageIndices = &image_index,
	};
	res = vkQueuePresentKHR(essentials->present_queue, &present_info);
	if (res < 0)
	{
		printf("Failed to queue image for presentation\n");
		return -1;
	}

	return 0;
}
