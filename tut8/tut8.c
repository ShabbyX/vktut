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
#include "tut8.h"

tut1_error tut8_make_graphics_layouts(struct tut2_device *dev, struct tut8_layout *layouts, uint32_t layout_count)
{
	/*
	 * We have created our buffers, images, shaders and everything else we need.  It is time to create descriptor
	 * sets, descriptor pools and pipelines just as we did in Tutorial 3.  Before that however, we need to define
	 * the layout of these descriptor sets and pipelines.
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	for (uint32_t i = 0; i < layout_count; ++i)
	{
		struct tut8_layout *layout = &layouts[i];
		struct tut8_resources *resources = layout->resources;

		layout->set_layout = NULL;
		layout->pipeline_layout = NULL;

		/*
		 * In Tutorial 3, we have already seen how to create descriptor set layouts and pipeline layouts.  The
		 * code here is just a little more involved version of the same code.
		 *
		 * The descriptor set layout we'll create for each layout would contain the images first and then the
		 * buffers.  Vulkan supports a few descriptor types, we'll cover a few of the more common ones:
		 *
		 * - Storage image: this is really a buffer that is accessed as an image.  GLSL: uniform imageND.
		 * - Sampler: this contains parameters (such as coordinates) to sample from an image.  While it's
		 *   possible to have separate images and samplers, let's ignore them for now.  GLSL: uniform sampler.
		 * - Sampled image: this is an image you can sample from.  You can use a sampler to take data from the
		 *   image.  We're just going to go with the next type that combines the two, so we'll ignore this as
		 *   well.  GLSL: uniform textureND.
		 * - Combined image sampler: just an image and sampler combined together.  Vulkan says having the two
		 *   combined can sometimes be more efficient.  In GLSL: uniform samplerND.
		 * - Uniform texel buffer: a read-only buffer to provide data to shaders.  While this is very useful
		 *   for compute shaders, in graphics an image serves a similar purpose and is likely more appropriate.
		 *   So we'll ignore this type.  In GLSL: uniform samplerBuffer.
		 * - Storage texel buffer: a read-write version of uniform texel buffer.  We'll ignore this too.  In
		 *   GLSL: uniform imageBuffer.
		 * - Uniform buffer: a read-only user-defined structure of data.  This is useful for medium size data,
		 *   such as transformations to be applied to an object etc.  In GLSL: uniform ... { ... }
		 * - Storage buffer: a read-write version of uniform buffer.  In GLSL: buffer ... { ... }
		 * - Dynamic uniform buffer: a uniform buffer whose address and length are specified when binding the
		 *   descriptor set to the command buffer.  We'll ignore this for simplicity.  In GLSL: same syntax as
		 *   uniform buffer.
		 * - Dynamic storage buffer: a read-write version of dynamic uniform buffer.  We'll ignore this as
		 *   well.  In GLSL: same syntax as storage buffer.
		 * - Input attachment: a read-only image that can be of the same format of color or depth/stencil
		 *   attachments of the framebuffer (more on this later), perhaps to feed the output of one pass of
		 *   rendering to another (don't take my word for it though).  It seems that this is mostly relevant to
		 *   mobile, so we'll ignore it.  Either way, it's not used in a descriptor set layout either.  In
		 *   GLSL: uniform subpassInput.
		 */
		VkDescriptorSetLayoutBinding set_layout_bindings[resources->image_count + resources->buffer_count];
		uint32_t binding_count = 0;

		for (uint32_t j = 0; j < resources->image_count; ++j)
		{
			if ((resources->images[j].usage & (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)) == 0)
				continue;

			/*
			 * Each image in the layout is either a storage image, or a combined image sampler (we had
			 * decided to ignore sampled images).  We can actually infer this type from the `usage` that
			 * the image was created with; if it's sampled, it's a combined image sampler, if it's storage,
			 * it's a storage image.
			 *
			 * The properties of the image as given by the input already contain the rest of the
			 * information needed here.  We're going to ignore array types, so `descriptorCount` is just 1,
			 * otherwise it would have shown the number of elements in the array.  `stageFlags` lets the
			 * driver know at which stages of the pipeline the shaders are going to access this image.  The
			 * combined image samplers also require a sampler to be bound to them.  We could either bind a
			 * sampler to the image right here, in which case the sampler can never be changed for this
			 * image, or bind the sampler later.  Let's do the binding later.
			 */
			set_layout_bindings[binding_count] = (VkDescriptorSetLayoutBinding){
				.binding = binding_count,
				.descriptorType = resources->images[j].usage & VK_IMAGE_USAGE_SAMPLED_BIT?
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = 1,
				.stageFlags = resources->images[j].stage,
			};

			++binding_count;
		}

		for (uint32_t j = 0; j < resources->buffer_count; ++j)
		{
			if ((resources->buffers[j].usage & (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) == 0)
				continue;

			/*
			 * Each buffer in the layout is either a uniform or storage buffer (we had decided to ignore
			 * all other types).  Again, from the `usage` of the buffer we can infer this type.
			 *
			 * The `descriptorCount` is again 1 here as we ignore array types.  `stageFlags` is also
			 * provided as input similar to images.
			 */
			set_layout_bindings[binding_count] = (VkDescriptorSetLayoutBinding){
				.binding = binding_count,
				.descriptorType = resources->buffers[j].usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT?
					VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = 1,
				.stageFlags = resources->buffers[j].stage,
			};

			++binding_count;
		}

		/*
		 * Creating a descriptor set layout is done by simply declaring all the bindings.  We already saw this
		 * in Tutorial 3.
		 */
		VkDescriptorSetLayoutCreateInfo set_layout_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = binding_count,
			.pBindings = set_layout_bindings,
		};

		res = vkCreateDescriptorSetLayout(dev->device, &set_layout_info, NULL, &layout->set_layout);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/*
		 * Creating a graphics pipeline layout is similar to compute pipelines.  All you need is to declare
		 * what are the descriptor set layouts.  Although we are not using push constants yet, let's include
		 * them here as well, in case in the future we got interested them!  Push constants are useful for
		 * updating very few bytes of data to be used by the shaders.
		 */
		VkPipelineLayoutCreateInfo pipeline_layout_info = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &layout->set_layout,
			.pushConstantRangeCount = resources->push_constant_count,
			.pPushConstantRanges = resources->push_constants,
		};

		res = vkCreatePipelineLayout(dev->device, &pipeline_layout_info, NULL, &layout->pipeline_layout);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == layout_count?VK_SUCCESS:VK_INCOMPLETE);
	return retval;
}

tut1_error tut8_make_graphics_pipelines(struct tut2_device *dev, struct tut8_pipeline *pipelines, uint32_t pipeline_count)
{
	/*
	 * Each pipeline we create is going to have a set of shaders bound to it.  This means that if in one scene you
	 * are going to render objects with different shaders, you would need a separate pipeline for each part of that
	 * scene.
	 *
	 * To create descriptor sets, we need to know what resources will be used by the pipeline and assign correct
	 * bindings to them.  The tut8_resources struct describes what images, buffers and shaders are going to be
	 * used.  The buffers should not include index, vertex, and indirect ones as they are bound with special
	 * commands (more on this later).
	 */
	uint32_t successful = 0;
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	for (uint32_t i = 0; i < pipeline_count; ++i)
	{
		struct tut8_pipeline *pipeline = &pipelines[i];
		struct tut8_layout *layout = pipeline->layout;
		struct tut8_resources *resources = layout->resources;

		pipeline->pipeline = NULL;
		pipeline->set_pool = NULL;

		VkGraphicsPipelineCreateInfo pipeline_info;

		/*
		 * For each stage of the pipeline, one shader must be specified, with some stages being optional (such
		 * as geometry and tessellation).  Here, we'll trust the user has provided the shaders in the
		 * `resources` correctly, and we'll create the pipeline stages correspondingly.  Like in Tutorial 3,
		 * we'll just assume the shader entry point is `main`.
		 */
		bool has_tessellation_shader = false;
		VkPipelineShaderStageCreateInfo stage_info[resources->shader_count];
		for (uint32_t j = 0; j < resources->shader_count; ++j)
		{
			struct tut7_shader *shader = &resources->shaders[j];

			stage_info[j] = (VkPipelineShaderStageCreateInfo){
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = shader->stage,
				.module = shader->shader,
				.pName = "main",
			};
			if (shader->stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || shader->stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
				has_tessellation_shader = true;
		}

		/*
		 * The stages of the pipeline as computed above are specified, as well as the pipeline layout, the
		 * render pass describing attachments to the pipeline and the subpass the pipeline would be used in
		 * (of which we have only one).
		 *
		 * Aside from all this, there is a load of "state" information required to define the graphics
		 * pipeline.  Most of these states are heavily dependent on the actual program, so we'll just take them
		 * as input and leave their definition to `main()` unless otherwise specified.
		 *
		 * - vertex input: this defines vertex input information.  When recording a command buffer, a vertex
		 *   buffer is bound to provide the actual vertices to the vertex shader.  When creating the graphics
		 *   pipeline, we need to specify how the buffer contents translate to shader inputs.  For example,
		 *   take the following glsl declaration:
		 *
		 *         layout(location=0) in vec3 in_position;
		 *         layout(location=1) in vec2 in_texture;
		 *
		 *   Then the elements of our vertex buffer would look something like this:
		 *
		 *         struct vertex
		 *         {
		 *             float position[3];
		 *             float texture[2];
		 *         };
		 *
		 *   For the pipeline creation therefore, we need to specify that there is going to be 1 vertex buffer
		 *   (containing both data; this is just an example and there are alternative ways), that each element
		 *   is `sizeof(struct vertex)` bytes apart, that the input at the first location is `0` bytes into the
		 *   element while the input at the second location is `sizeof(float[3])` bytes into the element, and
		 *   what are the formats of the data.
		 *
		 * - input assembly: this defines how the vertices are combined to draw shapes.  From OpenGL, you are
		 *   likely familiar with Points, Lines, Triangles, Triangle Strips, Triangle Fans etc.  These shapes
		 *   are either disjoint (which Vulkan refers to as a "list" of shapes), or overlap in vertices.  In
		 *   the later case, if the vertices are accessed using an index list, a special index (0xFFFFFFFF or
		 *   0xFFFF depending on index size) can be used to restart the shape from that point on.  For example,
		 *   if you have three triangle fans to draw, you can either bind a vertex buffer, draw a triangle fan,
		 *   and repeat for the other two, or bind one buffer and use the special index in between the three
		 *   triangle fan sequence of vertices to get three separate triangle fans all in one go.
		 *
		 * - tessellation state: if tessellation shader is used, this defines the number of control points per
		 *   patch.
		 *
		 * - viewport state: this specifies what viewports and scissors are to be used for rendering.  We have
		 *   the option to make this dynamic, so let's do that and worry about it later.  We still need to
		 *   specify how many viewports and scissors will be used.
		 *
		 * - rasterization state: this controls some knobs on the rasterization automatically done by the
		 *   device, including which triangle face to draw, whether to fill them or draw them wireframe, what
		 *   line width to use etc.  We can find sensible values for these, so we'll assume them already.
		 *
		 * - multisample state: we are not using multisampling yet, and we'll specify that here.
		 *
		 * - depth stencil state: this controls behavior of depth and stencil tests automatically done by the
		 *   device, such as whether they are enabled and how to compare the values.  Depth testing is good, so
		 *   let's enable that.  Stencil is nice too, but unnecessary for now, so we'll keep that disabled.
		 *   Note that if the depth/stencil attachment is not provided, the depth test always passes, so we can
		 *   always disable depth testing in the future by simply not providing an attachment for it.
		 *
		 * - color blend state: if we had multiple color attachments, here is where we would define how all of
		 *   them get blended to create the final image.  We're using only one color attachment however, so we
		 *   will just set some defaults for it to not do anything.
		 *
		 * Below, we have specified as much information as possible, leaving two details to dynamic setting;
		 * viewports and scissors.  We must explicitly specify that these parameters are dynamically set when
		 * recording the command buffer, and they should in fact be set at that time.
		 */
		VkPipelineViewportStateCreateInfo viewport_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		};
		VkPipelineRasterizationStateCreateInfo rasterization_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1,
		};
		VkPipelineMultisampleStateCreateInfo multisample_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		};
		VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
		};
		VkPipelineColorBlendAttachmentState color_blend_attachments[1] = {
			[0] = {
				.blendEnable = false,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
						| VK_COLOR_COMPONENT_G_BIT
						| VK_COLOR_COMPONENT_B_BIT
						| VK_COLOR_COMPONENT_A_BIT,
			},
		};
		VkPipelineColorBlendStateCreateInfo color_blend_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = color_blend_attachments,
		};
		VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_state = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 2,
			.pDynamicStates = dynamic_states,
		};

		pipeline_info = (VkGraphicsPipelineCreateInfo){
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT,
			.stageCount = resources->shader_count,
			.pStages = stage_info,
			.pVertexInputState = &pipeline->vertex_input_state,
			.pInputAssemblyState = &pipeline->input_assembly_state,
			.pTessellationState = has_tessellation_shader?&pipeline->tessellation_state:NULL,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization_state,
			.pMultisampleState = &multisample_state,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blend_state,
			.pDynamicState = &dynamic_state,
			.layout = layout->pipeline_layout,
			.renderPass = resources->render_pass,
			.subpass = 0,
			.basePipelineIndex = 0,
		};

		/*
		 * We can now make the pipeline.  In a scene, you may have a handful of different pipelines, and we may
		 * have other pipelines in other scenes too.  That's a lot of pipelines!  One might think, big deal,
		 * it's just during startup anyway.  But it seems that it could be a big deal.  There are many objects
		 * we have to make a lot of, but Vulkan has the idea that creating pipelines in particular is
		 * expensive.
		 *
		 * Vulkan offers a way to avoid rebuilding pipelines, called a Pipeline Cache.  The short version of it
		 * is that you create a pipeline cache, give it to `vkCreateGraphicsPipelines` which adds built
		 * pipelines to the cache.  Once done, you store the cache in file.  On next startup, you load the
		 * cache from file and give it to `vkCreateGraphicsPipelines` which in turn uses it to recover
		 * already-built pipelines when possible.  There are features in place to take care of versions, change
		 * of graphics card or driver etc (in either case the cache is invalidated, the pipeline is rebuilt and
		 * the cache is updated).  By all means, use the pipeline cache!  But it's of little interest to us
		 * here.  The second argument to `vkCreateGraphicsPipelines` is the pipeline cache.
		 */
		res = vkCreateGraphicsPipelines(dev->device, NULL, 1, &pipeline_info, NULL, &pipeline->pipeline);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		/*
		 * Just like in Tutorial 4, we will create a descriptor set pool to allocate the descriptor sets out
		 * of.  As a reminder, when creating a descriptor pool, we need to specify how many sets can be
		 * allocated from it (which is thread_count in this case), and for each resource type, how many
		 * resources of that type can be allocated in total (which is thread_count*R_i where R_i is the number
		 * of resources we have of some type T_i).
		 *
		 * The following code makes the same assumptions as in `tut8_make_graphics_layouts` regarding which
		 * resources to count, as those are the resources described in the layout.  We had decided to only
		 * allow combined image samplers, storage images, uniform buffers and storage buffers, so we have four
		 * resource types to count.
		 *
		 * Note: we have so far used the same set of resources for defining descriptor sets for all threads.
		 * It is important to make sure the threads actually have synchronized access to the resources if they
		 * are really shared.  For example, if there is one uniform buffer that contains the transformation
		 * matrices, then one thread shouldn't be updating it while another is using it.  Such resources need
		 * to be made sure they are exclusively used.  An alternative solution is to make thread_count copies
		 * of that resource, even though only one of those copies is specified here for the sake of layout and
		 * pool creations.  Resources that don't need to be updated, for example a texture, can be safely
		 * shared between threads, as long as the queue families sharing the resource are specified at
		 * creation time.
		 */
		uint32_t image_sampler_count = 0;
		uint32_t storage_image_count = 0;
		uint32_t uniform_buffer_count = 0;
		uint32_t storage_buffer_count = 0;

		for (uint32_t j = 0; j < resources->image_count; ++j)
		{
			if ((resources->images[j].usage & VK_IMAGE_USAGE_SAMPLED_BIT))
				++image_sampler_count;
			else if ((resources->images[j].usage & VK_IMAGE_USAGE_SAMPLED_BIT))
				++storage_image_count;
		}

		for (uint32_t j = 0; j < resources->buffer_count; ++j)
		{
			if ((resources->buffers[j].usage & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
				++uniform_buffer_count;
			else if ((resources->buffers[j].usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
				++storage_buffer_count;
		}

		uint32_t pool_size_count = 0;
		VkDescriptorPoolSize pool_sizes[4];
		if (image_sampler_count > 0)
			pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = pipeline->thread_count * image_sampler_count,
			};

		if (storage_image_count > 0)
			pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
				.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
				.descriptorCount = pipeline->thread_count * storage_image_count,
			};

		if (uniform_buffer_count > 0)
			pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
				.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = pipeline->thread_count * uniform_buffer_count,
			};

		if (storage_buffer_count > 0)
			pool_sizes[pool_size_count++] = (VkDescriptorPoolSize){
				.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount = pipeline->thread_count * storage_image_count,
			};

		VkDescriptorPoolCreateInfo set_info = {
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets = pipeline->thread_count,
			.poolSizeCount = pool_size_count,
			.pPoolSizes = pool_sizes,
		};

		res = vkCreateDescriptorPool(dev->device, &set_info, NULL, &pipeline->set_pool);
		tut1_error_sub_set_vkresult(&retval, res);
		if (res)
			continue;

		++successful;
	}

	tut1_error_set_vkresult(&retval, successful == pipeline_count?VK_SUCCESS:VK_INCOMPLETE);
	return retval;
}

void tut8_free_layouts(struct tut2_device *dev, struct tut8_layout *layouts, uint32_t layout_count)
{
	vkDeviceWaitIdle(dev->device);

	/* As always... */
	for (uint32_t i = 0; i < layout_count; ++i)
	{
		vkDestroyPipelineLayout(dev->device, layouts[i].pipeline_layout, NULL);
		vkDestroyDescriptorSetLayout(dev->device, layouts[i].set_layout, NULL);
	}
}

void tut8_free_pipelines(struct tut2_device *dev, struct tut8_pipeline *pipelines, uint32_t pipeline_count)
{
	vkDeviceWaitIdle(dev->device);

	/* ZZzzzz... */
	for (uint32_t i = 0; i < pipeline_count; ++i)
	{
		vkDestroyPipeline(dev->device, pipelines[i].pipeline, NULL);
		vkDestroyDescriptorPool(dev->device, pipelines[i].set_pool, NULL);
	}
}
