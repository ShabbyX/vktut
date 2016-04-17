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
#include <string.h>
#include <SDL2/SDL_syswm.h>
#include <X11/Xlib-xcb.h>
#include "tut6.h"

VkResult tut6_init_ext(VkInstance *vk, const char *ext_names[], uint32_t ext_count)
{
	/* This function once again, but with a possibility to enable extensions */
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan Tutorial",
		.applicationVersion = 0x010000,
		.pEngineName = "Vulkan Tutorial",
		.engineVersion = 0x010000,
		.apiVersion = VK_API_VERSION_1_0,
	};
	VkInstanceCreateInfo info;

	info = (VkInstanceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledExtensionCount = ext_count,
		.ppEnabledExtensionNames = ext_names,
	};

	return vkCreateInstance(&info, NULL, vk);
}

VkResult tut6_get_dev_ext(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count,
		const char *ext_names[], uint32_t ext_count)
{
	/* Here is to hoping we don't have to redo this function again ;) */
	*dev = (struct tut2_device){0};

	/* We have already seen how to create a logical device and request queues in Tutorial 2 and again in 5 */
	uint32_t max_queue_count = *queue_info_count;
	*queue_info_count = 0;

	uint32_t max_family_queues = 0;
	for (uint32_t i = 0; i < phy_dev->queue_family_count; ++i)
		if (max_family_queues < phy_dev->queue_families[i].queueCount)
			max_family_queues = phy_dev->queue_families[i].queueCount;
	float queue_priorities[max_family_queues];
	memset(queue_priorities, 0, sizeof queue_priorities);

	for (uint32_t i = 0; i < phy_dev->queue_family_count && i < max_queue_count; ++i)
	{
		/* Check if the queue has the desired capabilities.  If so, add it to the list of desired queues */
		if ((phy_dev->queue_families[i].queueFlags & qflags) != qflags)
			continue;

		queue_info[(*queue_info_count)++] = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = i,
			.queueCount = phy_dev->queue_families[i].queueCount,
			.pQueuePriorities = queue_priorities,
		};
	}

	/* If there are no compatible queues, there is little one can do here */
	if (*queue_info_count == 0)
		return VK_ERROR_FEATURE_NOT_PRESENT;

	VkDeviceCreateInfo dev_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = *queue_info_count,
		.pQueueCreateInfos = queue_info,
		.enabledExtensionCount = ext_count,
		.ppEnabledExtensionNames = ext_names,
		.pEnabledFeatures = &phy_dev->features,
	};

	return vkCreateDevice(phy_dev->physical_device, &dev_info, NULL, &dev->device);
}

VkResult tut6_get_swapchain(VkInstance vk, struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, SDL_Window *window, uint32_t thread_count)
{
	/*
	 * Now let's get to the interesting part.  We have our extensions enabled, so first, we need to get a surface.
	 * For that, we need to have a connection to the display manager.  This is platform-specific, and we can get
	 * the required information from SDL.  For now, we are going to use XCB, but someone might want to add support
	 * for others such as wayland etc.  The platform-specific is very short though, SDL is taking care of almost
	 * everything.
	 */
	SDL_SysWMinfo wm;
	VkResult retval;

	*swapchain = (struct tut6_swapchain){0};

	SDL_VERSION(&wm.version);
	if (!SDL_GetWindowWMInfo(window, &wm))
	{
		printf("Failed to get WM info from SDL: %s\n", SDL_GetError());
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	if (wm.subsystem != SDL_SYSWM_X11)
	{
		printf("Window manager not yet supported by this tutorial\n");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	/*
	 * To create a surface, we should provide a surface info that is dependent on the window system.  The
	 * information given is the usual structure type, and flags of other *Info structs, and a few platform-specific
	 * parameters, often regarding the connection to the window system and the window the surface is being placed
	 * on.
	 *
	 * Flags are currently unused on any supported platform.
	 *
	 * SDL is able to give out libX Window and Display pointers, but not xcb pointers directly.  Fortunately, xcb
	 * and X11 play nice together and there is a way to convert the values.  An X11 Display is converted to an
	 * xcb_connection_t using XGetXCBConnection() found in Xlib-xcb.h.  An X11 Window can just be used as an
	 * xcb_window_t (they are both just integers).
	 */
	VkXcbSurfaceCreateInfoKHR surface_info = {
		.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
		.connection = XGetXCBConnection(wm.info.x11.display),
		.window = wm.info.x11.window,
	};

	/*
	 * Creating the surface is like other vkCreate* functions.  This function is platform-specific, and takes the
	 * Vulkan instance, the create info, allocation functions (which we still don't use) and returns the surface in
	 * its last argument.
	 *
	 * This is it.  This function (and its create info struct) are all the platform-specific code you would need
	 * to get Vulkan to render something on the screen.
	 */
	retval = vkCreateXcbSurfaceKHR(vk, &surface_info, NULL, &swapchain->surface);
	if (retval)
		goto exit_failed;

	/*
	 * We need to query information about the surface we just created.  This information includes how many buffers
	 * we could allocate for our multi-buffer swapchain, what's the size of the window (surface to be more
	 * precise), whether the surface contents is rotated (likely a feature relevant to smartphones and tablets),
	 * the behavior of the surface when the output has an alpha channel, etc.  For now, we are particularly
	 * interested in the surface size, because we need that to create our swapchain buffers.
	 *
	 * There is an important point here.  Since creating buffers for the swapchain requires knowing the surface
	 * size, then resizing the window requires recreating the swapchain!  Since the buffers of the swapchain may be
	 * given to rendering threads for work, they will not be freed when you replace the swapchain with a new one.
	 * Once you destroy the swapchain, all buffers would be destroyed, so they shouldn't be owned by the
	 * application.  You would have to take care of doing that.  For now, we are going to ignore window resizes,
	 * but we may revisit this in a future tutorial.
	 */
	retval = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_dev->physical_device, swapchain->surface, &swapchain->surface_caps);
	if (retval)
		goto exit_failed;

	/*
	 * The buffers we render to are created by the swapchain.  Vulkan is, rightly, quite strict about ownership of
	 * these buffers.  Initially, the swapchain owns all these buffers.  The application requests a buffer to
	 * render into (vkAcquireNextImageKHR), and then queue it for presentation (vkQueuePresentKHR).  If the number
	 * of buffers is low, specifically compared to the number of rendering threads, then the threads would have to
	 * be blocked (by vkAcquireNextImageKHR) until a buffer is available to render to.  One buffer is always owned
	 * by the swapchain at any time (the one currently being presented on the surface), and depending on the
	 * implemenation one or more buffers may be also need to be owned by the swapchain.
	 *
	 * The surface capabilities queried above gives us minImageCount which tells how many buffers the swapchain
	 * would at least need.  maxImageCount would give us the upper limit (or 0 if infinite).  If there are N
	 * threads, you would likely want to get minImageCount + N - 1 buffers (if not limited by upper limit), so that
	 * none of your threads would ever have to wait to own a buffer to render into.  The -1 in the above formula
	 * comes from the fact that minImageCount already takes into consideration the fact that there is at least one
	 * rendering thread.
	 */
	uint32_t image_count = swapchain->surface_caps.minImageCount + thread_count - 1;
	if (swapchain->surface_caps.maxImageCount < image_count && swapchain->surface_caps.maxImageCount != 0)
		image_count = swapchain->surface_caps.maxImageCount;

	/*
	 * The surface can present the data in some format, for example 32-bit BGRA.  It also works in a color-space,
	 * such as sRGB.  When creating a swapchain, we should indicate which of the supported color formats and
	 * color-space we want to use.
	 *
	 * Here, we are going to go ahead and just take the first supported format.  If only one format is returned and
	 * it's "undefined", then we will go with 24-bit RGB.  This is not (yet) specified directly in the Vulkan
	 * standard, but is assumed from the following example in the specifications:
	 *
	 *     // If the format list includes just one entry of VK_FORMAT_UNDEFINED,
	 *     // the surface has no preferred format. Otherwise, at least one
	 *     // supported format will be returned (assuming that the
	 *     // vkGetPhysicalDeviceSurfaceSupportKHR function, in the
	 *     // VK_KHR_surface extension returned support for the surface).
	 *     if ((formatCount == 1) && (pSurfFormats[0].format == VK_FORMAT_UNDEFINED))
	 *         swapchainFormat = VK_FORMAT_R8G8B8_UNORM;
	 *     else
	 *     {
	 *         assert(formatCount >= 1);
	 *         swapchainFormat = pSurfFormats[0].format;
	 *     }
	 */
	uint32_t surface_format_count = 1;
	retval = vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev->physical_device, swapchain->surface, &surface_format_count, &swapchain->surface_format);
	if (retval < 0)
		goto exit_failed;

	if (surface_format_count == 1 && swapchain->surface_format.format == VK_FORMAT_UNDEFINED)
		swapchain->surface_format.format = VK_FORMAT_R8G8B8_UNORM;

	/*
	 * When a buffer (i.e., image) is rendered into, it needs to be sent to the presentation engine for rendering.
	 * The presentation engine has a queue of submitted images to draw and it can do so in a couple of modes.
	 *
	 * - Immediate: No queue is used and the submission results in an immediate draw.  There is no wait for VBLANK
	 *   and it can result in tearing.  This is the lowest latency mode.
	 *
	 * - Mailbox: There is wait for VBLANK and no tearing is possible.  A queue of size 1 is used and the queued
	 *   image is rendered after a VBLANK.  If the queue is full when a new submission occurs, the image is
	 *   replaced.  This is the lowest latency non-tearing mode.
	 *
	 * - FIFO: There is wait for VBLANK and no tearing is possible.  A queue is used that can hold as many
	 *   submissions as the application can (so no overflow is possible) and the images are presented one by one.
	 *   This mode guaranteed to be supported.
	 *
	 * - Relaxed FIFO: This is similar to FIFO, with the exception that if there is no image to replace the current
	 *   one on the next VBLANK, then the next submission would be immediately drawn, which can result in tearing.
	 *   This mode has lower latency than FIFO at the risk of tearing.
	 *
	 * We would go with Mailbox if available and fall back to FIFO if not, to make sure there is no tearing either
	 * way.
	 */
	swapchain->present_modes_count = TUT6_MAX_PRESENT_MODES;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	retval = vkGetPhysicalDeviceSurfacePresentModesKHR(phy_dev->physical_device, swapchain->surface,
			&swapchain->present_modes_count, swapchain->present_modes);
	if (retval >= 0)
	{
		for (uint32_t i = 0; i < swapchain->present_modes_count; ++i)
			if (swapchain->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
	}

	/*
	 * To create a swapchain, we need a VkSwapchainCreateInfoKHR struct as usual.  There is a lot of information
	 * that needs to be provided, and we have gathered them above already.  Some of the fields require further
	 * explanation though:
	 *
	 * imageExtent: This is the size of the swapchain buffers, which needs to be known for the swapchain to
	 * allocate them.  We already know this from the surface capabilities (currentExtent).  There is a small catch
	 * though.  `currentExtent` of the surface may be {0xFFFFFFFF, 0xFFFFFFFF} which is a special value meaning the
	 * image size is taken from the swapchain info struct.  In this case, we can request any size that respects the
	 * minImageExtent and maxImageExtent of the surface.  Let's go with minimum for no particular reason.
	 *
	 * imageArrayLayers: If the surface is multiview, this would be the number of views.  This is 1 for the usual
	 * surfaces and higher for stereoscopic applications.
	 *
	 * imageUsage: How the image is going to be used.  Since we want to show an image on the screen, we want
	 * VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, but there may be other usages possible, as long as the usage flags here
	 * is a subset of surface capability's `supportedUsageFlags`.
	 *
	 * Sharing mode: A few of the fields are dedicated to the case where the images can be shared between multiple
	 * queue families.  In that case, those queue families need to be identified, but we are not interested in
	 * sharing the images, so we'll go with the default.
	 *
	 * preTransform: A transformation to be done by the presentation engine on the final image before displaying
	 * it, mostly likely a feature useful for smartphones and tablets, such as rotating the image by 90 degrees and
	 * such.  We can just go with whatever transformation is currently applied to the surface, using the surface
	 * capability's currentTransform.
	 *
	 * compositeAlpha: In some window systems, if the surface has an alpha channel, it can get rendered with an
	 * alpha.  This field asks for the behavior in such a case, which is one of "ignore alpha, the surface is
	 * opaque", "the color is already multiplied by the alpha", "the color is not multiplied by the alpha" and
	 * "I'll setup alpha management with the window system myself".  We'll just go with the first option.
	 *
	 * clipped: If clipped is true, then the presentation engine can decide to not compute the parts of the surface
	 * that are not drawn on the screen, for example because they are blocked by other windows.  This is generally
	 * what one would want.  If the shaders working on the pixels have side-effects though, it may be necessary to
	 * set this to false to make sure all pixels are rendered, even if they cannot be shown.
	 *
	 * oldSwapchain: If this field is given, the old swapchain on this surface is returned.  The buffers owned by
	 * the presentation engine (and not the application) would be freed as soon as possible, but the ones owned by
	 * the application would be freed when the swapchain is destroyed.  The application can still send those
	 * buffers to the old swapchain for presentation (a moot point since the swapchain is not connected to the
	 * surface any more), but cannot get any new images from it.  This is useful for the application to quickly set
	 * up a new swapchain when the window is resized, and clean up the old one when it has time.  The "moot point"
	 * above helps the application set up the new swapchain and get some threads rendering on it without having to
	 * wait for the currently-rendering threads to finish.  For simplicity, we are not handling window resizes, and
	 * if it comes to that, we can just destroy the swapchain and make another one.  Nevertheless, this is
	 * something worth remembering.
	 */
	VkSwapchainCreateInfoKHR swapchain_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = swapchain->surface,
		.minImageCount = image_count,
		.imageFormat = swapchain->surface_format.format,
		.imageColorSpace = swapchain->surface_format.colorSpace,
		.imageExtent = swapchain->surface_caps.currentExtent.width == 0xFFFFFFFF?
			swapchain->surface_caps.minImageExtent:
			swapchain->surface_caps.currentExtent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = swapchain->surface_caps.currentTransform,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = present_mode,
		.clipped = true,
	};

	/* Finally, create the swapchain.  The usual pattern, with no memory allocation callbacks */
	retval = vkCreateSwapchainKHR(dev->device, &swapchain_info, NULL, &swapchain->swapchain);
	if (retval)
		goto exit_failed;

exit_failed:
	return retval;
}

void tut6_free_swapchain(VkInstance vk, struct tut2_device *dev, struct tut6_swapchain *swapchain)
{
	/*
	 * Before destroying the swapchain, make sure all threads that use its image have given the images back to be
	 * freed.
	 *
	 * As usual, no memory allocation callbacks.
	 */
	vkDestroySwapchainKHR(dev->device, swapchain->swapchain, NULL);
	vkDestroySurfaceKHR(vk, swapchain->surface, NULL);

	*swapchain = (struct tut6_swapchain){0};
}

void tut6_print_surface_capabilities(struct tut6_swapchain *swapchain)
{
	const char *transforms[] = {
		"IDENTITY",
		"ROTATE_90",
		"ROTATE_180",
		"ROTATE_270",
		"HORIZONTAL_MIRROR",
		"HORIZONTAL_MIRROR_ROTATE_90",
		"HORIZONTAL_MIRROR_ROTATE_180",
		"HORIZONTAL_MIRROR_ROTATE_270",
		"INHERIT",
	};
	const char *alphas[] = {
		"OPAQUE",
		"PRE_MULTIPLIED",
		"POST_MULTIPLIED",
		"INHERIT",
	};
	const char *image_usages[] = {
		"TRANSFER_SRC",
		"TRANSFER_DST",
		"SAMPLED",
		"STORAGE",
		"COLOR_ATTACHMENT",
		"DEPTH_STENCIL_ATTACHMENT",
		"TRANSIENT_ATTACHMENT",
		"INPUT_ATTACHMENT",
	};
	const char *present_modes[] = {
		"IMMEDIATE",
		"MAILBOX",
		"FIFO",
		"FIFO_RELAXED",
	};
	VkSurfaceCapabilitiesKHR *caps = &swapchain->surface_caps;

	printf( "Surface capabilities:\n"
		" - image count in range [%u, %u]\n"
		" - image extent between (%u, %u) and (%u, %u) (current: (%u, %u))\n"
		" - stereoscopic possible? %s\n"
		" - supported transforms:\n",
		caps->minImageCount,
		caps->maxImageCount,
		caps->minImageExtent.width,
		caps->minImageExtent.height,
		caps->maxImageExtent.width,
		caps->maxImageExtent.height,
		caps->currentExtent.width,
		caps->currentExtent.height,
		caps->maxImageArrayLayers > 1?"Yes":"No");

	for (size_t i = 0; i < sizeof transforms / sizeof *transforms; ++i)
		if ((caps->supportedTransforms & 1 << i))
			printf("    * %s%s\n", transforms[i], caps->currentTransform == 1 << i?" (current)":"");
	if (caps->supportedTransforms >= 1 << sizeof transforms / sizeof *transforms)
		printf("    * ...%s\n", caps->currentTransform >= 1 << sizeof transforms / sizeof *transforms?" (current)":"");

	printf(" - supported alpha composition:\n");
	for (size_t i = 0; i < sizeof alphas / sizeof *alphas; ++i)
		if ((caps->supportedCompositeAlpha & 1 << i))
			printf("    * %s\n", alphas[i]);
	if (caps->supportedCompositeAlpha >= 1 << sizeof alphas / sizeof *alphas)
		printf("    * ...\n");

	printf(" - supported image usages:\n");
	for (size_t i = 0; i < sizeof image_usages / sizeof *image_usages; ++i)
		if ((caps->supportedUsageFlags & 1 << i))
			printf("    * %s\n", image_usages[i]);
	if (caps->supportedUsageFlags >= 1 << sizeof image_usages / sizeof *image_usages)
		printf("    * ...\n");
	printf(" - supported present modes:\n");
	for (int i = 0; i < swapchain->present_modes_count; ++i)
		if (swapchain->present_modes[i] >= sizeof present_modes / sizeof *present_modes)
			printf("    * <UNKNOWN MODE(%u)>\n", swapchain->present_modes[i]);
		else
			printf("    * %s\n", present_modes[swapchain->present_modes[i]]);
}
