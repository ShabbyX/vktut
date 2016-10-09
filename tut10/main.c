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
#include "tut10_render.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128

/*
 * This tutorial covers the small but not-immediately clear subject of push constants.  So, most of what you see here
 * is nothing new, except the small parts related to push constants.
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

static tut1_error generate_texture(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *image, uint8_t pattern[8], const char *name);

enum
{
	IMAGE_TEXTURE1 = 0,
	IMAGE_TEXTURE2 = 1,
};
enum
{
	BUFFER_TRANSFORMATION = 0,
	BUFFER_VERTICES = 1,
	BUFFER_INDICES = 2,
};
enum
{
	SHADER_VERTEX = 0,
	SHADER_FRAGMENT = 1,
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
		} vertices[4];			/* In this tutorial, we draw two quad */

		uint16_t indices[4];		/* Indexed drawing */
	} objects;

	struct transformation
	{
		float mat[4][4];
	} transformation;

	/* Actual objects used in this tutorial */
	struct tut7_image images[2];
	struct tut7_buffer buffers[3];
	struct tut7_shader shaders[2];
	struct tut7_graphics_buffers *gbuffers;

	/* For rendering */
	VkRenderPass render_pass;
	struct tut8_layout layout;
	struct tut8_pipeline pipeline;
	VkDescriptorSet desc_set;
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

	/* Images */
	render_data->images[IMAGE_TEXTURE1] = (struct tut7_image){
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent = { .width = TEXTURE_WIDTH, .height = TEXTURE_HEIGHT },
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.make_view = true,
		.host_visible = false,
	};
	render_data->images[IMAGE_TEXTURE2] = render_data->images[IMAGE_TEXTURE1];

	retval = tut7_create_images(phy_dev, dev, render_data->images, 2);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create texture images\n");
		return retval;
	}

	/* The quad */
	render_data->objects = (struct objects){
		.vertices = {
			[0] = (struct vertex){ .pos = { 0.5,  0.5, 0.0}, .color = {0.8, 0.8, 0.8}, .tex = {1, 0}, },
			[1] = (struct vertex){ .pos = { 0.5, -0.5, 0.0}, .color = {0.8, 0.8, 0.8}, .tex = {1, 1}, },
			[2] = (struct vertex){ .pos = {-0.5,  0.5, 0.0}, .color = {0.8, 0.8, 0.8}, .tex = {0, 0}, },
			[3] = (struct vertex){ .pos = {-0.5, -0.5, 0.0}, .color = {0.8, 0.8, 0.8}, .tex = {0, 1}, },
		},
		.indices = {
			0, 1, 2, 3,
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
	/*
	 * These wrapper functions create and destroy a staging buffer automatically, so they are more convenient.
	 * However, this is less efficient than in Tutorial 9, where the same staging buffer was reused.  In a
	 * real-world application, one would possibly want to have a pool of staging buffers and reuse them.  Or maybe
	 * make one staging buffer as large as the largest possible data copy, and use it for everything.
	 */
	retval = tut10_render_init_buffer(phy_dev, dev, essentials, &render_data->buffers[BUFFER_VERTICES], render_data->objects.vertices, "vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut10_render_init_buffer(phy_dev, dev, essentials, &render_data->buffers[BUFFER_INDICES], render_data->objects.indices, "index");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * As in Tutorial 9, let's generate a texture, so we don't have to depend on an external library to read a
	 * texture file for us.
	 */
	uint8_t pattern1[8] = {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0};
	uint8_t pattern2[8] = {0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03};
	retval = generate_texture(phy_dev, dev, essentials, &render_data->images[IMAGE_TEXTURE1], pattern1, "texture1");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = generate_texture(phy_dev, dev, essentials, &render_data->images[IMAGE_TEXTURE2], pattern2, "texture2");
	if (!tut1_error_is_success(&retval))
		return retval;

	/* Shaders */
	render_data->shaders[SHADER_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut10.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut10.frag.spv",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	retval = tut7_load_shaders(dev, render_data->shaders, 2);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not load the shaders (expected location: ../shaders)\n");
		return retval;
	}

	/* Graphics buffers */
	render_data->gbuffers = malloc(essentials->image_count * sizeof *render_data->gbuffers);
	for (uint32_t i = 0; i < essentials->image_count; ++i)
		render_data->gbuffers[i] = (struct tut7_graphics_buffers){
			.surface_size = swapchain->surface_caps.currentExtent,
			.swapchain_image = essentials->images[i],
		};

	retval = tut7_create_graphics_buffers(phy_dev, dev, swapchain->surface_format, render_data->gbuffers, essentials->image_count, &render_data->render_pass);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create graphics buffers\n");
		return retval;
	}

	/* Depth/stencil image transition */
	for (uint32_t i = 0; i < essentials->image_count; ++i)
	{
		retval = tut8_render_transition_images(dev, essentials, &render_data->gbuffers[i].depth, 1,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, "depth");
		if (!tut1_error_is_success(&retval))
			return retval;
	}

	/* Layouts */

	/*
	 * Push constants are declared when creating a pipeline layout.  Push constants are very simple to understand,
	 * but are not very well explained.
	 *
	 * To send arbitrary data to shaders, we have so far used a uniform buffer; that has been for the
	 * transformation matrix.  However, it's clear that updating that buffer frequently is not as
	 * simple/convenient/efficient as one might have hoped; you have to write the data to some staging buffer and
	 * copy it over.
	 *
	 * Now imagine you get *one* small buffer for free, and sending data to the shaders using that buffer is both
	 * easy and efficient.  That's essentially what push constants are: pieces inside a special buffer.  The push
	 * constants are referred to as "ranges" inside this buffer, or an (offset, size) pair in other words.  You
	 * declare the push constants by saying what ranges of this special buffer is going to be used by which stage
	 * of the pipeline, and you write the push constant values by filling in data at desired offsets.
	 *
	 * Let's go over this with an example.  Say you want to send two float values to the vertex shader, and one
	 * float value to the fragment shader.  You can lay them out like this:
	 *
	 *     0    4    8    12
	 *     +----+----+----+
	 *     + v1 + v2 + v3 +
	 *     +----+----+----+
	 *
	 * You then declare two push constant ranges: (offset=0, size=8) accessible to the vertex shader, and
	 * (offset=8, size=4) accessible to the fragment shader.
	 *
	 * Later on, you can update the push constants by writing to this special buffer at any desired offset.  For
	 * example, you could update v2 by writing some value at offset 4 with size 4.  See the usage of
	 * `vkCmdPushConstants` in `render_loop()`.
	 *
	 * Check out `shaders/tut10.frag` to see how to declare push constants in GLSL.
	 *
	 * Tiny detail: offset and size must both be a multiple of 4.
	 *
	 * An interesting point is that the minimum size for this special push constant buffer is 128 bytes as required
	 * by Vulkan.  A 4x4 matrix of `float` values takes 64 bytes, so we could have actually sent our transformation
	 * matrix, which normally happens to change every frame, through push constants instead of a normal buffer.
	 * Isn't this awesome?
	 */
	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.offset = 0,
		.size = sizeof(float),
	};
	struct tut8_resources resources = {
		.images = render_data->images,
		.image_count = 2,
		.buffers = render_data->buffers,
		.buffer_count = 1,
		.shaders = render_data->shaders,
		.shader_count = 2,
		.push_constants = &push_constant_range,
		.push_constant_count = 1,
		.render_pass = render_data->render_pass,
	};
	render_data->layout = (struct tut8_layout){
		.resources = &resources,
	};
	/*
	 * Note: `tut8_make_graphics_layouts` is assigning bindings from 0 up, starting with images followed by
	 * buffers.  In this case: texture1: binding 0, texture2: binding 1, transformation matrix: binding 2.
	 */
	retval = tut8_make_graphics_layouts(dev, &render_data->layout, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create descriptor set or pipeline layouts\n");
		return retval;
	}

	/* Pipeline */
	VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof *render_data->objects.vertices,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	VkVertexInputAttributeDescription vertex_attributes[3] = {
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
		[2] = {
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32_SFLOAT,
			.offset = sizeof(float[3]) + sizeof(float[3]),
		},
	};
	render_data->pipeline = (struct tut8_pipeline){
		.layout = &render_data->layout,
		.vertex_input_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertex_binding,
			.vertexAttributeDescriptionCount = 3,
			.pVertexAttributeDescriptions = vertex_attributes,
		},
		.input_assembly_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,	/* Triangle strip instead of triangle list */
			.primitiveRestartEnable = true,				/* This enables using 0xFFFF as a "restart" marker */
		},
		.tessellation_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
		},
		.thread_count = 1,
	};

	retval = tut8_make_graphics_pipelines(dev, &render_data->pipeline, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create graphics pipeline\n");
		return retval;
	}

	/* Descriptor Set */
	VkDescriptorSetAllocateInfo set_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = render_data->pipeline.set_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &render_data->layout.set_layout,
	};
	res = vkAllocateDescriptorSets(dev->device, &set_info, &render_data->desc_set);
	retval = TUT1_ERROR_NONE;
	tut1_error_set_vkresult(&retval, res);
	if (res)
	{
		tut1_error_printf(&retval, "Could not allocate descriptor set from pool\n");
		return retval;
	}

	/* In this tutorial, we also have an image to bind to the descriptor set */
	VkDescriptorImageInfo set_write_image_info[2] = {
		[0] = {
			.sampler = render_data->images[IMAGE_TEXTURE1].sampler,
			.imageView = render_data->images[IMAGE_TEXTURE1].view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		[1] = {
			.sampler = render_data->images[IMAGE_TEXTURE2].sampler,
			.imageView = render_data->images[IMAGE_TEXTURE2].view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
	};
	VkDescriptorBufferInfo set_write_buffer_info = {
		.buffer = render_data->buffers[BUFFER_TRANSFORMATION].buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};
	VkWriteDescriptorSet set_write[3] = {
		[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->desc_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &set_write_image_info[0],
		},
		[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->desc_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &set_write_image_info[1],
		},
		[2] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->desc_set,
			.dstBinding = 2,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &set_write_buffer_info,
		},
	};
	vkUpdateDescriptorSets(dev->device, 3, set_write, 0, NULL);

	return retval;
}

static void free_render_data(struct tut2_device *dev, struct tut7_render_essentials *essentials, struct render_data *render_data)
{
	vkDeviceWaitIdle(dev->device);

	tut8_free_pipelines(dev, &render_data->pipeline, 1);
	tut8_free_layouts(dev, &render_data->layout, 1);
	tut7_free_images(dev, render_data->images, 2);
	tut7_free_buffers(dev, render_data->buffers, 3);
	tut7_free_shaders(dev, render_data->shaders, 2);
	tut7_free_graphics_buffers(dev, render_data->gbuffers, essentials->image_count, render_data->render_pass);

	free(render_data->gbuffers);
}

static tut1_error generate_texture(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *image, uint8_t pattern[8], const char *name)
{
	tut1_error retval = TUT1_ERROR_NONE;
	size_t texture_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4 * sizeof(uint8_t);

	uint8_t *generated_texture = malloc(texture_size);
	if (generated_texture == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_failed;
	}

	/* The following code generates a repetition of the bits of the given pattern.  You don't need to follow the math. */
	for (unsigned int i = 0; i < TEXTURE_HEIGHT; ++i)
	{
		for (unsigned int j = 0; j < TEXTURE_WIDTH; ++j)
		{
			unsigned int bit = pattern[(i % 8)] >> (j % 8) & 1;
			uint8_t color = bit?0xFF:0x00;
			size_t pixel = (i * TEXTURE_WIDTH + j) * 4 * sizeof(uint8_t);

			generated_texture[pixel + 0] = color;	/* B */
			generated_texture[pixel + 1] = color;	/* G */
			generated_texture[pixel + 2] = color;	/* R */
			generated_texture[pixel + 3] = 0xFF;	/* A */
		}
	}

	/*
	 * Fill the image.  This convenience function automatically creates and destroys a staging buffer.  Again, it
	 * would have been more efficient if we had pre-created one large-enough staging buffer and use it for all of
	 * our copies.
	 */
	retval = tut10_render_init_texture(phy_dev, dev, essentials, image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, generated_texture, name);

exit_failed:
	free(generated_texture);

	return retval;
}

static uint64_t get_time_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LLU + ts.tv_nsec;
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

	/* Allocate buffers and load shaders for the rendering in this tutorial. */
	retval = allocate_render_data(phy_dev, dev, swapchain, &essentials, &render_data);
	if (!tut1_error_is_success(&retval))
		goto exit_bad_render_data;

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

		uint32_t image_index;

		/* We saw all this in Tutorials 8 and 9.  Any changes are commented. */
		res = tut7_render_start(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &image_index);
		if (res)
			break;

		/* Render pass */
		VkClearValue clear_values[2] = {
			{ .color = { .float32 = {0.1, 0.1, 0.1, 255}, }, },
			{ .depthStencil = { .depth = -1000, }, },
		};
		VkRenderPassBeginInfo pass_info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = render_data.render_pass,
			.framebuffer = render_data.gbuffers[image_index].framebuffer,
			.renderArea = {
				.offset = { .x = 0, .y = 0, },
				.extent = render_data.gbuffers[image_index].surface_size,
			},
			.clearValueCount = 2,
			.pClearValues = clear_values,
		};
		vkCmdBeginRenderPass(essentials.cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

		/* Bindings */
		vkCmdBindPipeline(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_data.pipeline.pipeline);

		vkCmdBindDescriptorSets(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				render_data.layout.pipeline_layout, 0, 1, &render_data.desc_set, 0, NULL);

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

		/*
		 * In this tutorial, we are drawing one quad that takes its color from two textures.  The two textures
		 * are `mix`ed, in GLSL terms, using a value that we are going to update through the push constant!
		 * How cool is that?
		 *
		 * Let's make the quad gradually swing between the textures in a span of 2 seconds.
		 */
		uint64_t cur_time = get_time_ns();
		float mix_value = (cur_time - animation_time) % 2000000000 / 1000000000.0f;
		if (mix_value > 1)
			mix_value = 2 - mix_value;
		/*
		 * vkCmdPushConstants is quite straightforward.  It takes the following parameters: the command buffer,
		 * the pipeline layout, the pipeline stages that use the push constant, offset, size and finally the
		 * actual values to be updated.
		 *
		 * All it does is write your values at the specified offset in the special push constant buffer
		 * explained above.
		 */
		vkCmdPushConstants(essentials.cmd_buffer, render_data.layout.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &mix_value);

		/* Draw */
		vkCmdDrawIndexed(essentials.cmd_buffer, 4, 1, 0, 0, 0);

		vkCmdEndRenderPass(essentials.cmd_buffer);

		/* Stop recording and present image */
		res = tut7_render_finish(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image_index);
		if (res)
			break;
	}

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
