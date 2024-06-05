.SUFFIXES:
.DEFAULT:

BUILDDIR ?= build
CC ?= cc
CFLAGS += -Wall -Wextra
CFLAGS += -std=gnu99 -I/usr/include/libdrm
LIBS += -ldrm -lGL

ifeq ($(DEBUG), 1)
	CONFIG = dbg
	CFLAGS += -O0 -ggdb3
else
	CONFIG = rel
	CFLAGS += -O3
endif

DEPFLAGS = -MMD -MP
COMPILE.c = $(CC) -std=gnu99 $(CFLAGS) $(DEPFLAGS) -MT $@ -MF $@.d

OBJDIR ?= $(BUILDDIR)/$(CONFIG)

$(OBJDIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(COMPILE.c) -c $< -o $@

#ENUM_SOURCES = enum.c
#ENUM_OBJS = $(ENUM_SOURCES:%=$(OBJDIR)/%.o)
#ENUM_DEPS = $(ENUM_OBJS:%=%.d)
#-include $(ENUM_DEPS)
#enum: $(OBJDIR)/enum
#$(OBJDIR)/enum: $(ENUM_OBJS)
#	@mkdir -p $(dir $@)
#	$(CC) $^ $(LIBS) -o $@

SCREENGRAB_SRC = screengrab.c
SCREENGRAB_OBJS = $(SCREENGRAB_SRC:%=$(OBJDIR)/%.o)
SCREENGRAB_DEPS = $(SCREENGRAB_OBJS:%=%.d)
-include $(SCREENGRAB_DEPS)
kmsgrab: $(OBJDIR)/screengrab
$(OBJDIR)/screengrab: $(SCREENGRAB_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $^ $(LIBS) -lEGL -lX11 -o $@
