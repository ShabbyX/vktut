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

#ifndef TUT8_RENDER_H
#define TUT8_RENDER_H

#include "tut8.h"
#include "../tut7/tut7_render.h"

/* Fill the contents of a host-visible buffer to buffer with arbitrary data */
VkResult tut8_render_fill_buffer(struct tut2_device *dev, struct tut7_buffer *to, void *from, size_t size, const char *name);

/*
 * Copy a buffer to another, for example from a host-visible one to a device-local one.  This uses a command buffer,
 * submits it, and waits for it to finish, so it's not supposed to be used while recording a command buffer.
 */
VkResult tut8_render_copy_buffer(struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_buffer *to, struct tut7_buffer *from, size_t size, const char *name);

/*
 *
 * Transition an image to a new layout.  This uses a command buffer, submits it, and waits for it to finish, so it's
 * not supposed to be used while recording a command buffer.
 */
VkResult tut8_render_transition_images(struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *images, uint32_t image_count,
		VkImageLayout from, VkImageLayout to, VkImageAspectFlags aspect, const char *name);

#endif
