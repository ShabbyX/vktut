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

#ifndef TUT1_ERROR_H
#define TUT1_ERROR_H

/*
 * Note: this file handles error tracking and reporting, and has little to do with the tutorials themselves.  The main
 * purpose here is to track the errors precisely, while keeping the actual tutorials as clean as possible.
 */

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

enum tut1_error_type
{
	TUT1_ERROR_SUCCESS = 0,
	TUT1_ERROR_VKRESULT,
	TUT1_ERROR_VKRESULT_WARNING,	/* VK_INCOMPLETE for example */
	TUT1_ERROR_ERRNO,
};

struct tut1_error_data
{
	enum tut1_error_type type;
	union {
		VkResult vkresult;
		int err_no;
	};
	const char *file;
	unsigned int line;
} tut1_error_data;

typedef struct tut1_error
{
	struct tut1_error_data error;
	struct tut1_error_data sub_error;	/*
						 * Used in cases where error is e.g. "VK_INCOMPLETE", and it is due to
						 * another error.
						 */
} tut1_error;

#define TUT1_ERROR_NONE (struct tut1_error){ .error = { .type = TUT1_ERROR_SUCCESS, }, .sub_error = { .type = TUT1_ERROR_SUCCESS, }, }

#define tut1_error_set_vkresult(es, e)     tut1_error_data_set_vkresult(&(es)->error,     (e), __FILE__, __LINE__)
#define tut1_error_set_errno(es, e)        tut1_error_data_set_errno   (&(es)->error,     (e), __FILE__, __LINE__)
#define tut1_error_sub_set_vkresult(es, e) tut1_error_data_set_vkresult(&(es)->sub_error, (e), __FILE__, __LINE__)
#define tut1_error_sub_set_errno(es, e)    tut1_error_data_set_errno   (&(es)->sub_error, (e), __FILE__, __LINE__)
#define tut1_error_merge(es, os)                                \
do {                                                            \
	if (tut1_error_data_merge(&(es)->error, &(os)->error))  \
		(es)->sub_error = (os)->sub_error;              \
} while (0)
#define tut1_error_sub_merge(es, os)       tut1_error_data_merge(&(es)->sub_error, &(os)->error)

void tut1_error_data_set_vkresult(struct tut1_error_data *error, VkResult vkresult, const char *file, unsigned int line);
void tut1_error_data_set_errno(struct tut1_error_data *error, int err_no, const char *file, unsigned int line);
bool tut1_error_data_merge(struct tut1_error_data *error, struct tut1_error_data *other);

bool tut1_error_is_success(struct tut1_error *error);
bool tut1_error_is_warning(struct tut1_error *error);
bool tut1_error_is_error(struct tut1_error *error);
#define tut1_error_printf(es, ...) tut1_error_fprintf(stdout, (es), __VA_ARGS__)
void tut1_error_fprintf(FILE *fout, struct tut1_error *error, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

#endif
