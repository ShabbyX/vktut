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
#include <stdarg.h>
#include "tut1_error.h"

/*
 * Note: this file handles error tracking and reporting, and has little to do with the tutorials themselves.  The main
 * purpose here is to track the errors precisely, while keeping the actual tutorials as clean as possible.
 */

void tut1_error_data_set_vkresult(struct tut1_error_data *error, VkResult vkresult, const char *file, unsigned int line)
{
	/* If this is not an error, ignore it */
	if (vkresult == 0)
		return;

	/* If error is already set, keep the oldest error, but override warnings */
	if (error->type != TUT1_ERROR_SUCCESS && !(error->type == TUT1_ERROR_VKRESULT_WARNING && vkresult < 0))
		return;

	*error = (struct tut1_error_data){
		.type = vkresult < 0?TUT1_ERROR_VKRESULT:TUT1_ERROR_VKRESULT_WARNING,
		.vkresult = vkresult,
		.file = file,
		.line = line,
	};
}

void tut1_error_data_set_errno(struct tut1_error_data *error, int err_no, const char *file, unsigned int line)
{
	/* If this is not an error, ignore it */
	if (err_no == 0)
		return;

	/* If error is already set, keep the oldest error, but override warnings */
	if (error->type != TUT1_ERROR_SUCCESS && error->type != TUT1_ERROR_VKRESULT_WARNING)
		return;

	*error = (struct tut1_error_data){
		.type = TUT1_ERROR_ERRNO,
		.err_no = err_no,
		.file = file,
		.line = line,
	};
}

bool tut1_error_data_merge(struct tut1_error_data *error, struct tut1_error_data *other)
{
	/* If this is not an error, ignore it */
	if (other->type == TUT1_ERROR_SUCCESS)
		return false;

	/* If error is already set, keep the oldest error, but override warnings */
	if (error->type != TUT1_ERROR_SUCCESS && !(error->type == TUT1_ERROR_VKRESULT_WARNING && (other->type == TUT1_ERROR_VKRESULT || other->type == TUT1_ERROR_ERRNO)))
		return false;

	*error = *other;
	return true;
}

bool tut1_error_is_success(struct tut1_error *error)
{
	return error->error.type == TUT1_ERROR_SUCCESS;
}

bool tut1_error_is_warning(struct tut1_error *error)
{
	return error->error.type == TUT1_ERROR_VKRESULT_WARNING;
}

bool tut1_error_is_error(struct tut1_error *error)
{
	return !tut1_error_is_success(error) && !tut1_error_is_warning(error);
}

static const char *VkResult_string(VkResult res)
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

static void print_error(FILE *fout, struct tut1_error_data *error_data, const char *prefix)
{
	fprintf(fout, "%s:%u: %s", error_data->file, error_data->line, prefix);
	switch (error_data->type)
	{
	case TUT1_ERROR_VKRESULT_WARNING:
	case TUT1_ERROR_VKRESULT:
		fprintf(fout, "%s (VkResult %d)\n", VkResult_string(error_data->vkresult), error_data->vkresult);
		break;
	case TUT1_ERROR_ERRNO:
		fprintf(fout, "%s (errno %d)\n", strerror(error_data->err_no), error_data->err_no);
		break;
	default:
		fprintf(fout, "<internal error>\n");
		break;
	}
}

void tut1_error_fprintf(FILE *fout, struct tut1_error *error, const char *fmt, ...)
{
	/* if no error, don't print anything */
	if (error->error.type == TUT1_ERROR_SUCCESS)
		return;

	va_list args;
	va_start(args, fmt);
	vfprintf(fout, fmt, args);
	va_end(args);

	print_error(fout, &error->error, "");
	if (error->sub_error.type != TUT1_ERROR_SUCCESS)
		print_error(fout, &error->sub_error, "    Resulting from this error: ");
}
