//
// Created by ao on 6/19/24.
//


//#!cc -Werror -std=c99 -I/usr/include/libdrm -ldrm enum.c -o enum && ./enum /dev/dri/card0
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#include <X11/Xlib.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <gbm.h>

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
using namespace std;
#define ASSERT(cond) \
	if (!(cond)) { \
		MSG("ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond); \
		return; \
	}



void enumerateModeResources(int fd, const drmModeResPtr res) {
    MSG("\tcount_fbs = %d", res->count_fbs);
    for (int i = 0; i < res->count_fbs; ++i)
        MSG("\t\t%d: 0x%x", i, res->fbs[i]);

    MSG("\tcount_crtcs = %d", res->count_crtcs);
    for (int i = 0; i < res->count_crtcs; ++i) {
        MSG("\t\t%d: 0x%x", i, res->crtcs[i]);
        drmModeCrtcPtr crtc = drmModeGetCrtc(fd, res->crtcs[i]);
        if (crtc) {
            MSG("\t\t\tbuffer_id = 0x%x gamma_size = %d", crtc->buffer_id, crtc->gamma_size);
            MSG("\t\t\t(%u %u %u %u) %d",
                crtc->x, crtc->y, crtc->width, crtc->height, crtc->mode_valid);
            MSG("\t\t\tmode.name = %s", crtc->mode.name);
            drmModeFreeCrtc(crtc);
        }
    }

    MSG("\tcount_connectors = %d", res->count_connectors);
    for (int i = 0; i < res->count_connectors; ++i) {
        MSG("\t\t%d: 0x%x", i, res->connectors[i]);
        drmModeConnectorPtr conn = drmModeGetConnectorCurrent(fd, res->connectors[i]);
        if (conn) {
            drmModeFreeConnector(conn);
        }
    }

    MSG("\tcount_encoders = %d", res->count_encoders);
    for (int i = 0; i < res->count_encoders; ++i)
        MSG("\t\t%d: 0x%x", i, res->encoders[i]);

    MSG("\twidth: %u .. %u", res->min_width, res->max_width);
    MSG("\theight: %u .. %u", res->min_height, res->max_height);
}

uint32_t get_framebuffer_id() {
    const int available = drmAvailable();
    if (!available)
        return 1;

    const char *card = "/dev/dri/card0";

    const int fd = open(card, O_RDONLY);
    MSG("open = %d", fd);
    if (fd < 2)
        return 2;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);

    drmModeResPtr res = drmModeGetResources(fd);
    if (res) {
        enumerateModeResources(fd, res);
        drmModeFreeResources(res);
    }

#define MAX_FBS 16
    uint32_t fbs[MAX_FBS];
    int count_fbs = 0;

    drmModePlaneResPtr planes = drmModeGetPlaneResources(fd);
    if (planes) {
        MSG("count_planes = %u", planes->count_planes);
        for (uint32_t i = 0; i < planes->count_planes; ++i) {
            MSG("\t%u: %#x", i, planes->planes[i]);
            drmModePlanePtr plane = drmModeGetPlane(fd, planes->planes[i]);
            if (plane) {
                if (plane->fb_id) {
                    int found = 0;
                    for (int k = 0; k < count_fbs; ++k) {
                        if (fbs[k] == plane->fb_id) {
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        if (count_fbs == MAX_FBS) {
                            MSG("Max number of fbs (%d) exceeded", MAX_FBS);
                        } else {
                            fbs[count_fbs++] = plane->fb_id;
                        }
                    }
                }
                drmModeFreePlane(plane);
            }
        }
        drmModeFreePlaneResources(planes);
    }

    uint32_t highest_resolution_fb = 0x00;
    uint32_t highest_resolution_width = 0;
    MSG("count_fbs = %d", count_fbs);
    for (int i = 0; i < count_fbs; ++i) {
        MSG("Framebuffer id: %#x", fbs[i]);
        // Get a Framebuffer pointer
        drmModeFBPtr fb = drmModeGetFB(fd, fbs[i]);

        MSG("Framebuffer %d, Width: %d", i, fb->width);

        if (fb->width > highest_resolution_width) {
            highest_resolution_fb = fbs[i];
            highest_resolution_width = fb->width;
        }

        drmModeFreeFB(fb);
    }

    MSG("%#x is the highest resolution framebuffer", highest_resolution_fb);
    close(fd);

    return highest_resolution_fb;
}

static int width = 1280, height = 720;

typedef struct {
    int width, height;
    uint32_t fourcc;
    int fd, offset, pitch;
} DmaBuf;

void runEGL(const DmaBuf *img) {
    Display *xdisp;
    ASSERT(xdisp = XOpenDisplay(NULL));
    eglBindAPI(EGL_OPENGL_API);
    EGLDisplay edisp = eglGetDisplay(xdisp);
    EGLint ver_min, ver_maj;
    eglInitialize(edisp, &ver_maj, &ver_min);
    MSG("EGL: version %d.%d", ver_maj, ver_min);
    MSG("EGL: EGL_VERSION: '%s'", eglQueryString(edisp, EGL_VERSION));
    MSG("EGL: EGL_VENDOR: '%s'", eglQueryString(edisp, EGL_VENDOR));
    MSG("EGL: EGL_CLIENT_APIS: '%s'", eglQueryString(edisp, EGL_CLIENT_APIS));
    MSG("EGL: client EGL_EXTENSIONS: '%s'", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    MSG("EGL: EGL_EXTENSIONS: '%s'", eglQueryString(edisp, EGL_EXTENSIONS));

    static const EGLint econfattrs[] = {
            EGL_BUFFER_SIZE, 32,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,

            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

            EGL_NONE
    };
    EGLConfig config;
    EGLint num_config;
    eglChooseConfig(edisp, econfattrs, &config, 1, &num_config);

    XVisualInfo *vinfo = NULL;
    {
        XVisualInfo xvisual_info = {0};
        int num_visuals;
        ASSERT(eglGetConfigAttrib(edisp, config, EGL_NATIVE_VISUAL_ID, (EGLint*)&xvisual_info.visualid));
        ASSERT(vinfo = XGetVisualInfo(xdisp, VisualScreenMask | VisualIDMask, &xvisual_info, &num_visuals));
    }

    XSetWindowAttributes winattrs = {0};
    winattrs.event_mask = KeyPressMask | KeyReleaseMask |
                          ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                          ExposureMask | VisibilityChangeMask | StructureNotifyMask;
    winattrs.border_pixel = 0;
    winattrs.bit_gravity = StaticGravity;
    winattrs.colormap = XCreateColormap(xdisp,
                                        RootWindow(xdisp, vinfo->screen),
                                        vinfo->visual, AllocNone);
    ASSERT(winattrs.colormap != None);
    winattrs.override_redirect = False;

    Window xwin = XCreateWindow(xdisp, RootWindow(xdisp, vinfo->screen),
                                0, 0, width, height,
                                0, vinfo->depth, InputOutput, vinfo->visual,
                                CWBorderPixel | CWBitGravity | CWEventMask | CWColormap,
                                &winattrs);
    ASSERT(xwin);

    XStoreName(xdisp, xwin, "kmsgrab");

    {
        Atom delete_message = XInternAtom(xdisp, "WM_DELETE_WINDOW", True);
        XSetWMProtocols(xdisp, xwin, &delete_message, 1);
    }

    XMapWindow(xdisp, xwin);

    static const EGLint ectx_attrs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
    };
    EGLContext ectx = eglCreateContext(edisp, config,
                                       EGL_NO_CONTEXT, ectx_attrs);
    ASSERT(EGL_NO_CONTEXT != ectx);

    EGLSurface esurf = eglCreateWindowSurface(edisp, config, xwin, 0);
    ASSERT(EGL_NO_SURFACE != esurf);

    ASSERT(eglMakeCurrent(edisp, esurf,
                          esurf, ectx));

    MSG("%s", glGetString(GL_EXTENSIONS));

    // FIXME check for EGL_EXT_image_dma_buf_import
    EGLAttrib eimg_attrs[] = {
            EGL_WIDTH, img->width,
            EGL_HEIGHT, img->height,
            EGL_LINUX_DRM_FOURCC_EXT, img->fourcc,
            EGL_DMA_BUF_PLANE0_FD_EXT, img->fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, img->offset,
            EGL_DMA_BUF_PLANE0_PITCH_EXT, img->pitch,
            EGL_NONE
    };
    EGLImage eimg = eglCreateImage(edisp, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0,
                                   eimg_attrs);
    ASSERT(eimg);

    // FIXME check for GL_OES_EGL_image (or alternatives)
    GLuint tex = 1;
    //glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
            (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ASSERT(glEGLImageTargetTexture2DOES);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eimg);
    ASSERT(glGetError() == 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    const char *fragment =
            "#version 130\n"
            "uniform vec2 res;\n"
            "uniform sampler2D tex;\n"
            "void main() {\n"
            "vec2 uv = gl_FragCoord.xy / res;\n"
            "uv.y = 1. - uv.y;\n"
            "gl_FragColor = texture(tex, uv);\n"
            "}\n"
    ;
    int prog = ((PFNGLCREATESHADERPROGRAMVPROC)(eglGetProcAddress("glCreateShaderProgramv")))(GL_FRAGMENT_SHADER, 1, &fragment);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tex"), 0);

    for (;;) {
        while (XPending(xdisp)) {
            XEvent e;
            XNextEvent(xdisp, &e);
            switch (e.type) {
                case ConfigureNotify:
                {
                    width = e.xconfigure.width;
                    height = e.xconfigure.height;
                }
                    break;

                case KeyPress:
                    switch(XLookupKeysym(&e.xkey, 0)) {
                        case XK_Escape:
                        case XK_q:
                            goto exit;
                            break;
                    }
                    break;

                case ClientMessage:
                case DestroyNotify:
                case UnmapNotify:
                    goto exit;
                    break;
            }
        }

        {
            glViewport(0, 0, width, height);
            glClear(GL_COLOR_BUFFER_BIT);

            glUniform2f(glGetUniformLocation(prog, "res"), width, height);
            glRects(-1, -1, 1, 1);

            ASSERT(eglSwapBuffers(edisp, esurf));
        }
    }

    exit:
    eglMakeCurrent(edisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(edisp, ectx);
    eglDestroySurface(xdisp, esurf);
    XDestroyWindow(xdisp, xwin);
    eglTerminate(edisp);
    XCloseDisplay(xdisp);
}

// Function to convert DmaBuf to EGL image
bool convertDmaBufToEGLImage(DmaBuf &dmaBuf) {
    int fd = dmaBuf.fd;

    // Initialize GBM
    struct gbm_device *gbmDev = gbm_create_device(fd);
    if (!gbmDev) {
        std::cerr << "Failed to create GBM device." << std::endl;
        return false;
    }

    // Import the DMA-BUF buffer to GBM
    struct gbm_bo *bo = gbm_bo_import(gbmDev, GBM_BO_IMPORT_FD, reinterpret_cast<void *>(fd), GBM_BO_USE_SCANOUT);
    if (!bo) {
        std::cerr << "Failed to import DMA-BUF to GBM." << std::endl;
        gbm_device_destroy(gbmDev);
        return false;
    }

    // Initialize EGL
    EGLDisplay eglDpy = eglGetDisplay((EGLNativeDisplayType)gbmDev);
    if (eglDpy == EGL_NO_DISPLAY) {
        std::cerr << "Failed to get EGL display." << std::endl;
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    EGLBoolean eglInitResult = eglInitialize(eglDpy, NULL, NULL);
    if (eglInitResult != EGL_TRUE) {
        std::cerr << "Failed to initialize EGL." << std::endl;
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    // Create EGL surface
    EGLConfig config;
    EGLint numConfigs;
    EGLint configAttribs[] = {
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_NONE
    };

    if (!eglChooseConfig(eglDpy, configAttribs, &config, 1, &numConfigs)) {
        std::cerr << "Failed to choose EGL config." << std::endl;
        eglTerminate(eglDpy);
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    EGLSurface eglSurf = eglCreateWindowSurface(eglDpy, config, (EGLNativeWindowType)bo, NULL);
    if (eglSurf == EGL_NO_SURFACE) {
        std::cerr << "Failed to create EGL surface." << std::endl;
        eglTerminate(eglDpy);
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    // Create EGL context
    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext eglCtx = eglCreateContext(eglDpy, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglCtx == EGL_NO_CONTEXT) {
        std::cerr << "Failed to create EGL context." << std::endl;
        eglDestroySurface(eglDpy, eglSurf);
        eglTerminate(eglDpy);
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    // Make the context current
    if (!eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx)) {
        std::cerr << "Failed to make EGL context current." << std::endl;
        eglDestroyContext(eglDpy, eglCtx);
        eglDestroySurface(eglDpy, eglSurf);
        eglTerminate(eglDpy);
        gbm_bo_destroy(bo);
        gbm_device_destroy(gbmDev);
        return false;
    }

    // At this point, EGL context is set up and ready to use
    std::cout << "EGL initialized successfully." << std::endl;

    // Optionally, render or use the EGL surface here

    // Clean up EGL
    eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(eglDpy, eglCtx);
    eglDestroySurface(eglDpy, eglSurf);
    eglTerminate(eglDpy);

    // Clean up GBM
    gbm_bo_destroy(bo);
    gbm_device_destroy(gbmDev);

    // Close DMA-BUF file descriptor
    close(fd);

    return true;
}

int read_image_libdrm() {

    uint32_t fb_id = get_framebuffer_id();


    const char *card = "/dev/dri/card0";

    MSG("Opening card %s", card);
    const int drmfd = open(card, O_RDONLY);
    if (drmfd < 0) {
        perror("Cannot open card");
        return 1;
    }

    int dma_buf_fd = -1;
    drmModeFBPtr fb = drmModeGetFB(drmfd, fb_id);
    if (fb) {


        MSG("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
            fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

        if (fb->handle) {
            // With edge cases handled, we're ready

            DmaBuf img;
            img.width = fb->width;
            img.height = fb->height;
            img.pitch = fb->pitch;
            img.offset = 0;
            img.fourcc = DRM_FORMAT_XRGB8888; // FIXME

            const int ret = drmPrimeHandleToFD(drmfd, fb->handle, 0, &dma_buf_fd);
            MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
            img.fd = dma_buf_fd;

            // If in debug, mirror to
//            runEGL(&img);


            // Using EGL
            bool success = convertDmaBufToEGLImage(img);

        }
        else{
            MSG("Not permitted to get fb handles. Run either with sudo, or setcap cap_sys_admin+ep");
            drmModeFreeFB(fb);
            close(drmfd);
        }


    }
    else {
        close(drmfd);
        MSG("Cannot open fb %#x", fb_id);
    }

    return 0;



}

void is_read_image_libdrm_alive() {
    cout << "read_image_libdrm loaded" << endl;
}