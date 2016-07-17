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
#include <stdbool.h>
#include "tut4.h"

#define MAX_DEVICES 2

int main(int argc, char **argv)
{
	tut1_error res;
	int retval = EXIT_FAILURE;
	VkInstance vk;
	struct tut1_physical_device phy_devs[MAX_DEVICES];
	struct tut2_device devs[MAX_DEVICES];
	uint32_t dev_count = MAX_DEVICES;
	VkShaderModule shaders[MAX_DEVICES] = {NULL};
	struct tut3_pipelines pipelines[MAX_DEVICES];
	struct tut4_data test_data[MAX_DEVICES];
	int success = 0;

	/* How many threads to do the work on */
	size_t thread_count = 8;
	/* Whether the threads should take some CPU time as well */
	bool busy_threads = false;
	/* Default to 1MB of buffer data to work on */
	size_t buffer_size = 1024 * 1024 / sizeof(float);

	bool bad_args = false;
	if (argc < 2)
		bad_args = true;
	if (argc > 2 && sscanf(argv[2], "%zu", &thread_count) != 1)
		bad_args = true;
	if (argc > 3)
	{
		int temp;
		if (sscanf(argv[3], "%d", &temp) != 1)
			bad_args = true;
		else
			busy_threads = temp;
	}
	if (argc > 4)
	{
		if (sscanf(argv[4], "%zu", &buffer_size) != 1)
			bad_args = true;
		else
			buffer_size /= sizeof(float);
	}

	if (bad_args)
	{
		printf("Usage: %s shader_file [thread_count(8) [busy_threads(0) [buffer_size(1MB)]]]\n\n", argv[0]);
		return EXIT_FAILURE;
	}

	/* Fire up Vulkan */
	res = tut1_init(&vk);
	if (!tut1_error_is_success(&res))
	{
		tut1_error_printf(&res, "Could not initialize Vulkan\n");
		goto exit_bad_init;
	}

	/* Enumerate devices */
	res = tut1_enumerate_devices(vk, phy_devs, &dev_count);
	if (tut1_error_is_error(&res))
	{
		tut1_error_printf(&res, "Could not enumerate devices\n");
		goto exit_bad_enumerate;
	}

	/* Set up devices */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut2_setup(&phy_devs[i], &devs[i], VK_QUEUE_COMPUTE_BIT);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not setup logical device %u, command pools and queues\n", i);
			goto exit_bad_setup;
		}
	}

	/* Load our compute shader */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut3_load_shader(&devs[i], argv[1], &shaders[i]);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not load shader on device %u\n", i);
			goto exit_bad_shader;
		}
	}

	/*
	 * Create the pipelines.  There are as many pipelines created as command buffers (just for example).  If
	 * there are not actually enough resources for them, as many as possible are created.  In this test, we are
	 * not going to handle the case where some pipelines are not created.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		res = tut3_make_compute_pipeline(&devs[i], &pipelines[i], shaders[i]);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not allocate enough pipelines on device %u\n", i);
			goto exit_bad_pipeline;
		}
	}

	/*
	 * Prepare our test.  Both the buffers and threads are divided near-equally among the physical devices, which
	 * are likely to be just 1 in your case, but who knows.
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		size_t this_buffer_size = buffer_size / dev_count;
		size_t this_thread_count = thread_count / dev_count;

		/* Make sure the last device gets all the left-over */
		if (i == dev_count - 1)
		{
			this_buffer_size = buffer_size - buffer_size / dev_count * (dev_count - 1);
			this_thread_count = thread_count - thread_count / dev_count * (dev_count - 1);
		}

		res = tut4_prepare_test(&phy_devs[i], &devs[i], &pipelines[i], &test_data[i], this_buffer_size, this_thread_count);
		if (!tut1_error_is_success(&res))
		{
			tut1_error_printf(&res, "Could not allocate resources on device %u\n", i);
			goto exit_bad_test_prepare;
		}
	}

	/*
	 * Ok, this was a LOT of initializing!  But we are finally ready to run something.  tut4_start_test() creates
	 * a test thread for us, which further spawns the corresponding device's thread_count threads that do the
	 * calculations.  We then wait for the tests to finish with tut4_wait_test_end().
	 */
	for (uint32_t i = 0; i < dev_count; ++i)
	{
		if (tut4_start_test(&test_data[i], busy_threads))
		{
			printf("Could not start the test threads for device %u\n", i);
			perror("Error");
		}
	}

	printf("Running the tests...\n");

	for (uint32_t i = 0; i < dev_count; ++i)
		tut4_wait_test_end(&test_data[i]);

	success = 1;
	for (uint32_t i = 0; i < dev_count; ++i)
		if (!test_data[i].success)
		{
			if (!tut1_error_is_success(&test_data[i].error))
				tut1_error_printf(&test_data[i].error, "Error starting test on device %u\n", i);
			else
				printf("The test didn't produce expected results (device %u)\n", i);
			success = 0;
		}

	if (success)
		printf("Everything went well :) We just wasted your GPU doing something stupid\n");

	/*
	 * You can time the execution of the program with time(1):
	 *
	 *     $ time ./tut4/tut4 shaders/tut3.comp.spv <threads> ...
	 *
	 * Then try to play with different number of threads and see if the total execution time of the application
	 * changes and how!
	 *
	 * ...
	 *
	 * Did you try that?  Already?  Well, that was disappointing.  More threads probably resulted in higher
	 * execution time, right?  That actually makes sense.  You see, we have N data to compute, and whether you tell
	 * the GPU to do N computations from one thread, or N/T computations each from T threads, you aren't actually
	 * doing any less computation.  You probably just have more overhead from the threads.
	 *
	 * So what's the deal with multi-threaded and Vulkan?  Well, the problem is that this test was heavily
	 * GPU-bound, and as you have noticed, multi-CPU-threaded doesn't help.  For this reason, this test has a
	 * little feature to "fake" some execution on the CPU threads as well.  If you run the program like this:
	 *
	 *     $ time ./tut4/tut4 shaders/tut3.comp.spv <threads> <fake> ...
	 *
	 * where <fake> can be either 0 (no CPU usage) or 1 (some fake CPU usage), and then experiment with different
	 * number of threads, you can see the benefit of multi-threading.  In this case, while the GPU is working, the
	 * CPU thread spends time fake-doing something.  If there is only one thread, the CPU cannot keep the GPU
	 * constantly busy, so the computation slows down.  On the other hand, with multiple threads, the same amount
	 * of CPU work is spread out and done in parallel, so the threads together can feed the GPU with instructions
	 * faster.
	 *
	 * In this test, the total amount of time to waste is 3.2 seconds (32ms for each "render" operation, and there
	 * are a hundred of them).  Depending on your GPU, you may notice that above a certain number of threads, there
	 * is no more any speedup.  That is when the amount of time spent in each CPU thread becomes less than the time
	 * spent in the GPU for that thread's task, so whether the CPU spent time doing something before waiting for
	 * the GPU doesn't make a difference in the execution time.
	 */

	retval = 0;

	/* Cleanup after yourself */

exit_bad_test_prepare:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut4_free_test(&devs[i], &test_data[i]);

exit_bad_pipeline:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut3_destroy_pipeline(&devs[i], &pipelines[i]);

exit_bad_shader:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut3_free_shader(&devs[i], shaders[i]);

exit_bad_setup:
	for (uint32_t i = 0; i < dev_count; ++i)
		tut2_cleanup(&devs[i]);

exit_bad_enumerate:
	tut1_exit(vk);

exit_bad_init:
	return retval;
}
