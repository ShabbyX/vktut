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

#ifndef TUT12_H
#define TUT12_H

#include <curses.h>
#include "../tut6/tut6.h"

tut1_error tut12_get_swapchain(VkInstance vk, struct tut1_physical_device *phy_dev, struct tut2_device *dev,
		struct tut6_swapchain *swapchain, WINDOW *window, uint32_t thread_count, bool allow_no_vsync);

#endif
