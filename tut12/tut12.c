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

#include <stdlib.h>
#include "tut12.h"

/*
 * Since the terminal and thus ncurses is limited in colors, take the highest bit of R, G and B components as the
 * background color and the second highest bit of R, G and B components as the foreground color.  Use the bold #
 * character as a way to mix the two colors!  If the terminal doesn't support that many colors, just use the very
 * basic set of colors (only using the highest bit of R, G and B components).
 */
static bool supports_64_colors = true;

static void setup_ncurses_colors(void)
{
	NCURSES_COLOR_T basic_colors[8] = {
				/* BGR */
		COLOR_BLACK,	/* 000 */
		COLOR_RED,	/* 001 */
		COLOR_GREEN,	/* 010 */
		COLOR_YELLOW,	/* 011 */
		COLOR_BLUE,	/* 100 */
		COLOR_MAGENTA,	/* 101 */
		COLOR_CYAN,	/* 110 */
		COLOR_WHITE,	/* 111 */
	};

	if (COLOR_PAIRS > 64)
		supports_64_colors = true;
	else
		supports_64_colors = false;

	if (supports_64_colors)
		for (unsigned int i = 0; i < 8; ++i)
			for (unsigned int j = 0; j < 8; ++j)
				init_pair((i << 3 | j) + 1, basic_colors[j], basic_colors[i]);
	else
		for (unsigned int i = 0; i < 8; ++i)
			init_pair(i + 1, basic_colors[i], basic_colors[i]);
}

static inline void set_color(uint8_t r, uint8_t g, uint8_t b)
{
	NCURSES_PAIRS_T hi = (b & 0x80) >> 5 | (g & 0x80) >> 6 | (r & 0x80) >> 7;
	if (supports_64_colors)
	{
		NCURSES_PAIRS_T lo = (b & 0x40) >> 4 | (g & 0x40) >> 5 | (r & 0x40) >> 6;
		attron(COLOR_PAIR((hi << 3 | lo) + 1) | A_BOLD);
	}
	else
		attron(COLOR_PAIR(hi + 1) | A_BOLD);
}

/*
 * Create Vulkan-like structures and functions for Ncurses, like the ones we used with Xcb.  This is made by me
 * (Shahbaz Youssefi) not Khronos, so let's also use SHY instead of KHR in the names.
 */
#define VK_STRUCTURE_TYPE_NCURSES_SURFACE_CREATE_INFO_SHY 1000050000
typedef VkFlags VkNcursesSurfaceCreateFlagsSHY;
typedef struct VkNcursesSurfaceCreateInfoSHY
{
	VkStructureType sType;
	const void *pNext;		/*
					 * For extensions.  Of course, we don't actually use this, but it makes our
					 * fake extension look official and cool.
					 */
	VkNcursesSurfaceCreateFlagsSHY flags;
	WINDOW *window;
} VkNcursesSurfaceCreateInfoSHY;

VkResult vkCreateNcursesSurfaceSHY(VkInstance instance, const VkNcursesSurfaceCreateInfoSHY *create_info,
		const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface);

/*
 * NOTE: some function overrides later on, such as vkCreateSwapchainKHR need access to the physical and logical
 * devices.  If we were really the driver, we wouldn't have needed a pointer to the devices, you know, since we (the
 * driver) already know what device we are driving!  But here, since we are faking a driver, we don't have that
 * information.  There is no way yet to get the VkPhysicalDevice out of a VkDevice or to get VkDevice out of
 * VkSwapchainKHR, so we are left with no choice but to hack our way and keep a reference to the devices beforehands.
 * This limits us to a single physical and logical device, which for the sake of this tutorial is not such a big deal.
 */
static struct tut1_physical_device *g_phy_dev = NULL;
static struct tut2_device *g_dev = NULL;
tut1_error tut12_get_swapchain(VkInstance vk, struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, WINDOW *window, uint32_t thread_count, bool allow_no_vsync)
{
	g_phy_dev = phy_dev;
	g_dev = dev;

	/*
	 * This function is very similar to tut6_get_swapchain, with the difference that it instantiates an ncurses
	 * surface (which is, again, not a real Vulkan thing) instead of an Xcb one.
	 */

	/* Setup ncurses colors for rendering */
	setup_ncurses_colors();

	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	*swapchain = (struct tut6_swapchain){0};

	/* Create the surface */
	VkNcursesSurfaceCreateInfoSHY surface_info = {
		.sType = VK_STRUCTURE_TYPE_NCURSES_SURFACE_CREATE_INFO_SHY,
		.window = window,
	};
	res = vkCreateNcursesSurfaceSHY(vk, &surface_info, NULL, &swapchain->surface);
	tut1_error_set_vkresult(&retval, res);
	if (res)
		goto exit_failed;

	/* Everything else in this function is the same as tut6_get_swapchain */

	/* Surface capabilities */
	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phy_dev->physical_device, swapchain->surface, &swapchain->surface_caps);
	tut1_error_set_vkresult(&retval, res);
	if (res)
		goto exit_failed;

	/* Query surface info */
	uint32_t image_count = swapchain->surface_caps.minImageCount + thread_count - 1;
	if (swapchain->surface_caps.maxImageCount < image_count && swapchain->surface_caps.maxImageCount != 0)
		image_count = swapchain->surface_caps.maxImageCount;

	uint32_t surface_format_count = 1;
	res = vkGetPhysicalDeviceSurfaceFormatsKHR(phy_dev->physical_device, swapchain->surface, &surface_format_count, &swapchain->surface_format);
	tut1_error_set_vkresult(&retval, res);
	if (res < 0)
		goto exit_failed;

	if (surface_format_count == 1 && swapchain->surface_format.format == VK_FORMAT_UNDEFINED)
		swapchain->surface_format.format = VK_FORMAT_R8G8B8A8_UNORM;

	swapchain->present_modes_count = TUT6_MAX_PRESENT_MODES;
	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

	res = vkGetPhysicalDeviceSurfacePresentModesKHR(phy_dev->physical_device, swapchain->surface,
			&swapchain->present_modes_count, swapchain->present_modes);
	tut1_error_set_vkresult(&retval, res);
	if (res >= 0)
	{
		for (uint32_t i = 0; i < swapchain->present_modes_count; ++i)
		{
			if (allow_no_vsync && swapchain->present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
				break;
			}
			if (!allow_no_vsync && swapchain->present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
		}
	}

	/* Create swapchain */
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

	res = vkCreateSwapchainKHR(dev->device, &swapchain_info, NULL, &swapchain->swapchain);
	tut1_error_set_vkresult(&retval, res);

exit_failed:
	return retval;
}

/*
 * We don't really know what the types of VkSurfaceKHR and VkSwapchainKHR are, so let's play safe and use them as a
 * one-byte value, an index into a set of possible surfaces and swapchains.  This limits our ability to support more
 * than 256 surfaces or swapchains!
 *
 * These indices would point to objects of type struct ncurses_surface and ncurses_swapchain, which would each contain
 * information about the surface and swapchain for the driver (our fake ncurses driver) to know and use for rendering.
 * Once again, note that in this tutorial, we are not the users of surface and swapchains, but the implementers!
 *
 * Most of the fields in these structs are better explained in the re-implementation of vkCreateSwapchainKHR below.
 */
struct ncurses_surface
{
	WINDOW *window;			/* What ncurses window the surface was created for */
};

struct ncurses_swapchain_image
{
	VkImage image;
	VkDeviceMemory image_mem;
	bool owned_by_application;
	bool being_rendered;
};

struct ncurses_swapchain_submission
{
	uint32_t image_index;
};

#define MAX_SUBMISSION_QUEUE_SIZE 16
struct ncurses_swapchain_submission_buffer
{
	struct ncurses_swapchain_submission submissions[MAX_SUBMISSION_QUEUE_SIZE];
	uint32_t read_index, write_index;
};

static bool submission_buffer_empty(struct ncurses_swapchain_submission_buffer *buf)
{
	return buf->write_index == buf->read_index;
}
static bool submission_buffer_full(struct ncurses_swapchain_submission_buffer *buf)
{
	return (buf->write_index + 1) % MAX_SUBMISSION_QUEUE_SIZE == buf->read_index;
}
static void submission_buffer_write(struct ncurses_swapchain_submission_buffer *buf, struct ncurses_swapchain_submission *submission)
{
	buf->submissions[buf->write_index++] = *submission;
	buf->write_index %= MAX_SUBMISSION_QUEUE_SIZE;
}
static void submission_buffer_read(struct ncurses_swapchain_submission_buffer *buf, struct ncurses_swapchain_submission *submission)
{
	*submission = buf->submissions[buf->read_index++];
	buf->read_index %= MAX_SUBMISSION_QUEUE_SIZE;
}

struct ncurses_swapchain
{
	VkDevice device;

	struct ncurses_surface *surface;	/* What surface the swapchain is created for */
	size_t width, height;		/* What is the surface size at creation time (which is also the swapchain image size) */
	struct ncurses_swapchain_image *images;	/* The swapchain images */
	uint32_t image_count;

	uint32_t render_queue_family;	/* The queue family supported for rendering */
	VkCommandPool render_cmd_pool;
	VkCommandBuffer render_cmd_buf;	/* The extra command buffer used to copy images */
	VkQueue render_queue;		/* The queue used to execute render_cmd_buf */
	VkFence render_fence;
	VkFence present_fence;
	struct ncurses_swapchain_image render_image;	/* The host-visible image to copy swapchain images to for reading */

	struct ncurses_swapchain_submission_buffer submission_buffer;

	pthread_t render_thread;	/* The rendering thread */
	pthread_mutex_t mutex;		/* For interaction with the rendering thread */
	bool thread_created;		/* To know if we should join it */
	bool request_stop;		/* To stop the thread */
};

/* A cache to store surface and swapchain objects.  VkSurfaceKHR and VkSwapchainKHR would be used as indices to this cache */
#define MAX_SURFACE_COUNT 16
#define MAX_SWAPCHAIN_COUNT 32
static struct ncurses_surface *surface_cache[MAX_SURFACE_COUNT] = {NULL};
static struct ncurses_swapchain *swapchain_cache[MAX_SWAPCHAIN_COUNT] = {NULL};

/*
 * Since we are pretending to be the driver (i.e. the implementation of the Vulkan API), it's time to also take a look
 * at those allocator callbacks we have been ignoring until now.
 *
 * The allocation callbacks are easy, really.  There are two sets of callbacks:
 *
 * - Equivalents of malloc, free and realloc (pfnAllocation, pfnFree, pfnReallocation): these should be always provided
 *   if the allocator structure is given.  These are used to allocate objects you are asking for.  For example, if you
 *   ask to create a surface and Vulkan needs to allocate host memory for it, it will use these callbacks.
 * - Notifications of internal allocation and free (pfnInternalAllocation, pfnInternalFree): these are optional (either
 *   both can be given or none).  They are used by Vulkan to allocate internal memory that is too complex for the
 *   application to handle (currently, this contains only "executable" memory).  As a result, the application is merely
 *   informed that the allocation/free has happened.
 *
 * There is also a user provided pointer (`void *pUserData`) that is sent back to every callback, which is the proper
 * way of using callbacks (always give a `void *user_data` to callbacks, or you would be no better than GLUT).
 *
 * Each allocation is given a scope which defines its lifetime, such as COMMAND (allocation lives and dies within the
 * same Vulkan command), OBJECT (a Vulkan object, like an image or a swapchain), DEVICE or INSTANCE.  Each function
 * that might allocate something (and thus takes an allocator), first tries to allocate using the given allocator.  If
 * no allocator is given, it will see if the device was created with an allocator and use that instead.  If the device
 * doesn't have allocators either, it will use the instance's.
 *
 * Here, we don't have access to the internals of the Vulkan implementation, so we can't easily look "up" to see if the
 * device or instance were created with the allocator callbacks, so let's just use default ones right away if no
 * callback was given.
 */

static void *default_allocation(void *user_data, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
	/* Never heard of aligned_alloc?  That's a C11 function!  It's like malloc, but also takes alignment. */
	return aligned_alloc(alignment, size);
}
static void *default_reallocation(void *user_data, void *original, size_t size, size_t alignment, VkSystemAllocationScope scope)
{
	/*
	 * Note: realloc may not respect `alignment`.  The implementation is the one that asks for `alignment`, so it
	 * generally has an easier time guaranteeing alignment with its default allocation functions.  We could go
	 * through hoops, to ensure alignment, but there's not much vulkan-tutorial value in that here, so long as you
	 * know this matters.
	 */
	printf("Possible alignment issue using realloc\n");
	return realloc(original, size);
}
static void default_free(void *user_data, void *memory)
{
	free(memory);
}
static const VkAllocationCallbacks default_allocator = {
	.pfnAllocation = default_allocation,
	.pfnReallocation = default_reallocation,
	.pfnFree = default_free,
};

static int new_surface(const VkAllocationCallbacks *allocator)
{
	if (allocator == NULL)
		allocator = &default_allocator;

	for (unsigned int i = 0; i < MAX_SURFACE_COUNT; ++i)
		if (surface_cache[i] == NULL)
		{
			surface_cache[i] = allocator->pfnAllocation(allocator->pUserData, sizeof *surface_cache[i], 4, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
			return i;
		}
	return -1;
}

static int new_swapchain(const VkAllocationCallbacks *allocator)
{
	if (allocator == NULL)
		allocator = &default_allocator;

	for (unsigned int i = 0; i < MAX_SWAPCHAIN_COUNT; ++i)
		if (swapchain_cache[i] == NULL)
		{
			swapchain_cache[i] = allocator->pfnAllocation(allocator->pUserData, sizeof *swapchain_cache[i], 4, VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
			return i;
		}
	return -1;
}

static void free_surface(unsigned int i, const VkAllocationCallbacks *allocator)
{
	if (allocator == NULL)
		allocator = &default_allocator;

	if (i >= MAX_SURFACE_COUNT)
		return;
	allocator->pfnFree(allocator->pUserData, surface_cache[i]);
	surface_cache[i] = NULL;
}

static void free_swapchain(unsigned int i, const VkAllocationCallbacks *allocator)
{
	if (allocator == NULL)
		allocator = &default_allocator;

	if (i >= MAX_SWAPCHAIN_COUNT)
		return;
	allocator->pfnFree(allocator->pUserData, swapchain_cache[i]);
	swapchain_cache[i] = NULL;
}

/* Use the first byte of VkSurfaceKHR and VkSwapchainKHR to store the index */
#define SURFACE_INDEX(s) (*(uint8_t *)&s)
#define SWAPCHAIN_INDEX(s) (*(uint8_t *)&s)

/* First, the new made-up vkCreateNcursesSurfaceSHY! */
VkResult vkCreateNcursesSurfaceSHY(VkInstance instance, const VkNcursesSurfaceCreateInfoSHY *create_info,
		const VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
	/* Allocate a new surface cache entry to store this surface */
	int surface_index = new_surface(allocator);
	if (surface_index < 0)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	*surface_cache[surface_index] = (struct ncurses_surface){
		.window = create_info->window,
	};

	/* Store its index in the VkSurfaceKHR variable */
	SURFACE_INDEX(*surface) = surface_index;

	return VK_SUCCESS;
}

/*
 * From here on, the surface-and-swapchain-related Vulkan functions are rewritten to use our ncurses-based surfaces and
 * swapchains.
 *
 * Easy stuff first: list capabilities and properties
 */
VkResult vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physical_device, uint32_t queue_family_index,
		VkSurfaceKHR surface, VkBool32 *supported)
{
	/*
	 * Rendering through ncurses is done by the application (on CPU), so one could think we can really support any
	 * queue family.  However, as you would see in the rendering thread below, we would need to copy images, so
	 * let's support only queues that support Transfer or Graphics.
	 *
	 * For reasons you will see later (when talking about copying the swapchain images), let's only support the
	 * first such queue.
	 */
	VkQueueFamilyProperties queue_families[queue_family_index + 1];
	uint32_t queue_family_count = queue_family_index + 1;
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

	*supported = false;
	if (queue_family_count <= queue_family_index)
		return VK_SUCCESS;

	/* If a lower-id queue family is supported, don't support this */
	for (uint32_t i = 0; i < queue_family_index; ++i)
		if ((queue_families[i].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT)) != 0)
			return VK_SUCCESS;

	*supported = (queue_families[queue_family_index].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT)) != 0;
	return VK_SUCCESS;
}

VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
		VkSurfaceCapabilitiesKHR *surface_capabilities)
{
	int width, height;
	getmaxyx(surface_cache[SURFACE_INDEX(surface)]->window, height, width);

	*surface_capabilities = (VkSurfaceCapabilitiesKHR){
		/* One image is always locked for rendering, and at least one can be spared to the application */
		.minImageCount = 2,
		/* There is no real maximum swapchain image count limit; 0 indicates infinity here */
		.maxImageCount = 0,
		/* Size of the image is the size of the terminal, such as 80x25.  This is retrieved from ncurses above */
		.currentExtent = { .width = width, .height = height, },
		/* Don't accept any other image size for the swapchains */
		.minImageExtent = { .width = width, .height = height, },
		.maxImageExtent = { .width = width, .height = height, },
		/* Only one layer */
		.maxImageArrayLayers = 1,
		/* No transformation */
		.supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		/* No alpha */
		.supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		/*
		 * Possible usages of swapchain images derived from this surface:
		 * - COLOR_ATTACHMENT: mandatory, otherwise we can't render into the images
		 * - TRANSFER_SRC: to be able to copy the image (explained somewhere below)
		 */
		.supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
			| VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	};

	return VK_SUCCESS;
}

VkResult vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
		uint32_t *surface_format_count, VkSurfaceFormatKHR *surface_formats)
{
	VkResult res = *surface_format_count < 1?VK_INCOMPLETE:VK_SUCCESS;

	/*
	 * For simplicity, only support one format: R8G8B8A8_UNORM.  Note that we don't really need the alpha channel,
	 * but Vulkan has better guarantees of support for 32-bit texel sizes than 24-bit ones.  On Nvidia 370.28 for
	 * example, only B8G8R8A8_UNORM/SRGB formats are supported for surfaces, and for image creation, R8G8B8 and
	 * B8G8R8 formats are not supported at all.
	 */
	if (*surface_format_count >= 1)
		surface_formats[0] = (VkSurfaceFormatKHR){
			.format = VK_FORMAT_R8G8B8A8_UNORM,
			.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
		};

	*surface_format_count = 1;
	return res;
}

VkResult vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
		uint32_t *present_mode_count, VkPresentModeKHR *present_modes)
{
	VkResult res = *present_mode_count < 1?VK_INCOMPLETE:VK_SUCCESS;

	/* For simplicity, only support one mode: FIFO (the only mandatory mode) */
	if (*present_mode_count >= 1)
		present_modes[0] = VK_PRESENT_MODE_FIFO_KHR;

	*present_mode_count = 1;
	return res;
}

/* Helper for vkCreateSwapchainKHR below */
static VkResult create_image(VkDevice device, const VkSwapchainCreateInfoKHR *create_info,
		const VkAllocationCallbacks *allocator, struct ncurses_swapchain_image *image, bool host_visible)
{
	/* Create the image (this is similar to tut7_create_images) */
	struct VkImageCreateInfo image_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = create_info->imageFormat,
		.extent = {create_info->imageExtent.width, create_info->imageExtent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = host_visible?VK_IMAGE_TILING_LINEAR:VK_IMAGE_TILING_OPTIMAL,
		.usage = (create_info->imageUsage
			& ~(host_visible?VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT:0))
			| (host_visible?VK_IMAGE_USAGE_TRANSFER_DST_BIT:VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
		.sharingMode = create_info->imageSharingMode,
		.queueFamilyIndexCount = create_info->queueFamilyIndexCount,
		.pQueueFamilyIndices = create_info->pQueueFamilyIndices,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};
	VkResult res = vkCreateImage(device, &image_info, allocator, &image->image);
	if (res)
		return res;

	/* Find a suitable memory backing for it (this is similar to tut4_find_suitable_memory) */
	VkPhysicalDeviceMemoryProperties memories;
	vkGetPhysicalDeviceMemoryProperties(g_phy_dev->physical_device, &memories);

	VkMemoryRequirements mem_req = {0};
	vkGetImageMemoryRequirements(device, image->image, &mem_req);
	VkMemoryPropertyFlags properties = host_visible?
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	uint32_t mem_index = memories.memoryTypeCount;

	for (uint32_t i = 0; i < memories.memoryTypeCount; ++i)
	{
		if ((mem_req.memoryTypeBits & 1 << i) == 0)
			continue;
		if (memories.memoryHeaps[memories.memoryTypes[i].heapIndex].size < mem_req.size)
			continue;
		if ((memories.memoryTypes[i].propertyFlags & properties) == properties)
		{
			mem_index = i;
			break;
		}
	}

	if (mem_index >= memories.memoryTypeCount)
		return VK_ERROR_INCOMPATIBLE_DRIVER;

	/* Allocate memory for the image */
	VkMemoryAllocateInfo mem_info = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_req.size,
		.memoryTypeIndex = mem_index,
	};
	res = vkAllocateMemory(device, &mem_info, allocator, &image->image_mem);
	if (res)
		return res;

	/* Bind memory to image */
	return vkBindImageMemory(device, image->image, image->image_mem, 0);
}

/*
 * Now, let's do create and destroy functions.  The destroy functions would be trivial once the create function is
 * written, so let's focus on that.  The surface create function is already written, so the swapchain remains.
 *
 * To do the swapchain create function, we would need a bit of planning.  In particular, we need to know how we are
 * actually going to do the rendering.  Assuming you gave it some thought right now, here is how I have done it:
 *
 * There are N images, each with flags that state whether they are acquired by the user or not and whether they are
 * busy being presented.  These flags are useful for vkAcquireNextImageKHR to know which image to hand out.  A thread
 * is used to simulate how the GPU presents an image asynchronously.  A circular buffer is used to queue submissions
 * for presentation to the "presentation thread".
 *
 * The image to present is in PRESENT_SRC_KHR layout, which is great for the driver to present it, but we are not the
 * driver.  Not only that, but the tiling of the image is also going to be OPTIMAL which we can't read.  So we are
 * forced to copy the image to a LINEAR tiling image with GENERAL layout to be able to read it and render it using
 * ncurses.  For this, we need a command buffer, which we should allocate.  This is also the reason why we forced the
 * user to stick to a single queue family (in vkGetPhysicalDeviceSurfaceSupportKHR), so that we know which queue family
 * to allocate this extra command buffer from.
 *
 * One last point!  We need a queue to execute this command buffer for the image copy (and other operations).  A
 * complication here is that the queue given to vkQueueSubmit needs to be "externally synchronized", which means we
 * would need a mutex for example to make sure the calls to vkQueueSubmit don't happen at the same time (one by our
 * rendering thread and one by the application).  This is not possible without the application doing something extra
 * that it would normally not do with a real driver.  The alternative is to use different queues.  This means that the
 * application would be limited in which queues it uses to submit.  Let's allow that, and reserve the last queue of the
 * queue family for the renderer!  We already know that the application uses the first queue of the famliy.  Naturally,
 * this is not a portable solution!
 */
static void *render_thread(void *);

VkResult vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *create_info,
		const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain)
{
	VkResult res = VK_ERROR_OUT_OF_HOST_MEMORY;

	/* Allocate a new swapchain cache entry to store this swapchain */
	int swapchain_index = new_swapchain(allocator);
	if (swapchain_index < 0)
		return VK_ERROR_OUT_OF_HOST_MEMORY;

	struct ncurses_swapchain *sw = swapchain_cache[swapchain_index];
	*sw = (struct ncurses_swapchain){
		.device = device,
	};

	/* Remember the surface and the size of the images, so if the terminal changes size, we can detect it */
	sw->surface = surface_cache[SURFACE_INDEX(create_info->surface)];
	sw->width = create_info->imageExtent.width;
	sw->height = create_info->imageExtent.height;

	/* Use the first queue family that supports graphics for doing the image copies */
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g_phy_dev->physical_device, &queue_family_count, NULL);
	VkQueueFamilyProperties queue_families[queue_family_count];
	vkGetPhysicalDeviceQueueFamilyProperties(g_phy_dev->physical_device, &queue_family_count, queue_families);

	sw->render_queue_family = queue_family_count;
	for (uint32_t i = 0; i < queue_family_count; ++i)
		if ((queue_families[i].queueFlags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT)) != 0)
		{
			sw->render_queue_family = i;
			break;
		}
	if (sw->render_queue_family >= queue_family_count || queue_families[sw->render_queue_family].queueCount < 2)
	{
		/*
		 * As explained above, to circumvent synchronization issues, we decided to reserve the last queue of this family for
		 * this fake driver, so the queue family must support at least two queues.
		 */
		res = VK_ERROR_INCOMPATIBLE_DRIVER;
		goto exit_failed;
	}

	/* Allocate a command buffer for this family */
	VkCommandPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex = sw->render_queue_family,
	};
	res = vkCreateCommandPool(device, &pool_info, allocator, &sw->render_cmd_pool);
	if (res < 0)
		goto exit_failed;
	VkCommandBufferAllocateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool = sw->render_cmd_pool,
		.commandBufferCount = 1,
	};
	res = vkAllocateCommandBuffers(device, &buffer_info, &sw->render_cmd_buf);
	if (res)
		goto exit_failed;

	/*
	 * Take the last queue of this family for rendering (remember that in Tutorial 2, we had one command pool per command
	 * buffer, and the queues were stored together with command pools (which in hindsight was not a very good idea)).
	 */
	struct tut2_commands *cmds = &g_dev->command_pools[sw->render_queue_family];
	sw->render_queue = cmds->queues[cmds->queue_count - 1];

	/* Get a fence to throttle and order the presentations.  Another fence for waiting on semaphores on vkQueuePresentKHR */
	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	res = vkCreateFence(device, &fence_info, allocator, &sw->render_fence);
	if (res)
		goto exit_failed;
	res = vkCreateFence(device, &fence_info, allocator, &sw->present_fence);
	if (res)
		goto exit_failed;

	/* Create the swapchain images */
	sw->images = calloc(create_info->minImageCount, sizeof *sw->images);
	if (sw->images == NULL)
		goto exit_failed;
	sw->image_count = create_info->minImageCount;
	for (uint32_t i = 0; i < sw->image_count; ++i)
	{
		res = create_image(device, create_info, allocator, &sw->images[i], false);
		if (res)
			goto exit_failed;
	}

	/* Additionally, create a host-visible image to be able to read back rendered swapchain images */
	res = create_image(device, create_info, allocator, &sw->render_image, true);
	if (res)
		goto exit_failed;

	/* Finally, create the rendering thread */
	int err_no = pthread_mutex_init(&sw->mutex, NULL);
	if (err_no)
		goto exit_failed;
	err_no = pthread_create(&sw->render_thread, NULL, render_thread, sw);
	if (err_no)
		goto exit_failed;
	sw->thread_created = true;

	/* Store the swapchain cache index in the VkSwapchainKHR variable */
	SWAPCHAIN_INDEX(*swapchain) = swapchain_index;

	return VK_SUCCESS;

exit_failed:
	SWAPCHAIN_INDEX(*swapchain) = swapchain_index;
	vkDestroySwapchainKHR(device, *swapchain, allocator);

	return res;
}

void vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocator)
{
	struct ncurses_swapchain *sw = swapchain_cache[SWAPCHAIN_INDEX(swapchain)];
	if (sw == NULL)
		return;

	sw->request_stop = true;
	if (sw->thread_created)
		pthread_join(sw->render_thread, NULL);
	pthread_mutex_destroy(&sw->mutex);

	vkDeviceWaitIdle(device);
	for (uint32_t i = 0; i < sw->image_count; ++i)
	{
		vkDestroyImage(device, sw->images[i].image, allocator);
		vkFreeMemory(device, sw->images[i].image_mem, allocator);
	}
	vkDestroyImage(device, sw->render_image.image, allocator);
	vkFreeMemory(device, sw->render_image.image_mem, allocator);

	vkDestroyCommandPool(device, sw->render_cmd_pool, allocator);

	vkDestroyFence(device, sw->render_fence, allocator);
	vkDestroyFence(device, sw->present_fence, allocator);

	free(sw->images);
	free_swapchain(SWAPCHAIN_INDEX(swapchain), allocator);
}

void vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface, const VkAllocationCallbacks *allocator)
{
	free_surface(SURFACE_INDEX(surface), allocator);
}

/*
 * vkGetSwapchainImagesKHR is really easy.  Like all other Vulkan functions that look like this, the `count` tells how
 * many objects (swapchain images in this case) we can return, and in the end, it will contain the actual number of
 * objects there are.  This way, the total number of objects can be queried by giving a `count` of 0, and then the
 * actual objects retrieved as many as the function previously returned.
 */
VkResult vkGetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain, uint32_t *swapchain_image_count, VkImage *swapchain_images)
{
	struct ncurses_swapchain *sw = swapchain_cache[SWAPCHAIN_INDEX(swapchain)];

	uint32_t images_to_get = sw->image_count;
	if (swapchain_image_count)
	{
		/* Return as many images as there are, not overflowing the input array */
		if (images_to_get > *swapchain_image_count)
			images_to_get = *swapchain_image_count;

		/* Return the actual number of images in `count` */
		*swapchain_image_count = sw->image_count;
	}

	if (swapchain_images)
		for (uint32_t i = 0; i < images_to_get; ++i)
			swapchain_images[i] = sw->images[i].image;

	return VK_SUCCESS;
}

/*
 * vkAcquireNextImageKHR is not a difficult function either.  All it does is find an image that is not already owned by
 * the application and is not busy being rendered, and return its index.  The function needs to wait `timeout`
 * nanoseconds for an image to become available.
 *
 * The most tricky part about this function is signalling the semaphore and the fence if provided.  Vulkan doesn't
 * provide a way for the application to do this (this is the job of the driver).  We are an application who is
 * pretending to be a driver, so we have to once again find a way around this.  Luckily, we have a queue dedicated here
 * which we can use for the sake of signal generation.
 */
static uint64_t get_time_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LLU + ts.tv_nsec;
}

static bool surface_size_changed(struct ncurses_swapchain *sw)
{
	int width, height;
	getmaxyx(sw->surface->window, height, width);
	return width != sw->width || height != sw->height;
}

VkResult vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *image_index)
{
	struct ncurses_swapchain *sw = swapchain_cache[SWAPCHAIN_INDEX(swapchain)];
	uint32_t found_index = sw->image_count;

	uint64_t start_time = get_time_ns();
	do
	{
		/*
		 * Search for a free image.  We don't really need the mutex here because we are just testing a flag.  Even if we miss
		 * it "just finishing rendering", we will catch it in the next cycle.
		 */
		for (uint32_t i = 0; i < sw->image_count; ++i)
		{
			if (!sw->images[i].owned_by_application && !sw->images[i].being_rendered)
			{
				found_index = i;
				break;
			}
		}
	} while (get_time_ns() - start_time < timeout && found_index == sw->image_count);

	/* If no image available, this is a timeout */
	if (found_index == sw->image_count)
		return timeout == 0?VK_NOT_READY:VK_TIMEOUT;
	*image_index = found_index;

	/*
	 * If the surface has changed size, signal this as error.  If we wanted to scale the image in the rendering thread, we
	 * could return VK_SUBOPTIMAL_KHR, but we aren't going to bother with that.
	 */
	if (surface_size_changed(sw))
		return VK_ERROR_OUT_OF_DATE_KHR;

	/* Mark the image as owned by the application from now on (until presented) */
	sw->images[*image_index].owned_by_application = true;

	/*
	 * Use the rendering queue (which needs to be synchronized with the rendering thread) to signal the semaphore and the
	 * fence.  Fortunately, it is possible to make a submission without a command buffer.
	 */
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.signalSemaphoreCount = semaphore == NULL?0:1,
		.pSignalSemaphores = &semaphore,
	};
	pthread_mutex_lock(&sw->mutex);
	VkResult res = vkQueueSubmit(sw->render_queue, 1, &submit_info, fence);
	pthread_mutex_unlock(&sw->mutex);

	return res;
}

/*
 * vkQueuePresentKHR is a bridge here to transfer the submission information to the rendering thread.  The only thing
 * that makes this function a little involved is the fact that there could be multiple presentations at the same time,
 * and of course a similar deal with waiting on semaphores.
 */
VkResult vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *present_info)
{
	/*
	 * VkPresentInfo can contain multiple presentations.  We will make a single submission for each of those.  The
	 * wait semaphores are waited on right here, because the rendering thread cannot use them without requiring
	 * synchronization with the application (the semaphores are externally synchronized objects).  The queue is
	 * ignored and we will use the render_queue for the reasons previously mentioned.
	 */
	VkResult overall_res = VK_SUCCESS;

	if (present_info->swapchainCount == 0)
		return overall_res;

	/* Wait for the semaphores, if any */
	if (present_info->waitSemaphoreCount > 0)
	{
		/* Use the queue and fence of the first swapchain to wait on the semaphores. */
		struct ncurses_swapchain *sw = swapchain_cache[SWAPCHAIN_INDEX(present_info->pSwapchains[0])];

		VkResult res = vkResetFences(sw->device, 1, &sw->present_fence);
		if (res)
			return res;

		VkPipelineStageFlags wait_sem_stages[present_info->waitSemaphoreCount];
		for (uint32_t i = 0; i < present_info->waitSemaphoreCount; ++i)
			wait_sem_stages[i] = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		VkSubmitInfo submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = present_info->waitSemaphoreCount,
			.pWaitSemaphores = present_info->pWaitSemaphores,
			.pWaitDstStageMask = wait_sem_stages,
		};
		pthread_mutex_lock(&sw->mutex);
		res = vkQueueSubmit(sw->render_queue, 1, &submit_info, sw->present_fence);
		pthread_mutex_unlock(&sw->mutex);
		if (res)
			return res;

		res = vkWaitForFences(sw->device, 1, &sw->present_fence, true, 1000000000);
		if (res)
			return res;
	}

	for (uint32_t i = 0; i < present_info->swapchainCount; ++i)
	{
		struct ncurses_swapchain *sw = swapchain_cache[SWAPCHAIN_INDEX(present_info->pSwapchains[i])];

		/*
		 * We need to check and see if the ncurses window is still the same size.  If not, again, we can return
		 * either VK_SUBOPTIMAL_KHR or VK_ERROR_OUT_OF_DATE_KHR depending on whether we can scale the image at
		 * presention time or not.  We decided to not complicate things further, and require that the surface
		 * doesn't change size.  vkQueuePresentKHR is supposed to return VK_ERROR_OUT_OF_DATE_KHR if any of the
		 * presentations had that error.  Individual return errors are still set per presentation.
		 */
		VkResult res = surface_size_changed(sw)?VK_ERROR_OUT_OF_DATE_KHR:VK_SUCCESS;
		if (res == VK_ERROR_OUT_OF_DATE_KHR)
			overall_res = VK_ERROR_OUT_OF_DATE_KHR;

		if (present_info->pResults)
			present_info->pResults[i] = res;

		struct ncurses_swapchain_submission submission = {
			.image_index = present_info->pImageIndices[i],
		};

		/* The image is no longer owned by the application, but is being rendered (so still unavailable) */
		sw->images[present_info->pImageIndices[i]].being_rendered = true;
		sw->images[present_info->pImageIndices[i]].owned_by_application = false;

		bool success = false;
		while (!success)
		{
			pthread_mutex_lock(&sw->mutex);
			if (!submission_buffer_full(&sw->submission_buffer))
			{
				submission_buffer_write(&sw->submission_buffer, &submission);
				success = true;
			}
			pthread_mutex_unlock(&sw->mutex);
		}
	}

	return overall_res;
}

/* A couple of helper functions for rendering */
static VkResult start_recording(struct ncurses_swapchain *sw)
{
	vkResetCommandBuffer(sw->render_cmd_buf, 0);
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	return vkBeginCommandBuffer(sw->render_cmd_buf, &begin_info);
}

static int stop_recording_and_submit(struct ncurses_swapchain *sw)
{
	vkEndCommandBuffer(sw->render_cmd_buf);

	VkResult res = vkResetFences(sw->device, 1, &sw->render_fence);
	if (res)
		return res;

	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &sw->render_cmd_buf,
	};

	pthread_mutex_lock(&sw->mutex);
	vkQueueSubmit(sw->render_queue, 1, &submit_info, sw->render_fence);
	pthread_mutex_unlock(&sw->mutex);

	return vkWaitForFences(sw->device, 1, &sw->render_fence, true, 1000000000);
}

static void record_image_barrier(VkCommandBuffer cmd_buf, VkImage image,
		VkAccessFlags src_access, VkImageLayout src_layout, VkPipelineStageFlags src_stage,
		VkAccessFlags dst_access, VkImageLayout dst_layout, VkPipelineStageFlags dst_stage)
{
	VkImageMemoryBarrier image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.oldLayout = src_layout,
		.newLayout = dst_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS,
		},
		.image = image,
	};

	vkCmdPipelineBarrier(cmd_buf,
			src_stage,
			dst_stage,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */
}

/* Finally, the rendering thread! */
static void *render_thread(void *arg)
{
	struct ncurses_swapchain *sw = arg;

	/* First step, transition the linear-tiling image to GENERAL layout, so we can read it */
	if (start_recording(sw))
		return NULL;

	record_image_barrier(sw->render_cmd_buf, sw->render_image.image,
			0, VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			0, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	if (stop_recording_and_submit(sw))
		return NULL;

	/*
	 * To be able to read an image, besides the fact that it should be in LINEAR tiling and GENERAL layout, we
	 * should know how it's actually laid out in memory.  That is for each texel coordinate, we should know what
	 * address in the image memory we should take data from.  This is important, because the driver may have done
	 * some padding for example for optimization, so we can't just assume the texel colors are put one after the
	 * other.  Vulkan nicely gives this information as a set of parameters and a C expression.  Essentially, the
	 * vkGetImageSubresourceLayout function is used to get these parameters and the following expression to
	 * calculate the position of the desired texel:
	 *
	 * // (x,y,z,layer) are in texel coordinates
	 * address(x,y,z,layer) = layer*arrayPitch + z*depthPitch + y*rowPitch + x*texelSize + offset
	 *
	 * We know our image is 2D and with only one layer, so `z` and `layer` are both 0.  texelSize comes from the
	 * format given to VkSwapchainCreateInfoKHR, saying how many bytes there are per texel.  In
	 * vkGetPhysicalDeviceSurfaceFormatsKHR, we decided to support only R8G8B8A8_UNORM for simplicity.  This is
	 * where that simplicity is; texelSize is 3 and the bytes are in the order of Red (lower address), Green and
	 * Blue (higher address); we don't need to calculate that!
	 */
	VkImageSubresource render_image_subresource = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.mipLevel = 0,
		.arrayLayer = 0,
	};
	VkSubresourceLayout render_image_layout;
	vkGetImageSubresourceLayout(sw->device, sw->render_image.image, &render_image_subresource, &render_image_layout);

	/* Let's render the FPS why not, since the application would have a hard time doing it */
	unsigned int frames = 0;
	unsigned int fps = 1;
	uint64_t before = get_time_ns();

	while (!sw->request_stop)
	{
		/* Wait for a submission */
		struct ncurses_swapchain_submission submission;
		bool any_submission = false;

		pthread_mutex_lock(&sw->mutex);
		if (!submission_buffer_empty(&sw->submission_buffer))
		{
			submission_buffer_read(&sw->submission_buffer, &submission);
			any_submission = true;
		}
		pthread_mutex_unlock(&sw->mutex);

		if (!any_submission)
			continue;

		/* Note: clear() makes the FPS look nicer, but gives a lot of flicker. */
		/* clear(); */

		/*
		 * The image to be presented is in PRESENT_SRC_KHR layout, which is great were we a driver, but we are
		 * not.  As discussed before, we should take this image, copy it to the image with linear tiling, read
		 * that and render it.
		 */
		if (start_recording(sw))
			return NULL;

		/*
		 * Step 1: transition the presentation image from PRESENT_SRC_KHR to TRANSFER_SRC layout so we can copy
		 * from it.
		 */
		record_image_barrier(sw->render_cmd_buf, sw->images[submission.image_index].image,
				VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		/* Step 2: copy the image to something we can read it */
		VkImageCopy copy_region = {
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
			},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.layerCount = 1,
			},
			.extent = {
				.width = sw->width,
				.height = sw->height,
			},
		};
		vkCmdCopyImage(sw->render_cmd_buf, sw->images[submission.image_index].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				sw->render_image.image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy_region);

		/*
		 * I'm **sure** you remember from Tutorial 4 that we need a memory barrier to make sure the above copy
		 * is finished before we can read from the image.
		 *
		 * All WRITEs before the end of the pipeline must be done before all READs by the host.
		 */
		record_image_barrier(sw->render_cmd_buf, sw->render_image.image,
				VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_HOST_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_HOST_BIT);

		if (stop_recording_and_submit(sw))
			return NULL;

		/* Step 3: get access to the image */
		void *mem = NULL;
		if (vkMapMemory(sw->device, sw->render_image.image_mem, 0, render_image_layout.size, 0, &mem))
			return NULL;

		/* Step 4: render the image with ncurses (don't render first row and leave it for information) */
		for (uint32_t r = 1; r < sw->height; ++r)
			for (uint32_t c = 0; c < sw->width; ++c)
			{
				uint8_t *texel = (uint8_t *)mem + r * render_image_layout.rowPitch + c * 4 + render_image_layout.offset;
				uint8_t red = texel[0], green = texel[1], blue = texel[2];

				set_color(red, green, blue);
				mvwprintw(sw->surface->window, r, c, "#");
			}

		vkUnmapMemory(sw->device, sw->render_image.image_mem);

		if (start_recording(sw))
			return NULL;

		/*
		 * Step 5: get the presentation image back to PRESENT_SRC_KHR, because that's the layout the user
		 * expects to find the image after it's reacquired.
		 */
		record_image_barrier(sw->render_cmd_buf, sw->images[submission.image_index].image,
				0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

		if (stop_recording_and_submit(sw))
			return NULL;

		/* Mark the image is available for acquisition by the application. */
		sw->images[submission.image_index].being_rendered = false;

		++frames;
		uint64_t now = get_time_ns();
		if (now - before > 1000000000)
		{
			fps = frames;
			frames = 0;
			before += 1000000000;
		}

		set_color(0x40, 0x40, 0x40);
		mvwprintw(sw->surface->window, 0, 0, "%u FPS", fps);
		mvwprintw(sw->surface->window, 0, 10, "Corruption? Uncomment clear() in tut12.c", fps);
		refresh();
	}

	return NULL;
}
