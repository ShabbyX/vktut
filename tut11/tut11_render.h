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

#ifndef TUT11_RENDER_H
#define TUT11_RENDER_H

#include "tut11.h"
#include "../tut10/tut10_render.h"

/*
 * Similar to tut7_render_start/finish, but allow additional wait and signal semaphores so the submission will be
 * synchronized with off-screen renders as well.
 */
int tut11_render_start(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout to_layout, uint32_t *image_index);
int tut11_render_finish(struct tut7_render_essentials *essentials, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, VkImageLayout from_layout, uint32_t image_index,
		VkSemaphore wait_sem, VkSemaphore signal_sem);

#endif
