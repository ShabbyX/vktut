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
#include <unistd.h>
#include "tut7.h"

#define MAX_DEVICES 2

static int process_events()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			return -1;
		case SDL_WINDOWEVENT:
			/* rage quit like before */
			if (event.window.type == SDL_WINDOWEVENT_RESIZED)
			{
				printf("Warning: window resizing is currently not supported\n");
				return -1;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

static void render_loop(uint32_t dev_count, struct tut1_physical_device *phy_devs, struct tut2_device *devs, struct tut6_swapchain *swapchains)
{
	VkResult res;
	uint8_t color = 0;

	/* Like in Tutorial 6, take the list of swapchain images for future */
	uint32_t image_count[dev_count];
	uint32_t max_image_count = 1;
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		uint32_t count = 0;
		res = vkGetSwapchainImagesKHR(devs[i].device, swapchains[i].swapchain, &count, NULL);
		if (res < 0)
		{
			printf("Failed to count the number of images in swapchain of device %u: %s\n", i, tut1_VkResult_string(res));
			return;
		}

		image_count[i] = count;
		if (count > max_image_count)
			max_image_count = count;
	}

	VkImage images[dev_count][max_image_count];
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = vkGetSwapchainImagesKHR(devs[i].device, swapchains[i].swapchain, &image_count[i], images[i]);
		if (res < 0)
		{
			printf("Failed to get the images in swapchain of device %u: %s\n", i, tut1_VkResult_string(res));
			return;
		}
	}

	/*
	 * Take the first queue out of each presentable queue family (and command buffer on it) to use for presentation
	 * (for now)
	 */
	VkQueue present_queue[dev_count];
	VkCommandBuffer cmd_buffer[dev_count];

	for (uint32_t i = 0; i < dev_count; ++i)
	{
		uint32_t *presentable_queues = NULL;
		uint32_t presentable_queue_count = 0;

		if (tut7_get_presentable_queues(&phy_devs[i], &devs[i], swapchains[i].surface,
					&presentable_queues, &presentable_queue_count) || presentable_queue_count == 0)
		{
			printf("No presentable queue families!  What kind of graphics card is this!\n");
			return;
		}

		present_queue[i] = devs[i].command_pools[presentable_queues[i]].queues[0];
		cmd_buffer[i] = devs[i].command_pools[presentable_queues[i]].buffers[0];
		free(presentable_queues);
	}

	/* Create semaphores for synchronization (details below) */
	VkSemaphore sem_post_acquire[dev_count];
	VkSemaphore sem_pre_submit[dev_count];

	for (uint32_t i = 0; i < dev_count; ++i)
	{
		VkSemaphoreCreateInfo sem_info = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};

		res = vkCreateSemaphore(devs[i].device, &sem_info, NULL, &sem_post_acquire[i]);
		if (res)
		{
			printf("Failed to create post-acquire semaphore: %s\n", tut1_VkResult_string(res));
			return;
		}

		res = vkCreateSemaphore(devs[i].device, &sem_info, NULL, &sem_pre_submit[i]);
		if (res)
		{
			printf("Failed to create pre-submit semaphore: %s\n", tut1_VkResult_string(res));
			return;
		}
	}

	/* Process events from SDL and render.  If process_events returns non-zero, it signals application exit. */
	while (process_events() == 0)
	{
		/*
		 * We are not yet ready to actually render something.  For that, we would need descriptor sets and
		 * pipelines, but we'll get to that soon.  In tut7.c, we have a repository of functions to create
		 * resources for the eventual rendering.  Here, we'll ignore all that and do what we ignored in
		 * Tutorial 6, and that is properly transitioning the swapchain images between "present src" and
		 * something we can render to.  With a graphics pipeline, we would want to transition to
		 * "color attachment optimal".  Since we don't have one, we are going to "clear" the screen which
		 * doesn't need a graphics pipeline.  In that case, the layout of the image should be GENERAL.
		 */

		for (uint32_t i = 0; i < dev_count; ++i)
		{
			/*
			 * To render to an image and present it on the screen, the following sequence of operations
			 * need to be done:
			 *
			 * - acquire from swapchain
			 * - transition to color attachment optimal
			 * - render
			 * - transition to present src
			 * - present the image
			 *
			 * One way to implement this would be to call the corresponding functions one by one, wait and
			 * make sure the image passes through each section, and repeat.  The problem with this way is
			 * that there is wasted time between each function call.  Not that function call itself takes
			 * measurable time, but the setup and finish times of each call, especially because we are
			 * interacting with the GPU.
			 *
			 * Another way is to provide the work early, and synchronize acquire and present with
			 * semaphores.  This could be done in different ways depending on how everything is managed,
			 * but here is a simple example:
			 *
			 * - create a command buffer with 1) first transition, 2) render, 3) third transition
			 * - submit the command buffer with a semaphore waiting in the beginning and a semaphore
			 *   signalling the end
			 * - acquire from swapchain, signalling the first semaphore
			 * - present to swapchain, waiting on the second semaphore
			 *
			 * This way, the process of sending the command buffer to the queue is done in parallel to
			 * waiting for a swapchain image to become available.  More importantly, when we actually do
			 * render, all stages of the pipeline except fragment shader can actually execute without
			 * having to first wait for a swapchain image to be acquired.  Depending on the situation, this
			 * could significantly cut down on latencies.
			 *
			 * We could be using this scheme here.  While we're rebuilding the command buffer here every
			 * time, one could (and most of the times should) also pre-build command buffers and just
			 * replay them here (and likely with different parameters).  We'll do that in a future Tutorial.
			 *
			 * There is only one complication here though.  The command buffer that does the transitioning
			 * needs to know which image it is transitioning, which is known only after acquiring it from
			 * the swapchain!  As of yet, I don't know how to overcome this issue, but the above method is
			 * such an obvious gain that I'm either going to find out how to do it, or ask Khronos to make
			 * it possible!  So while that happens, we can just put the acquire part of the above algorithm
			 * first.
			 */

			/* Use `vkAcquireNextImageKHR` to get an image to render to */
			uint32_t image_index;

			res = vkAcquireNextImageKHR(devs[i].device, swapchains[i].swapchain, 1000000000, sem_post_acquire[i], NULL, &image_index);
			if (res == VK_TIMEOUT)
			{
				printf("A whole second and no image.  I give up.\n");
				return;
			}
			else if (res == VK_SUBOPTIMAL_KHR)
				printf("Did you change the window size?  I didn't recreate the swapchains,\n"
					"so the presentation is now suboptimal.\n");
			else if (res < 0)
			{
				printf("Couldn't acquire image: %s\n", tut1_VkResult_string(res));
				return;
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
			vkResetCommandBuffer(cmd_buffer[i], 0);
			VkCommandBufferBeginInfo begin_info = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
			};
			res = vkBeginCommandBuffer(cmd_buffer[i], &begin_info);
			if (res)
			{
				printf("Couldn't even begin recording a command buffer: %s\n", tut1_VkResult_string(res));
				return;
			}

			/*
			 * To transition an image to a new layout, an image barrier is used.  Before we see how that is
			 * done, let's see what it even means.
			 *
			 * In Vulkan, there are barriers on different kinds of resources (images, buffers and memory)
			 * and other means to specify execution dependency.  In each case, you want to make sure some
			 * actions A are all executed before some actions B.  In the specific case of barriers, A could
			 * be actions that do something to the resource and B could be actions that need the result of
			 * those actions.
			 *
			 * In our specific case, we want to change the layout of a swapchain image.  For the transition
			 * from present src, we want to make sure that all writes to the image are done after the
			 * transition is done.  For the transition to present src, we want to make sure that all writes
			 * to the image are done before the transition is done.  Note: if we had a graphics pipeline,
			 * we would be talking about "color attachment writes" instead of just "writes".  Keep that in
			 * mind.
			 *
			 * Using a VkImageMemoryBarrier, we are not only specifying how the image layout should change
			 * (if changed at all), but also defining the actions A and B where an execution dependency
			 * would be created.  In the first barrier (transition from present src), all reads of the
			 * image (for previous presentation) must happen before the barrier (A is the set of READ
			 * operations), and all writes must be done after the barrier (B is the set of WRITE
			 * operations).  The situation is reversed with the second barrier (transition to present src).
			 *
			 * In Vulkan, actions A are referred to as `src` and actions B are referred to as `dst`.
			 *
			 * Using an image barrier, it's also possible to transfer one image from a queue family to
			 * another, in which case A is the set of actions accessing the image in the first queue family
			 * and B is the set of actions accessing the image in the second queue family.  We are not
			 * moving between queue families, so we'll specify this intention as well.
			 *
			 * In our layout transition, we are transitioning from present src to general and back.
			 * However, the first time the transition happens, the swapchain image layout is actually
			 * UNDEFINED.  Either way, since we are not interested in what was previously in the image when
			 * we are just about to render into it, we can set the `oldLayout` (the layout transitioning
			 * from) to UNDEFINED.  This makes the transition more efficient because Vulkan knows it can
			 * just throw away the contents of the image.  Note: if we had a graphics pipeline, we would be
			 * transition to the "color attachment optimal" layout instead of "general".
			 *
			 * Finally, we need to specify which part of the image (subresource) is being transitioned.  We
			 * want to transition COLOR parts of the image (which in this case, all of the image is COLOR),
			 * and all mip levels and arrays (which are both in this case single).
			 */
			VkImageMemoryBarrier image_barrier = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_GENERAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = images[i][image_index],
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			/*
			 * The image barrier structure above defines the execution dependency of sets of actions A and
			 * B.  When applying the barrier, we also need to specify which pipeline stages these sets of
			 * actions are taken from.
			 *
			 * In our barrier, first we want to make sure all READs from the image (by the previous
			 * presentation) is done before the barrier.  These reads are not part of our rendering.  In
			 * fact, they are really done before the graphics pipeline even begins.  So the pipeline stage
			 * we specify for `src` would be the top of the pipeline, which means before the pipeline
			 * begins.  Second, we want to make sure all writes to the image (for rendering) is done after
			 * the barrier.  The writes to the image are likely to happen at later stages of the graphics
			 * pipeline, so we can specify those stages as `dst` stages of the barrier.  We have already
			 * specified that the barrier works on WRITEs, so we can also be a bit lazy and say that the
			 * `dst` stage is all graphics pipeline stages.
			 *
			 * Let's rephrase the above to make sure it's clear.  The vkCmdPipelineBarrier takes a src and
			 * dst stage mask.  The arguments are called srcStageMask and dstStageMask.  They can contain
			 * more than one pipeline stage.  Take the combinations (srcAccessMask, srcStageMask) and
			 * (dstAccessMask, dstStageMask).  Say we make a barrier from (A, Sa) to (B, Sb) as src and dst
			 * parts of the barrier respectively.  The barrier then means that all actions A in stages Sa
			 * are done before all actions B in stages Sb.  So, if Sb is all graphics stages, it means that
			 * all actions A in stages Sa are done before all actions B anywhere.  If Sa is top of the
			 * pipeline, it means that all actions A before the pipeline are done before all actions B
			 * anywhere.
			 *
			 * All READs before the pipeline must be done before all WRITEs anywhere.
			 */
			vkCmdPipelineBarrier(cmd_buffer[i],
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
					0,			/* no flags */
					0, NULL,		/* no memory barriers */
					0, NULL,		/* no buffer barriers */
					1, &image_barrier);	/* our image transition */

			/*
			 * We did all this, just to clear the image.  Like I said, it's possible to clear an image
			 * outside a pipeline.  It is also possible to clear it inside a pipeline, so fear not!  When
			 * we have a graphics pipeline, we can transition the image directly to "color attachment
			 * optimal" and clear it, and we don't have to first transition to "general" and then
			 * transition again to "color attachment optimal".
			 *
			 * Clearing the image outside the pipeline is quite straightforward, and in fact has no notion
			 * of the image being used for presentation later.  It's just clearing a general image.
			 *
			 * The vkCmdClearColorImage takes the command buffer, the image, the layout the image is in
			 * (which is "general", we just transitioned it), the color to clear the image with, and a set
			 * of "subresources" to clear.  We are going to clear everything, and we have just a single mip
			 * level and a single array layer, so the subresource range to be cleared is similar to the
			 * `subresourceRange` in image barrier.
			 *
			 * The clear color needs to be specified based on the format of the image.  The
			 * `VkClearColorValue` is a union which accepts RGBA values in float, uint32_t or int32_t, and
			 * we should choose the appropriate field based on swapchains[i].surface_format.format.  If we
			 * weren't so lazy, we could write a simple lookup table that tells us which field to use for
			 * each format, but luckily we are lazy, so let's assume `float` is good for now and hope it's
			 * portable enough.
			 *
			 * For fun, let's change the background color on each frame!
			 */
			VkImageSubresourceRange clear_subresource_range = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			};
			VkClearColorValue clear_color = {
				.float32 = {color, (color + 64) % 256 / 255.0f, (color + 128) % 256 / 255.0f, 1},
			};
			++color;
			vkCmdClearColorImage(cmd_buffer[i], images[i][image_index], VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &clear_subresource_range);

			/* Invert the memory barrier for the second transition */
			image_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			image_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			image_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			image_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			/*
			 * For the second barrier, we want the opposite of the first barrier.  We want to make sure
			 * image reads by the presentation engine are done after the image is written to during
			 * rendering.  Just as top of the pipeline means before the pipeline begins, bottom of the
			 * pipeline means after it ends.
			 *
			 * With the explanation on the previous barrier, this should already makes sense to you:
			 *
			 * All WRITEs anywhere must be done before all READs after the pipeline.
			 */
			vkCmdPipelineBarrier(cmd_buffer[i],
					VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					0,			/* no flags */
					0, NULL,		/* no memory barriers */
					0, NULL,		/* no buffer barriers */
					1, &image_barrier);	/* our image transition */

			vkEndCommandBuffer(cmd_buffer[i]);

			/*
			 * Having built the command buffer, we are ready to submit it to a queue for presentation.
			 * We wanted our submission to wait for the image acquisition semaphore and subsequently signal
			 * the presentation semaphore, so we'll simply specify exactly that.  The semaphore wait can be
			 * done at different stages of the pipeline, so that the pipeline could go ahead with its
			 * calculations before the stage where it really needs to wait for the semaphore.  Since we
			 * don't have a pipeline yet, we'll ask it to wait at the top of the pipeline.
			 *
			 * Unlike in Tutorial 4, we don't need a fence to signal the end of execution anymore, because
			 * the semaphores take care of all the synchronization we needed.
			 *
			 * Side note: we didn't need to submit the command buffer to the same queue we use for
			 * presentation, or even a queue in the same family, but we do that now for simplicity.
			 */
			VkPipelineStageFlags wait_sem_stages[1] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
			VkSubmitInfo submit_info = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &sem_post_acquire[i],
				.pWaitDstStageMask = wait_sem_stages,
				.commandBufferCount = 1,
				.pCommandBuffers = &cmd_buffer[i],
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &sem_pre_submit[i],
			};
			vkQueueSubmit(present_queue[i], 1, &submit_info, NULL);

			/* Use `vkQueuePresentKHR` to give the image back for presentation */
			VkPresentInfoKHR present_info = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &sem_pre_submit[i],
				.swapchainCount = 1,
				.pSwapchains = &swapchains[i].swapchain,
				.pImageIndices = &image_index,
			};
			res = vkQueuePresentKHR(present_queue[i], &present_info);
			if (res < 0)
			{
				printf("Failed to queue image for presentation on device %u\n", i);
				return;
			}
		}

		/* Make sure we don't end up busing looping in the GPU */
		usleep(10000);
	}

	for (uint32_t i = 0; i < dev_count; ++i)
	{
		vkDestroySemaphore(devs[i].device, sem_post_acquire[i], NULL);
		vkDestroySemaphore(devs[i].device, sem_pre_submit[i], NULL);
	}
}

int main(int argc, char **argv)
{
	VkResult res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_devs[MAX_DEVICES];
	struct tut2_device devs[MAX_DEVICES];
	struct tut6_swapchain swapchains[MAX_DEVICES] = {0};
	SDL_Window *windows[MAX_DEVICES] = {NULL};
	uint32_t dev_count = MAX_DEVICES;

	/* Fire up Vulkan */
	res = tut6_init(&vk);
	if (res)
	{
		printf("Could not initialize Vulkan: %s\n", tut1_VkResult_string(res));
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (res < 0)
	{
		printf("Could not enumerate devices: %s\n", tut1_VkResult_string(res));
		goto exit_bad_enumerate;
	}

	/* Get logical devices and enable WSI extensions */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut6_setup(&phy_devs[i], &devs[i], VK_QUEUE_COMPUTE_BIT);
		if (res < 0)
		{
			printf("Could not setup logical device %u, command pools and queues: %s\n", i, tut1_VkResult_string(res));
			goto exit_bad_setup;
		}
	}

	/* Set up SDL */
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		goto exit_bad_sdl;
	}

	for (uint32_t i = 0; i < dev_count; ++i)
	{
		char title[50];
		snprintf(title, sizeof title, "Vk on device %u\n", i);
		windows[i] = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, 0);
		if (windows[i] == NULL)
		{
			printf("Could not create window #%u: %s\n", i + 1, SDL_GetError());
			goto exit_bad_window;
		}
	}

	/* Get the surface and swapchain */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		/* Let's still not bother with threads and use just 1 (the current thread) */
		res = tut6_get_swapchain(vk, &phy_devs[i], &devs[i], &swapchains[i], windows[i], 1);
		if (res)
		{
			printf("Could not create surface and swapchain for device %u: %s\n", i, tut1_VkResult_string(res));
			goto exit_bad_swapchain;
		}
	}

	/* Render loop similar to Tutorial 6 */
	render_loop(dev_count, phy_devs, devs, swapchains);

	retval = 0;

	/* Cleanup after yourself */

exit_bad_swapchain:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut6_free_swapchain(vk, &devs[i], &swapchains[i]);

exit_bad_window:
	for (uint32_t i = 0; i < dev_count; ++i)
		if (windows[i])
			SDL_DestroyWindow(windows[i]);
exit_bad_sdl:
	SDL_Quit();

exit_bad_setup:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut2_cleanup(&devs[i]);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
