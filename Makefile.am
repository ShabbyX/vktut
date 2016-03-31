ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = COPYING README.md

bin_PROGRAMS = tut1/tut1
tut1_tut1_SOURCES = tut1/tut1.c \
                    tut1/main.c \
                    tut1/tut1.h

bin_PROGRAMS += tut2/tut2
tut2_tut2_SOURCES = tut2/tut2.c \
                    tut2/main.c \
                    tut2/tut2.h \
                    tut1/tut1.c

bin_PROGRAMS += tut3/tut3
tut3_tut3_SOURCES = tut3/tut3.c \
                    tut3/main.c \
                    tut3/tut3.h \
                    tut2/tut2.c \
                    tut1/tut1.c

bin_PROGRAMS += tut4/tut4
tut4_tut4_SOURCES = tut4/tut4.c \
                    tut4/main.c \
                    tut4/tut4.h \
                    tut3/tut3.c \
                    tut2/tut2.c \
                    tut1/tut1.c

bin_PROGRAMS += tut5/tut5
tut5_tut5_SOURCES = tut5/tut5.c \
                    tut5/main.c \
                    tut5/tut5.h \
                    tut2/tut2.c \
                    tut1/tut1.c

bin_PROGRAMS += tut6/tut6
tut6_tut6_SOURCES = tut6/tut6.c \
                    tut6/main.c \
                    tut6/tut6.h \
                    tut2/tut2.c \
                    tut1/tut1.c

shaderdir = $(datadir)/shaders
shader_DATA = shaders/tut3.comp.spv
CLEANFILES = $(shader_DATA)

V_GLSLANG = $(V_GLSLANG_@AM_V@)
V_GLSLANG_ = $(V_GLSLANG_@AM_DEFAULT_V@)
V_GLSLANG_0 = @echo "  GLSLANG " $@;
V_GLSLANG_1 =

%.spv: %
	@$(mkdir_p) $(builddir)/shaders
	$(V_GLSLANG)glslangValidator -V $< -o $@
