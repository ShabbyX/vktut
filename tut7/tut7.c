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
#include <string.h>
#include "tut7.h"

tut1_error tut7_create_images(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut7_image *images, uint32_t image_count)
{
	/*
	 * In this function, we will create a bunch of images.  Images in graphics serve essentially two purposes.  One
	 * is to provide data to shaders, traditionally known as textures.  Another is to render into either as the
	 * final result or for further use, traditionally also known as textures.  Vulkan calls all of these "images",
	 * which are just glorified "buffers".  We already worked with a Vulkan buffer in Tutorial 4, which was just an
	 * array of data.  Images on the other hand can have up to 3 dimensions, a format (such as BGRA), multisampling
	 * properties, tiling properties and a layout.  They are glorified buffers because all of these features can be
	 * emulated with buffers, although besides requiring more work in the shaders, using images also allows a lot
	 * more optimization by the device and its driver.
	 *
	 * That said, creating images is fairly similar to buffers.  You create an image, allocate memory to it, create
	 * an image view for access to the image, you bind it to a command buffer through a descriptor set and go on
	 * using it in the shaders.  Like buffers, you can choose to initialize the image.  The data sent through
	 * images could be anything, such as textures used to draw objects, patterns used by a shader to apply an
	 * effect, or just general data for the shaders.  The image can be written to as well.  The data written to an
	 * image could also be for anything, such as the final colors that go on to be displayed on the screen, the
	 * depth or stencil data, the output of a filter used for further processing, the processed image to be
	 * retrieved by the application (e.g. used by gimp), a texture that evolves over time, etc.
	 *
	 * Loading the image data is outside the scope of this tutorial, so we'll leave that for another time.  Once
	 * the image is created, it's device memory can be mapped, loaded and unmapped, so it is not necessary to do
	 * that in this function either.
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	for (uint32_t i = 0; i < image_count; ++i)
	{
		images[i].image = NULL;
		images[i].image_mem = NULL;
		images[i].view = NULL;
		images[i].sampler = NULL;

		/*
		 * To create an image, we need a CreateInfo struct as usual.  Some parts of this struct is similar to
		 * VkBufferCreateInfo from Tutorial 3.  The ones that need explanation are explained here.  The image
		 * type specifies what are the dimensions of the image.  In these tutorial series, we will use 2D
		 * images for simplicity.  Also for simplicity, let's ignore mipmapping and image layers.  The image
		 * format is one of VK_FORMAT.  A normal format could be VK_FORMAT_B8G8R8A8_UNORM, but it might make
		 * sense to use other formats, especially for images that would get their data from a texture file.
		 *
		 * If the image is going to be initialized, for example from a texture file, then the structure of the
		 * image data, otherwise known as "tiling", must be set to linear.  This means that the image is stored
		 * as a normal row-major array, so that when its memory is mapped by the application, it would make
		 * sense!  If the image is not to be initialized on the other hand, it is better to keep the tiling as
		 * optimal, which means whatever format the GPU likes best.  It is necessary for the application to
		 * copy a linear image to an optimal one for GPU usage.  If the application wants to read the image
		 * back, it must copy it from an optimal image to a linear one.  This also means that the `usage` of
		 * the linear images can contain only TRANSFER_SRC and TRANSFER_DST bits.  More on image copies when we
		 * actually start using them.
		 *
		 * Linear images are sampled only once.  Optimal images can be multisampled.  You can read about
		 * multisampling online (from OpenGL), but in short it asks for each pixel to be sampled at multiple
		 * locations inside the pixel, which helps with antialiasing.  Here, we will simply choose a higher
		 * number of samples as allowed by the GPU (retrieved with vkGetPhysicalDeviceImageFormatProperties
		 * below).
		 *
		 * Linear images are also restricted to 2D, no mipmapping and no layers, which is fortunate because we
		 * wanted those for simplicity anyway!  There is also a restriction on the format of the image, which
		 * cannot be depth/stencil.
		 *
		 * In Tutorial 3, we specified the buffer usage as storage, which was a rather generic specification.
		 * For the image, we have more options to specify the usage.  Choosing the minimum usage bits for each
		 * image, specifying only what we actually want to do with the image allows the GPU to possibly place
		 * the image in the most optimal memory location, or load/unload the image at necessary times.  This is
		 * left to the application to provide as it varies from case to case.  The usages in short are:
		 *
		 * Transfer src/dst: whether the image can be used as a source/destination of an image copy.
		 * Sampled: whether the image can be sampled by a shader.
		 * Storage: whether the image can be read from and written to by a shader.
		 * Color attachment: whether the image can be used as a render target (for color).
		 * Depth/stencil attachment: whether the image can be used as a render target (for depth/stencil).
		 * Transient attachment: whether the image is lazily allocated (ignored for now).
		 * Input attachment: whether the image can be read (unfiltered) by a shader (ignored for now).
		 *
		 * If the image was to be shared between queue families, it should be declared with a special
		 * `sharingMode` specifying that there would be concurrent access to the image, and by which queue
		 * families.  We are going to use the images and views created here in multiple pipelines, one for each
		 * swapchain image.  Since those pipelines may be created on top of different queue families, we need
		 * to tell Vulkan that these images would be shared.  At the time of this writing, on Nvidia cards
		 * there is only one queue family and sharing is meaningless.  However, it is legal for a driver to
		 * expose multiple similar queue families instead of one queue family with multiple queues.  The
		 * application is expected to provide the queue families that would use this image.  Most likely, the
		 * result of `tut7_get_presentable_queues` is what you would want.
		 *
		 * Finally, an image has a layout.  Each layout is limited in what operations can be done in it, but
		 * instead is optimal for a task.  The possible image layouts are:
		 *
		 * Undefined: no device access is allowed.  This is used as an initial layout and must be transitioned
		 *   away from before use.
		 * Preinitialized: no device access is allowed.  Similar to undefined, this is only an initial layout
		 *   and must be transitioned away from before use.  The only difference is that the contents of the
		 *   image are kept during the transition.
		 * General: supports all types of device access.
		 * Color attachment optimal: only usable with color attachment images.
		 * Depth/stencil attachment optimal: only usable with depth/stencil attachment images.
		 * Depth/stencil read-only optimal: only usable with depth/stencil attachment images.  The difference
		 *   between this and the depth/stencil attachment optimal layout is that this image can also be used
		 *   as a read-only sampled image or input attachment for use by the shaders.
		 * Shader read-only optimal: only usable with sampled and input attachment images.  Similar to
		 *   depth/stencil read-only optimal, this layout can be used as a read-only image or input attachment
		 *   for use by the shaders.
		 * Transfer src/dst optimal: only usable with transfer src/dst images and must only be used as the
		 *   source or destination of an image transfer.
		 * Present src (extension): used for presenting an image to a swapchain.  An image taken from the
		 *   swapchain is in this layout and must be transitioned away before use after
		 *   vkAcquireNextImageKHR.  Before giving the image back with vkQueuePresentKHR, it must be
		 *   transitioned again to this layout.
		 *
		 * For linear images, which are going to be initialized by the application, we will use the
		 * preinitialized layout.  Otherwise, the layout must be undefined and later transitioned to the
		 * desired layout using a pipeline barrier (more on this later).
		 */
		VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (images[i].will_be_initialized)
		{
			images[i].usage &= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
			tiling = VK_IMAGE_TILING_LINEAR;
		}
		else if (images[i].multisample)
		{
			/*
			 * To get the format properties for an image, we need to tell Vulkan how we expect to create the image,
			 * i.e. what is its format, type, tiling, usage and flags (which we didn't use).  We could check many
			 * of the parameters given to this function with the properties returned from this function, but we'll
			 * just take a possible sampling count out of it, and assume the parameters are fine.  In a real
			 * application, you would want to do more validity checks.
			 */
			VkImageFormatProperties format_properties;
			res = vkGetPhysicalDeviceImageFormatProperties(phy_dev->physical_device, images[i].format, VK_IMAGE_TYPE_2D,
					tiling, images[i].usage, 0, &format_properties);
			tut1_error_sub_set_vkresult(&retval, res);
			if (res == 0)
			{
				for (uint32_t s = VK_SAMPLE_COUNT_16_BIT; s != 0; s >>= 1)
					if ((format_properties.sampleCounts & s))
					{
						samples = s;
						break;
					}
			}
		}

		/*
		 * Create the image with the above description as usual.  The CreateInfo struct takes the parameters
		 * and memory allocation callbacks are not used.
		 */
		bool shared = images[i].sharing_queue_count > 1;
		struct VkImageCreateInfo image_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = images[i].format,
			.extent = {images[i].extent.width, images[i].extent.height, 1},
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = samples,
			.tiling = tiling,
			.usage = images[i].usage,
			.sharingMode = shared?VK_SHARING_MODE_CONCURRENT:VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = shared?images[i].sharing_queue_count:0,
			.pQueueFamilyIndices = shared?images[i].sharing_queues:NULL,
			.initialLayout = layout,
		};
		res = vkCreateImage(dev->device, &image_info, NULL, &images[i].image);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/*
		 * In Tutorial 4, we created a buffer, allocated memory for it and bound the memory to the buffer.
		 * Images are glorified buffers and the process is similar.  The same argument regarding host-coherent
		 * memory holds here as well.  So, if the image requires device-local memory, we will look for that,
		 * otherwise we will look for memory that is not just host-visible, but also host-coherent.
		 */
		VkMemoryRequirements mem_req = {0};
		vkGetImageMemoryRequirements(dev->device, images[i].image, &mem_req);
		uint32_t mem_index = tut4_find_suitable_memory(phy_dev, dev, &mem_req,
				images[i].host_visible?
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (mem_index >= phy_dev->memories.memoryTypeCount)
			continue;

		VkMemoryAllocateInfo mem_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = mem_req.size,
			.memoryTypeIndex = mem_index,
		};

		res = vkAllocateMemory(dev->device, &mem_info, NULL, &images[i].image_mem);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		res = vkBindImageMemory(dev->device, images[i].image, images[i].image_mem, 0);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/*
		 * Once we have an image, we need a view on the image to be able to use it.  This is just like in
		 * Tutorial 4 where we had a view on the buffer to work with it.  In Tutorial 4, we had divided up the
		 * buffer for concurrent processing in the shaders, and each view looked at a specific part of the
		 * buffer.  With images, this could also be useful, for example if one large image contains multiple
		 * areas of interest (such as a texture) where different shaders need to look at.  However, let's keep
		 * things as simple as possible and create a view that is as large as the image itself.
		 *
		 * The image view's CreateInfo is largely similar to the one for buffer views.  For image views, we
		 * need to specify which components of the image we want to view and the range is not a simple
		 * (offset, size) as was in the buffer view.
		 *
		 * For the components, we have the option to not only select which components (R, G, B and A) to view,
		 * but also to remap them (this operation is called swizzle).  For example to get the value of the red
		 * component in place of alpha etc.  The mapping for each component can be specified separately, and
		 * mapping 0 means identity.  We are not going to remap anything, so we'll leave all fields in
		 * `components` be 0.
		 *
		 * The range of the image asks for which mipmap levels and image array layers we are interested in,
		 * which are simply both 0 because we have only one of each.  As part of the range of the view, we also
		 * need to specify which aspect of the image we are looking it.  This could be color, depth, stencil
		 * etc.  Here, we will decide the aspect based on the image usage; if it's used as depth/stencil, we
		 * will set both depth and stencil aspects.  Otherwise we will view the color aspect.
		 */
		VkImageViewCreateInfo view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = images[i].image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = images[i].format,
			.subresourceRange = {
				.aspectMask = (images[i].usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0?
						VK_IMAGE_ASPECT_COLOR_BIT:
						VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
				.baseMipLevel = 0,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
		};

		res = vkCreateImageView(dev->device, &view_info, NULL, &images[i].view);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		if ((images[i].usage & VK_IMAGE_USAGE_SAMPLED_BIT))
		{
			/*
			 * If the image is going to be sampled, we can create a sampler for it as well.  A sampler
			 * specifies how to sample an image.  An image is just a glorified buffer, i.e., it's just an
			 * array, as I have said before as well.  However, the sampler is what makes using images so
			 * much more powerful.  When accessing a buffer, you can access each index individually.  With
			 * a sampler, you can access an image at non-integer indices.  The sampler then "filters" the
			 * image to provide some data for that index.
			 *
			 * The simplest example is magnification.  If you sample the image at coordinates (u+0.5,v)
			 * where u and v are integer pixel locations, then the color you get could be the average of
			 * the colors (values) at coordinates (u,v) and (u+1,v).  Vulkan uses the term `texel` to refer
			 * to these "texture" pixels.
			 *
			 * The sampler parameters are explained below:
			 *
			 * - magFilter, minFilter: what to do if asked to sample between the texels.  The options are
			 *   to take the value of the nearest texel, or interpolate between neighbors.  Think about it
			 *   as what to do if you try to zoom in or out of an image.  We'll go with interpolation,
			 *   since it's nicer.
			 * - mipmapMode: similarly, if the image has multiple mipmap levels, accessing between the
			 *   levels could either interpolate between two levels or clamp to the nearest.  We don't use
			 *   mipmaps here, so this doesn't matter, but let's tell it to interpolate anyway.
			 * - addressModeU/V/W: this specifies what happens if you access outside the image.  The
			 *   options are to:
			 *   * repeat the image as if it was a tiled to infinity in each direction,
			 *   * mirrored repeat the image as if a larger image containing the image and its mirror
			 *     was tiled to infinity in each direction,
			 *   * clamp to edge so that any access out of the image boundaries returns the value at the
			 *     closest point on the edge of the image,
			 *   * clamp to border so that any access out of the image boundaries returns a special
			 *     "border" value for the image (border value defined below),
			 *   * mirrored clamp to edge so that any access out of the image boundaries returns the value
			 *     at the closest point on the edge of a larger image that is made up of the image and its
			 *     mirror.
			 *   Each of these modes is useful in different situations.  "Repeat" is probably the most
			 *   problematic as it introduces discontinuity around the edges.  "Mirrored" solves this
			 *   problem and can add some interesting effects.  "Clamp to edge" also solves this problem,
			 *   and let's just use that.  "Clamp to border" would introduce other edges, and I imagine is
			 *   most useful for debugging.
			 * - anisotropyEnable, maxAnisotropy: whether anisotropic filtering is enabled and by how
			 *   much.  Anisotropic filtering is expensive but nice, so let's enable it.  The maximum value
			 *   for anisotropic filtering can be retrieved from the device's limits.
			 * - compareEnable, compareOp: this is used with depth images to result in a reading of 0 or 1
			 *   based on the result of a compare operation.  We are not interested in this for now.
			 * - minLod, maxLod: the level-of-detail value (mip level) gets clamped to these values.  We
			 *   are not using mipmapped images, so we'll just give 0 and 1 respectively.
			 * - borderColor: if the "clamp to border" addressing mode was selected, out-of-bound accesses
			 *   to the image would return the border color, which is set here.  Options are limited:
			 *   transparent, white and black.  Since we're using "clamp to edge" addressing, this value is
			 *   not used.
			 * - unnormalizedCoordinates: with Vulkan, you can either index the image using the
			 *   unnormalized coordinates, so that u and v span from 0 to size of the image, or you can
			 *   access the image using normalized coordinates, so that u and v span from 0 to 1.
			 *   Unnormalized coordinates can be useful in some circumstances, but normalized coordinates
			 *   lets you access the image without dealing with its sizes.  Aside from that, with
			 *   unnormalized coordinates, you are limited in the type of images you can access; only 1D
			 *   and 2D images with a single layer and single mip level are acceptable and essentially all
			 *   other features of the sampler must be disabled too.  Needless to say, we will use
			 *   normalized coordinates.
			 */
			VkSamplerCreateInfo sampler_info = {
				.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
				.magFilter = VK_FILTER_LINEAR,
				.minFilter = VK_FILTER_LINEAR,
				.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
				.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
				.anisotropyEnable = true,
				.maxAnisotropy = phy_dev->properties.limits.maxSamplerAnisotropy,
				.minLod = 0,
				.maxLod = 1,
			};

			res = vkCreateSampler(dev->device, &sampler_info, NULL, &images[i].sampler);
			tut1_error_sub_set_vkresult(&retval, res);
			if (res)
				continue;
		}

		++successful;
	}

	/*
	 * Now that you have learned all about images, we're not going to use them in this tutorial.  Please don't hate
	 * me.  There is already so much here that rendering textured images can wait.  It was not all in vein though
	 * because we would need image views on the swapchain images anyway.  Now at least you understand the
	 * properties and restrictions of the swapchain images better.
	 */

	tut1_error_set_vkresult(&retval, successful == image_count?VK_SUCCESS:VK_INCOMPLETE);
	return retval;
}

tut1_error tut7_create_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut7_buffer *buffers, uint32_t buffer_count)
{
	/* We have already seen buffer create in Tutorial 4, so we'll go over this quickly. */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	for (uint32_t i = 0; i < buffer_count; ++i)
	{
		buffers[i].buffer = NULL;
		buffers[i].buffer_mem = NULL;
		buffers[i].view = NULL;

		/*
		 * The buffer CreateInfo is much simpler than the image CreateInfo.  The only part of it we didn't see
		 * in Tutorial 4 is sharing the buffer between queue families.  The parameters for that are exactly the
		 * same as the image CreateInfo.
		 *
		 * The size of the buffer depends on its format, but let's not worry about translating the size for
		 * each possible format and lazily assume 4 bytes, which covers a lot of formats (even if
		 * overestimating them).  Naturally, doing this is not really advised.
		 */
		bool shared = buffers[i].sharing_queue_count > 1;
		VkBufferCreateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = buffers[i].size * sizeof(float),
			.usage = buffers[i].usage,
			.sharingMode = shared?VK_SHARING_MODE_CONCURRENT:VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = shared?buffers[i].sharing_queue_count:0,
			.pQueueFamilyIndices = shared?buffers[i].sharing_queues:NULL,
		};
		res = vkCreateBuffer(dev->device, &buffer_info, NULL, &buffers[i].buffer);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		VkMemoryRequirements mem_req = {0};
		vkGetBufferMemoryRequirements(dev->device, buffers[i].buffer, &mem_req);
		uint32_t mem_index = tut4_find_suitable_memory(phy_dev, dev, &mem_req,
				buffers[i].host_visible?
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (mem_index >= phy_dev->memories.memoryTypeCount)
			continue;

		VkMemoryAllocateInfo mem_info = {
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = mem_req.size,
			.memoryTypeIndex = mem_index,
		};

		res = vkAllocateMemory(dev->device, &mem_info, NULL, &buffers[i].buffer_mem);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		res = vkBindBufferMemory(dev->device, buffers[i].buffer, buffers[i].buffer_mem, 0);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/* A buffer view can only be created on uniform and storage texel buffers */
		if ((buffers[i].usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT) || (buffers[i].usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT))
		{
			VkBufferViewCreateInfo view_info = {
				.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
				.buffer = buffers[i].buffer,
				.format = buffers[i].format,
				.offset = 0,
				.range = VK_WHOLE_SIZE,
			};

			res = vkCreateBufferView(dev->device, &view_info, NULL, &buffers[i].view);
			tut1_error_sub_set_vkresult(&retval, res);
			if (res)
				continue;
		}

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == buffer_count?VK_SUCCESS:VK_INCOMPLETE);
	return retval;
}

tut1_error tut7_load_shaders(struct tut2_device *dev,
		struct tut7_shader *shaders, uint32_t shader_count)
{
	/*
	 * We already saw how to load a shader in Tutorial 3.  This function is just an array version of it.  Nothing
	 * fancy here.
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	tut1_error err;

	for (uint32_t i = 0; i < shader_count; ++i)
	{
		err = tut3_load_shader(dev, shaders[i].spirv_file, &shaders[i].shader);
		tut1_error_sub_merge(&retval, &err);
		if (!tut1_error_is_success(&err))
			continue;

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == shader_count?VK_SUCCESS:VK_INCOMPLETE);
	return retval;
}

tut1_error tut7_create_graphics_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkSurfaceFormatKHR surface_format,
		struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count, VkRenderPass *render_pass)
{
	/*
	 * To render on a screen, we need a series of stuff.  We need images to render to.  We also need to tell our
	 * graphics pipeline that we are going to use those images.  In fact, similar to how we make descriptor set and
	 * pipeline layouts to define stuff, then bind the actual sets and pipeline, we only specify what "kind" of
	 * images we will use and then bind the actual images.
	 *
	 * Vulkan uses the concept of render passes to perform the rendering.  A render pass could consist of multiple
	 * subpasses, but let's not bother with that for now.  Only thing to know is that the images used in rendering,
	 * whether it's color, depth/stencil, input etc, are called "attachments".  When creating a render pass, we
	 * define what sort of attachments it would take and what are the dependencies between the subpasses.  We are
	 * going with a single subpass, so things would be simpler.  The render pass is used to create a graphics
	 * pipeline.
	 *
	 * When we are going to actually render something, we need to bind those attachments using real images.  The
	 * construct that holds the attachments together is called a "framebuffer".  Commonly, we need color and
	 * depth/stencil attachments for rendering, so we need to provide views on them to a framebuffer.  We already
	 * have an image created by the swapchain for our color output.  We need to create the depth/stencil buffer
	 * ourselves.
	 *
	 * So I lied when I said in the end of `tut7_create_images` that we won't use that function.  For the color
	 * image, we will just create a view (which we already saw how to do in that function), and we will use that
	 * function to create our depth/stencil image and its view.
	 *
	 * If we wanted to render to an image, without presenting on the screen, here is where the difference would be,
	 * that is, we would be creating an image for the color attachment ourselves, instead of using one created by
	 * the swapchain.
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;
	tut1_error err;

	for (uint32_t i = 0; i < graphics_buffer_count; ++i)
	{
		graphics_buffers[i].color_view = NULL;
		graphics_buffers[i].depth = (struct tut7_image){0};
		graphics_buffers[i].framebuffer = NULL;
	}

	/*
	 * The format of the depth/stencil image must be one that supports depth/stencil.  At the time of this writing,
	 * the following formats in order of precision of depth are optionally available (the last in this last is
	 * always available though):
	 *
	 * - VK_FORMAT_D32_SFLOAT_S8_UINT: 32-bit float for depth, 8 bits for stencil
	 * - VK_FORMAT_D32_SFLOAT: 32-bit float for depth, no stencil
	 * - VK_FORMAT_D24_UNORM_S8_UINT: 24 bits for depth, 8 bits for stencil
	 * - VK_FORMAT_X8_D24_UNORM_PACK32: 24 bits for depth, 8 bits unused
	 * - VK_FORMAT_D16_UNORM: 16 bits for depth, no stencil
	 *
	 * We need to make sure the format is supported, so we will query from the highest precision format to the
	 * lowest and check this one by one!  We don't really care about stencil at this point, otherwise we should
	 * have checked only for the formats that also support stencil.
	 *
	 * Since we are not going to access the depth buffer from the application but only from the shaders, we should
	 * create it with optimal tiling, and therefore when querying for the supported formats, we should look for
	 * formats supported for optimal tiling.
	 */
	VkFormat depth_formats[] = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_X8_D24_UNORM_PACK32,
		VK_FORMAT_D16_UNORM,
	};
	VkFormat selected_format = VK_FORMAT_UNDEFINED;

	for (size_t i = 0; i < sizeof depth_formats / sizeof *depth_formats; ++i)
	{
		VkFormatProperties format_properties;
		vkGetPhysicalDeviceFormatProperties(phy_dev->physical_device, depth_formats[i], &format_properties);
		if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
		{
			selected_format = depth_formats[i];
			break;
		}
	}
	/*
	 * the standard says VK_FORMAT_D16_UNORM must always be available, but just for safety make sure some
	 * format was found!
	 */
	if (selected_format == VK_FORMAT_UNDEFINED)
	{
		tut1_error_set_vkresult(&retval, VK_ERROR_FEATURE_NOT_PRESENT);
		goto exit_failed;
	}

	/*
	 * Since the render buffer just defines how the attachments look like, we need only one for use with all of our
	 * swapchain images.  On the other hand, we need a different framebuffer for each swapchain image (together
	 * with its corresponding depth/stencil image).
	 *
	 * Our render pass has two attachments; the color image and the depth/stencil image.  Each of these attachments
	 * needs to be specified separately:
	 *
	 * - In the case of the color image, the format is given by `surface_format`, and in the case of the
	 *   depth/stencil image, the format is the one we just decided on above.
	 * - For now, we don't do multisampling, so the number of samples is set to 1 for both attachments.
	 * - At the beginning and end of each subpass, the render pass can either keep or clear/discard the contents of
	 *   an attachment.  We use one subpass, so this doesn't really matter, but we'll go with clearing the image at
	 *   the beginning and keeping the contents at the end.  If not specified, the default action (value 0) is to
	 *   keep the previous data at the beginning and preserve it at the end as well.
	 * - The render pass also declares a promise that the driver would find the attachment in a certain layout at
	 *   the beginning of the render pass, and that the attachment would be at a certain layout at the end.  For our
	 *   color attachment, it will start and end in the color attachment layout.  The case is similar for the
	 *   depth/stencil buffer.  We don't intend to transition them to another layout.
	 *
	 * Next, we need to declare the subpasses of the render pass.  We use only one, so this is more
	 * straightforward.  The information required here are:
	 *
	 * - pipeline bind point: whether compute or graphics pipelines are going to use this subpass.  We want
	 *   graphics now, so we'll go with that, but anyway compute is not even supported (at the time of this
	 *   writing).
	 * - Attachments: which attachments to use, and which to simply preserve.  If an attachment is neither used nor
	 *   preserved, its contents become undefined.  If the attachment is decorated with `location=X` in glsl, then
	 *   pInputAttachments[X] is used if it's an input, or pColorAttachments[X] is used if it's an output.  This is
	 *   other than the `binding=Y` decoration that specifies the buffer/image as specified by the descriptor set.
	 *   There can be multiple input and color attachments (hence the `location=X` binding required above), but
	 *   only one depth/stencil attachment.
	 *
	 * In the end, if we had multiple subpasses, their dependencies would also need to be declared.  What's nice
	 * about subpasses is that there is an automatic layout transition between subpasses (as specified by
	 * VkAttachmentReference values), and there is a whole set of rules that allow the driver to safely perform
	 * this transition before or after the subpass has actually started.  Again, we have a single subpass, so none
	 * of this matters now.
	 */
	VkAttachmentDescription render_pass_attachments[2] = {
		[0] = {
			.format = surface_format.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		[1] = {
			.format = selected_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		},
	};
	VkAttachmentReference render_pass_attachment_references[2] = {
		[0] = {
			.attachment = 0,	/* corresponds to the index in pAttachments of VkRenderPassCreateInfo */
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		[1] = {
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		},
	};
	VkSubpassDescription render_pass_subpasses[1] = {
		[0] = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &render_pass_attachment_references[0],
			.pDepthStencilAttachment = &render_pass_attachment_references[1],
		},
	};
	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = 2,
		.pAttachments = render_pass_attachments,
		.subpassCount = 1,
		.pSubpasses = render_pass_subpasses,
	};

	res = vkCreateRenderPass(dev->device, &render_pass_info, NULL, render_pass);
	tut1_error_set_vkresult(&retval, res);
	if (res)
		goto exit_failed;

	for (uint32_t i = 0; i < graphics_buffer_count; ++i)
	{
		/*
		 * The view we create for the swapchain image is very similar to the one we created in
		 * `tut7_create_images`.  We take the format from the surface format, specified when it was created (in
		 * Tutorial 6).  We already know also that the aspect of the image we are interested in is its color.
		 */
		VkImageViewCreateInfo view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = graphics_buffers[i].swapchain_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = surface_format.format,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = VK_REMAINING_MIP_LEVELS,
				.baseArrayLayer = 0,
				.layerCount = VK_REMAINING_ARRAY_LAYERS,
			},
		};

		res = vkCreateImageView(dev->device, &view_info, NULL, &graphics_buffers[i].color_view);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/*
		 * The image and view we create for the depth/stencil buffer is easily created with
		 * `tut7_create_images`.  The parameters we need to specify are as follows.
		 *
		 * We have already found a format that supports depth/stencil.
		 *
		 * As explained above, we are not going to access the depth buffer from the application, so we should
		 * create it with optimal tiling.
		 *
		 * The usage of the depth/stencil image naturally contains DEPTH_STENCIL_ATTACHMENT.  Furthermore, the
		 * image needs to be transitioned to the depth/stencil optimal layout, so it needs TRANSFER_SRC usage
		 * as well.
		 *
		 * The depth/stencil image is entirely accessed by the device, so it doesn't need to be host-visible.
		 *
		 * Multisampling is a feature for the color attachment and is irrelevant here.
		 */
		graphics_buffers[i].depth = (struct tut7_image){
			.format = selected_format,
			.extent = graphics_buffers[i].surface_size,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			.multisample = false,
			.will_be_initialized = false,
		};

		err = tut7_create_images(phy_dev, dev, &graphics_buffers[i].depth, 1);
		tut1_error_sub_merge(&retval, &err);
		if (!tut1_error_is_success(&err))
			continue;

		/*
		 * While the render pass specifies what kind of attachments are going to be present during rendering,
		 * a framebuffer holds the actual attachments.  To create a framebuffer, we need to specify a
		 * corresponding render pass, the actual attachments, and the dimensions of the framebuffer.
		 */
		VkImageView framebuffer_attachments[2] = {
			graphics_buffers[i].color_view,
			graphics_buffers[i].depth.view,
		};
		VkFramebufferCreateInfo framebuffer_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = *render_pass,
			.attachmentCount = 2,
			.pAttachments = framebuffer_attachments,
			.width = graphics_buffers[i].surface_size.width,
			.height = graphics_buffers[i].surface_size.height,
			.layers = 1,
		};

		res = vkCreateFramebuffer(dev->device, &framebuffer_info, NULL, &graphics_buffers[i].framebuffer);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == graphics_buffer_count?VK_SUCCESS:VK_INCOMPLETE);
exit_failed:
	return retval;
}

tut1_error tut7_get_presentable_queues(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkSurfaceKHR surface, uint32_t **presentable_queues, uint32_t *presentable_queue_count)
{
	/*
	 * Eventually, we need to know which queue families can be used to present our images.  In Tutorial 6, we made
	 * a quick search to find one queue for this purpose.  This function is doing the same thing, but finding all
	 * queue families.  So there is not much new here.
	 *
	 * Just a reminder that in Tutorial 2, we created a command pool for each queue family.  That's why
	 * `dev->command_pool_count` is used as the number of queue families.
	 *
	 * One could perform this check before creating the swapchain, to make sure the surface is at all compatible
	 * with the driver.  The LUNARG_swapchain validation layer actually complains about this, so TODO: move this
	 * to Tutorial 6 and call it before creating the swapchain.
	 */
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	*presentable_queues = malloc(dev->command_pool_count * sizeof **presentable_queues);
	if (*presentable_queues == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_failed;
	}
	*presentable_queue_count = 0;

	for (uint32_t i = 0; i < dev->command_pool_count; ++i)
	{
		VkBool32 supports = false;
		res = vkGetPhysicalDeviceSurfaceSupportKHR(phy_dev->physical_device, i, surface, &supports);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res || !supports)
			continue;

		(*presentable_queues)[(*presentable_queue_count)++] = i;
	}

	/* If no queue is able to present to a surface, this device/driver is of little use to us. */
	if (*presentable_queue_count == 0)
	{
		free(*presentable_queues);
		*presentable_queues = NULL;
	}

	tut1_error_set_vkresult(&retval, *presentable_queue_count == 0?VK_ERROR_INCOMPATIBLE_DRIVER:VK_SUCCESS);
exit_failed:
	return retval;
}

void tut7_free_images(struct tut2_device *dev, struct tut7_image *images, uint32_t image_count)
{
	vkDeviceWaitIdle(dev->device);

	/*
	 * Cleaning up an image, its memory and its view is pretty similar to a buffer as seen in Tutorial 4.  There is
	 * not much new here.  As always, memory allocation functions are not provided.
	 */
	for (uint32_t i = 0; i < image_count; ++i)
	{
		vkDestroyImageView(dev->device, images[i].view, NULL);
		vkDestroyImage(dev->device, images[i].image, NULL);
		vkFreeMemory(dev->device, images[i].image_mem, NULL);
		vkDestroySampler(dev->device, images[i].sampler, NULL);
	}
}

void tut7_free_buffers(struct tut2_device *dev, struct tut7_buffer *buffers, uint32_t buffer_count)
{
	vkDeviceWaitIdle(dev->device);

	/* Cleaning up a buffer, its memory and its view is pretty similar to an image */
	for (uint32_t i = 0; i < buffer_count; ++i)
	{
		vkDestroyBufferView(dev->device, buffers[i].view, NULL);
		vkDestroyBuffer(dev->device, buffers[i].buffer, NULL);
		vkFreeMemory(dev->device, buffers[i].buffer_mem, NULL);
	}
}

void tut7_free_shaders(struct tut2_device *dev, struct tut7_shader *shaders, uint32_t shader_count)
{
	vkDeviceWaitIdle(dev->device);

	/* Usual stuff */
	for (uint32_t i = 0; i < shader_count; ++i)
		tut3_free_shader(dev, shaders[i].shader);
}

void tut7_free_graphics_buffers(struct tut2_device *dev, struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count,
		VkRenderPass render_pass)
{
	vkDeviceWaitIdle(dev->device);

	/* Same old, same old */
	for (uint32_t i = 0; i < graphics_buffer_count; ++i)
	{
		tut7_free_images(dev, &graphics_buffers[i].depth, 1);
		if (graphics_buffers[i].color_view)
			vkDestroyImageView(dev->device, graphics_buffers[i].color_view, NULL);

		vkDestroyFramebuffer(dev->device, graphics_buffers[i].framebuffer, NULL);
	}

	vkDestroyRenderPass(dev->device, render_pass, NULL);
}
