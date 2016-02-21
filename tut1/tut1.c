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

#include "tut1.h"

VkResult tut1_init(VkInstance *vk)
{
	/*
	 * Vulkan is not just for graphics.  As the slogan goes, graphics and computation belong together.  As a
	 * result, initialization in Vulkan is rather verbose to allow the application to adapt to a wide set of
	 * possible hardware configurations.
	 *
	 * Initializing Vulkan is thus somewhat verbose, and unsurprisingly, independent of window management systems.
	 * In fact, there is nothing related to graphics in this tutorial.
	 *
	 * To initialize Vulkan, a set of information needs to be given to it.  Most importantly, this includes Vulkan
	 * layers and extensions that are desired (none in this tutorial).
	 *
	 * Some of Vulkan structs, such as those ending in Info need to have their `sType` set, which is the first
	 * member of the struct.  Generally, VkSomeStruct has type VK_STRUCTURE_TYPE_SOME_STRUCT, so this is easy to
	 * remember.  The rest of the struct members are described as they are used.
	 */

	/*
	 * The vkApplicationInfo struct contains some information about what application is going to run.  This may seem
	 * completely unnecessary and it in fact is!  However, giving this information helps the drivers know which
	 * application, or better yet which engine, they are dealing with.  They might have foreseen special
	 * optimizations knowing the engine, for example.
	 *
	 * With that in mind, the application and engine names are arbitrarily set in this tutorial as well as their
	 * versions.  I have used 0xAABBCC to denote version AA.BB.CC.  For example 0x010000 for version 1.0.0.
	 */
	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan Tutorial",
		.applicationVersion = 0x010000,
		.pEngineName = "Vulkan Tutorial",
		.engineVersion = 0x010000,
		/*
		 * the apiVersion field is used to make sure your application is going to work with the driver.  I have
		 * set it to VK_API_VERSION, which is the current version of the API installed on your computer.  If
		 * you want to say that your application would work with any version above 1.0.3 for example, you would
		 * set apiVersion to VK_MAKE_VERSION(1, 0, 3)
		 */
		.apiVersion = VK_API_VERSION,
	};

	/*
	 * The vkInstanceCreateInfo struct takes the previous application information, as well as the names of layers
	 * and extensions your application needs to use.  This tutorial uses none, so they are left out (set to 0).
	 */
	VkInstanceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
	};

	/*
	 * vkCreateInstance then takes the above info and initializes the vkInstance struct.  This struct replaces
	 * all the global state traditionally found in OpenGL.  This means that the application can actually have
	 * multiple instances of Vulkan at the same time, each bound to a separate implementation.  Imagine your PC
	 * having one nvidia card and one amd, and your application is rendering/computing something different on
	 * each of them!
	 *
	 * The second argument of vkCreateInstance gives callbacks to replace the default memory allocation functions.
	 * This is useful if an application wants to track its memory usage (there is a Vulkan layer doing this
	 * already by the way), improve the speed of the allocation functions by using custom allocation algorithms
	 * that are better suited to the allocation patterns of the application, or as the Vulkan specs mentions,
	 * embedded systems debugging or logging.  Using custom allocation functions will be explored in a future
	 * tutorial.
	 */
	return vkCreateInstance(&info, NULL, vk);
}

void tut1_exit(VkInstance vk)
{
	/*
	 * Once the application is finished with Vulkan, it can perform cleanups.  The second argument is the same
	 * as the one with vkCreateInstance(*).  Since we didn't provide one to vkCreateInstance, we don't need to
	 * provide one to vkDestroyInstance either.
	 *
	 * (*) technically, it doesn't need to be the same, as long as the callbacks are compatible.  That is, as
	 * long as memory allocated with one can be freed with the other, the callbacks are ok.
	 */
	vkDestroyInstance(vk, NULL);
}

VkResult tut1_enumerate_devices(VkInstance vk, struct tut1_physical_device *devs, uint32_t *count)
{
	VkPhysicalDevice phy_devs[*count];
	VkResult retval, res;

	/*
	 * A physical device in Vulkan is any actual device with Vulkan capabilities.  For example, a physical GPU is
	 * such a device.  Vulkan later on associates logical devices to these physical devices as a way to access
	 * them.  For now, we just want to peek at what devices there are.
	 *
	 * The vkEnumeratePhysicalDevices takes an array of VkPhysicalDevices to fill in any physical device it finds,
	 * with the maximum size of the array in its `count` argument.  This `count` argument also serves the purpose
	 * of returning the actual number of devices filled in the array.
	 *
	 * If the array (`phy_devs` below) is not provided (i.e., is `NULL`), then the `count` argument would return
	 * the total number of physical devices available.  I could have used this to query the number of devices and
	 * allocate memory large enough for all of them.  For the sake of simplicity however, let's just assume there
	 * can't be _that_ many devices and use a fixed upper bound.  If there are more devices than I foresaw, then
	 * VK_INCOMPLETE would be returned, stating that the array doesn't contain all that there is.
	 */
	retval = vkEnumeratePhysicalDevices(vk, count, phy_devs);
	if (retval < 0)
		goto exit_failed;

	for (uint32_t i = 0; i < *count; ++i)
	{
		devs[i].physical_device = phy_devs[i];

		/*
		 * Once we have handles to each physical device, we can query information regarding the device. This
		 * is done with vkGetPhysicalDeviceProperties.  The device properties include vendor and device ids,
		 * driver version, device name, device limits and such information.
		 */
		vkGetPhysicalDeviceProperties(devs[i].physical_device, &devs[i].properties);

		/*
		 * Each physical device has certain abilities, such as being able to support graphics operations or
		 * general computation.  Vulkan uses _queues_ to transmit operations to the devices for various
		 * purposes (e.g. graphics or compute).  A device groups its queues in _families_ and indicates how
		 * many queues in each family it supports.  The application can thus know which queues to use to
		 * perform the desired operation.
		 *
		 * The vkGetPhysicalDeviceQueueFamilyProperties function gets this information.  Note again that I
		 * could have used the technique described above to first get the total number of queue families,
		 * dynamically allocate enough space for it and then query the queues themselves, making sure I avoid
		 * getting a VK_INCOMPLETE return value.  Once again, for the sake of simplicity, a maximum possible
		 * number of queue families is assumed.
		 */
		uint32_t qfc = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i].physical_device, &qfc, NULL);
		vkGetPhysicalDeviceQueueFamilyProperties(devs[i].physical_device, &devs[i].queue_family_count, devs[i].queue_families);

		devs[i].queue_families_incomplete = devs[i].queue_family_count < qfc;
	}

	/*
	 * At this point, there is a great deal of information regarding the physical devices available and their
	 * capabilities stored in `devs`.  See main.c for some of this information getting printed out.
	 */

exit_failed:
	return retval;
}

/* The following functions get a readable string out of the Vulkan standard enums */

const char *tut1_VkResult_string(VkResult res)
{
	switch (res)
	{
	case VK_SUCCESS:
		return "Success";
	case VK_NOT_READY:
		return "Not ready";
	case VK_TIMEOUT:
		return "Timeout";
	case VK_EVENT_SET:
		return "Event set";
	case VK_EVENT_RESET:
		return "Event reset";
	case VK_INCOMPLETE:
		return "Incomplete";
	case VK_ERROR_OUT_OF_HOST_MEMORY:
		return "Out of host memory";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY:
		return "Out of device memory";
	case VK_ERROR_INITIALIZATION_FAILED:
		return "Initialization failed";
	case VK_ERROR_DEVICE_LOST:
		return "Device lost";
	case VK_ERROR_MEMORY_MAP_FAILED:
		return "Memory map failed";
	case VK_ERROR_LAYER_NOT_PRESENT:
		return "Layer not present";
	case VK_ERROR_EXTENSION_NOT_PRESENT:
		return "Extension not present";
	case VK_ERROR_FEATURE_NOT_PRESENT:
		return "Feature not present";
	case VK_ERROR_INCOMPATIBLE_DRIVER:
		return "Incompatible driver";
	case VK_ERROR_TOO_MANY_OBJECTS:
		return "Too many objects";
	case VK_ERROR_FORMAT_NOT_SUPPORTED:
		return "Format not supported";
	default:
		return "Unrecognized error";
	}
}

const char *tut1_VkPhysicalDeviceType_string(VkPhysicalDeviceType type)
{
	switch (type)
	{
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		return "Neither GPU nor CPU";
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return "Integrated GPU";
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return "Discrete GPU";
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return "Virtual GPU";
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		return "CPU";
	default:
		return "Unrecognized device type";
	}
}
