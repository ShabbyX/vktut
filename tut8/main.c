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
#include "tut8.h"
#include "tut8_render.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

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

	/*
	 * Here, we're finally going to use the tut7.c functions, that got all sorts of resources for us.  You should
	 * already know at least some of the basics of modern OpenGL at this point.  If not, here's a good read:
	 *
	 *     http://opengl.datenwolf.net/gltut/html/index.html
	 *
	 * Let's make it simple though, since the focus here is learning about the Vulkan API, not graphics in general.
	 * We therefore allocate two buffers, one for vertex data and one for a transformation.  The vertex data would
	 * simply contain position and color for the vertices, and the transformation would be the usual 4x4 matrix.
	 *
	 * We also just assume a single pipeline with two stages by using a vertex shader and a fragment shader.  All
	 * the resources would be put in a single layout similar to Tutorial 4.
	 */

	/*
	 * The transformation matrix is the combination of the projection, model and view matrices.  You should be able
	 * to do the math, and there are libraries for doing matrix operations in C too.  Here, we'll skip that part
	 * also and use a prebuilt matrix.
	 *
	 * The usage of this buffer is as a general read-only input to the shaders, i.e. it's a uniform buffer.  It's
	 * only used in the vertex shader, so it's `stage` is the same as other buffer.
	 */
	render_data->buffers[BUFFER_TRANSFORMATION] = (struct tut7_buffer){
		.format = VK_FORMAT_R32_SFLOAT,		/* Note: this is used for the buffer view, which we are not using */
		.size = sizeof render_data->transformation,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.host_visible = true,
	};

	/*
	 * The vertices have position and color components interleaved.  We will see shortly how to let the vertex
	 * shader know where to get each component.  The vertices are also going to be used as a Vertex Buffer which
	 * gets bound to the command buffer, so we will mark its `usage` as such.  Furthermore, only the vertex shader
	 * would be using it, so we'll also specify this fact.
	 *
	 * There is one tiny detail regarding vertex buffers.  They are a lot more efficient if they reside in
	 * device-local memory.  We can't map a device-local memory to copy data over (unless they are also
	 * host-visible), so what we are going to do is to allocate another buffer on the host side, fill it with our
	 * vertex data, use a command buffer to copy its contents to the device-local memory, and then destroy that
	 * "staging" buffer.  This adds the TRANSFER_DST usage to the vertex buffer, and the TRANSFER_SRC usage to the
	 * staging one.
	 */
	render_data->buffers[BUFFER_VERTICES] = (struct tut7_buffer){
		.format = VK_FORMAT_R32_SFLOAT,		/* Note: same as the previous buffer; unused */
		.size = sizeof render_data->vertices,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.host_visible = false,
	};
	render_data->buffers[BUFFER_VERTICES_STAGING] = (struct tut7_buffer){
		.format = VK_FORMAT_R32_SFLOAT,		/* Note: same as the previous buffer; unused */
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

	/* Let's define our triangle.  Simple stuff. */
	render_data->vertices[0] = (struct vertex){ .pos = {-0.5,  0.0, 0}, .color = {1.0, 0.6, 0.4}, };
	render_data->vertices[1] = (struct vertex){ .pos = { 0.1,  0.7, 0}, .color = {0.2, 1.0, 0.3}, };
	render_data->vertices[2] = (struct vertex){ .pos = { 0.3, -0.7, 0}, .color = {0.3, 0.1, 1.0}, };

	/* Then, make up our transformation matrix, assuming it was made based on some calculation */
	render_data->transformation = (struct transformation){
		.mat = {
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 0},
			{0, 0, 0, 1},
		},
	};

	/*
	 * Now we need to copy our data over to the buffer memories.  We saw how this was done in Tutorial 4; map the
	 * memory, write the contents, unmap the memory.
	 */
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], render_data->vertices, sizeof render_data->vertices, "staging vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_TRANSFORMATION], &render_data->transformation, sizeof render_data->transformation, "transformation");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * As a special case with the vertex buffer, we should copy over the data from the staging buffer to the one we
	 * are actually going to use, and then destroy the staging buffer.
	 */
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

	/*
	 * We have also seen how shaders are loaded in Tutorial 4.  We load a simple vertex and fragment shader here,
	 * which are located in shaders/ (tut8.vert and tut8.frag).
	 */
	render_data->shaders[SHADER_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut8.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut8.frag.spv",
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
	};

	retval = tut7_load_shaders(dev, render_data->shaders, 2);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Could not load the shaders (expected location: ../shaders)\n");
		return retval;
	}

	/*
	 * We are almost done with creating our resources.  Only thing left is views on swapchain images, the
	 * depth/stencil images and a framebuffer.
	 */
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

	/*
	 * The depth/stencil image created by tut7_create_graphics_buffers is in UNDEFINED format, and we need to
	 * transition it to the DEPTH_STENCIL_OPTIMAL format before we can actually use it.
	 */
	for (uint32_t i = 0; i < essentials->image_count; ++i)
	{
		retval = tut8_render_transition_images(dev, essentials, &render_data->gbuffers[i].depth, 1,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, "depth");
		if (!tut1_error_is_success(&retval))
			return retval;
	}

	/*
	 * Now that we have our resources, we need to specify the layout in which they will be placed, so the shaders
	 * can pick them up.  tut8_make_graphics_layouts makes the layout for us, and it automatically assigns the
	 * images and buffers to sequential bindings all in the same set.
	 */
	struct tut8_resources resources = {
		.buffers = render_data->buffers,
		.buffer_count = 2,
		.shaders = render_data->shaders,
		.shader_count = 2,
		.graphics_buffers = render_data->gbuffers,
		.graphics_buffer_count = essentials->image_count,
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

	/*
	 * Finally, we can create the pipeline.  We are doing single threaded rendering for now, so the thread count is
	 * just 1.  There are a set of states we need to define for our pipeline, which tut8_make_graphics_pipelines
	 * couldn't really assume.  These are vertex_input_state, which says how the vertex data are given to the
	 * pipeline (where are the positions, colors etc), input_assembly_state, which says what the vertices make
	 * (triangles, lines, etc), and tessellation_state, which is not used here.
	 *
	 * The vertex buffer is the first buffer bound in the descriptor set, so `binding` here is 0.  The `stride`
	 * between the data of each vertex is one `struct vertex`.  `inputRate` is either specified as vertex or
	 * instance, the later is useful for drawing multiple instances of the same mesh (not used here).
	 *
	 * There are two sets of information in the vertex buffer (attributes as Vulkan calls them): position and
	 * color.  The first is at offset 0 of the vertex data and the second is at offset sizeof(float[3]).  We need
	 * to specify a location for each of these attributes, and we'll put location 0 for position and 1 for color,
	 * why not.
	 */
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

	/*
	 * Are we there yet?  Almost.  We just need to allocate our descriptor set like in Tutorial 4 and bind our
	 * resources (only the transformation matrix in this case) to it.
	 */
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

	/*
	 * Ok.  Let's take a breath.  This is an exciting moment.  We are finally ready to render something on the
	 * screen.  I hope you appreciate now that we took a detour in the first few tutorials to explore the compute
	 * side of Vulkan instead of graphics, because in the tutorials that followed you were already familiar with a
	 * lot of stuff.
	 *
	 * Let's head over to render_loop() and do this!
	 */

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

static void render_loop(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut6_swapchain *swapchain)
{
	int res;
	tut1_error retval = TUT1_ERROR_NONE;
	struct tut7_render_essentials essentials;

	struct render_data render_data = { .gbuffers = NULL, };

	/* Allocate render essentials.  See this function in tut7_render.c for explanations */
	res = tut7_render_get_essentials(&essentials, phy_dev, dev, swapchain);
	if (res)
		goto exit_bad_essentials;

	/* Allocate buffers and load shaders for the rendering in this tutorial */
	retval = allocate_render_data(phy_dev, dev, swapchain, &essentials, &render_data);
	if (!tut1_error_is_success(&retval))
		goto exit_bad_render_data;

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

		/* See this function in tut7_render.c for explanations */
		res = tut7_render_start(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, &image_index);
		if (res)
			return;

		/*
		 * As a reminder, tut7_render_start() starts recording in our command buffer, and tut7_render_finish()
		 * stops it.  **In a real application, you would certainly want to pre-record your command buffers and
		 * just execute them over an over.**
		 *
		 * That said, let's do a recap of what needs to be done.  Some of this is the same as in Tutorial 4:
		 *
		 * - Begin render pass
		 *   * Bind pipeline
		 *   * Bind descriptor set
		 *   * Set the dynamic states of pipeline (viewport and scissor as specified in
		 *     tut8_make_graphics_pipelines().
		 *   * Clear the screen (we can get this for free in Begin Render Pass, since we specified
		 *     VK_ATTACHMENT_LOAD_OP_CLEAR as loadOp in tut7_create_graphics_buffers)
		 *   * Draw triangles
		 * - End render pass
		 */
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
		/*
		 * In Vulkan, you can record "secondary buffers" that can be executed from within a subpass of a render
		 * pass, or you could record the subpass contents inline when recording the primary command buffer.
		 * This could be an example:
		 *
		 * - Record secondary command buffer A
		 * - Record secondary command buffer B
		 *
		 * - Record primary command buffer C
		 *   * Start render pass (this will automatically start first subpass) as inline
		 *     + Record some stuff (cannot execute secondary command buffer because it's inline)
		 *   * Next subpass as secondary command buffer
		 *     + Execute buffer A and B (can only execute secondary command buffer)
		 *   * Next subpass as inline
		 *     + Record other stuff
		 *   * End render pass
		 *
		 * Secondary command buffers are useful if you have multiple command buffers that share some of their
		 * work.  A very good example of this is that, each execution of the command buffer, in a typical
		 * render-to-screen case, needs to know which swapchain image it is rendering to.  However, much of the
		 * work could actually be independent of the buffer.  Assuming N swapchain images, you could have N
		 * primary command buffers where they do some minimal swapchain-image-specific work, and then call a
		 * unique secondary command buffer in a subpass.  In terms of coding, this might be slightly more work
		 * to implement, but the end result is that you save GPU memory, require less bandwidth, and be a lot
		 * more cache-friendly.
		 *
		 * What we do here is quite simple (and we are not even prebuilding command buffers), so we'll just do
		 * everything inline.
		 */
		vkCmdBeginRenderPass(essentials.cmd_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_data.pipeline.pipeline);

		vkCmdBindDescriptorSets(essentials.cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
				render_data.layout.pipeline_layout, 0, 1, &render_data.desc_set, 0, NULL);

		VkDeviceSize vertices_offset = 0;
		vkCmdBindVertexBuffers(essentials.cmd_buffer, 0, 1, &render_data.buffers[BUFFER_VERTICES].buffer, &vertices_offset);

		/*
		 * You are probably already familiar with what's a viewport from OpenGL.  Just to recap though, the
		 * viewport defines how the vertex positions are transformed into window coordinates.  Here, let's
		 * put the origin of the coordinates right in the middle of the window.  In the viewport description,
		 * (x, y) is the upper left corner of the window, (width, height) is the window size, and
		 * (minDepth, maxDepth) is scaling of the depth (values must be in [0, 1]).  The Vulkan specifications
		 * section "Controlling the Viewport" has the math if you are interested.
		 */
		VkViewport viewport = {
			.x = 0,
			.y = 0,
			.width = WINDOW_WIDTH,
			.height = WINDOW_HEIGHT,
			.minDepth = 0,
			.maxDepth = 1,
		};
		vkCmdSetViewport(essentials.cmd_buffer, 0, 1, &viewport);

		/*
		 * A scissor is used to contain the rendering to a specific rectangle.  If renderArea given when
		 * beginning a render pass doesn't encompass the whole framebuffer, a scissor must be used to make sure
		 * no rendering actually happens outside that area.  We are rendering to the whole screen though, so
		 * scissor is set to the whole screen, size effectively disabling it.
		 */
		VkRect2D scissor = {
			.offset = { .x = 0, .y = 0, },
			.extent = render_data.gbuffers[image_index].surface_size,
		};
		vkCmdSetScissor(essentials.cmd_buffer, 0, 1, &scissor);

		/*
		 * Time to render the triangle.
		 *
		 * So exciting.
		 */
		vkCmdDraw(essentials.cmd_buffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(essentials.cmd_buffer);

		/* See this function in tut7_render.c for explanations */
		res = tut7_render_finish(&essentials, dev, swapchain, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, image_index);
		if (res)
			return;
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

	/* Still 1 thread for now (the current thread) */
	res = tut6_get_swapchain(vk, &phy_dev, &dev, &swapchain, window, 1, no_vsync);
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
