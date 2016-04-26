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

#ifndef TUT6_H
#define TUT6_H

/*
 * To create a window and interact with it, we are going to use good old SDL.  The core of Vulkan doesn't care about
 * displaying the images it creates.  This is good, because it means you can render all you want, in parallel, and in
 * different images without having to create windows for it.  (A nudge to the gimp's developers)
 *
 * How to put those images on the screen then?  This is where extensions come in.  Vulkan already comes with a couple
 * of extensions for WSI (Window System Integration).  Let's see exactly what must happen for the images to get
 * rendered to the screen:
 *
 * 1. The window is created.  This is platform-specific, but let's say SDL does that for us.
 * 2. A rendering surface is acquired.  This is through a Vulkan WSI extension.  There is one for libX, XCB, Mir,
 *    Wayland, Android etc.  On Linux, we are likely running X11, so we'd use XCB as the better API to talk with X11.
 * 3. A "swapchain" with multiple `VkImage`s is created.  The VkImages are what we render into, and they are presented
 *    one by one on the screen.  This covers the usual multi-buffering there is with OpenGL for example.  But it also
 *    gives us the ability to render multiple frames at a time and present them in order.
 * 4. Each VkImage is either owned by the swapchain or the application.  The application requests ownership of the
 *    VkImage, renders into it, and passes it back to the swapchain which in turn presents it on the surface, through a
 *    queue family that supports presentation.
 *
 * There are a lot of details regarding passing ownership between the swapchain and the application and we'll get to
 * them in time.
 *
 * To use the XCB extension, we need to have VK_USE_PLATFORM_XCB_KHR defined before including vulkan.h.
 */
#define VK_USE_PLATFORM_XCB_KHR 1

#include <SDL2/SDL.h>
#include "../tut1/tut1.h"
#include "../tut2/tut2.h"

#define TUT6_MAX_PRESENT_MODES 4

struct tut6_swapchain
{
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;

	VkSurfaceFormatKHR surface_format;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkPresentModeKHR present_modes[TUT6_MAX_PRESENT_MODES];
	uint32_t present_modes_count;
};

VkResult tut6_init_ext(VkInstance *vk, const char *ext_names[], uint32_t ext_count);
VkResult tut6_get_dev_ext(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count,
		const char *ext_names[], uint32_t ext_count);

/* This tutorial replaces tut1_init and tut2_get_dev with versions that enable WSI extensions */
static inline VkResult tut6_init(VkInstance *vk)
{
	/*
	 * In Tutorial 5, we enabled all layers and extensions there were, which is not really the way things should be
	 * done!  In this tutorial, we are going to redo the init function one more time, enabling only the extensions
	 * we are interested in.  We are not enabling layers, since they can be enabled from the command line anyway.
	 *
	 * The helper _ext functions do the actual job, so if in a future tutorial we decided to enable a different set
	 * of extensions, we don't have to rewrite the functions.
	 */
	const char *extension_names[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_XCB_SURFACE_EXTENSION_NAME,
	};
	return tut6_init_ext(vk, extension_names, sizeof extension_names / sizeof *extension_names);
}
static inline VkResult tut6_get_dev(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count)
{
	/*
	 * The surface and XCB extensions were "instance" extensions and are enabled in tut6_init().  For the device,
	 * we need to enable the swapchain extension.  If you run the code for Tutorial 5, you should be able to see
	 * VK_KHR_surface and VK_KHR_xcb_surface belonging to instance extensions and VK_KHR_swapchain to your device.
	 *
	 * As a matter of fact, if you don't see the above extensions, your driver likely needs an update.
	 */
	const char *extension_names[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	return tut6_get_dev_ext(phy_dev, dev, qflags, queue_info, queue_info_count, extension_names,
			sizeof extension_names / sizeof *extension_names);
}

static inline VkResult tut6_setup(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags)
{
	VkDeviceQueueCreateInfo queue_info[phy_dev->queue_family_count];
	uint32_t queue_info_count = phy_dev->queue_family_count;

	VkResult res = tut6_get_dev(phy_dev, dev, qflags, queue_info, &queue_info_count);
	if (res == 0)
		res = tut2_get_commands(phy_dev, dev, queue_info, queue_info_count);
	return res;
}

VkResult tut6_get_swapchain(VkInstance vk, struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, SDL_Window *window, uint32_t thread_count);
void tut6_free_swapchain(VkInstance vk, struct tut2_device *dev, struct tut6_swapchain *swapchain);

void tut6_print_surface_capabilities(struct tut6_swapchain *swapchain);

#endif
