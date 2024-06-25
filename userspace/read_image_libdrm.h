//
// Created by ao on 6/19/24.
//

#ifndef USERSPACE_READ_IMAGE_LIBDRM_H
#define USERSPACE_READ_IMAGE_LIBDRM_H

#endif //USERSPACE_READ_IMAGE_LIBDRM_H


#include <GL/gl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <sys/mman.h>

#include <X11/Xlib.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <gbm.h>
#include <cstring>

#pragma once

GLuint  read_image_libdrm();

void is_read_image_libdrm_alive();

void verify_extensions_exit();


