AC_PREREQ([2.68])
AC_INIT([Vulkan Tutorials], [0.1], [https://github.com/ShabbyX/vktut/issues])
AC_CONFIG_AUX_DIR([bin])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([1.14 subdir-objects -Wall -Wno-portability -Werror foreign])

AC_CONFIG_SRCDIR([tut1/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs
AC_PROG_CC_C99
AC_PROG_INSTALL
AM_PROG_AR

LT_INIT

# Make sure C99 is available
AS_IF([test x"$ac_cv_prog_cc_c99" = xno],
  AC_MSG_ERROR([C99 support is required]))

# Checks for header files
AC_CHECK_HEADERS([stddef.h stdlib.h string.h time.h pthread.h], [], [AC_MSG_ERROR(Required header missing)])

# Checks for typedefs, structures, and compiler characteristics
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions
AC_CHECK_FUNCS([malloc realloc memmove memset], [], [AC_MSG_ERROR(Required function missing)])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([rt], [clock_gettime])

# Now that the basics are checked, make sure vulkan is installed
AC_CHECK_HEADER([vulkan/vulkan.h], [], [AC_MSG_ERROR(Vulkan header not found)])
AC_CHECK_LIB([vulkan], [vkCreateInstance], [], [AC_MSG_ERROR(Vulkan library not found)])

# Make sure external dependencies are also installed
# - SDL2:
AC_CHECK_HEADER([SDL2/SDL.h], [], [AC_MSG_ERROR(SDL2 headers not found)])
AC_CHECK_LIB([SDL2], [SDL_Init], [], [AC_MSG_ERROR(SDL2 library not found)])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
