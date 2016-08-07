ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = COPYING README.md tut5/README.md tut5/vulkan-objects.svg tut9/README.md

bin_PROGRAMS = tut1/tut1
tut1_tut1_SOURCES = tut1/tut1.c tut1/tut1_error.c tut1/main.c tut1/tut1.h tut1/tut1_error.h

bin_PROGRAMS += tut2/tut2
tut2_tut2_SOURCES = tut2/tut2.c tut2/main.c tut2/tut2.h \
                    tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut3/tut3
tut3_tut3_SOURCES = tut3/tut3.c tut3/main.c tut3/tut3.h \
                    tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut4/tut4
tut4_tut4_SOURCES = tut4/tut4.c tut4/main.c tut4/tut4.h \
                    tut3/tut3.c tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut5/tut5
tut5_tut5_SOURCES = tut5/tut5.c tut5/main.c tut5/tut5.h \
                    tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut6/tut6
tut6_tut6_SOURCES = tut6/tut6.c tut6/main.c tut6/tut6.h \
                    tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut7/tut7
tut7_tut7_SOURCES = tut7/tut7.c tut7/tut7_render.c tut7/main.c tut7/tut7.h tut7/tut7_render.h \
                    tut6/tut6.c tut4/tut4.c tut3/tut3.c tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut8/tut8
tut8_tut8_SOURCES = tut8/tut8.c tut8/tut8_render.c tut8/main.c tut8/tut8.h tut8/tut8_render.h \
                    tut7/tut7.c tut7/tut7_render.c tut6/tut6.c tut4/tut4.c tut3/tut3.c tut2/tut2.c \
                    tut1/tut1.c tut1/tut1_error.c

bin_PROGRAMS += tut9/tut9
tut9_tut9_SOURCES = tut9/main.c \
                    tut8/tut8.c tut8/tut8_render.c tut7/tut7.c tut7/tut7_render.c tut6/tut6.c \
                    tut4/tut4.c tut3/tut3.c tut2/tut2.c tut1/tut1.c tut1/tut1_error.c

shaderdir = $(datadir)/shaders
shader_DATA = shaders/tut3.comp.spv \
              shaders/tut8.vert.spv shaders/tut8.frag.spv \
              shaders/tut9.vert.spv shaders/tut9.frag.spv
CLEANFILES = $(shader_DATA)

V_GLSLANG = $(V_GLSLANG_@AM_V@)
V_GLSLANG_ = $(V_GLSLANG_@AM_DEFAULT_V@)
V_GLSLANG_0 = @echo "  GLSLANG " $@;
V_GLSLANG_1 =

%.spv: %
	@$(mkdir_p) $(builddir)/shaders
	$(V_GLSLANG)$(GLSLANGVALIDATOR) -V $< -o $@
