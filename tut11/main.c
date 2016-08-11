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
#include "tut11_render.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

/*
 * In this tutorial, we'll do some post-processing.  This is both an exercise in off-screen rendering (which is quite
 * similar to on-screen rendering), and synchronization between multiple queue submissions.
 *
 * Essentially what we will do is render a scene (the same as in Tutorial 8) into an off-screen buffer, then use that
 * as an input image to render the final image.  More in details, we use one pipeline to render into an image (not one
 * created by the swapchain, but one we create here), whose layout is COLOR_ATTACHMENT_OPTIMAL, and after the draw
 * call, we change the layout to SHADER_READ_ONLY_OPTIMAL.  We submit this to a queue for rendering.  We then use
 * another pipeline whose descriptor set contains the previous image to do the post-processing.  Since the two
 * submissions are not necessarily executed in order by the driver, we use a semaphore to make sure the first finishes
 * before the second starts.
 */

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

enum
{
	BUFFER_TRANSFORMATION = 0,
	BUFFER_VERTICES = 1,
	BUFFER_INDICES = 2,
};
enum
{
	SHADER_RENDER_VERTEX = 0,
	SHADER_RENDER_FRAGMENT = 1,
	SHADER_POSTPROC_VERTEX = 2,
	SHADER_POSTPROC_FRAGMENT = 3,
};
struct render_data
{
	struct objects
	{
		struct vertex
		{
			float pos[3];
			float color[3];
			float tex[2];
		} vertices[3 + 4];		/* In this tutorial, we draw a triangle.  A quad (the screen) is used for post-processing */

		uint16_t indices[3 + 4];	/* Indexed drawing (3 for triangle, 4 for quad) */
	} objects;

	struct transformation
	{
		float mat[4][4];
	} transformation;

	/* Push constants used here are described below */
	struct push_constants
	{
		float pixel_size;
		float hue_levels;
		float saturation_levels;
		float intensity_levels;
	} push_constants;

	/* Actual objects used in this tutorial */
	struct tut7_buffer buffers[3];
	struct tut7_shader shaders[4];
	struct tut7_graphics_buffers *gbuffers;
	struct tut11_offscreen_buffers obuffers;

	/* For rendering: two stages of rendering, so two sets of everything! */
	VkRenderPass render_render_pass;
	struct tut8_layout render_layout;
	struct tut8_pipeline render_pipeline;
	VkDescriptorSet render_desc_set;

	VkRenderPass postproc_render_pass;
	struct tut8_layout postproc_layout;
	struct tut8_pipeline postproc_pipeline;
	VkDescriptorSet postproc_desc_set;
};

static tut1_error allocate_render_data(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, struct tut7_render_essentials *essentials, struct render_data *render_data)
{
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/* Buffers */
	render_data->buffers[BUFFER_TRANSFORMATION] = (struct tut7_buffer){
		.size = sizeof render_data->transformation,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.host_visible = true,
	};

	render_data->buffers[BUFFER_VERTICES] = (struct tut7_buffer){
		.size = sizeof render_data->objects.vertices,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.host_visible = false,
	};

	render_data->buffers[BUFFER_INDICES] = (struct tut7_buffer){
		.size = sizeof render_data->objects.indices,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.host_visible = false,
	};

	retval = tut7_create_buffers(phy_dev, dev, render_data->buffers, 3);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create vertex, index and transformation buffers\n");
		return retval;
	}

	/* The objects */
	render_data->objects = (struct objects){
		.vertices = {
			/* The triangle to be rendered (texture coordinates unused) */
			[0] = (struct vertex){ .pos = {-0.5,  0.0, 0.0}, .color = {1.0, 0.6, 0.4}, .tex = {0, 0}, },
			[1] = (struct vertex){ .pos = { 0.1,  0.7, 0.0}, .color = {0.2, 1.0, 0.3}, .tex = {0, 0}, },
			[2] = (struct vertex){ .pos = { 0.3, -0.7, 0.0}, .color = {0.3, 0.1, 1.0}, .tex = {0, 0}, },

			/* The whole screen to be post-processed (color unused) */
			[3] = (struct vertex){ .pos = { 1.0,  1.0, 0.0}, .color = {1.0, 1.0, 1.0}, .tex = {1, 0}, },
			[4] = (struct vertex){ .pos = { 1.0, -1.0, 0.0}, .color = {1.0, 1.0, 1.0}, .tex = {1, 1}, },
			[5] = (struct vertex){ .pos = {-1.0,  1.0, 0.0}, .color = {1.0, 1.0, 1.0}, .tex = {0, 0}, },
			[6] = (struct vertex){ .pos = {-1.0, -1.0, 0.0}, .color = {1.0, 1.0, 1.0}, .tex = {0, 1}, },
		},
		.indices = {
			0, 1, 2,	/* The triangle */
			3, 4, 5, 6,	/* The quad */
		},
	};

	/* Transformation matrix, assuming it was made based on some calculation */
	render_data->transformation = (struct transformation){
		.mat = {
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{0, 0, 0, 1},
		},
	};

	/* Fill and copy buffers */
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_TRANSFORMATION], &render_data->transformation, sizeof render_data->transformation, "transformation");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut10_render_init_buffer(phy_dev, dev, essentials, &render_data->buffers[BUFFER_VERTICES], render_data->objects.vertices, "vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut10_render_init_buffer(phy_dev, dev, essentials, &render_data->buffers[BUFFER_INDICES], render_data->objects.indices, "index");
	if (!tut1_error_is_success(&retval))
		return retval;

	/* Shaders */
	render_data->shaders[SHADER_RENDER_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut11_render.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_RENDER_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut11_render.frag.spv",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	render_data->shaders[SHADER_POSTPROC_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut11_postproc.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_POSTPROC_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut11_postproc.frag.spv",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	retval = tut7_load_shaders(dev, render_data->shaders, 4);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not load the shaders (expected location: ../shaders)\n");
		return retval;
	}

	/* Graphics buffers */

	/*
	 * The situation here is a bit different from before.  So far, we had one set of images to render to (the ones
	 * taken from the swapchain) and for each we had a corresponding depth buffer.  A render pass was created to
	 * show this structure and for each of those images, a framebuffer was created to give actual references to
	 * these images.  `tut7_create_graphics_buffers` did all of this.
	 *
	 * Here, however, we have one image to render to, with a corresponding depth buffer.  We need a render pass and
	 * framebuffer for this rendering.  The post-processing phase renders to the swapchain images, but it doesn't
	 * need depth buffers.  tut7_create_graphics_buffers made too many assumptions about what attachments we need,
	 * so its replaced with two variants, tut11_create_offscreen_buffers and tut11_create_graphics_buffers.  They
	 * are quite similar to tut7_create_graphics_buffers, though.
	 */
	render_data->gbuffers = malloc(essentials->image_count * sizeof *render_data->gbuffers);
	for (uint32_t i = 0; i < essentials->image_count; ++i)
		render_data->gbuffers[i] = (struct tut7_graphics_buffers){
			.surface_size = swapchain->surface_caps.currentExtent,
			.swapchain_image = essentials->images[i],
		};
	render_data->obuffers = (struct tut11_offscreen_buffers){
		.surface_size = swapchain->surface_caps.currentExtent,
	};

	retval = tut11_create_offscreen_buffers(phy_dev, dev, swapchain->surface_format.format, &render_data->obuffers, 1,
			&render_data->render_render_pass, TUT11_CLEAR, TUT11_WITH_DEPTH);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create off-screen buffers\n");
		return retval;
	}
	retval = tut11_create_graphics_buffers(phy_dev, dev, swapchain->surface_format.format, render_data->gbuffers, essentials->image_count,
			&render_data->postproc_render_pass, TUT11_CLEAR, TUT11_WITHOUT_DEPTH);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create graphics buffers\n");
		return retval;
	}

	/* Depth/stencil image transition */
	retval = tut8_render_transition_images(dev, essentials, &render_data->obuffers.depth, 1,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, "depth");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*******************
	 * THE RENDER PART *
	 *******************/

	/* Layouts */

	/*
	 * The resources used here are the transformation matrix, vertex and fragment shaders.
	 */
	struct tut8_resources resources = {
		.buffers = render_data->buffers,
		.buffer_count = 1,
		.shaders = &render_data->shaders[SHADER_RENDER_VERTEX],
		.shader_count = 2,
		.render_pass = render_data->render_render_pass,
	};
	render_data->render_layout = (struct tut8_layout){
		.resources = &resources,
	};
	/*
	 * Note: transformation matrix: binding 0.
	 */
	retval = tut8_make_graphics_layouts(dev, &render_data->render_layout, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create descriptor set or pipeline layouts for rendering\n");
		return retval;
	}

	/* Pipeline */
	VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof *render_data->objects.vertices,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	/* Note: only using position and color for rendering our triangle */
	VkVertexInputAttributeDescription vertex_attributes[2] = {
		[0] = {
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = 0,
		},
		[1] = {
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = sizeof(float[3]),
		},
	};
	render_data->render_pipeline = (struct tut8_pipeline){
		.layout = &render_data->render_layout,
		.vertex_input_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_binding,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = vertex_attributes,
		},
		.input_assembly_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		},
		.tessellation_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		},
		.thread_count = 1,
	};

	retval = tut8_make_graphics_pipelines(dev, &render_data->render_pipeline, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create graphics pipeline for rendering\n");
		return retval;
	}

	/* Descriptor Set */
	VkDescriptorSetAllocateInfo set_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = render_data->render_pipeline.set_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &render_data->render_layout.set_layout,
	};
	res = vkAllocateDescriptorSets(dev->device, &set_info, &render_data->render_desc_set);
	retval = TUT1_ERROR_NONE;
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Could not allocate descriptor set from pool for rendering\n");
		return retval;
	}

	VkDescriptorBufferInfo set_write_buffer_info = {
		.buffer = render_data->buffers[BUFFER_TRANSFORMATION].buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};
	VkWriteDescriptorSet set_write[1] = {
		[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->render_desc_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &set_write_buffer_info,
		},
	};
	vkUpdateDescriptorSets(dev->device, 1, set_write, 0, NULL);

	/*********************
	 * THE POSTPROC PART *
	 *********************/

	/* Layouts */

	/*
	 * The resources used here are the vertex and fragment shaders only.
	 */

	/*
	 * Let's use some push constants for post-processing too, why not!  The post-processing I did here is to
	 * pixelify the output and reduce the number of colors, so it looks like one of those 8-bit games.  The
	 * parameters are:
	 *
	 * - pixel size (N): the image is divided in NxN squares with the same color inside (averaged from the source),
	 * - hue levels (H), saturation levels (S) and intensity levels (L): each color is converted to HSL, and its
	 *   hue, saturation and lightness are snapped to the closest value allowed by levels.  For example, allowed
	 *   levels for hue would be 0, 1/H, 2/H, ..., 1.
	 *
	 * So, in total, we are going to have 4 `float`s to send as push constants.  This is only sent to the
	 * post-processing part though.
	 */
	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof render_data->push_constants,
	};
	/*
	 * The post-processing shaders don't need a transformation matrix, so we don't have to provide that buffer
	 * here.  It uses the off-screen image as input though!
	 */
	resources = (struct tut8_resources){
		.images = &render_data->obuffers.color,
		.image_count = 1,
		.shaders = &render_data->shaders[SHADER_POSTPROC_VERTEX],
		.shader_count = 2,
		.push_constants = &push_constant_range,
		.push_constant_count = 1,
		.render_pass = render_data->postproc_render_pass,
	};
	render_data->postproc_layout = (struct tut8_layout){
		.resources = &resources,
	};
	retval = tut8_make_graphics_layouts(dev, &render_data->postproc_layout, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create descriptor set or pipeline layouts\n");
		return retval;
	}

	/* Pipeline */
	vertex_binding = (VkVertexInputBindingDescription){
		.binding = 0,
		.stride = sizeof *render_data->objects.vertices,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	/* Note: only using position and texture coordinates for post-processing */
	vertex_attributes[0] = (VkVertexInputAttributeDescription){
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = 0,
	};
	vertex_attributes[1] = (VkVertexInputAttributeDescription){
		/*
		 * So far, we always had position at location 0, color at location 1 and when applicable,
		 * texture coordinates at location 2.  Here, we don't use the color input and we have two
		 * choices.  One choice would be to set the texture coordinates at location 1; this could be
		 * slightly more efficient.  The other choice would be to set the texture coordinates at
		 * location 2 as usual; this would be more uniform among our shaders.  Just to show it's not
		 * necessary for the locations to be sequential, let's go with the second option.
		 */
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = sizeof(float[3]) + sizeof(float[3]),
	};
	render_data->postproc_pipeline = (struct tut8_pipeline){
		.layout = &render_data->postproc_layout,
		.vertex_input_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_binding,
			.vertexAttributeDescriptionCount = 2,
			.pVertexAttributeDescriptions = vertex_attributes,
		},
		.input_assembly_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	/* Triangle strip for the quad */
		},
		.tessellation_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		},
		.thread_count = 1,
	};

	retval = tut8_make_graphics_pipelines(dev, &render_data->postproc_pipeline, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create graphics pipeline for post-processing\n");
		return retval;
	}

	/* Descriptor Set */
	set_info = (VkDescriptorSetAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = render_data->postproc_pipeline.set_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &render_data->postproc_layout.set_layout,
	};
	res = vkAllocateDescriptorSets(dev->device, &set_info, &render_data->postproc_desc_set);
	retval = TUT1_ERROR_NONE;
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Could not allocate descriptor set from pool for post-processing\n");
		return retval;
	}

	VkDescriptorImageInfo set_write_image_info = {
		.sampler = render_data->obuffers.color.sampler,
		.imageView = render_data->obuffers.color.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	set_write[0] = (VkWriteDescriptorSet){
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = render_data->postproc_desc_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = &set_write_image_info,
	};
	vkUpdateDescriptorSets(dev->device, 1, set_write, 0, NULL);

	return retval;
}

static void free_render_data(struct tut2_device *dev, struct tut7_render_essentials *essentials, struct render_data *render_data)
{
	vkDeviceWaitIdle(dev->device);

	tut8_free_pipelines(dev, &render_data->render_pipeline, 1);
	tut8_free_layouts(dev, &render_data->render_layout, 1);
	tut8_free_pipelines(dev, &render_data->postproc_pipeline, 1);
	tut8_free_layouts(dev, &render_data->postproc_layout, 1);

	tut7_free_buffers(dev, render_data->buffers, 3);
	tut7_free_shaders(dev, render_data->shaders, 4);

	tut11_free_offscreen_buffers(dev, &render_data->obuffers, 1, render_data->render_render_pass);
	tut11_free_graphics_buffers(dev, render_data->gbuffers, essentials->image_count, render_data->postproc_render_pass);

	free(render_data->gbuffers);
}

static uint64_t get_time_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LLU + ts.tv_nsec;
}

static int prerecord(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct render_data *render_data, VkCommandBuffer cmd_buffer)
{
	/*
	 * Note: if you are reading from top to bottom, you may want to skip to `render_loop()` and come back when you
	 * reach the usage of this function.
	 */

	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/* This function prerecords the triangle rendering stage using the off-screen buffers. */
	vkResetCommandBuffer(cmd_buffer, 0);
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	res = vkBeginCommandBuffer(cmd_buffer, &begin_info);
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Couldn't even begin recording a command buffer\n");
		return -1;
	}

	/* Transition the image from SHADER_READ_ONLY_OPTIMAL to COLOR_ATTACHMENT_OPTIMAL for rendering */
	VkImageMemoryBarrier image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = render_data->obuffers.color.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	/* All READs before the pipeline must be done before all WRITEs anywhere. */
	vkCmdPipelineBarrier(cmd_buffer,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */

	/* The actual rendering */

	/* Clear both the color and depth/stencil buffers */
	VkClearValue clear_values[2] = {
		{ .color = { .float32 = {0.1, 0.1, 0.1, 255}, }, },
		{ .depthStencil = { .depth = -1000, }, },
	};
	VkRenderPassBeginInfo pass_info = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = render_data->render_render_pass,
		.framebuffer = render_data->obuffers.framebuffer,
		.renderArea = {
			.offset = { .x = 0, .y = 0, },
			.extent = render_data->obuffers.surface_size,
		},
		.clearValueCount = 2,
		.pClearValues = clear_values,
	};

	/* Start render pass as inline recording */
	vkCmdBeginRenderPass(cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

	/* Bind everything */
	vkCmdBindPipeline(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_data->render_pipeline.pipeline);
	vkCmdBindDescriptorSets(cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			render_data->render_layout.pipeline_layout, 0, 1, &render_data->render_desc_set, 0, NULL);
	VkDeviceSize vertices_offset = 0;
	vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &render_data->buffers[BUFFER_VERTICES].buffer, &vertices_offset);
	vkCmdBindIndexBuffer(cmd_buffer, render_data->buffers[BUFFER_INDICES].buffer, 0, VK_INDEX_TYPE_UINT16);

	/* Dynamic pipeline states */
	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = WINDOW_WIDTH,
		.height = WINDOW_HEIGHT,
		.minDepth = 0,
		.maxDepth = 1,
	};
	vkCmdSetViewport(cmd_buffer, 0, 1, &viewport);

	VkRect2D scissor = {
		.offset = { .x = 0, .y = 0, },
		.extent = render_data->obuffers.surface_size,
	};
	vkCmdSetScissor(cmd_buffer, 0, 1, &scissor);

	/* Draw: the triangle indices are the first three in the index buffer. */
	vkCmdDrawIndexed(cmd_buffer, 3, 1, 0, 0, 0);

	/* Finished drawing */
	vkCmdEndRenderPass(cmd_buffer);

	/* Transition the image back from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL for post-processing */
	image_barrier = (VkImageMemoryBarrier){
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = render_data->obuffers.color.image,
		.subresourceRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};

	/* All WRITEs anywhere must be done before all READs after the pipeline. */
	vkCmdPipelineBarrier(cmd_buffer,
			VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,			/* no flags */
			0, NULL,		/* no memory barriers */
			0, NULL,		/* no buffer barriers */
			1, &image_barrier);	/* our image transition */

	vkEndCommandBuffer(cmd_buffer);

	return 0;
}

static void render_loop(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut6_swapchain *swapchain)
{
	int res;
	tut1_error retval = TUT1_ERROR_NONE;
	struct tut7_render_essentials essentials;

	struct render_data render_data = { .gbuffers = NULL, };

	/* Allocate render essentials. */
	res = tut7_render_get_essentials(&essentials, phy_dev, dev, swapchain);
	if (res)
		goto exit_bad_essentials;

	/*
	 * We need two command buffers in this tutorial.  One is used to do the rendering, the other used to do the
	 * pre-processing.  tut2_get_commands allocated as many command buffers as there were queues.  Now, that is
	 * wasteful if there are many unused queues, and limiting if there are not enough queues.  In our case, we need
	 * two command buffers to submit to a single queue.  We could also submit to two queues, and the result should
	 * be the same.  To prove a point, let's submit to two different queues.
	 *
	 * tut7_render_get_essentials found a presentable queue for us to record our command buffer and submit it.
	 * Unfortunately, it didn't tell us which queue family it got the command buffer from, so let's do a quick
	 * lookup ourselves.  Knowing that tut7_render_get_essentials got the first queue and command buffer of the
	 * queue family and put it in `essentials`, we can find the same queue and take the second queue and command
	 * buffer instead.
	 */
	uint32_t *presentable_queues = NULL;
	uint32_t presentable_queue_count = 0;

	retval = tut7_get_presentable_queues(phy_dev, dev, swapchain->surface, &presentable_queues, &presentable_queue_count);
	if (!tut1_error_is_success(&retval) || presentable_queue_count == 0)
	{
		printf("No presentable queue families.  You should have got this error in tut7_render_get_essentials before.\n");
		free(presentable_queues);
		goto exit_bad_essentials;
	}

	if (dev->command_pools[presentable_queues[0]].queue_count < 2)
	{
		printf("Not enough queues in the presentable queue family %u\n", presentable_queues[0]);
		free(presentable_queues);
		goto exit_bad_essentials;
	}

	/* Note again that queues[0] and buffers[0] is already referred to in `essentials` */
	VkQueue offscreen_queue = dev->command_pools[presentable_queues[0]].queues[1];
	VkCommandBuffer offscreen_cmd_buffer = dev->command_pools[presentable_queues[0]].buffers[1];

	free(presentable_queues);

	/* Allocate buffers and load shaders for the rendering in this tutorial. */
	retval = allocate_render_data(phy_dev, dev, swapchain, &essentials, &render_data);
	if (!tut1_error_is_success(&retval))
		goto exit_bad_render_data;

	/*
	 * In the previous tutorials, we have re-recorded the command buffer on every frame.  This was not always
	 * necessary, especially when there was nothing changing in the command buffer!  In this tutorial, we have our
	 * triangle rendering which is static, so let's pre-build it.  The post-processing changes with the push
	 * constant changes, so we have to rebuild that one every time.
	 */

	/*
	 * The post-processing stage of rendering needs the off-screen image as input, so the layout of that image
	 * should be SHADER_READ_ONLY_OPTIMAL when we are not rendering to it.  Naturally, during rendering the layout
	 * of it should be COLOR_ATTACHMENT_OPTIMAL.  Let's transition the image to SHADER_READ_ONLY_OPTIMAL first, so
	 * that the command buffer can always transition away from that layout (instead of having to remember what was
	 * the previous layout).
	 */
	retval = tut8_render_transition_images(dev, &essentials, &render_data.obuffers.color, 1,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_ASPECT_COLOR_BIT, "off-screen color");
	if (!tut1_error_is_success(&retval))
		goto exit_bad_render_data;

	if (prerecord(phy_dev, dev, &essentials, &render_data, offscreen_cmd_buffer))
		goto exit_bad_prerecord;

	/*
	 * We have two command buffers to submit for each frame.  The driver may choose to run the command buffers in
	 * parallel or out of order, even if they were sent to the same queue.  Therefore, we need to make sure our
	 * post-processing submission waits until the rendering submission is actually finished.  What better way to
	 * do this than to use a semaphore!  This ordering semaphore can be signalled by the first submission and
	 * waited on by the second submission.
	 *
	 * But wait, there is more!  Once the second submission is sent, we loop back and send the first submission
	 * again.  There is a fence on the second submission, so we could have waited on that fence to make sure the
	 * second submission is finished before sending the first submission again.  This could introduce a tiny bit of
	 * a drop in the FPS, as the GPU waits for the CPU to submit the first submission (an expensive operation on
	 * its own).  Alternatively, we can delay the wait on the fence as late as possible, which has been what we did
	 * so far, and instead use another semaphore to signal the end of the second submission.
	 *
	 * Since we delayed waiting for the post-processing fence as much as possible, we must make sure the render
	 * command buffer is also not re-sent while it is still being rendered.  Another fence it is.
	 *
	 * Here is a diagram of how all these synchronization mechanisms are being used:
	 *
	 *                               CPU                               GPU                           Possible parallel execution on GPU
	 *                /    Send render command buffer ------
	 *               |                                      \
	 * First Render <    Record postproc command buffer    Execute render command buffer
	 *  (No waits)   |               BUSY                              BUSY
	 *               |    Send postproc command buffer ----------------BUSY-------------------------------
	 *                \                                                BUSY                               \                   ------------
	 *                                                                 BUSY                                \                 /            |
	 *                /      Wait for render fence                     BUSY                              Wait on render semaphore         |
	 *               |                                                 BUSY                                                               |
	 *               |                                        Signal render semaphore -----------------                                   |
	 *               |                            ------------- Signal render fence                    \                                  |
	 *               |                           /                                                     Execute postproc command buffer    |
	 *               |     Send render command buffer --------                                                      BUSY                  |
	 *               |                                        \                                                     BUSY                  |
	 *     Loop     <       Wait for postproc fence         Wait on postproc semaphore                              BUSY                  |
	 *               |                                                                                              BUSY                  |
	 *               |                                                               -------------------- Signal postproc semaphore       |
	 *               |                           -----------------------------------/---------------------- Signal postproc fence         |
	 *               |                          /          Execute render command buffer                                                  |
	 *               |   Record postproc command buffer                BUSY                                                               |
	 *               |               BUSY                              BUSY                                                               |
	 *                \   Send postproc command buffer ----------------BUSY---------------------------------------------------------------
	 *
	 *
	 * Simple!  (I didn't even include the semaphores used for presentation)
	 *
	 * I'd like you to pay attention to two points though while going through the spaghetti above again:
	 *
	 * 1. The post-processing command buffer is being recorded while the GPU is likely busy, so this is some
	 * parallelization win.  I say likely because if the render command buffer is tiny, such as in our case (just a
	 * triangle), the render command buffer could finish sooner, but the general idea is that naturally the
	 * rendering takes time.
	 * 2. The GPU is always busy.  The render command buffer signals the post-processing command buffer and vice
	 * versa.  Again, in our case the work is so simple that the GPU may actually be too quick for the CPU, but
	 * also again, this is generally speaking.
	 *
	 * Just for comparison, compare the scheme above with the simpler version below, especially in terms of the
	 * last point I just mentioned:
	 *
	 *                               CPU                               GPU                           Possible parallel execution on GPU
	 *                                             ---------------------------------------------------------------------------------------
	 *                                            /                                                                                       |
	 *                /    Send render command buffer ------                      *** GPU NOT BUSY ***                                    |
	 *               |                                      \                                                                             |
	 *               |   Record postproc command buffer    Execute render command buffer                                                  |
	 *               |               BUSY                              BUSY                                                               |
	 *               |    Send postproc command buffer ----------------BUSY-------------------------------                                |
	 *               |                                                 BUSY                               \                               |
	 *     Loop     <        Wait for postproc fence                   BUSY                              Wait on render semaphore         |
	 *               |                                                 BUSY                                                               |
	 *               |                                        Signal render semaphore -----------------                                   |
	 *               |                                                                                 \                                  |
	 *               |                                                                                 Execute postproc command buffer    |
	 *               |                                                                                              BUSY                  |
	 *                \                                                                                     Signal postproc fence --------
	 */
	VkSemaphore wait_render_sem = NULL, wait_postproc_sem = NULL;
	VkFence offscreen_fence = NULL;

	VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
	};
	res = vkCreateSemaphore(dev->device, &sem_info, NULL, &wait_render_sem);
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Failed to create wait-render semaphore\n");
		goto exit_bad_semaphore;
	}
	res = vkCreateSemaphore(dev->device, &sem_info, NULL, &wait_postproc_sem);
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Failed to create wait-post-process semaphore\n");
		goto exit_bad_semaphore;
	}

	VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
	};
	res = vkCreateFence(dev->device, &fence_info, NULL, &offscreen_fence);
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Failed to create fence\n");
		goto exit_bad_fence;
	}

	bool first_submission = true;

	uint64_t animation_time = get_time_ns();

	unsigned int frames = 0;
	time_t before = time(NULL);

	/* Process events from SDL and render.  If process_events returns non-zero, it signals application exit. */
	while (process_events() == 0)
	{
		time_t now = time(NULL);
		if (now != before)
		{
			printf("%lds: %u frames\n", now - before, frames);
			frames = 0;
			before = now;
		}
		++frames;

		if (!first_submission)
		{
			res = vkWaitForFences(dev->device, 1, &offscreen_fence, true, 1000000000);
			tut1_error_set_vkresult(&retval, res);
			if (res)
			{
				tut1_error_printf(&retval, "Wait for fence failed\n");
				break;
			}
		}
		/*
		 * Right from the start, submit the prerecorded command buffer for rendering.  Wait on the
		 * post-processing semaphore and signal the rendering semaphore.  Note that semaphores are unsignalled
		 * when created, so to make sure we don't deadlock, we shouldn't wait on the semaphore on the first
		 * submission.
		 */
		VkPipelineStageFlags wait_sem_stages[1] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
		VkSubmitInfo submit_info = {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = first_submission?0:1,
			.pWaitSemaphores = &wait_postproc_sem,
			.pWaitDstStageMask = wait_sem_stages,
			.commandBufferCount = 1,
			.pCommandBuffers = &offscreen_cmd_buffer,
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &wait_render_sem,
		};
		res = vkResetFences(dev->device, 1, &offscreen_fence);
		tut1_error_set_vkresult(&retval, res);
		if (res)
		{
			tut1_error_printf(&retval, "Failed to reset fence\n");
			break;
		}
		vkQueueSubmit(offscreen_queue, 1, &submit_info, offscreen_fence);
		first_submission = false;

		/*
		 * The usual process!  Note that tut7_render_start/finish are changed to tut11_render_start/finish to
		 * be able to wait on/signal the semaphores we talked about just above.
		 */

		uint32_t image_index;

		/* Acquire images and start recording */
		res = tut11_render_start(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &image_index);
		if (res)
			break;

		/* Render pass */
		VkClearValue clear_values = {
			.color = { .float32 = {0.1, 0.1, 0.1, 255}, },
		};
		VkRenderPassBeginInfo pass_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_data.postproc_render_pass,
			.framebuffer = render_data.gbuffers[image_index].framebuffer,
			.renderArea = {
				.offset = { .x = 0, .y = 0, },
				.extent = render_data.gbuffers[image_index].surface_size,
			},
			.clearValueCount = 1,
			.pClearValues = &clear_values,
		};
		vkCmdBeginRenderPass(essentials.cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

		/* Bindings */
		vkCmdBindPipeline(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_data.postproc_pipeline.pipeline);

		vkCmdBindDescriptorSets(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				render_data.postproc_layout.pipeline_layout, 0, 1, &render_data.postproc_desc_set, 0, NULL);

		VkDeviceSize vertices_offset = 0;
		vkCmdBindVertexBuffers(essentials.cmd_buffer, 0, 1, &render_data.buffers[BUFFER_VERTICES].buffer, &vertices_offset);
		vkCmdBindIndexBuffer(essentials.cmd_buffer, render_data.buffers[BUFFER_INDICES].buffer, 0, VK_INDEX_TYPE_UINT16);

		/* Dynamic pipeline states */
		VkViewport viewport = {
			.x = 0,
			.y = 0,
			.width = WINDOW_WIDTH,
			.height = WINDOW_HEIGHT,
			.minDepth = 0,
			.maxDepth = 1,
		};
		vkCmdSetViewport(essentials.cmd_buffer, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = { .x = 0, .y = 0, },
			.extent = render_data.gbuffers[image_index].surface_size,
		};
		vkCmdSetScissor(essentials.cmd_buffer, 0, 1, &scissor);

		/* Push constants */

		/* Put every value swinging back and forth (1 through 16 for pixels, 256 divided by that for color levels). */
		uint64_t diff_time_ms = (get_time_ns() - animation_time) / 1000000;
		render_data.push_constants = (struct push_constants){
			.pixel_size = (diff_time_ms / 700) % 31 + 1,
			.hue_levels = (diff_time_ms / 100) % 31 + 1,
			.saturation_levels = (diff_time_ms / 150) % 31 + 1,
			.intensity_levels = (diff_time_ms / 130) % 31 + 1,
		};
		if (render_data.push_constants.pixel_size > 16.5)
			render_data.push_constants.pixel_size = 32 - render_data.push_constants.pixel_size;
		if (render_data.push_constants.hue_levels > 16.5)
			render_data.push_constants.hue_levels = 32 - render_data.push_constants.hue_levels;
		if (render_data.push_constants.saturation_levels > 16.5)
			render_data.push_constants.saturation_levels = 32 - render_data.push_constants.saturation_levels;
		if (render_data.push_constants.intensity_levels > 16.5)
			render_data.push_constants.intensity_levels = 32 - render_data.push_constants.intensity_levels;
		render_data.push_constants.hue_levels = 256 / render_data.push_constants.hue_levels;
		render_data.push_constants.saturation_levels = 256 / render_data.push_constants.saturation_levels;
		render_data.push_constants.intensity_levels = 256 / render_data.push_constants.intensity_levels;

		vkCmdPushConstants(essentials.cmd_buffer, render_data.postproc_layout.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
				sizeof render_data.push_constants, &render_data.push_constants);

		/* Draw: the quad indices start from index 3 */
		vkCmdDrawIndexed(essentials.cmd_buffer, 4, 1, 3, 0, 0);

		vkCmdEndRenderPass(essentials.cmd_buffer);

		res = tut11_render_finish(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image_index,
				wait_render_sem, wait_postproc_sem);
		if (res)
			break;
	}

exit_bad_fence:
exit_bad_semaphore:
	vkDeviceWaitIdle(dev->device);

	vkDestroyFence(dev->device, offscreen_fence, NULL);
	vkDestroySemaphore(dev->device, wait_render_sem, NULL);
	vkDestroySemaphore(dev->device, wait_postproc_sem, NULL);

exit_bad_prerecord:
exit_bad_render_data:
	free_render_data(dev, &essentials, &render_data);

exit_bad_essentials:
	tut7_render_cleanup_essentials(&essentials, dev);
}

int main(int argc, char **argv)
{
	tut1_error res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_dev;
	struct tut2_device dev;
	struct tut6_swapchain swapchain = {0};
	SDL_Window *window = NULL;
	uint32_t dev_count = 1;

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

	srand(time(NULL));

	/* Fire up Vulkan */
	res = tut6_init(&vk);
	if (!tut1_error_is_success(&res))
	{
		tut1_error_printf(&res, "Could not initialize Vulkan\n");
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, &phy_dev, &dev_count);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not enumerate devices\n");
		goto exit_bad_enumerate;
	}

	if (dev_count < 1)
	{
		printf("No graphics card? Shame on you\n");
		goto exit_bad_enumerate;
	}

	/* Get logical devices and enable WSI extensions */
	res = tut6_setup(&phy_dev, &dev, VK_QUEUE_GRAPHICS_BIT);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not setup logical device, command pools and queues\n");
		goto exit_bad_setup;
	}

	/* Set up SDL */
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Could not initialize SDL: %s\n", SDL_GetError());
		goto exit_bad_sdl;
	}

	window = SDL_CreateWindow("Vk Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
	if (window == NULL)
	{
		printf("Could not create window: %s\n", SDL_GetError());
		goto exit_bad_window;
	}

	/* Get the surface and swapchain */
	res = tut6_get_swapchain(vk, &phy_dev, &dev, &swapchain, window, 1, no_vsync);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not create surface and swapchain\n");
		goto exit_bad_swapchain;
	}

	/* Render loop similar to Tutorial 8 */
	render_loop(&phy_dev, &dev, &swapchain);

	retval = 0;

	/* Cleanup after yourself */

exit_bad_swapchain:
	tut6_free_swapchain(vk, &dev, &swapchain);

exit_bad_window:
	if (window)
		SDL_DestroyWindow(window);
exit_bad_sdl:
	SDL_Quit();

exit_bad_setup:
	tut2_cleanup(&dev);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
