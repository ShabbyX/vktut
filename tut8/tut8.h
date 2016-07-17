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

#ifndef TUT8_H
#define TUT8_H

#include "../tut7/tut7.h"

struct tut8_resources
{
	/* list of images */
	struct tut7_image *images;
	uint32_t image_count;

	/* list of buffers */
	struct tut7_buffer *buffers;
	uint32_t buffer_count;

	/* shaders */
	struct tut7_shader *shaders;
	uint32_t shader_count;

	/* push constants */
	VkPushConstantRange *push_constants;
	uint32_t push_constant_count;

	/* buffers to render to */
	struct tut7_graphics_buffers *graphics_buffers;
	uint32_t graphics_buffer_count;
	VkRenderPass render_pass;
};

struct tut8_layout
{
	/* inputs */

	struct tut8_resources *resources;

	/* outputs */

	/* layouts based on resources */
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;
};

struct tut8_pipeline
{
	/* inputs */

	struct tut8_layout *layout;

	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineTessellationStateCreateInfo tessellation_state;

	size_t thread_count;

	/* outputs */

	/* one pipeline per layout (i.e. set of resources) */
	VkPipeline pipeline;

	/* pool to allocate from */
	VkDescriptorPool set_pool;
};

tut1_error tut8_make_graphics_layouts(struct tut2_device *dev, struct tut8_layout *layouts, uint32_t layout_count);
tut1_error tut8_make_graphics_pipelines(struct tut2_device *dev, struct tut8_pipeline *pipelines, uint32_t pipeline_count);

void tut8_free_layouts(struct tut2_device *dev, struct tut8_layout *layouts, uint32_t layout_count);
void tut8_free_pipelines(struct tut2_device *dev, struct tut8_pipeline *pipelines, uint32_t pipeline_count);

#endif
