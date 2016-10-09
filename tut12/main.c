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
#include "tut12.h"
#include "../tut8/tut8_render.h"

/*
 * This tutorial is a fun one!  The rendering is exactly the same as Tutorial 8, so there is actually nothing new in
 * using the Vulkan API.  Instead, this tutorial delves somewhat deeper into the Vulkan WSI (Window System Integration)
 * extension, by implementing a rudimentary presentation engine based on ncurses.  Now isn't that what you have always
 * been thinking of doing since you started reading these Tutorials?  Oh my god, me too!
 *
 * Until now (and after this tutorial too) we have been using SDL to create a window and allocate our surface and
 * swapchains based on that.  There was some automatic image allocation done by the driver at that point and we were
 * happily enjoying the lack of responsibility, not to mention all the buffer swapping and presentation.  It's ok to
 * have it easy and just use Vulkan like every other programmer could (buzzinga), but experiencing the difficulties of
 * implementing the internals is surely going to leave a ~~scar~~ better understanding.
 *
 * Here's what we're going to do then.  One step is to substitute SDL with ncurses, of course.  Then, we should
 * substitute the few platform-dependent functions of Vulkan with our own implementation.  In reality, one would add
 * support for a new window system by implementing it in the driver, but we don't always have access to the code (thank
 * you Nvidia).  Besides, ncurses is not even a window system, so in reality, one would simply do off-screen rendering
 * and then print it to the screen using ncurses.  This is just for practice, don't open an issue and call me an idiot
 * please.
 *
 * Luckily, ld (the linker) makes our life easy: if we implement functions such as vkCreateSwapchainKHR or
 * vkAcquireNextImageKHR and then link with -lvulkan, the linker first picks up the implementation from the .o files.
 * So, we can use the same object files from the previous tutorials and in a way override the implementation of a
 * handful of Vulkan functions.  That's what tut12.c does.  In this file, the only changes you would really see is the
 * use of ncurses instead of SDL.
 */

static int process_events()
{
	switch (getch())
	{
	case ERR:
	default:
		break;
	case 27:		/* Escape */
	case 'q':
	case 'Q':
		return -1;
	}

	return 0;
}

enum
{
	BUFFER_TRANSFORMATION = 0,
	BUFFER_VERTICES = 1,
	BUFFER_VERTICES_STAGING = 2,
};
enum
{
	SHADER_VERTEX = 0,
	SHADER_FRAGMENT = 1,
};
struct render_data
{
	struct vertex
	{
		float pos[3];
		float color[3];
	} vertices[3];

	struct transformation
	{
		float mat[4][4];
	} transformation;

	/* Actual objects used in this tutorial */
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
		.format = VK_FORMAT_R32_SFLOAT,		/* Note: this is used for the buffer view, which we are not using */
		.size = sizeof render_data->transformation,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.host_visible = true,
	};

	render_data->buffers[BUFFER_VERTICES] = (struct tut7_buffer){
		.size = sizeof render_data->vertices,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.host_visible = false,
	};

	render_data->buffers[BUFFER_VERTICES_STAGING] = (struct tut7_buffer){
		.size = sizeof render_data->vertices,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.host_visible = true,
	};

	retval = tut7_create_buffers(phy_dev, dev, render_data->buffers, 3);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create vertex and transformation buffers\n");
		return retval;
	}

	/* The object (triangle) */
	render_data->vertices[0] = (struct vertex){ .pos = {-0.8,  0.0, 0}, .color = {1.0, 0.2, 0.0}, };
	render_data->vertices[1] = (struct vertex){ .pos = { 0.2,  0.9, 0}, .color = {0.0, 1.0, 0.2}, };
	render_data->vertices[2] = (struct vertex){ .pos = { 0.6, -0.9, 0}, .color = {0.2, 0.0, 1.0}, };

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
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], render_data->vertices, sizeof render_data->vertices, "staging vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_TRANSFORMATION], &render_data->transformation, sizeof render_data->transformation, "transformation");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_copy_buffer(dev, essentials, &render_data->buffers[BUFFER_VERTICES], &render_data->buffers[BUFFER_VERTICES_STAGING],
			sizeof render_data->vertices, "vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	/*
	 * TODO: This buffer doesn't have a view, so it's buffer view object is NULL.  Enable this after the NVidia
	 * driver fixes its bug with handling NULL pointers.
	 *
	 * tut7_free_buffers(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], 1);
	 */

	/* Shaders */
	render_data->shaders[SHADER_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut12.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut12.frag.spv",
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
	VkPushConstantRange push_constant_range = {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(float),
	};
	struct tut8_resources resources = {
		.buffers = render_data->buffers,
		.buffer_count = 2,
		.shaders = render_data->shaders,
		.shader_count = 2,
		.push_constants = &push_constant_range,
		.push_constant_count = 1,
		.render_pass = render_data->render_pass,
	};
	render_data->layout = (struct tut8_layout){
		.resources = &resources,
	};
	retval = tut8_make_graphics_layouts(dev, &render_data->layout, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not create descriptor set or pipeline layouts\n");
		return retval;
	}

	/* Pipeline */
	VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,
		.stride = sizeof *render_data->vertices,
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
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
	render_data->pipeline = (struct tut8_pipeline){
		.layout = &render_data->layout,
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

	VkDescriptorBufferInfo set_write_buffer_info = {
		.buffer = render_data->buffers[BUFFER_TRANSFORMATION].buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};
	VkWriteDescriptorSet set_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = render_data->desc_set,
		.dstBinding = 0,
		.descriptorCount = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.pBufferInfo = &set_write_buffer_info,
	};
	vkUpdateDescriptorSets(dev->device, 1, &set_write, 0, NULL);

	return retval;
}

static void free_render_data(struct tut2_device *dev, struct tut7_render_essentials *essentials, struct render_data *render_data)
{
	vkDeviceWaitIdle(dev->device);

	tut8_free_pipelines(dev, &render_data->pipeline, 1);
	tut8_free_layouts(dev, &render_data->layout, 1);
	tut7_free_buffers(dev, render_data->buffers, 2);	/* Note: BUFFER_VERTICES_STAGING is already freed */
	tut7_free_shaders(dev, render_data->shaders, 2);
	tut7_free_graphics_buffers(dev, render_data->gbuffers, essentials->image_count, render_data->render_pass);

	free(render_data->gbuffers);
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

	/* Process events from ncurses and render.  If process_events returns non-zero, it signals application exit. */
	while (process_events() == 0)
	{
		uint32_t image_index;

		/* Acquire images and start recording */
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

		/* Dynamic pipeline states */
		VkViewport viewport = {
			.x = 0,
			.y = 0,
			.width = render_data.gbuffers[image_index].surface_size.width,
			.height = render_data.gbuffers[image_index].surface_size.height,
			.minDepth = 0,
			.maxDepth = 1,
		};
		vkCmdSetViewport(essentials.cmd_buffer, 0, 1, &viewport);

		VkRect2D scissor = {
			.offset = { .x = 0, .y = 0, },
			.extent = render_data.gbuffers[image_index].surface_size,
		};
		vkCmdSetScissor(essentials.cmd_buffer, 0, 1, &scissor);

		/* Make the triangle slowly turn (@30 deg/s), for added fun */
		uint64_t cur_time = get_time_ns();
		float angle = (cur_time - animation_time) % 12000000000 / 1000000000.0f;
		angle *= 3.14159f / 6;

		vkCmdPushConstants(essentials.cmd_buffer, render_data.layout.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float), &angle);

		/* Draw triangle */
		vkCmdDraw(essentials.cmd_buffer, 3, 1, 0, 0);

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
	/*
	 * we proved our point so far about multiple graphics cards.  Until we actually get to use them together
	 * however, there is little point in repeating everything for each card.  So, until then, we'll just work with
	 * the first card.
	 */
	struct tut1_physical_device phy_dev;
	struct tut2_device dev;
	struct tut6_swapchain swapchain = {0};
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

	/* Set up ncurses */
	initscr();
	raw();
	noecho();
	timeout(0);
	if (!has_colors())
	{
		printf("No color support in the terminal\n");
		goto exit_bad_ncurses;
	}
	start_color();

	/* Get the surface and swapchain */
	res = tut12_get_swapchain(vk, &phy_dev, &dev, &swapchain, stdscr, 1, no_vsync);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not create surface and swapchain\n");
		goto exit_bad_swapchain;
	}

	/* Render loop similar to Tutorial 7 */
	render_loop(&phy_dev, &dev, &swapchain);

	retval = 0;

	/* Cleanup after yourself */

exit_bad_swapchain:
	tut6_free_swapchain(vk, &dev, &swapchain);

exit_bad_ncurses:
	endwin();

exit_bad_setup:
	tut2_cleanup(&dev);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
