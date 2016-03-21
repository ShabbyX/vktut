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
#include <string.h>
#include "tut5.h"

#define TUT5_MAX_LAYER_COUNT 20
#define TUT5_MAX_EXTENSION_COUNT 10

static VkResult get_layers_and_extensions(VkLayerProperties layers[TUT5_MAX_LAYER_COUNT], uint32_t *layer_count,
		VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT], uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1])
{
	VkResult retval, res;

	/*
	 * To get a list of layers, vkEnumerateInstanceLayerProperties can be simply called.  If there are more layers
	 * than our array can hold, VK_INCOMPLETE is returned, in which case we don't really mind and we'll enable as
	 * many layers as possible.
	 */
	retval = vkEnumerateInstanceLayerProperties(layer_count, layers);
	if (retval < 0)
		goto exit_failed;

	/*
	 * Extensions are either independent, or are based on a layer.  Therefore, we will enumerate the extensions
	 * once given no layer name, and once per layer name.
	 */
	extensions_count[0] = TUT5_MAX_EXTENSION_COUNT;
	res = vkEnumerateInstanceExtensionProperties(NULL, &extensions_count[0], extensions[0]);
	if (res)
		retval = res;
	if (retval < 0)
		goto exit_failed;
	for (uint32_t i = 0; i < *layer_count; ++i)
	{
		extensions_count[i + 1] = TUT5_MAX_EXTENSION_COUNT;
		res = vkEnumerateInstanceExtensionProperties(layers[i].layerName, &extensions_count[i + 1], extensions[i + 1]);
		if (res)
			retval = res;
		if (retval < 0)
			goto exit_failed;
	}

exit_failed:
	return retval;
}

static VkResult get_device_layers_and_extensions(VkPhysicalDevice phy_dev, VkLayerProperties layers[TUT5_MAX_LAYER_COUNT], uint32_t *layer_count,
		VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT], uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1])
{
	VkResult retval, res;

	/*
	 * Enumerating layers and extensions that are specific to a device is very similar to enumerating those that
	 * are generic to a Vulkan instance.  The only difference is to use Device instead of Instance name, and give
	 * the physical device as the first argument.
	 */

	retval = vkEnumerateDeviceLayerProperties(phy_dev, layer_count, layers);
	if (retval < 0)
		goto exit_failed;

	extensions_count[0] = TUT5_MAX_EXTENSION_COUNT;
	res = vkEnumerateDeviceExtensionProperties(phy_dev, NULL, &extensions_count[0], extensions[0]);
	if (res)
		retval = res;
	if (retval < 0)
		goto exit_failed;
	for (uint32_t i = 0; i < *layer_count; ++i)
	{
		extensions_count[i + 1] = TUT5_MAX_EXTENSION_COUNT;
		res = vkEnumerateDeviceExtensionProperties(phy_dev, layers[i].layerName, &extensions_count[i + 1], extensions[i + 1]);
		if (res)
			retval = res;
		if (retval < 0)
			goto exit_failed;
	}

exit_failed:
	return retval;
}

static void pack_layer_and_extension_names(VkLayerProperties layers[TUT5_MAX_LAYER_COUNT], uint32_t layer_count,
		VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT], uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1],
		const char *layer_names[], const char *extension_names[], uint32_t *total_extensions_count)
{
	for (uint32_t j = 0; j < extensions_count[0]; ++j)
		extension_names[(*total_extensions_count)++] = extensions[0][j].extensionName;
	for (uint32_t i = 0; i < layer_count; ++i)
	{
		layer_names[i] = layers[i].layerName;
		for (uint32_t j = 0; j < extensions_count[i + 1]; ++j)
			extension_names[(*total_extensions_count)++] = extensions[i + 1][j].extensionName;
	}
}

VkResult tut5_init(VkInstance *vk)
{
	VkLayerProperties layers[TUT5_MAX_LAYER_COUNT];
	uint32_t layer_count = TUT5_MAX_LAYER_COUNT;

	VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT];
	uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1];

	const char *layer_names[TUT5_MAX_LAYER_COUNT] = {NULL};
	const char *extension_names[(TUT5_MAX_LAYER_COUNT + 1) * TUT5_MAX_EXTENSION_COUNT] = {NULL};
	uint32_t total_extensions_count = 0;

	VkApplicationInfo app_info = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "Vulkan Tutorial",
		.applicationVersion = 0x010000,
		.pEngineName = "Vulkan Tutorial",
		.engineVersion = 0x010000,
		.apiVersion = VK_MAKE_VERSION(1, 0, 3),
	};
	VkInstanceCreateInfo info;

	VkResult retval;

	/*
	 * We have already seen how to create a Vulkan instance in Tutorial 1.  In this tutorial, we will enumerate all
	 * layers and extensions (depending on the layer or not) and enable all of them, why not!
	 */
	retval = get_layers_and_extensions(layers, &layer_count, extensions, extensions_count);
	if (retval < 0)
		goto exit_failed;

	/*
	 * Now that we have the layer and extension information, we need to pack the names together to be given to
	 * VkInstanceCreateInfo.
	 *
	 * In a real-world application, you would likely just use an array of predefined layer and extensions names,
	 * knowing which layers and extensions you are in fact interested in.
	 */
	pack_layer_and_extension_names(layers, layer_count, extensions, extensions_count,
			layer_names, extension_names, &total_extensions_count);

	/*
	 * Enabling the layers and extensions is now just a matter of giving these names to the VkInstanceCreateInfo
	 * struct.  In Tutorial 1, we had just given 0 as the layer and extension counts.
	 */
	info = (VkInstanceCreateInfo){
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = &app_info,
		.enabledLayerCount = layer_count * 0,
		.ppEnabledLayerNames = layer_names,
		.enabledExtensionCount = total_extensions_count * 0,
		.ppEnabledExtensionNames = extension_names,
	};

	return vkCreateInstance(&info, NULL, vk);

exit_failed:
	return retval;
}

VkResult tut5_get_dev(struct tut1_physical_device *phy_dev, struct tut2_device *dev, VkQueueFlags qflags,
		VkDeviceQueueCreateInfo queue_info[], uint32_t *queue_info_count)
{
	VkLayerProperties layers[TUT5_MAX_LAYER_COUNT];
	uint32_t layer_count = TUT5_MAX_LAYER_COUNT;

	VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT];
	uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1];

	const char *layer_names[TUT5_MAX_LAYER_COUNT] = {NULL};
	const char *extension_names[(TUT5_MAX_LAYER_COUNT + 1) * TUT5_MAX_EXTENSION_COUNT] = {NULL};
	uint32_t total_extensions_count = 0;

	VkResult retval;

	*dev = (struct tut2_device){0};

	/* We have already seen how to create a logical device and request queues in Tutorial 2 */
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
		/* Check if the queue has the desired capabilities.  If so, add it to the list of desired queues */
		if ((phy_dev->queue_families[i].queueFlags & qflags) != qflags)
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
		return VK_ERROR_FEATURE_NOT_PRESENT;

	/*
	 * If we want to enable layers and extensions for a specific device, we have to have enabled them for the		// TODO: verify
	 * Vulkan instance before.  In tut5_init, we enabled all layers and extensions that there are.  Here, we will
	 * again enable every layer and extension available.  In reality, you would likely have a predefined array of
	 * layer and extension names that you are interested in, and therefore you wouldn't need to enumerate anything.
	 */
	retval = get_device_layers_and_extensions(phy_dev->physical_device, layers, &layer_count, extensions, extensions_count);
	if (retval < 0)
		return retval;

	/* Once again, we need to pack the names in a simple array to be given to VkDeviceQueueCreateInfo */
	pack_layer_and_extension_names(layers, layer_count, extensions, extensions_count,
			layer_names, extension_names, &total_extensions_count);

	VkDeviceCreateInfo dev_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = *queue_info_count,
		.pQueueCreateInfos = queue_info,
		.enabledLayerCount = layer_count * 0,
		.ppEnabledLayerNames = layer_names,
		.enabledExtensionCount = total_extensions_count * 0,
		.ppEnabledExtensionNames = extension_names,
		.pEnabledFeatures = &phy_dev->features,
	};

	return vkCreateDevice(phy_dev->physical_device, &dev_info, NULL, &dev->device);
}

static void print_layers_and_extensions(const char *indent, VkLayerProperties layers[TUT5_MAX_LAYER_COUNT], uint32_t layer_count,
		VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT], uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1])
{
	/*
	 * First, let's print the extensions that are independent of layers.  Then, for each layer we will enumerate
	 * its own extensions as well.
	 */
	for (uint32_t j = 0; j < extensions_count[0]; ++j)
	{
		VkExtensionProperties *ext = &extensions[0][j];
		printf("%s* Extension: %s (versions: spec: 0x%08X)\n", indent, ext->extensionName, ext->specVersion);
	}
	for (uint32_t i = 0; i < layer_count; ++i)
	{
		VkLayerProperties *layer = &layers[i];
		printf("%s* Layer: %s (versions: spec: 0x%08X, implementation: 0x%08X)\n", indent, layer->layerName,
				layer->specVersion, layer->implementationVersion);
		printf("%s         %s\n", indent, layer->description);
		for (uint32_t j = 0; j < extensions_count[i + 1]; ++j)
		{
			VkExtensionProperties *ext = &extensions[i + 1][j];
			printf("%s  * Extension: %s (versions: spec: 0x%08X)\n", indent, ext->extensionName, ext->specVersion);
		}
	}
}

void tut5_print_layers_and_extensions(void)
{
	VkLayerProperties layers[TUT5_MAX_LAYER_COUNT];
	uint32_t layer_count = TUT5_MAX_LAYER_COUNT;

	VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT];
	uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1];

	VkResult res = get_layers_and_extensions(layers, &layer_count, extensions, extensions_count);
	if (res < 0)
		printf("Failed to enumerate instance layers and extensions: %s\n", tut1_VkResult_string(res));
	else if (res > 0)
		printf("Much instance layers and extensions! Such Wow!\n");

	printf("Instance:\n");
	print_layers_and_extensions("", layers, layer_count, extensions, extensions_count);
}

void tut5_print_device_layers_and_extensions(struct tut1_physical_device *phy_dev)
{
	VkLayerProperties layers[TUT5_MAX_LAYER_COUNT];
	uint32_t layer_count = TUT5_MAX_LAYER_COUNT;

	VkExtensionProperties extensions[TUT5_MAX_LAYER_COUNT + 1][TUT5_MAX_EXTENSION_COUNT];
	uint32_t extensions_count[TUT5_MAX_LAYER_COUNT + 1];

	VkResult res = get_device_layers_and_extensions(phy_dev->physical_device, layers, &layer_count, extensions, extensions_count);
	if (res < 0)
		printf("%s: Failed to enumerate device layers and extensions: %s\n",
				phy_dev->properties.deviceName, tut1_VkResult_string(res));
	else if (res > 0)
		printf("%s: Much instance layers and extensions! Such Wow!\n", phy_dev->properties.deviceName);

	printf("- Device %s:\n", phy_dev->properties.deviceName);
	print_layers_and_extensions("  ", layers, layer_count, extensions, extensions_count);
}
