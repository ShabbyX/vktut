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
#include "tut6.h"

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
			/*
			 * If the window is resized, the next time a buffer is requested from the presentation engine,
			 * it either fails or returns VK_SUBOPTIMAL_KHR.  In the former case, the swapchain must be
			 * recreated, and in the later case the presentation engine could somehow show the image
			 * anyway, but not with great performance.  Optimally, we would want to recreate the swapchain
			 * on window resize, but for now we just rage quit.
			 */
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

	/*
	 * At first, we need to get a reference to all the images of the swapchains, so that later when we acquire or
	 * queue them, we would be working with indices only.
	 */
	VkImage *images[dev_count];

	for (uint32_t i = 0; i < dev_count; ++i)
	{
		images[i] = tut6_get_swapchain_images(&devs[i], &swapchains[i], NULL);
		if (images[i] == NULL)
		{
			printf("-- failed for device %u\n", i);
			return;
		}
	}

	/*
	 * The images to be presented are submitted to queues, which we already created in Tutorial 2.  Not all queues
	 * may be able to present images, so we have to query this information for each queue family.  In Tutorial 2,
	 * we created a command pool for each queue family, so we can use the indices from 0 to command_pool_count-1 as
	 * queue family indices for querying this information.
	 */
	VkQueue present_queue[dev_count];
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		VkBool32 supports = false;
		present_queue[i] = NULL;
		for (uint32_t j = 0; j < devs[i].command_pool_count; ++j)
		{
			/*
			 * Asking whether a queue family is able to present images is straightforward.  You need to
			 * give the `vkGetPhysicalDeviceSurfaceSupportKHR` function the physical device, the surface
			 * you want to present to and the queue family index.  It returns the result in the last
			 * argument.
			 *
			 * Note that this information is queried from the physical device, which means you can already
			 * know which queue families can support presentation before creating a logical device.  If you
			 * recall from Tutorial 2, creating a logical device requires requesting queues from the
			 * device's queue families.  If you perform this operation beforehand and you are only
			 * interested in presenting images, you would be able to avoid requesting for queue families
			 * that don't support it.  In Tutorial 2, we just asked for all queue families there are, which
			 * is not the nicest thing.
			 */
			res = vkGetPhysicalDeviceSurfaceSupportKHR(phy_devs[i].physical_device, j, swapchains[i].surface, &supports);
			if (res)
			{
				printf("Failed to determine whether queue family index %u on device %u supports presentation: %s\n",
						j, i, tut1_VkResult_string(res));
				return;
			}

			/* Just present to the first queue of the first family we find, for now. */
			if (supports)
			{
				present_queue[i] = devs[i].command_pools[j].queues[0];
				break;
			}
		}

		if (present_queue[i] == NULL)
		{
			printf("Failed to find any family queue on device %u that supports presentation\n", i);
			return;
		}
	}

	/* Process events from SDL and render.  If process_events returns non-zero, it signals application exit. */
	while (process_events() == 0)
	{
		/*
		 * For now, we are going to use 1 thread (this one) to render, mostly because we are not actually
		 * rendering anything, but rather practicing moving images back and forth between the application
		 * and the presentation engine.
		 *
		 * At first, all buffers (images) are owned by the swapchain (presentation engine).  We ask for an
		 * image, render into it, and return it.  In a multi-threaded application, you would ask for images as
		 * soon as you can get them, render to them and queue them in parallel.  The function that "acquires"
		 * an image can signal a fence and/or a semaphore after acquiring an image.  This is very helpful with
		 * respect to parallelism, because the command buffer being executed can wait for the signal when it
		 * really needs the images and is able to do some of its work already before the image is available.
		 *
		 * On the other hand, the function that "queues" back an image on the swapchain, can wait on a
		 * number of semaphores before doing so.  This is also very helpful for parallelism, as it allows the
		 * command buffer being executed to signal immediately when it is done with the image before going on
		 * with finishing up the execution, or calculating more side effects.
		 *
		 * Since we are just using 1 thread, the task is a lot simpler.  We simply take an image, do something
		 * with it, and give it back.  To actually render something, we would need to have a graphics pipeline
		 * set up.  This would need more work, so we'll leave that to another tutorial.
		 */

		for (uint32_t i = 0; i < dev_count; ++i)
		{
			/*
			 * The `vkAcquireNextImageKHR` function takes the device and swapchain as other swapchain
			 * functions do.  It then takes a timeout value which tells how many nanoseconds to wait before
			 * an image is available.  This value can be used to make sure this function doesn't
			 * indefinitely block you, and so that you can take action in case something is wrong.  A value
			 * of 0 will return immediately with results, if any.  A value of UINT64_MAX will make the wait
			 * infinite (even if it didn't, that value is so large, it's infinite anyway).  Here, we will
			 * use a value of 1 second, and if we get a timeout, it means that we cannot render for a whole
			 * second.
			 *
			 * The next two arguments are references to a semaphore and a fence.  The function would signal
			 * the semaphore and the fence as soon as the image is acquired.  Since we are not yet
			 * rendering anything, this is not useful.
			 *
			 * The index of the image that is now available is returned through the last argument.
			 */
			uint32_t image_index;

			res = vkAcquireNextImageKHR(devs[i].device, swapchains[i].swapchain, 1000000000, NULL, NULL, &image_index);
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
			 * Here is where the rendering would take place, if we had not been so lazy to set a graphics
			 * pipeline and descriptor set up.
			 *
			 * Just a note though: initially, all the images in the swap chain have an undefined layout
			 * (VK_IMAGE_LAYOUT_UNDEFINED).  The command that was supposed to be executed here would
			 * transition the image into a valid layout.  We will get to what this means in the next
			 * tutorial.  More importantly, it must be transitioned to "present src" layout before
			 * presentation.  Now we are not actually doing this last operation, so if the queue operation
			 * fails, it may be rightly doing so.
			 */

			/*
			 * The `vkQueuePresentKHR` function takes the queue to present the image through and can queue
			 * multiple images from multiple swapchains at the same time.  A set of semaphores to wait on
			 * before doing so are taken which may not necessarily have a one-to-one correspondence with
			 * the swapchains.
			 *
			 * For each swapchain, the swapchain itself as well as the index of the image to present are
			 * given.  In this example, we could present all dev_count images at the same time outside this
			 * for loop, but I just went with presenting them one by one.
			 *
			 * Since there could be multiple submissions at once, an array of `VkResult`s can also be given
			 * to specify exactly which queue operations succeeded and which failed.  We are making only
			 * one submission, so the return value of the function is enough.
			 */
			VkPresentInfoKHR present_info = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
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

		/* Make sure we don't end up busy looping in the GPU */
		usleep(10000);
	}

	for (uint32_t i = 0; i < dev_count; ++i)
		free(images[i]);
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

	/*
	 * Once again, tut2_setup is replaced with tut6_setup, which enables WSI (window system integration) extensions
	 * so that we can actually show something on the screen.  Remember that Vulkan nicely separates its work from
	 * presenting images on the screen, so you can work with graphics without actually having a window.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut6_setup(&phy_devs[i], &devs[i], VK_QUEUE_GRAPHICS_BIT);
		if (res < 0)
		{
			printf("Could not setup logical device %u, command pools and queues: %s\n", i, tut1_VkResult_string(res));
			goto exit_bad_setup;
		}
	}

	/*
	 * We need to setup SDL now so that we can create a surface and a swapchain over it.  We will do this for each
	 * device, just for example, but in reality you probably just want to have one screen!  The way this tutorial
	 * is done, is like running two separate applications one based on each of your graphics cards.  In real
	 * applications one might want to use the other graphics cards for additional rendering, transfer it back to
	 * the main graphics card and do the final rendering using that.
	 */
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
			printf("Could not create %u%s window: %s\n", i + 1,
					i == 0?"st":i == 1?"nd":i == 2?"rd":"th", /* Assuming you have fewer than 20 graphics cards ;) */
					SDL_GetError());
			goto exit_bad_window;
		}
	}

	/* Get the surface and swapchain now that we have an actual window */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		/*
		 * The swapchain creates a set of buffers which in tut6_get_swapchain is calculated based on the number
		 * of rendering threads.  For now, let's not bother with threads and just use 1 thread (the current
		 * one).
		 */
		res = tut6_get_swapchain(vk, &phy_devs[i], &devs[i], &swapchains[i], windows[i], 1);
		if (res)
		{
			printf("Could not create surface and swapchain for device %u: %s\n", i, tut1_VkResult_string(res));
			goto exit_bad_swapchain;
		}
	}

	/* Let's print the surface capabilities too, just to see what we are up against */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		printf("On device %u:\n", i);
		tut6_print_surface_capabilities(&swapchains[i]);
		printf("\n");
	}

	/*
	 * We need to do some more work to render something on the screen, but we've already covered a lot in this
	 * tutorial.  Let's just go over the basics of exchanging buffers between the application and the presentation
	 * engine, by simply clearing the screen (with a different color each time).
	 */
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
