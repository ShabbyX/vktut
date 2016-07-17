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

#ifndef TUT3_H
#define TUT3_H

#include "../tut2/tut2.h"

struct tut3_pipeline
{
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;
	VkPipeline pipeline;
};

struct tut3_pipelines
{
	struct tut3_pipeline *pipelines;
	uint32_t pipeline_count;
};

tut1_error tut3_load_shader(struct tut2_device *dev, const char *spirv_file, VkShaderModule *shader);
void tut3_free_shader(struct tut2_device *dev, VkShaderModule shader);

tut1_error tut3_make_compute_pipeline(struct tut2_device *dev, struct tut3_pipelines *pipeline, VkShaderModule shader);
void tut3_destroy_pipeline(struct tut2_device *dev, struct tut3_pipelines *pipelines);

#endif
