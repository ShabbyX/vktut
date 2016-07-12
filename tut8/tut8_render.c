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

#include "tut8_render.h"

VkResult tut8_render_fill_buffer(struct tut2_device *dev, struct tut7_buffer *to, void *from, size_t size, const char *name)
{
	void *mem = NULL;

	VkResult res = vkMapMemory(dev->device, to->buffer_mem, 0, size, 0, &mem);
	if (res)
	{
		printf("Failed to map memory of the %s buffer: %s\n", name, tut1_VkResult_string(res));
		return res;
	}

	memcpy(mem, from, size);

	vkUnmapMemory(dev->device, to->buffer_mem);

	return 0;
}

VkResult tut8_render_copy_buffer(struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_buffer *to, struct tut7_buffer *from, size_t size, const char *name)
{
	VkResult res;

	vkResetCommandBuffer(essentials->cmd_buffer, 0);
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	res = vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
	if (res)
	{
		printf("Couldn't begin recording a command buffer to copy the %s buffer: %s\n", name, tut1_VkResult_string(res));
		return res;
	}

	/* Let's see if you can figure out this very complicated operation! */
	VkBufferCopy copy_region = {
		.srcOffset = 0,
		.dstOffset = 0,
		.size = size,
	};
	vkCmdCopyBuffer(essentials->cmd_buffer, from->buffer, to->buffer, 1, &copy_region);

	vkEndCommandBuffer(essentials->cmd_buffer);

	res = vkResetFences(dev->device, 1, &essentials->exec_fence);
	if (res)
	{
		printf("Failed to reset fence: %s\n", tut1_VkResult_string(res));
		return res;
	}

	/* Submit the command buffer to go ahead with the copy, and wait for it to finish */
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &essentials->cmd_buffer,
	};

	vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
	return vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
}

VkResult tut8_render_transition_images(struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *images, uint32_t image_count,
		VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect, const char *name)
{
	VkResult res;

	vkResetCommandBuffer(essentials->cmd_buffer, 0);
	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	};
	res = vkBeginCommandBuffer(essentials->cmd_buffer, &begin_info);
	if (res)
	{
		printf("Couldn't begin recording a command buffer to transition the %s image: %s\n", name, tut1_VkResult_string(res));
		return res;
	}

	/*
	 * We have already seen how image transition is done in Tutorial 7.  This is very similar, and in fact simpler,
	 * because we are only doing the transition in this command buffer submission.  In other words, we don't need
	 * to think about pipeline stages, or src and dst accesses.
	 */
	VkImageMemoryBarrier image_barrier = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = 0,//VK_ACCESS_MEMORY_READ_BIT,
		.dstAccessMask = 0,//VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = from,
		.newLayout = to,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.subresourceRange = {
			.aspectMask = aspect,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS,
		},
	};

	for (uint32_t i = 0; i < image_count; ++i)
	{
		image_barrier.image = images[i].image;
		vkCmdPipelineBarrier(essentials->cmd_buffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				0,			/* no flags */
				0, NULL,		/* no memory barriers */
				0, NULL,		/* no buffer barriers */
				1, &image_barrier);	/* our image transition */
	}

	vkEndCommandBuffer(essentials->cmd_buffer);

	res = vkResetFences(dev->device, 1, &essentials->exec_fence);
	if (res)
	{
		printf("Failed to reset fence: %s\n", tut1_VkResult_string(res));
		return res;
	}

	/* Submit the command buffer to go ahead with the copy, and wait for it to finish */
	VkSubmitInfo submit_info = {
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount = 1,
		.pCommandBuffers = &essentials->cmd_buffer,
	};

	vkQueueSubmit(essentials->present_queue, 1, &submit_info, essentials->exec_fence);
	return vkWaitForFences(dev->device, 1, &essentials->exec_fence, true, 1000000000);
}
