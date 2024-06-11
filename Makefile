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
	CFLAGS += -O2
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

# SCREENGRAB_SRC = screengrab.c
# SCREENGRAB_OBJS = $(SCREENGRAB_SRC:%=$(OBJDIR)/%.o)
# SCREENGRAB_DEPS = $(SCREENGRAB_OBJS:%=%.d)
# -include $(SCREENGRAB_DEPS)
# kmsgrab: $(OBJDIR)/screengrab
# $(OBJDIR)/screengrab: $(SCREENGRAB_OBJS)
# 	@mkdir -p $(dir $@)
# 	$(CC) $^ $(LIBS) -lEGL -lX11 -o $@


# WRITE_IMG_SRC = writeimg.c
# WRITE_IMG_OBJS = $(WRITE_IMG_SRC:%=$(OBJDIR)/%.o)
# WRITE_IMG_DEPS = $(WRITE_IMG_OBJS:%=%.d)
# -include $(WRITE_IMG_DEPS)
# imggrab: $(OBJDIR)/writeimg
# $(OBJDIR)/writeimg: $(WRITE_IMG_OBJS)
# 	@mkdir -p $(dir $@)
# 	$(CC) $^ $(LIBS) -lEGL -lX11 -o $@


# WRITE_IMG_KERNEL_SRC = writeimg-kernel.c
# WRITE_IMG_KERNEL_OBJS = $(WRITE_IMG_KERNEL_SRC:%=$(OBJDIR)/%.o)
# WRITE_IMG_KERNEL_DEPS = $(WRITE_IMG_KERNEL_OBJS:%=%.d)
# -include $(WRITE_IMG_KERNEL_DEPS)
# imggrab_kernel: $(OBJDIR)/writeimg-kernel
obj-m += fb_sys_write_override.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

# $(OBJDIR)/writeimg-kernel: $(WRITE_IMG_KERNEL_OBJS)
# 	@mkdir -p $(dir $@)
# 	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
#	$(CC) $^ $(LIBS) -lEGL -lX11 -o $@

# .PHONY: clean
# clean:
# 	rm -rf $(BUILDDIR)

# .PHONY: clean-build
# clean-build: clean imggrab_kernel
