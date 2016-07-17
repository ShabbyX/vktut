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
#include <stdio.h>
#include <string.h>
#include "tut3.h"

tut1_error tut3_load_shader(struct tut2_device *dev, const char *spirv_file, VkShaderModule *shader)
{
	/*
	 * Allocating a shader is easy.  Similar to other vkCreate* functions, a CreateInfo structure is taken that
	 * describes the shader itself.  A set of memory allocator callbacks could be given which we don't use for now.
	 * The VkShaderModuleCreateInfo struct, besides the usual attributes (such as sType or flags), simply takes the
	 * SPIR-V code of the shader.
	 */
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;
	void *code = NULL;
	size_t size = 0, cur = 0;
	FILE *fin = fopen(spirv_file, "rb");

	*shader = NULL;

	if (fin == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_no_file;
	}

	/* Get the size of the file */
	fseek(fin, 0, SEEK_END);
	size = ftell(fin);
	fseek(fin, 0, SEEK_SET);

	/* Allocate memory for the code */
	code = malloc(size);
	if (code == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_no_mem;
	}

	/* Read all of the SPIR-V file */
	while (cur < size)
	{
		size_t read = fread(code + cur, 1, size - cur, fin);
		if (read == 0)
		{
			tut1_error_set_errno(&retval, errno);
			goto exit_io_error;
		}
		cur += read;
	}

	/* Finally, create the shader module by specifying its code */
	VkShaderModuleCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = size,
		.pCode = code,
	};

	res = vkCreateShaderModule(dev->device, &info, NULL, shader);
	tut1_error_set_vkresult(&retval, res);

exit_io_error:
	free(code);
exit_no_mem:
	fclose(fin);
exit_no_file:
	return retval;
}

void tut3_free_shader(struct tut2_device *dev, VkShaderModule shader)
{
	/*
	 * Destroying a shader is similar to other vkDestroy* functions.  The shader object itself is taken as well
	 * as the device for which it was created for.  Since we didn't use custom allocators, we are not providing
	 * them here either.
	 */
	vkDestroyShaderModule(dev->device, shader, NULL);
}

tut1_error tut3_make_compute_pipeline(struct tut2_device *dev, struct tut3_pipelines *pipelines, VkShaderModule shader)
{
	/*
	 * For the command buffers to execute commands, they need to be bound to a pipeline.  From OpenGL, you must be
	 * familiar with what a pipeline is: the elements to draw (points, lines, triangles etc) are sent to the vertex
	 * shader, possibly tessellation shader, possibly geometric shader, rasterization and finally fragment shader.
	 * Vulkan pipelines are the same, except they are explicitly defined objects and you can have more than one of
	 * them.   Compute pipelines are simpler than graphics pipelines; data is sent to a compute shader and that's
	 * it.  That's why in this tutorial we focus on the simpler Compute pipelines.  Once we are comfortable with
	 * the way Vulkan handles the objects and how they interact with the pipelines, we will move to graphics.
	 *
	 * I recommend taking a look at the Pipelines section of the Vulkan specification, there is a nice graph of
	 * the graphics and compute pipelines.
	 *
	 * To be able to send data to the pipelines, and be able to locate those data from the shaders, a fair bit of
	 * object layout management needs to be done by the application.  First, let's talk about descriptor sets:
	 *
	 * A descriptor set is a set of objects (such as buffer or image).  Each object in the set is called a binding.
	 * The descriptor set groups these objects so they can be bound together for efficiency reasons.  Each object
	 * could also be an array.  In GLSL, this would look like this:
	 *
	 *     layout (set=m, binding=n) uniform sampler2D variableName;
	 *     layout (set=r, binding=s) uniform sampler2D variableNameArray[L];
	 *
	 * The above code makes the shader use `variableName` to refer to binding index `n` from descriptor set index
	 * `m`, and use `variableNameArray` to refer to binding index `s` from descriptor set index `s`, where the
	 * array elements are in the same order as defined by the application.
	 *
	 * The following is an example of a descriptor set *layout*:
	 *
	 *      set_4: { Buffer, Buffer, Image, Buffer, Image[10], Image }
	 *
	 * which (declared to vulkan as the 5th descriptor set layout) says that set index 4 has 6 bindings, where the
	 * first two are buffers, the third is an image, the fourth is a buffer, the fifth is an array of images of
	 * size 10, and the last is also an image.
	 *
	 * Note that the descriptor set layout declares just the layout!  At a later stage, actual data needs to be
	 * bound to these objects.
	 *
	 * ---
	 *
	 * In each stage of a pipeline, various descriptor sets can be made accessible.  For example, if there are
	 * three descriptor sets, descriptor sets 0 and 2 could be made available to the vertex shader and descriptor
	 * sets 1 and 2 to the fragment shader.  To define this relationship, a pipeline layout is created.  The
	 * pipeline layout additionally declares the push constants available to each state.  Push constants are
	 * special small values that can be sent to the shaders more efficiently than other methods (such as buffers
	 * and images).  For now, let's ignore push constants.
	 */

	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;
	uint32_t cmd_buffer_count = 0;

	*pipelines = (struct tut3_pipelines){0};

	/* Count the total number of command buffers, to create that many pipelines */
	for (uint32_t i = 0; i < dev->command_pool_count; ++i)
		cmd_buffer_count += dev->command_pools[i].buffer_count;

	/* Allocate memory for the pipelines */
	pipelines->pipelines = malloc(cmd_buffer_count * sizeof *pipelines->pipelines);
	if (pipelines->pipelines == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_failed;
	}
	memset(pipelines->pipelines, 0, cmd_buffer_count * sizeof *pipelines->pipelines);
	pipelines->pipeline_count = cmd_buffer_count;

	for (uint32_t i = 0; i < cmd_buffer_count; ++i)
	{
		VkDescriptorSetLayoutBinding set_layout_binding;
		VkDescriptorSetLayoutCreateInfo set_layout_info;
		VkPipelineLayoutCreateInfo pipeline_layout_info;
		VkComputePipelineCreateInfo pipeline_info;

		struct tut3_pipeline *pl = &pipelines->pipelines[i];

		/*
		 * For our compute shader, we are going to choose a simple layout; a single binding that is a buffer.
		 * Creating the descriptor set layout is similar to all other vkCreate* functions.
		 *
		 * The CreateInfo of the descriptor set layout simply takes the bindings in the set as an array.  We
		 * have only one of it.  The binding description itself consists of several fields.  The binding index
		 * is the index the shader would use to refer to this object.  In the GLSL example above, `binding=n`
		 * matches the object with `binding` index set to `n`.  The descriptor type is the type of the object,
		 * which could be a buffer, an image, texel buffer, etc.  The descriptor count is the size of the
		 * array, if the object is an array.  A size of 1 means that it's not an array.  Finally, the binding
		 * description tells which pipeline stages the object can be used in.  In our case, we want to use the
		 * object in the compute stage (our only stage!).
		 */
		set_layout_binding = (VkDescriptorSetLayoutBinding){
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		};

		set_layout_info = (VkDescriptorSetLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &set_layout_binding,
		};

		res = vkCreateDescriptorSetLayout(dev->device, &set_layout_info, NULL, &pl->set_layout);
		tut1_error_set_vkresult(&retval, res);
		if (res)
			goto exit_failed;

		/*
		 * To create a pipeline layout, we need to know the descriptor set layouts and push constant ranges
		 * used within the pipelines.  The single descriptor set layout we are going to use is already created
		 * and we are going to ignore push constants, so this is quite simple.
		 */
		pipeline_layout_info = (VkPipelineLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = 1,
			.pSetLayouts = &pl->set_layout,
		};

		res = vkCreatePipelineLayout(dev->device, &pipeline_layout_info, NULL, &pl->pipeline_layout);
		tut1_error_set_vkresult(&retval, res);
		if (res)
			goto exit_failed;

		/*
		 * Pipelines can be created more than one at a time.  In this tutorial, we will create one pipeline for
		 * command buffer (created in tutorial 2).  Creating the pipelines themselves is similar to all other
		 * vkCreate* functions, but with the pipelines and the CreateInfo struct possibly an array.  As always,
		 * we don't use the allocator callbacks for now.  There is one more argument to vkCreate*Pipeline
		 * though, and that's a pipeline cache.
		 *
		 * Pipeline caches can be used to store a pipeline to file, and later on retrieve the pipeline instead
		 * of building one anew.  This is purely for performance, and we will ignore it for now.
		 *
		 * In this tutorial, we will create the pipeline objects one by one for simplicity.
		 *
		 * The CreateInfo of the pipeline takes a set of flags.  One of the flags disallows optimization of the
		 * pipeline, for faster creation.  We don't care about that.  Another flag says that other pipelines
		 * can be derived from this one, again as optimization, where the pipelines are very similar.  The
		 * other flag indicates that this pipeline is derived from another pipeline.  We won't be using
		 * pipeline derivation for now either.
		 *
		 * The CreateInfo also takes the shader itself.  Since we are creating a compute shader, there is only
		 * one shader stage; a compute shader.  Let's assume the entry point of this shader is called "main".
		 *
		 * The CreateInfo gets the pipeline layout as well, which we have already created above.
		 */

		pipeline_info = (VkComputePipelineCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_COMPUTE_BIT,
				.module = shader,
				.pName = "main",
			},
			.layout = pl->pipeline_layout,
		};

		res = vkCreateComputePipelines(dev->device, NULL, 1, &pipeline_info, NULL, &pl->pipeline);
		tut1_error_set_vkresult(&retval, res);
		if (res)
			goto exit_failed;
	}

exit_failed:
	return retval;
}

void tut3_destroy_pipeline(struct tut2_device *dev, struct tut3_pipelines *pipelines)
{
	vkDeviceWaitIdle(dev->device);

	for (uint32_t i = 0; i < pipelines->pipeline_count; ++i)
	{
		struct tut3_pipeline *pl = &pipelines->pipelines[i];

		/*
		 * Destroying the pipeline, pipeline layout and descriptor set layout is similar to other vkDestroy*
		 * functions we have seen so far.  The allocator callbacks are not used since they were not provided
		 * when the objects were created.
		 */
		vkDestroyPipeline(dev->device, pl->pipeline, NULL);
		vkDestroyPipelineLayout(dev->device, pl->pipeline_layout, NULL);
		vkDestroyDescriptorSetLayout(dev->device, pl->set_layout, NULL);
	}

	free(pipelines->pipelines);

	*pipelines = (struct tut3_pipelines){0};
}
