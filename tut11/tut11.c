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
#include "tut11.h"

static tut1_error create_render_pass(struct tut2_device *dev, VkFormat color_format, VkFormat depth_format, VkRenderPass *render_pass,
		enum tut11_render_pass_load_op keeps_contents, enum tut11_make_depth_buffer has_depth)
{
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/* Create the render pass containing both attachments, similar to tut7_create_graphics_buffers. */
	VkAttachmentDescription render_pass_attachments[2] = {
		[0] = {
			.format = color_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = keeps_contents?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		},
		[1] = {
			.format = depth_format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = keeps_contents?VK_ATTACHMENT_LOAD_OP_LOAD:VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		},
	};
	VkAttachmentReference render_pass_attachment_references[2] = {
		[0] = {
			.attachment = 0,
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
			.pDepthStencilAttachment = has_depth?&render_pass_attachment_references[1]:NULL,
		},
	};
	VkRenderPassCreateInfo render_pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount = has_depth?2:1,
		.pAttachments = render_pass_attachments,
		.subpassCount = 1,
		.pSubpasses = render_pass_subpasses,
	};

	res = vkCreateRenderPass(dev->device, &render_pass_info, NULL, render_pass);
	tut1_error_set_vkresult(&retval, res);

	return retval;
}

tut1_error tut11_create_offscreen_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkFormat format,
		struct tut11_offscreen_buffers *offscreen_buffers, uint32_t offscreen_buffer_count, VkRenderPass *render_pass,
		enum tut11_render_pass_load_op keeps_contents, enum tut11_make_depth_buffer has_depth)
{
	/*
	 * This is similar to tut7_create_graphics_buffers, but is more tailored towards off-screen rendering.  To that
	 * end, it creates a color image, a depth image, a render pass (unique among all offscreen_buffers) and a
	 * framebuffer.  The depth buffer is optional, in case this is an off-screen post-processing for example.
	 * Additionally, unlike tut7_create_graphics_buffers it doesn't assume that the contents of the color image
	 * should be cleared on load!
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;
	tut1_error err;

	for (uint32_t i = 0; i < offscreen_buffer_count; ++i)
	{
		offscreen_buffers[i].color= (struct tut7_image){0};
		offscreen_buffers[i].depth = (struct tut7_image){0};
		offscreen_buffers[i].framebuffer = NULL;
	}

	/* Choose a supported format for depth/stencil attachment */
	VkFormat depth_format = tut7_get_supported_depth_stencil_format(phy_dev);

	/* Render pass */
	retval = create_render_pass(dev, format, depth_format, render_pass, keeps_contents, has_depth);
	if (!tut1_error_is_success(&retval))
		goto exit_failed;

	for (uint32_t i = 0; i < offscreen_buffer_count; ++i)
	{
		/*
		 * Create an image for color attachment, an image for depth attachment, and a framebuffer holding them
		 * together.  Again, somewhat similar to tut7_create_graphics_buffers.
		 */

		/*
		 * Here, we create an off-screen image to render into.  In essence, what we are doing here is creating
		 * an image just like the images the swapchain makes for us.  Well, not exactly like it!  The swapchain
		 * images only necessarily support the COLOR_ATTACHMENT usage (remember supportedUsageFlags of
		 * VkSurfaceCapabilitiesKHR?  No?  See `tut6_get_swapchain`), but here we want to later be able to
		 * SAMPLE the image for post-processing.
		 */
		offscreen_buffers[i].color = (struct tut7_image){
			.format = format,
			.extent = offscreen_buffers[i].surface_size,
			.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT		/* Use as target of rendering */
				| VK_IMAGE_USAGE_SAMPLED_BIT,			/* Use as input in post-processing */
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,			/* Used in fragment shader stage in both rendering and post-processing */
			.make_view = true,
		};
		err = tut7_create_images(phy_dev, dev, &offscreen_buffers[i].color, 1);
		tut1_error_sub_merge(&retval, &err);
		if (!tut1_error_is_success(&err))
			continue;

		if (has_depth)
		{
			/* The depth/stencil buffer is exactly like in tut7_create_graphics_buffers. */
			offscreen_buffers[i].depth = (struct tut7_image){
				.format = depth_format,
				.extent = offscreen_buffers[i].surface_size,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.make_view = true,
			};

			err = tut7_create_images(phy_dev, dev, &offscreen_buffers[i].depth, 1);
			tut1_error_sub_merge(&retval, &err);
			if (!tut1_error_is_success(&err))
				continue;
		}

		/* The framebuffer is also exactly like in tut7_create_graphics_buffers. */
		VkImageView framebuffer_attachments[2] = {
			offscreen_buffers[i].color.view,
			offscreen_buffers[i].depth.view,
		};
		VkFramebufferCreateInfo framebuffer_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = *render_pass,
			.attachmentCount = has_depth?2:1,
			.pAttachments = framebuffer_attachments,
			.width = offscreen_buffers[i].surface_size.width,
			.height = offscreen_buffers[i].surface_size.height,
			.layers = 1,
		};

		res = vkCreateFramebuffer(dev->device, &framebuffer_info, NULL, &offscreen_buffers[i].framebuffer);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == offscreen_buffer_count?VK_SUCCESS:VK_INCOMPLETE);
exit_failed:
	return retval;
}

tut1_error tut11_create_graphics_buffers(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkFormat format,
		struct tut7_graphics_buffers *graphics_buffers, uint32_t graphics_buffer_count, VkRenderPass *render_pass,
		enum tut11_render_pass_load_op keeps_contents, enum tut11_make_depth_buffer has_depth)
{
	/*
	 * This function is exactly like tut7_create_graphics_buffers, but with the new configuration arguments
	 * keeps_contents and has_depth.
	 *
	 * The difference between this function and tut11_create_offscreen_buffers is essentially that the color image
	 * is taken from swapchain here, but created from scratch there.
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

	/* Supported depth/stencil format */
	VkFormat depth_format = tut7_get_supported_depth_stencil_format(phy_dev);;

	/* Render pass */
	retval = create_render_pass(dev, format, depth_format, render_pass, keeps_contents, has_depth);
	if (!tut1_error_is_success(&retval))
		goto exit_failed;

	for (uint32_t i = 0; i < graphics_buffer_count; ++i)
	{
		/* View on swapchain images + depth buffer */
		VkImageViewCreateInfo view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = graphics_buffers[i].swapchain_image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = format,
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

		if (has_depth)
		{
			graphics_buffers[i].depth = (struct tut7_image){
				.format = depth_format,
				.extent = graphics_buffers[i].surface_size,
				.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				.make_view = true,
				.multisample = false,
				.will_be_initialized = false,
			};

			err = tut7_create_images(phy_dev, dev, &graphics_buffers[i].depth, 1);
			tut1_error_sub_merge(&retval, &err);
			if (!tut1_error_is_success(&err))
				continue;
		}

		/* The framebuffer */
		VkImageView framebuffer_attachments[2] = {
			graphics_buffers[i].color_view,
			graphics_buffers[i].depth.view,
		};
		VkFramebufferCreateInfo framebuffer_info = {
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = *render_pass,
			.attachmentCount = has_depth?2:1,
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

void tut11_free_offscreen_buffers(struct tut2_device *dev, struct tut11_offscreen_buffers *offscreen_buffers, uint32_t graphics_buffer_count,
		VkRenderPass render_pass)
{
	vkDeviceWaitIdle(dev->device);

	/* Same old, same old */
	for (uint32_t i = 0; i < graphics_buffer_count; ++i)
	{
		tut7_free_images(dev, &offscreen_buffers[i].color, 1);
		tut7_free_images(dev, &offscreen_buffers[i].depth, 1);

		vkDestroyFramebuffer(dev->device, offscreen_buffers[i].framebuffer, NULL);
	}

	vkDestroyRenderPass(dev->device, render_pass, NULL);
}

void tut11_free_graphics_buffers(struct tut2_device *dev, struct tut7_graphics_buffers *graphics_buffers,uint32_t graphics_buffer_count,
		VkRenderPass render_pass)
{
	tut7_free_graphics_buffers(dev, graphics_buffers, graphics_buffer_count, render_pass);
}
