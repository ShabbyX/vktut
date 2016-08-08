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

#include "tut10_render.h"

static tut1_error create_staging_buffer(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_buffer *staging, uint8_t *contents, size_t size, const char *name)
{
	tut1_error retval = TUT1_ERROR_NONE;

	/* Create a buffer to hold the data. */
	*staging = (struct tut7_buffer){
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.host_visible = true,
	};

	retval = tut7_create_buffers(phy_dev, dev, staging, 1);
	if (!tut1_error_is_success(&retval))
	{
		tut1_error_printf(&retval, "Failed to create staging %s buffer\n", name);
		return retval;
	}

	/* Copy the data over to the buffer. */
	char staging_name[50];
	snprintf(staging_name, 50, "staging %s", name);
	retval = tut8_render_fill_buffer(dev, staging, contents, size, staging_name);

	return retval;
}

tut1_error tut10_render_init_texture(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *image, VkImageLayout layout, uint8_t *contents, const char *name)
{
	tut1_error retval = TUT1_ERROR_NONE;

	/* Create a buffer to hold the texture data. */
	struct tut7_buffer staging;
	retval = create_staging_buffer(phy_dev, dev, essentials, &staging, contents, image->extent.width * image->extent.height * 4, name);
	if (!tut1_error_is_success(&retval))
		goto exit_cleanup;

	/* Transition the image to a layout we can copy to. */
	retval = tut8_render_transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, name);
	if (!tut1_error_is_success(&retval))
		goto exit_cleanup;

	/* Copy the buffer to the image. */
	VkBufferImageCopy image_copy = {
		.imageSubresource = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.layerCount = 1,
		},
		.imageExtent = {
			.width = image->extent.width,
			.height = image->extent.height,
		},
	};

	retval = tut8_render_copy_buffer_to_image(dev, essentials, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &staging, &image_copy, name);
	if (!tut1_error_is_success(&retval))
		goto exit_cleanup;

	/* Transition the image to the desired layout */
	retval = tut8_render_transition_images(dev, essentials, image, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout, VK_IMAGE_ASPECT_COLOR_BIT, name);

exit_cleanup:
	/*
	 * TODO: Enable after the NVidia driver fixes its bug with handling NULL pointers.
	 * tut7_free_buffers(dev, &staging, 1);
	 */

	return retval;
}

tut1_error tut10_render_init_buffer(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_buffer *buffer, void *contents, const char *name)
{
	tut1_error retval = TUT1_ERROR_NONE;

	/* Create a buffer to hold the data. */
	struct tut7_buffer staging;
	retval = create_staging_buffer(phy_dev, dev, essentials, &staging, contents, buffer->size, name);
	if (!tut1_error_is_success(&retval))
		goto exit_cleanup;

	/* Copy staging buffer over to the real buffer. */
	retval = tut8_render_copy_buffer(dev, essentials, buffer, &staging, buffer->size, name);

exit_cleanup:
	/*
	 * TODO: Enable after the NVidia driver fixes its bug with handling NULL pointers.
	 * tut7_free_buffers(dev, &staging, 1);
	 */

	return retval;
}
