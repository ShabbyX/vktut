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
#include <time.h>
#include "tut7.h"
#include "tut7_render.h"

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
	int res;
	struct tut7_render_essentials essentials[dev_count];

	/* Allocate render essentials.  See this function in tut7_render.c for explanations. */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut7_render_get_essentials(&essentials[i], &phy_devs[i], &devs[i], &swapchains[i]);
		if (res)
		{
			printf("-- failed for device %u\n", i);
			return;
		}
	}

	unsigned int frames = 0;
	time_t before = time(NULL);

	uint8_t color = 0;

	/* Process events from SDL and render.  If process_events returns non-zero, it signals application exit. */
	while (process_events() == 0)
	{
		/*
		 * A simple imprecise FPS calculator.  Try the --no-vsync option to this program to see the difference.
		 *
		 * On Linux, with Nvidia GTX 970, and Vulkan 1.0.8, --no-vsync got me about 12000 FPS.
		 */
		time_t now = time(NULL);
		if (now != before)
		{
			printf("%lds: %u frames\n", now - before, frames);
			frames = 0;
			before = now;
		}
		++frames;

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
			uint32_t image_index;

			/*
			 * To render to an image and present it on the screen, the following sequence of operations
			 * needs to be done:
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
			 * Vulkan is made for parallelism and efficiency, so naturally it's not stupid in this regard!
			 * There are different ways to do the above in parallel, and synchronize them.  One nice thing
			 * is that command buffers can call other secondary command buffers.  So, while a small part of
			 * the command buffer requires knowledge of which presentable image it is working with, the
			 * majority of it doesn't, so they could be pre-recorded or recorded in parallel by other
			 * threads.  Another nice thing is that many of the functions work asynchronously, such as
			 * submission to queue for rendering.  This allows the CPU to go ahead with executing the rest
			 * of the above algorithm, only wait for the GPU to finish rendering when it has to, and let
			 * synchronization mechanisms take care of handling the flow of execution in the back.
			 *
			 * One could imagine different ways of doing things, but here is a simple example:
			 *
			 * - acquire from swapchain, signalling semaphore A
			 * - wait on fence C (for previous frame to finish)
			 * - create a command buffer with 1) first transition, 2) render, 3) second transition
			 * - submit the command buffer with semaphore A waiting in the beginning and semaphore B
			 *   signalling the end, with fence C signalling the end as well
			 * - present to swapchain, waiting on the second semaphore
			 *
			 * The significance of the fence above is the following.  In Tutorial 6, we used `usleep` to
			 * avoid busy looping.  That was bad, because it put a hard limit and the frame rate.  The
			 * issue is not just busy looping though.  Since the submissions to queues happen
			 * asynchronously, we risk submitting work faster than the card can actually perform them, with
			 * the result being that frames we send now are rendered much later, after all our previous
			 * work is finished.  This delay can easily become unacceptable; imagine a player has hit the
			 * key to move forwards, you detect this and generate the next frame accordingly, but the
			 * player doesn't actually see her character move forward while several older frames are still
			 * being rendered.
			 *
			 * The location of the fence is chosen as such, to allow maximum overlap between GPU and CPU
			 * work.  In this case, while the GPU is still rendering, the CPU can wait for the swapchain
			 * image to be acquired.  The wait on the fence could not be delayed any further, because we
			 * can't re-record a command buffer that is being executed.  Interestingly, if we use two
			 * command buffers and alternate between them, we could also wait for the fence later!  Let's
			 * not go that far yet.
			 */

			/* See this function in tut7_render.c for explanations */
			res = tut7_render_start(&essentials[i], &devs[i], &swapchains[i], VK_IMAGE_LAYOUT_GENERAL, &image_index);
			if (res)
			{
				printf("-- failed for device %u\n", i);
				goto exit_fail;
			}

			/*
			 * We did everything just to clear the image.  Like I said, it's possible to clear an image
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
			vkCmdClearColorImage(essentials[i].cmd_buffer, essentials[i].images[image_index], VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &clear_subresource_range);

			/* See this function in tut7_render.c for explanations */
			res = tut7_render_finish(&essentials[i], &devs[i], &swapchains[i], VK_IMAGE_LAYOUT_GENERAL, image_index);
			if (res)
			{
				printf("-- failed for device %u\n", i);
				goto exit_fail;
			}
		}
	}

exit_fail:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut7_render_cleanup_essentials(&essentials[i], &devs[i]);
}

int main(int argc, char **argv)
{
	tut1_error res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_devs[MAX_DEVICES];
	struct tut2_device devs[MAX_DEVICES];
	struct tut6_swapchain swapchains[MAX_DEVICES] = {0};
	SDL_Window *windows[MAX_DEVICES] = {NULL};
	uint32_t dev_count = MAX_DEVICES;

	bool no_vsync = false;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[1], "--help") == 0)
		{
			printf("Usage: %s [--no-vsync]\n\n", argv[0]);
			return 0;
		}
		if (strcmp(argv[1], "--no-vsync") == 0)
			no_vsync = true;
	}

	/* Fire up Vulkan */
	res = tut6_init(&vk);
	if (!tut1_error_is_success(&res))
	{
		tut1_error_printf(&res, "Could not initialize Vulkan\n");
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not enumerate devices\n");
		goto exit_bad_enumerate;
	}

	/* Get logical devices and enable WSI extensions */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut6_setup(&phy_devs[i], &devs[i], VK_QUEUE_GRAPHICS_BIT);
		if (tut1_error_is_error(&res))
		{
			tut1_error_printf(&res, "Could not setup logical device %u, command pools and queues\n", i);
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
		res = tut6_get_swapchain(vk, &phy_devs[i], &devs[i], &swapchains[i], windows[i], 1, no_vsync);
		if (tut1_error_is_error(&res))
		{
			tut1_error_printf(&res, "Could not create surface and swapchain for device %u\n", i);
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
