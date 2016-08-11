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
#include "../tut8/tut8_render.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768

#define TEXTURE_WIDTH 128
#define TEXTURE_HEIGHT 128

/*
 * From this tutorial on, the setup and rendering process is similar to Tutorial 8.  Depending on what is being
 * experimented on, bits and pieces would change.  I'll point out where something has changed.
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

static tut1_error generate_texture(struct tut2_device *dev, struct tut7_image *image);

enum
{
	IMAGE_TEXTURE = 0,
	IMAGE_TEXTURE_STAGING = 1,
};
enum
{
	BUFFER_TRANSFORMATION = 0,
	BUFFER_VERTICES = 1,
	BUFFER_INDICES = 2,
	BUFFER_VERTICES_STAGING = 3,
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
			float tex[2];		/* In this tutorial, we use a texture */
		} vertices[8];			/* And draw two quads instead of a triangle */

		uint16_t indices[9];		/* We will do indexed drawing this time (read on) */
	} objects;

	struct transformation
	{
		float mat[4][4];
	} transformation;

	/* Actual objects used in this tutorial */
	struct tut7_image images[2];
	struct tut7_buffer buffers[4];
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
	render_data->buffers[BUFFER_VERTICES_STAGING] = render_data->buffers[BUFFER_VERTICES];
	render_data->buffers[BUFFER_VERTICES_STAGING].usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	render_data->buffers[BUFFER_VERTICES_STAGING].host_visible = true,

	/* To draw in indexed mode, we should have an index before alongside our vertex buffer */
	render_data->buffers[BUFFER_INDICES] = (struct tut7_buffer){
		.size = sizeof render_data->objects.indices,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		.host_visible = false,
	};

	retval = tut7_create_buffers(phy_dev, dev, render_data->buffers, 4);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create vertex, index and transformation buffers\n");
		return retval;
	}

	/*
	 * Similar to buffers, we need a staging image which is host-visible to fill with our texture, and then copy it
	 * to the real image residing in device memory.  The staging image is marked as "will be initialized", which
	 * means `tut7_create_images` will make its initial layout `VK_IMAGE_LAYOUT_PREINITIALIZED` as well as giving
	 * it linear tiling.  In other words, we can access the staging image as if it was a normal C array.
	 */
	render_data->images[IMAGE_TEXTURE] = (struct tut7_image){
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.extent = { .width = TEXTURE_WIDTH, .height = TEXTURE_HEIGHT },
		.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.make_view = true,
		.host_visible = false,
	};
	render_data->images[IMAGE_TEXTURE_STAGING] = render_data->images[IMAGE_TEXTURE];
	render_data->images[IMAGE_TEXTURE_STAGING].usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	render_data->images[IMAGE_TEXTURE_STAGING].make_view = false,
	render_data->images[IMAGE_TEXTURE_STAGING].will_be_initialized = true,
	render_data->images[IMAGE_TEXTURE_STAGING].host_visible = true,

	retval = tut7_create_images(phy_dev, dev, render_data->images, 2);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create texture image\n");
		return retval;
	}

	/*
	 * Let's make two quads.  One would be a normal rectangle, another as if it was being looked at at an angle.
	 * The second one can be used to experiment with anisotropic filtering.  Feel free to change the value given in
	 * `tut7_create_images` and see for yourself.
	 *
	 * Unlike OpenGL, Vulkan doesn't have a QUAD primitive, so we will draw a triangle strip (with two triangles)
	 * for each.
	 *
	 * Since we have two triangle strips, we could call vkCmdDraw* twice, one for each strip.  This is somewhat
	 * inefficient because these calls are expensive.  Vulkan has a nice little feature when using indexed drawing,
	 * and that's if an index of 0xFFFF (in case of 16-bit indices) or 0xFFFFFFFF (in case of 32-bit indices) is
	 * seen, the topology is restarted.  In this case, we will put a 0xFFFF in the middle of the index list and we
	 * will end up with two triangle strips while issuing only a single call to vkCmdDrawIndexed.
	 */
	render_data->objects = (struct objects){
		.vertices = {
			[0] = (struct vertex){ .pos = { 0.5, -0.2, 0.0}, .color = {0.8, 0.4, 0.1}, .tex = {1, 0}, },
			[1] = (struct vertex){ .pos = { 0.5, -0.8, 0.0}, .color = {0.8, 0.4, 0.1}, .tex = {1, 1}, },
			[2] = (struct vertex){ .pos = {-0.5, -0.2, 0.0}, .color = {0.8, 0.4, 0.1}, .tex = {0, 0}, },
			[3] = (struct vertex){ .pos = {-0.5, -0.8, 0.0}, .color = {0.8, 0.4, 0.1}, .tex = {0, 1}, },

			[4] = (struct vertex){ .pos = { 0.1,  0.8, 0.8}, .color = {0.8, 0.3, 0.2}, .tex = {1, 0}, },
			[5] = (struct vertex){ .pos = { 0.1,  0.2, 0.8}, .color = {0.8, 0.3, 0.2}, .tex = {1, 1}, },
			[6] = (struct vertex){ .pos = { 0.0,  0.8, 0.0}, .color = {0.8, 0.3, 0.2}, .tex = {0, 0}, },
			[7] = (struct vertex){ .pos = { 0.0,  0.2, 0.0}, .color = {0.8, 0.3, 0.2}, .tex = {0, 1}, },
		},
		.indices = {
			0, 1, 2, 3,
			0xFFFF,
			4, 5, 6, 7,
		},
	};

	/* Then, make up our transformation matrix, assuming it was made based on some calculation */
	render_data->transformation = (struct transformation){
		.mat = {
			{1, 0, 0, 0},
			{0, 1, 0, 0},
			{0, 0, 1, 1},
			{0, 0, 0, 1},
		},
	};

	/* We already saw how to fill and copy buffers in Tutorial 8 */
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], render_data->objects.vertices, sizeof render_data->objects.vertices, "staging vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_TRANSFORMATION], &render_data->transformation, sizeof render_data->transformation, "transformation");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_copy_buffer(dev, essentials, &render_data->buffers[BUFFER_VERTICES], &render_data->buffers[BUFFER_VERTICES_STAGING],
			sizeof render_data->objects.vertices, "vertex");
	if (!tut1_error_is_success(&retval))
		return retval;
	/*
	 * Since the vertex buffer is bigger than the index buffer, we can use the staging vertex buffer to copy data
	 * to the index buffer as well.
	 */
	retval = tut8_render_fill_buffer(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], render_data->objects.indices, sizeof render_data->objects.indices, "staging index");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_copy_buffer(dev, essentials, &render_data->buffers[BUFFER_INDICES], &render_data->buffers[BUFFER_VERTICES_STAGING],
			sizeof render_data->objects.indices, "index");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * TODO: Enable after the NVidia driver fixes its bug with handling NULL pointers.
	 * tut7_free_buffers(dev, &render_data->buffers[BUFFER_VERTICES_STAGING], 1);
	 */

	/*
	 * We are going to do a similar thing with our image texture; first fill the staging image, then copy it to the
	 * image we are actually going to use.
	 *
	 * Now the contents of the image usually comes from a file, e.g. in a `.ktx` format.  To avoid adding more
	 * dependencies to the tutorials, let's just generate the image with something simple.
	 */
	retval = generate_texture(dev, &render_data->images[IMAGE_TEXTURE_STAGING]);
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * The situation with buffers was very simple; copy the staging buffer to the real buffer and just use it.
	 * Images however, have layouts.  The staging buffer had a PREINITIALIZED layout so that when we change its
	 * layout, the contents would remain.  Our real image currently has an UNDEFINED layout.  This was decided in
	 * `tut7_create_images`.  To copy an image A into an image B, Vulkan requires the layout of the images to be
	 * either GENERAL, or TRANSFER_SRC_OPTIMAL for A or TRANSFER_DST_OPTIMAL for B.
	 *
	 * After the image copy, we would want to use our texture inside the shaders, so we should change its layout
	 * again and this time to SHADER_READ_ONLY_OPTIMAL.
	 */
	retval = tut8_render_transition_images(dev, essentials, &render_data->images[IMAGE_TEXTURE_STAGING], 1,
			VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, "staging texture");
	if (!tut1_error_is_success(&retval))
		return retval;
	retval = tut8_render_transition_images(dev, essentials, &render_data->images[IMAGE_TEXTURE], 1,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, "texture");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * We are ready to copy the image now.  Note that we could have also used a staging *buffer* instead of an
	 * image, and copy the buffer over the image and we would have had one less transition to make.  This tutorial
	 * meant to highlight the necessary transitions that need to take place for images.
	 */
	VkImageCopy image_copy = {
		.srcSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.dstSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.extent = {
			.width = TEXTURE_WIDTH,
			.height = TEXTURE_HEIGHT,
		},
	};
	retval = tut8_render_copy_image(dev, essentials, &render_data->images[IMAGE_TEXTURE], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			&render_data->images[IMAGE_TEXTURE_STAGING], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, &image_copy, "texture");
	if (!tut1_error_is_success(&retval))
		return retval;

	/* Now, transition the image to SHADER_READ_ONLY_OPTIMAL */
	retval = tut8_render_transition_images(dev, essentials, &render_data->images[IMAGE_TEXTURE], 1,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, "texture");
	if (!tut1_error_is_success(&retval))
		return retval;

	/*
	 * TODO: Enable after the NVidia driver fixes its bug with handling NULL pointers.
	 * tut7_free_images(dev, &render_data->images[IMAGE_TEXTURE_STAGING], 1);
	 */

	/* Shaders */
	render_data->shaders[SHADER_VERTEX] = (struct tut7_shader){
		.spirv_file = "../shaders/tut9.vert.spv",
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
	};
	render_data->shaders[SHADER_FRAGMENT] = (struct tut7_shader){
		.spirv_file = "../shaders/tut9.frag.spv",
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
	struct tut8_resources resources = {
		.images = render_data->images,
		.image_count = 1,
		.buffers = render_data->buffers,
		.buffer_count = 1,
		.shaders = render_data->shaders,
		.shader_count = 2,
		.render_pass = render_data->render_pass,
	};
	render_data->layout = (struct tut8_layout){
		.resources = &resources,
	};
	/*
	 * Note: `tut8_make_graphics_layouts` is assigning bindings from 0 up, starting with images followed by
	 * buffers.  In this case therefore, our texture would become binding 0 and our transformation matrix becomes
	 * binding 1.
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
		/* We have an additional attribute here; the texture coordinates */
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
	VkDescriptorImageInfo set_write_image_info = {
		.sampler = render_data->images[IMAGE_TEXTURE].sampler,
		.imageView = render_data->images[IMAGE_TEXTURE].view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	VkDescriptorBufferInfo set_write_buffer_info = {
		.buffer = render_data->buffers[BUFFER_TRANSFORMATION].buffer,
		.offset = 0,
		.range = VK_WHOLE_SIZE,
	};
	VkWriteDescriptorSet set_write[2] = {
		[0] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->desc_set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &set_write_image_info,
		},
		[1] = {
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = render_data->desc_set,
			.dstBinding = 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &set_write_buffer_info,
		},
	};
	vkUpdateDescriptorSets(dev->device, 2, set_write, 0, NULL);

	return retval;
}

static void free_render_data(struct tut2_device *dev, struct tut7_render_essentials *essentials, struct render_data *render_data)
{
	vkDeviceWaitIdle(dev->device);

	tut8_free_pipelines(dev, &render_data->pipeline, 1);
	tut8_free_layouts(dev, &render_data->layout, 1);
	tut7_free_images(dev, render_data->images, 1);
	tut7_free_buffers(dev, render_data->buffers, 3);
	tut7_free_shaders(dev, render_data->shaders, 2);
	tut7_free_graphics_buffers(dev, render_data->gbuffers, essentials->image_count, render_data->render_pass);

	free(render_data->gbuffers);
}

static tut1_error generate_texture(struct tut2_device *dev, struct tut7_image *image)
{
	tut1_error retval = TUT1_ERROR_NONE;
	size_t texture_size = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4 * sizeof(uint8_t);

	uint8_t *generated_texture = malloc(texture_size);
	if (generated_texture == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_failed;
	}

	/* The following code generates a brick-wall texture.  You don't need to follow the math. */
#define BRICK_WIDTH 50
#define BRICK_HEIGHT 17
#define BRICK_NOISE 40
	for (unsigned int i = 0; i < TEXTURE_HEIGHT; ++i)
	{
		unsigned int row = i / BRICK_HEIGHT;
		unsigned int h_gap_dist = i - row * BRICK_HEIGHT;
		bool h_gap = h_gap_dist == 0;
		bool h_dark_edge = h_gap_dist == 1;
		bool h_light_edge = h_gap_dist == BRICK_HEIGHT - 1;

		for (unsigned int j = 0; j < TEXTURE_WIDTH; ++j)
		{
			unsigned int col_offset = row % 2?0:BRICK_WIDTH / 2;
			unsigned int col = (j + col_offset) / BRICK_WIDTH;
			unsigned int v_gap_dist = j + col_offset - col * BRICK_WIDTH;
			bool v_gap = v_gap_dist == 0;
			bool v_light_edge = v_gap_dist == 1;
			bool v_dark_edge = v_gap_dist == BRICK_WIDTH - 1;

			uint8_t color = 0xAF;
			if (h_gap || v_gap)
				color = 0;
			else if (h_dark_edge || v_dark_edge)
				color = 0x5F;
			else if (h_light_edge || v_light_edge)
				color = 0xFF;

			if (color > BRICK_NOISE)
				color -= BRICK_NOISE + rand() % (BRICK_NOISE + 1);

			size_t pixel = (i * TEXTURE_WIDTH + j) * 4 * sizeof(uint8_t);
			generated_texture[pixel + 0] = color;	/* B */
			generated_texture[pixel + 1] = color;	/* G */
			generated_texture[pixel + 2] = color;	/* R */
			generated_texture[pixel + 3] = 0xFF;	/* A */
		}
	}
#undef BRICK_WIDTH
#undef BRICK_HEIGHT
#undef BRICK_NOISE

	/*
	 * Fill the image with what we just generated.  We could have avoided an allocation by generating the texture
	 * directly into the memory-mapped contents of the image.
	 */
	retval = tut8_render_fill_image(dev, image, generated_texture, texture_size, "staging texture");

exit_failed:
	free(generated_texture);

	return retval;
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

		/* We saw all this in Tutorial 8.  Any changes are commented. */
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
		/* Bind the index buffer as well.  Our indices are 16-bit values, so we have to indicate that here. */
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

		/* Draw */

		/*
		 * The draw call here is now using the indexed variant.  We are saying here that 9 indices must be used
		 * (4 for the first triangle strip, 1 for the 0xFFFF separator, and 4 for the second triangle strip).
		 * The instance count is 1 (as in Tutorial 8) and we are not particularly interested in it now.
		 *
		 * Vulkan allows us to also add an offset to every index read (0 here).  In our case, since the two
		 * triangle strips have a similar pattern, we could have issued two calls to vkCmdDrawIndexed, one with
		 * offset 0 and one with offset 4.  That way, the size of our index list would have been reduced to 4.
		 * In this particular case, the memory saved is not worth the cost of the second Draw call.
		 */
		vkCmdDrawIndexed(essentials.cmd_buffer, 9, 1, 0, 0, 0);

		vkCmdEndRenderPass(essentials.cmd_buffer);

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
