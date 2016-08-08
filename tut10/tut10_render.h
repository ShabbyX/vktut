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

#ifndef TUT10_RENDER_H
#define TUT10_RENDER_H

#include "../tut8/tut8_render.h"

/*
 * Create a texture image filled with BGRA data.  This uses a command buffer, submits it, and waits for it to finish,
 * so it's not supposed to be used while recording a command buffer.  It creates and destroys a staging buffer in the
 * process.  In the end, it transitions the image to the desired layout.
 */
tut1_error tut10_render_init_texture(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_image *image, VkImageLayout layout, uint8_t *contents, const char *name);

/*
 * Copy over arbitrary data to the buffer.  This uses a command buffer, submits it, and waits for it to finish, so it's
 * not supposed to be used while recording a command buffer.  It creates and destroys a staging buffer in the process.
 */
tut1_error tut10_render_init_buffer(struct tut1_physical_device *phy_dev, struct tut2_device *dev, struct tut7_render_essentials *essentials,
		struct tut7_buffer *buffer, void *contents, const char *name);

#endif
