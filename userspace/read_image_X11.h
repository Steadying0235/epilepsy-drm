//
// Created by ao on 6/12/24.
//

#ifndef USERSPACE_READ_IMAGE_X11_H
#define USERSPACE_READ_IMAGE_X11_H

#endif //USERSPACE_READ_IMAGE_X11_H

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <iostream>

#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#ifdef __linux__
#include <sys/time.h>
#endif


XImage* * read_image_from_xserver(bool debug);

void is_read_image_X11_alive();