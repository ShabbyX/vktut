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
#include <time.h>
#include <assert.h>
#include "tut4.h"

VkResult tut4_prepare_test(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut3_pipelines *pipelines,
		struct tut4_data *test_data, size_t buffer_size, size_t thread_count)
{
	/*
	 * In this tutorial, we will finally submit some work to the GPU!  For that, we need a couple of things.  To be
	 * able to compare performance with different number of threads, the test is designed to do some computation,
	 * such as tut3.comp shader, on a buffer with constant size, but divided among a couple of worker threads.
	 *
	 * In Tutorial 3, we set up the necessary pipelines and described the layout of the resources.  Here, we need
	 * to allocate those resources.  One storage texel buffer of given size is allocated.  A storage texel buffer
	 * is a generic buffer of compact data with both read and write access by the shader.  In the tut3.comp shader
	 * file, the format of this buffer is set to r32f, which means that each element is a 32-bit floating point.
	 * We will allocate buffer_size floats to this memory.
	 *
	 * Speaking of memory allocation, buffers and images require device memory to be allocated separately and
	 * assigned to them.  This is to allow optimization, which especially affects smaller objects, where large
	 * memory can be allocated on the device and divided up between the objects, preventing memory fragmentation
	 * as well as reducing book-keeping costs (book-keeping costs on the driver, that is).
	 *
	 * Furthermore, when it comes to texel buffers, a "buffer view" must be created to allow the shader to work
	 * with a specific section of the buffer.  This is once more for optimization; one large buffer where multiple
	 * shaders work on, each on a separate part.  In this tutorial, we will use buffer views to divide up the
	 * buffer among the worker threads.
	 *
	 * Once the resources are allocated, we need to create a descriptor set that realizes the layout we designed
	 * in Tutorial 3, give it the actual buffer view and bind the pipeline, command buffer and descriptor set all
	 * together.
	 *
	 * Finally, we need one more item to run the tests; a "fence".  There are multiple synchronization primitives
	 * in Vulkan to make sure you access data in order, memory caches won't bother you and such.  Briefly, these
	 * primitives are (note: host means your application, device means the GPU):
	 *
	 * - Fences: Used by the host to know when the device has finished executing a command buffer,
	 * - Semaphores: Used to coordinate command buffer execution between queues.  For example, queue A may execute
	 *   its fragment shader stage, and only when it moves to the next stage, queue B would move to its fragment
	 *   shader stage.  In this way, multiple threads could take turns going through their pipeline stages, with
	 *   the end result showing on the screen one after the other.  There are other uses as well.
	 * - Events: More fine-grained communication between queues can be achieved with events.  At any point, a queue
	 *   may wait for some other event from a different queue.  Your imagination can go wild.
	 * - Barriers: Barriers are used to create an execution dependency between commands in a command buffer, making
	 *   sure that memory access before the barrier happen before are visible to memory access after it.  In other
	 *   words, it makes sure memory accesses before and after don't get rearranged and that caches don't cause
	 *   problems.
	 *
	 * For now, we just want to know when each command buffer execution is finished, so we need a simple fence.
	 */

	VkResult retval;
	VkBufferCreateInfo buffer_info;
	VkMemoryAllocateInfo mem_info;
	VkDescriptorPoolCreateInfo set_pool_info;
	VkDescriptorPoolSize pool_size;
	VkMemoryRequirements mem_req;
	uint32_t mem_index;

	/*
	 * We want the buffer_size to be a multiple of 64 for each thread.  I will explain why when dispatching commands
	 * in the worker thread.
	 */
	buffer_size -= buffer_size % (64 * thread_count);

	*test_data = (struct tut4_data){
		.buffer_size = buffer_size,
		.dev = dev,
		.pipelines = pipelines,
	};

	/* At first, make sure there are enough command buffers for the threads */
	uint32_t cmd_buffer_count = 0;
	for (uint32_t i = 0; i < dev->command_pool_count; ++i)
		cmd_buffer_count += dev->command_pools[i].buffer_count;
	if (cmd_buffer_count < thread_count)
	{
		retval = VK_ERROR_TOO_MANY_OBJECTS;
		goto exit_failed;
	}

	/*
	 * The information needed to create a buffer are as follows.  If the buffer is sparse, some flags are required,
	 * but we are going to ignore sparse buffers for now.  The size of the buffer is taken, although memory is not
	 * allocated for it.  The usage of the buffer is taken of course, which would be as a storage texel buffer for
	 * this tutorial.  If the buffer was to be used to transfer data (whether to send or receive it), additional
	 * usage bit should be set.  Furthermore, if any part of the buffer was supposed to be shared between queue
	 * families, it should be declared with a special `sharingMode` requesting concurrent access to the buffer, and
	 * the list of queue families that could access the buffer should be declared.  We are dividing the buffer
	 * between the threads in such a way that each has its own part, so we don't need to have any of these set.
	 *
	 * As you know by now, we are not providing memory allocation callbacks.
	 */
	buffer_info = (VkBufferCreateInfo){
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size * sizeof(float),
		.usage = VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT,
	};

	retval = vkCreateBuffer(dev->device, &buffer_info, NULL, &test_data->buffer);
	if (retval)
		goto exit_failed;

	/*
	 * To allocate memory for the buffer, Vulkan can be queried for memory requirements for the object.  These
	 * requirements are size (which is at least buffer_info.size), alignment (which is automatically taken into
	 * account for device memory allocations), and memory types supporting this object.  We want the buffer to
	 * be accessible by host for initialization and verification, so we have to search through the memory types
	 * supporting the buffer and get one that is host visible.
	 *
	 * If the memory is not host-coherent, then we need to flush application writes, invalidate application caches
	 * before reads, and insert memory barriers after device writes.  For the sake of simplicity, as this tutorial
	 * is already huge, let's just ask for host-coherent memory.  Such a memory may not be available on all
	 * devices, so ideally you would want to look for host-coherent memory, and if not found be prepared to do the
	 * above flushes, cache invalidations and barriers.
	 */
	vkGetBufferMemoryRequirements(dev->device, test_data->buffer, &mem_req);
	mem_index = tut4_find_suitable_memory(phy_dev, dev, &mem_req,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (mem_index >= phy_dev->memories.memoryTypeCount)
	{
		retval = VK_ERROR_OUT_OF_DEVICE_MEMORY;
		goto exit_failed;
	}

	mem_info = (VkMemoryAllocateInfo){
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize = mem_req.size,
		.memoryTypeIndex = mem_index,
	};

	retval = vkAllocateMemory(dev->device, &mem_info, NULL, &test_data->buffer_mem);
	if (retval)
		goto exit_failed;

	/*
	 * We have the buffer and we have the underlying memory.  Let's bind them!  The memory is used only for this
	 * buffer, so the "offset" in memory where the buffer data resides is just 0.
	 */
	retval = vkBindBufferMemory(dev->device, test_data->buffer, test_data->buffer_mem, 0);
	if (retval)
		goto exit_failed;

	/*
	 * To create descriptor sets, Vulkan uses descriptor pools for efficiency reasons.  We saw a similar situation
	 * with command buffers in Tutorial 2, so this should be quite straightforward.
	 *
	 * If the pool allows individual descriptor sets to be returned to the pool, a corresponding flag needs to be
	 * set.  We won't need that in this tutorial.  Other information given to create the pool are the maximum
	 * number of descriptor sets that can be allocated from the pool, and how many descriptors of each type are
	 * allowed to be allocated.  We are going to have thead_count descriptor sets, all of the same type (storage
	 * texel buffer), so this is quite simple.
	 */
	pool_size = (VkDescriptorPoolSize){
		.type = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
		.descriptorCount = thread_count,
	};
	set_pool_info = (VkDescriptorPoolCreateInfo){
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = thread_count,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
	};

	retval = vkCreateDescriptorPool(dev->device, &set_pool_info, NULL, &test_data->set_pool);
	if (retval)
		goto exit_failed;

	/*
	 * Now that we have our buffer and descriptor pool allocated, we should create one buffer view, descriptor set
	 * and fence for each thread.
	 */
	test_data->per_cmd_buffer = malloc(thread_count * sizeof *test_data->per_cmd_buffer);
	if (test_data->per_cmd_buffer == NULL)
	{
		retval = VK_ERROR_OUT_OF_HOST_MEMORY;
		goto exit_failed;
	}
	memset(test_data->per_cmd_buffer, 0, thread_count * sizeof *test_data->per_cmd_buffer);
	test_data->per_cmd_buffer_count = thread_count;

	for (uint32_t i = 0; i < thread_count; ++i)
	{
		VkBufferViewCreateInfo buffer_view_info;
		VkDescriptorSetAllocateInfo set_info;
		VkWriteDescriptorSet set_write;
		VkFenceCreateInfo fence_info;
		struct tut4_per_cmd_buffer_data *per_cmd_buffer_data = &test_data->per_cmd_buffer[i];

		/*
		 * The buffer is divided into (nearly) equal chunks between the threads.  Each thread is assigned
		 * buffer_size / thread_count floats.  We ensured that buffer_size is divisible by thread_count at the
		 * top of this function.  The start and end index for each thread is kept for running the test.
		 */
		per_cmd_buffer_data->start_index = i * (buffer_size / thread_count);
		per_cmd_buffer_data->end_index = per_cmd_buffer_data->start_index + buffer_size / thread_count;
		assert(i != thread_count - 1 || per_cmd_buffer_data->end_index == buffer_size);

		/*
		 * Creating a buffer view is as usual.  The CreateInfo struct the take buffer for which the view is
		 * created, the format of the data (r32f as indicated in the shader), the offset in buffer memory (in
		 * bytes) where the view start and the range of the view (in bytes), that is how many bytes after the
		 * offset are visible through the view.
		 */
		buffer_view_info = (VkBufferViewCreateInfo){
			.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,
			.buffer = test_data->buffer,
			.format = VK_FORMAT_R32_SFLOAT,
			.offset = per_cmd_buffer_data->start_index * sizeof(float),
			.range = (per_cmd_buffer_data->end_index - per_cmd_buffer_data->start_index) * sizeof(float),
		};

		retval = vkCreateBufferView(dev->device, &buffer_view_info, NULL, &per_cmd_buffer_data->buffer_view);
		if (retval)
			goto exit_failed;

		/*
		 * Allocating descriptor sets from the pool are similar to allocating command buffers in Tutorial 2.  A
		 * number of descriptors can be allocated altogether, but for simplicity we'll allocate them one by one.
		 *
		 * The AllocateInfo struct for allocating descriptor sets takes the pool it is allocating from and the
		 * number of sets to allocate.  Most outstandingly, it requires the descriptor set layouts which the
		 * allocated sets would correspond to.  In Tutorial 3, we had created the descriptor set layouts
		 * already, so yeay.
		 */
		set_info = (VkDescriptorSetAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = test_data->set_pool,
			.descriptorSetCount = 1,
			.pSetLayouts = &pipelines->pipelines[i].set_layout,
		};

		retval = vkAllocateDescriptorSets(dev->device, &set_info, &per_cmd_buffer_data->set);
		if (retval)
			goto exit_failed;

		/*
		 * We now need to tell the descriptor set to use our buffer view.  This operation is called "updating
		 * the set".  The update can be achieved either through writing some of the bindings or copying them
		 * from elsewhere.  We will just write them one by one for now.  The information required for writing
		 * to the set are provided through VkWriteDescriptorSet objects, which can be provided multiple at a
		 * time (although we use only one because we have only one binding).  Each VkWriteDescriptorSet object
		 * itself can provide multiple contiguous bindings of the same type.  The information given to this
		 * object include the set we are writing to, the starting binding (0 in our case), the array element in
		 * that binding (unused here), the number of descriptors to update (just 1), the type of the descriptor
		 * (storage texel buffer) and the texel buffer view we created above, in the pTexelBufferView field.
		 * If we were updating images or normal buffers, the pImageInfo or pBufferInfo fields, respectively,
		 * would be filled instead.
		 */
		set_write = (VkWriteDescriptorSet){
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = per_cmd_buffer_data->set,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,
			.pTexelBufferView = &per_cmd_buffer_data->buffer_view,
		};

		/*
		 * With the write operation information ready, updating the set is as simple as telling Vulkan how many
		 * writes (1) and how many copies (0) to do.
		 */
		vkUpdateDescriptorSets(dev->device, 1, &set_write, 0, NULL);

		/*
		 * Creating a fence could not be any more straightforward.  The only information needed to create a
		 * fence is whether it starts as "signaled" or not.  When a command buffer finishes execution, it sets
		 * the fence to signaled, and that's how we know the command buffer is finished.  In the beginning of
		 * each execution, we reset the fence to unsignaled state, so we can't really care less what is its
		 * state after creation.
		 */
		fence_info = (VkFenceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		};

		retval = vkCreateFence(dev->device, &fence_info, NULL, &per_cmd_buffer_data->fence);
		if (retval)
			goto exit_failed;
	}

exit_failed:
	return retval;
}

void tut4_free_test(struct tut2_device *dev, struct tut4_data *test_data)
{
	vkDeviceWaitIdle(dev->device);

	for (uint32_t i = 0; i < test_data->per_cmd_buffer_count; ++i)
	{
		struct tut4_per_cmd_buffer_data *per_cmd_buffer_data = &test_data->per_cmd_buffer[i];

		/*
		 * Destroying objects by now should be known to you.  Objects that are "Create"d are "Destroy"ed, with
		 * memory allocation callbacks ignored.  Objects that are "Allocate"d from pools are automatically
		 * freed when the pool is destroyed.
		 */
		vkDestroyFence(dev->device, per_cmd_buffer_data->fence, NULL);
		vkDestroyBufferView(dev->device, per_cmd_buffer_data->buffer_view, NULL);
	}

	vkDestroyDescriptorPool(dev->device, test_data->set_pool, NULL);
	vkDestroyBuffer(dev->device, test_data->buffer, NULL);

	/*
	 * Although memory is taken and given back with Allocate/Free functions, but they behave the same way as
	 * Create/Destroy.
	 */
	vkFreeMemory(dev->device, test_data->buffer_mem, NULL);

	free(test_data->per_cmd_buffer);

	*test_data = (struct tut4_data){0};
}

uint32_t tut4_find_suitable_memory(struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		VkMemoryRequirements *mem_req, VkMemoryPropertyFlags properties)
{
	/*
	 * To find a suitable memory for the buffer, we will look inside at all the memory types available that support
	 * the memory requirements indicated by Vulkan (i.e. memory size and type) and that have the requested
	 * properties (i.e. visible to the shaders in the device).
	 */
	for (uint32_t i = 0; i < phy_dev->memories.memoryTypeCount; ++i)
	{
		/* If Vulkan says this type doesn't support the object, ignore it */
		if ((mem_req->memoryTypeBits & 1 << i) == 0)
			continue;

		/* If the device doesn't have enough memory for the object, ignore it */
		if (phy_dev->memories.memoryHeaps[phy_dev->memories.memoryTypes[i].heapIndex].size < mem_req->size)
			continue;

		/*
		 * If the memory has the properties we are looking for, then the first one we find is the most optimal
		 * for the job.  This is guaranteed by Vulkan.
		 */
		if ((phy_dev->memories.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}

	/* If we don't find any suitable memory, return an invalid index */
	return phy_dev->memories.memoryTypeCount;
}

#define TEST_ITERATIONS 100

static uint64_t get_time_ns()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LLU + ts.tv_nsec;
}

static void *worker_thread(void *args)
{
	struct tut4_per_cmd_buffer_data *per_cmd_buffer = args;

	VkResult retval;
	VkCommandBufferBeginInfo begin_info;

	/*
	 * Perform the test.  This is quite simply redoing the same command TEST_ITERATIONS times.  Since the command
	 * to execute is constant, we can actually create the command once and just resubmit is over and over again.
	 */

	/* First, reset the buffer to make sure there is nothing in it */
	vkResetCommandBuffer(per_cmd_buffer->cmd_buffer, 0);

	/*
	 * Start recording.  The BeginInfo takes some flags that we don't care about at this point, such as whether this
	 * is going to be a one-time submission, or it can be queued multiple times while it is still pending execution.
	 */
	begin_info = (VkCommandBufferBeginInfo){
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
	};
	retval = vkBeginCommandBuffer(per_cmd_buffer->cmd_buffer, &begin_info);
	if (retval)
		goto exit_failed;

	/*
	 * We need to bind the pipeline to the command buffer, which needs to be done while recording!  The only
	 * information needed is whether we are binding to a compute pipeline or a graphics one.  We already created the
	 * pipeline as a compute pipeline, so this information is rather redundant, but whatever.
	 */
	vkCmdBindPipeline(per_cmd_buffer->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, per_cmd_buffer->pipeline);

	/*
	 * After binding the pipeline, the descriptor set and the command buffer need to be bound as well.  This
	 * binding requires a number of information, such as whether the binding is to a compute or graphics pipeline,
	 * the pipeline layout and the descriptor sets to bind.  The binding also requires the "assigned number" of the
	 * sets being bound to be specified.  For example, if the shader has a `layout` specifying `set=m`, then a
	 * descriptor set should be bound to number `m`.  The vkCmdBindDescriptorSets function can bind many sets at
	 * the same time, and they will be assigned to numbers `m`, `m+1`, etc. sequentially.
	 *
	 * In our compute shader, we have one set and its number is 0.
	 *
	 * This function also takes dynamic offset information, which is not used by our descriptor sets and we
	 * will ignore that for now.
	 */
	vkCmdBindDescriptorSets(per_cmd_buffer->cmd_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
			per_cmd_buffer->pipeline_layout, 0, 1, &per_cmd_buffer->set, 0, NULL);

	/*
	 * To dispatch work to be done, we need to tell how many workgroups to dispatch.  In a shader, you can specify
	 * local "workgroup" sizes like this:
	 *
	 *     layout (local_size_x = X, local_size_y = Y, local_size_z = Z) in;
	 *
	 * This means that the shader meaningfully works on data blocks of (X, Y, Z) dimensions.  This doesn't mean
	 * that a single shader invocation accesses all these data.  Each shader invocation accesses only one data, but
	 * the invocations within the same local workgroup can more efficiently inter-communicate.  The details are
	 * beyond the scope of this tutorial.  In tut3.comp shader, we have only the X dimension set to 64 (no
	 * particular reason why, just for example), and the rest are the default (1).  When we dispatch work, we tell
	 * the command buffer to execute how many of these (X, Y, Z) workgroups to execute and in which directions.
	 * For example, if we have:
	 *
	 *     layout (local_size_x = 8, local_size_y = 4, local_size_z = 2) in;
	 *
	 * and we dispatch the following work:
	 *
	 *     vkCmdDispatch(cmd_buffer, 3, 5, 7);
	 *
	 * then the shader is executed 3*5*7*4*2 times, each with a different invocation id.  The shader can use this
	 * invocation id to understand which part of its resources it should work on.
	 *
	 * In our shader example (tut3.comp), we have the range the command buffer works on divided in 64 element to
	 * work on locally, even though each shader invocation works on one data anyway.  To simplify matters when
	 * dispatching work groups, we had ensured in tut4_prepare_test that each thread has a range of buffer
	 * divisible by 64.  This was not really a necessity.
	 *
	 * Needless to say, our storage texel buffer is 1-dimensional, so only work in the X axis is dispatched.
	 */
	vkCmdDispatch(per_cmd_buffer->cmd_buffer, (per_cmd_buffer->end_index - per_cmd_buffer->start_index) / 64, 1, 1);

	/* Stop recording */
	vkEndCommandBuffer(per_cmd_buffer->cmd_buffer);

	for (uint32_t i = 0; i < TEST_ITERATIONS; ++i)
	{
		VkSubmitInfo submit_info;

		/*
		 * Before submitting the command buffer, we need to make sure the fence we are going to wait on is not
		 * already signaled.  Resetting the fence is a simple call.  The vkResetFences can reset multiple
		 * fences at the same, but we are just using 1.
		 */
		vkResetFences(per_cmd_buffer->device, 1, &per_cmd_buffer->fence);

		/*
		 * Multiple command buffers can be submitted to a queue.  Here, we are submitting only one.
		 * Furthermore, a set of semaphores can be given for each pipeline stage to wait on, and another set
		 * of semaphores to signal upon completion of execution.  We have neatly separated the work in the
		 * buffer so that we don't really need synchronization between our threads.  That's why don't use any
		 * of these semaphores.
		 *
		 * vkQueueSubmit takes the fence to signal for us as well, which is all we need to know to make sure
		 * the command buffer execution is finished.
		 */
		submit_info = (VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &per_cmd_buffer->cmd_buffer,
		};

		vkQueueSubmit(per_cmd_buffer->queue, 1, &submit_info, per_cmd_buffer->fence);

		/*
		 * This test has a feature where the thread can pretend to be busy.  The more threads there are, the
		 * less busy each are, although the total busy time is set to 32ms.  This is supposed to simulate the
		 * effect of distributing work on multiple CPU threads on parallel dispatch of GPU commands.
		 */
		if (per_cmd_buffer->busy_time_ns > 0)
		{
			/*
			 * busy loop for 32ms/thread_count, pretending that the thread is actually doing something
			 * CPU-bound.
			 */
			uint64_t end_time_ns = get_time_ns() + per_cmd_buffer->busy_time_ns;
			while (get_time_ns() < end_time_ns);
		}

		/*
		 * Wait for the fence to become signaled.  Multiple fences can be waited on, and the function can
		 * return either when all or at least one of the fences are signaled (fourth argument).  There is also
		 * a timeout on this wait, to return (with VK_TIMEOUT) if the fences are not signaled.
		 *
		 * We are waiting for only one fence, so the fourth argument is useless.  We know the work will finish,
		 * so we are just going to patiently wait for it until it does.
		 */
		while (vkWaitForFences(per_cmd_buffer->device, 1, &per_cmd_buffer->fence, true, 1000000) == VK_TIMEOUT);
	}

	per_cmd_buffer->success = 1;

exit_failed:
	per_cmd_buffer->error = retval;
	return NULL;
}

static void *start_test(void *args)
{
	struct tut4_data *test_data = args;

	/*
	 * At this point, we have a large buffer that is divided among separate threads.  The worker threads are almost
	 * ready to run.  We just need to initialize the buffer memory!  The buffer memory needs to be mapped for the
	 * host (the application) to be able to write to it (or read from it, for that matter), and then unmapped so
	 * that the data is actually moved to the device.
	 *
	 * We will initialize the buffer in such a way that each thread would start with having the same value in its
	 * section of the buffer, but different from others.  So thread 0 would see all 0s, thread 1 would see all 1s
	 * etc.  The shaders increments the buffer contents, so after N iterations, thread i should see N+i in all its
	 * section.  We will actually use this to make sure nothing went wrong.
	 */

	VkResult retval;
	pthread_t threads[test_data->per_cmd_buffer_count];
	uint32_t pool_index, buffer_index;

	memset(threads, 0, test_data->per_cmd_buffer_count * sizeof *threads);

	/*
	 * Mapping the device memory to a host-visible address is simple.  The device that created the memory and the
	 * memory itself are required.  The offset and size of the range to be mapped are also taken, which in our case
	 * is all of the memory.  The virtual address corresponding to the device address is given back.
	 */
	void *mem = NULL;
	retval = vkMapMemory(test_data->dev->device, test_data->buffer_mem, 0, test_data->buffer_size * sizeof(float), 0, &mem);
	if (retval)
		goto exit_failed;

	for (uint32_t i = 0; i < test_data->per_cmd_buffer_count; ++i)
	{
		struct tut4_per_cmd_buffer_data *per_cmd_buffer_data = &test_data->per_cmd_buffer[i];

		for (size_t j = per_cmd_buffer_data->start_index; j < per_cmd_buffer_data->end_index; ++j)
			((float *)mem)[j] = i;
	}

	/* Finally, we unmap the memory because we don't really want it right now */
	vkUnmapMemory(test_data->dev->device, test_data->buffer_mem);

	/* Let's create our threads then! */
	pool_index = 0;
	buffer_index = 0;
	for (size_t i = 0; i < test_data->per_cmd_buffer_count; ++i)
	{
		test_data->per_cmd_buffer[i].device = test_data->dev->device;
		test_data->per_cmd_buffer[i].queue = test_data->dev->command_pools[pool_index].queues[buffer_index];
		test_data->per_cmd_buffer[i].cmd_buffer = test_data->dev->command_pools[pool_index].buffers[buffer_index];
		test_data->per_cmd_buffer[i].pipeline = test_data->pipelines->pipelines[i].pipeline;
		test_data->per_cmd_buffer[i].pipeline_layout = test_data->pipelines->pipelines[i].pipeline_layout;
		test_data->per_cmd_buffer[i].busy_time_ns = test_data->busy_threads?32000000 / test_data->per_cmd_buffer_count:0;
		++buffer_index;
		if (buffer_index >= test_data->dev->command_pools[pool_index].buffer_count)
		{
			++pool_index;
			buffer_index = 0;
		}

		if (pthread_create(&threads[i], NULL, worker_thread, &test_data->per_cmd_buffer[i]))
		{
			retval = VK_ERROR_OUT_OF_HOST_MEMORY;
			goto exit_failed;
		}
	}

	/* Wait for them to finish */
	for (size_t i = 0; i < test_data->per_cmd_buffer_count; ++i)
		pthread_join(threads[i], NULL);

	/* And make sure they did all the computations correctly, there were no races or cache problems etc */
	retval = vkMapMemory(test_data->dev->device, test_data->buffer_mem, 0, test_data->buffer_size * sizeof(float), 0, &mem);
	if (retval)
		goto exit_failed;

	test_data->success = 1;
	for (uint32_t i = 0; i < test_data->per_cmd_buffer_count; ++i)
	{
		struct tut4_per_cmd_buffer_data *per_cmd_buffer_data = &test_data->per_cmd_buffer[i];
		if (!per_cmd_buffer_data->success)
			test_data->success = 0;

		/* Test to see if the worker threads did their job correctly */
		for (size_t j = per_cmd_buffer_data->start_index; j < per_cmd_buffer_data->end_index; ++j)
			if (((float *)mem)[j] != TEST_ITERATIONS + i)
				test_data->success = 0;
	}

	vkUnmapMemory(test_data->dev->device, test_data->buffer_mem);

exit_failed:
	test_data->error = retval;
	return NULL;
}

int tut4_start_test(struct tut4_data *test_data, bool busy_threads)
{
	test_data->busy_threads = busy_threads;
	return pthread_create(&test_data->test_thread, NULL, start_test, test_data);
}

void tut4_wait_test_end(struct tut4_data *test_data)
{
	pthread_join(test_data->test_thread, NULL);
}
