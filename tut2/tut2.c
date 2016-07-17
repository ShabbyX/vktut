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
#include "tut2.h"

tut1_error tut2_get_dev(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count)
{
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/*
	 * A Vulkan logical device is a connection to a physical device, which is used to interact with that device.
	 * In Tutorial 1, we gathered information regarding the physical device.  Now, we want to actually do something
	 * with it.
	 *
	 * By now, you should be familiar with how this works.  There is a VkDeviceCreateInfo struct that gives
	 * information on what is expected of the device, and a vkDeviceCreate function that performs the action.
	 * There is the usual optional argument to use user-provided allocation callbacks, which is ignored in this
	 * tutorial.
	 */
	*dev = (struct tut2_device){0};

	/*
	 * As we have seen in Tutorial 1, the physical device can possibly support multiple queue families, each with
	 * different capabilities.  When create a logical device, we also ask for a set of queues to be allocated for
	 * us.  Here, I'm using the information we recovered in Tutorial 1 and ask for all available queues that match
	 * any of the capabilities requested (`qflags`).
	 *
	 * The VkDeviceQueueCreateInfo takes the index of queue family (as recovered in the list of
	 * VkQueueFamilyProperties) so that the driver knows which queue family you are referring to, as well as the
	 * number of actual queues that need to be dedicated to the application.  Each queue in the queue family is
	 * given a priority between 0 (low) and 1 (high) and therefore an array of priorities is also provided to
	 * VkDeviceQueueCreateInfo.  What effects priorities actually have on the execution of queues are left to the
	 * drivers.  We'll ignore that and just give them all an equal priority of 0.
	 */
	uint32_t max_queue_count = *queue_info_count;
	*queue_info_count = 0;

	uint32_t max_family_queues = 0;
	for (uint32_t i = 0; i < phy_dev->queue_family_count; ++i)
		if (max_family_queues < phy_dev->queue_families[i].queueCount)
			max_family_queues = phy_dev->queue_families[i].queueCount;
	float queue_priorities[max_family_queues];
	memset(queue_priorities, 0, sizeof queue_priorities);

	for (uint32_t i = 0; i < phy_dev->queue_family_count && i < max_queue_count; ++i)
	{
		/* Check if the queue has any of the desired capabilities.  If so, add it to the list of desired queues */
		if ((phy_dev->queue_families[i].queueFlags & qflags) == 0)
			continue;

		queue_info[(*queue_info_count)++] = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = i,
			.queueCount = phy_dev->queue_families[i].queueCount,
			.pQueuePriorities = queue_priorities,
		};
	}

	/* If there are no compatible queues, there is little one can do here */
	if (*queue_info_count == 0)
	{
		tut1_error_set_vkresult(&retval, VK_ERROR_FEATURE_NOT_PRESENT);
		goto exit_failed;
	}

	/*
	 * The VkDeviceCreateInfo asks for resources in the driver to be dedicated to the application.  We already saw
	 * this with the defining the desired queues above.  We should also do this with the features we request from
	 * the device.  Here, I will ask for all the features that the device has (information which was queried in
	 * Tutorial 1).  This may in general not be desirable though; for example robostBufferAccess may be forced to
	 * VK_FALSE to disable the overhead of array bounds checking.
	 *
	 * Additionally, one could enable layers and extensions similar to vkCreateInstance.  The layers and extensions
	 * enabled with vkCreateInstance are those that apply the Vulkan API in general, such as a validation layer,
	 * but the layers and extensions enabled with vkDeviceCreate are more specific to the device itself.  In either
	 * case, they will be explored in a future tutorial.
	 */
	VkDeviceCreateInfo dev_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = *queue_info_count,
		.pQueueCreateInfos = queue_info,
		.pEnabledFeatures = &phy_dev->features,
	};

	/*
	 * All API functions that create something follow the same general rule.  The handle of the object to be
	 * created is provided last, an optional argument providing memory allocation callbacks is present, and a
	 * CreateInfo struct is passed.  The parent object is passed first (except for vkCreateInstance, where
	 * there is no parent).
	 *
	 * vkCreateDevice is no exception.  We are not yet using custom memory allocation functions, so they are
	 * unused.  The rest is already explained.
	 */
	res = vkCreateDevice(phy_dev->physical_device, &dev_info, NULL, &dev->device);
	tut1_error_set_vkresult(&retval, res);

exit_failed:
	return retval;
}

tut1_error tut2_get_commands(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkDeviceQueueCreateInfo queue_info[], uint32_t queue_info_count)
{
	tut1_error retval = TUT1_ERROR_NONE;
	VkResult res;

	/*
	 * Now that we have a device handle, we can start talking with it using command buffers.  A command buffer is
	 * a recording of actions to be taken by the device.  Vulkan works by allowing a thread to record its command
	 * buffers and once done, submit it to the driver for execution.  The commands themselves are numerous and
	 * including commands to draw (for graphics), compute, execute a secondary buffer, copy images, etc.
	 *
	 * To efficiently create command buffers, a command buffer pool is used for each queue family.  A pool is
	 * generally a set of preallocated objects (possibly in contiguous memory), which can be used to "allocate"
	 * and "free" objects of those type more efficiently.  If you don't already know what a pool is, you can think
	 * of it as a specialized `malloc`/`free` implementation that is faster because it already knows the object
	 * sizes in advance.  The pool memory itself is created using the usual memory allocation functions.
	 */
	dev->command_pools = malloc(queue_info_count * sizeof *dev->command_pools);
	if (dev->command_pools == NULL)
	{
		tut1_error_set_errno(&retval, errno);
		goto exit_failed;
	}

	for (uint32_t i = 0; i < queue_info_count; ++i)
	{
		struct tut2_commands *cmd = &dev->command_pools[i];
		*cmd = (struct tut2_commands){0};

		/*
		 * Remember the actual queue flags for each queue, so that later we can know which queue had which of
		 * the capabilities we asked of it.
		 */
		cmd->qflags = phy_dev->queue_families[queue_info[i].queueFamilyIndex].queueFlags;

		/*
		 * The vkCreateCommandPool takes a VkCommandPoolCreateInfo that tells it what queue family the pool
		 * would be creating command buffers from.  Apart from that, you can provide flags that indicate your
		 * usage of the command pool, to help it better optimize the process.  For example, if you create and
		 * destroy command buffers frequently, you could give VK_COMMAND_POOL_CREATE_TRANSIENT_BIT to the
		 * flags.  If you would like each command buffer to be resettable separately (we will get to what this
		 * means later), you can give VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT to flags.  For now, let's
		 * go with resettable command buffers.
		 */
		VkCommandPoolCreateInfo pool_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = queue_info[i].queueFamilyIndex,
		};

		/* Does this start to look familiar? */
		res = vkCreateCommandPool(dev->device, &pool_info, NULL, &cmd->pool);
		tut1_error_set_vkresult(&retval, res);
		if (res < 0)
			goto exit_failed;
		++dev->command_pool_count;

		/*
		 * Once a pool is created, we are free to allocate as many command buffers as we like.  Beforehand,
		 * however, you need to know more about queues.  We already saw how there are different queue families
		 * with different properties.  The driver may allow multiple queues of the same family.  Different
		 * threads can submit their work on different queues without conflict.  Later, they would need to
		 * synchronize the queues, but we will get to that in a future tutorial.
		 *
		 * For now, let's create one command buffer per queue.
		 */

		cmd->queues = malloc(queue_info[i].queueCount * sizeof *cmd->queues);
		cmd->buffers = malloc(queue_info[i].queueCount * sizeof *cmd->buffers);
		if (cmd->queues == NULL || cmd->buffers == NULL)
		{
			tut1_error_set_errno(&retval, errno);
			goto exit_failed;
		}

		/*
		 * To get a queue, you can simply call vkGetDeviceQueue, asking for the family index and queue index
		 * of the queue you are interested from the device.  The number of queues available for a family is
		 * retrieved in Tutorial 1 in VkQueueFamilyProperties, which we used in the beginning of this function
		 * to set VkDeviceQueueCreateInfo's queueCount.
		 */
		for (uint32_t j = 0; j < queue_info[i].queueCount; ++j)
			vkGetDeviceQueue(dev->device, queue_info[i].queueFamilyIndex, j, &cmd->queues[j]);
		cmd->queue_count = queue_info[i].queueCount;

		/*
		 * To allocate command buffers, we can do take them in bulk from the pool.  Here, a vkAllocate*
		 * function is used instead of the usual vkCreate* functions we have seen so far.  However, the usage
		 * of the function is quite similar.  There is a VkCommandBufferAllocateInfo struct that defines how
		 * the allocation needs to take place, such as which pool to take from and how many command buffers to
		 * allocate.
		 *
		 * The command buffers can be primary or secondary.  A primary command buffer is one that can be
		 * submitted to a queue for execution, while a secondary command buffer is one that can be invoked by
		 * a primary command buffer, like a subroutine.  For now, we will only consider primary command
		 * buffers.
		 */
		VkCommandBufferAllocateInfo buffer_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = cmd->pool,
			.commandBufferCount = queue_info[i].queueCount,
		};

		/*
		 * vkAllocateCommandBuffers would do the actual allocation of the command buffers.  Its usage is
		 * similar to the vkCreate* functions we have so far seen in Vulkan.  Notable difference is the lack
		 * of memory allocation functions.  This is because the command buffer pool that the allocation is
		 * being made from already took these callbacks!
		 */
		res = vkAllocateCommandBuffers(dev->device, &buffer_info, cmd->buffers);
		tut1_error_set_vkresult(&retval, res);
		if (res)
			goto exit_failed;

		cmd->buffer_count = queue_info[i].queueCount;
	}

exit_failed:
	return retval;
}

void tut2_cleanup(struct tut2_device *dev)
{
	/*
	 * Before destroying a device, we must make sure that there is no ongoing work.  vkDeviceWaitIdle, as the name
	 * suggests, blocks until the device is idle.
	 */
	vkDeviceWaitIdle(dev->device);

	/*
	 * Before cleaning up a device, any allocation made with the device needs to be freed to prevent memory leaks.
	 * In this tutorial, we have created command pools, and therefore we need to destroy them.  Any command buffer
	 * allocated from the pool is implicitly freed, so we don't need to take explicit actions for that.
	 *
	 * The optional memory allocation callbacks are unused because they were unused when creating the pool.
	 */
	for (uint32_t i = 0; i < dev->command_pool_count; ++i)
	{
		free(dev->command_pools[i].queues);
		free(dev->command_pools[i].buffers);
		vkDestroyCommandPool(dev->device, dev->command_pools[i].pool, NULL);
	}
	free(dev->command_pools);

	/*
	 * The device can now be destroyed.  As common with other vkDestroy* functions, vkDestroyDevice takes the
	 * device to destroy and the memory allocation callbacks, which are unused.  The allocated queues are
	 * automatically freed as the device is destroyed.
	 */
	vkDestroyDevice(dev->device, NULL);

	*dev = (struct tut2_device){0};
}
