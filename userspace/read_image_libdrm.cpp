#include "read_image_libdrm.h"
#include <chrono>

// debug texture exporter
#ifdef DEBUG_LIBDRM
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
using namespace std;
#define ASSERT(cond) \
	if (!(cond)) { \
		MSG("ERROR @ %s:%d: (%s) failed", __FILE__, __LINE__, #cond); \
		throw 0; \
	}



void enumerateModeResources(int fd, const drmModeResPtr res) {
//    MSG("\tcount_fbs = %d", res->count_fbs);
//    for (int i = 0; i < res->count_fbs; ++i)
//        MSG("\t\t%d: 0x%x", i, res->fbs[i]);

//    MSG("\tcount_crtcs = %d", res->count_crtcs);
    for (int i = 0; i < res->count_crtcs; ++i) {
//        MSG("\t\t%d: 0x%x", i, res->crtcs[i]);
        drmModeCrtcPtr crtc = drmModeGetCrtc(fd, res->crtcs[i]);
        if (crtc) {
//            MSG("\t\t\tbuffer_id = 0x%x gamma_size = %d", crtc->buffer_id, crtc->gamma_size);
//            MSG("\t\t\t(%u %u %u %u) %d",
//                crtc->x, crtc->y, crtc->width, crtc->height, crtc->mode_valid);
//            MSG("\t\t\tmode.name = %s", crtc->mode.name);
            drmModeFreeCrtc(crtc);
        }
    }

//    MSG("\tcount_connectors = %d", res->count_connectors);
    for (int i = 0; i < res->count_connectors; ++i) {
//        MSG("\t\t%d: 0x%x", i, res->connectors[i]);
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
    cout << "start grabbing framebuffer id" << endl;
    const int available = drmAvailable();
    if (available == 0){
        perror("DRM not available");
        return 1;
    }

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
//    MSG("count_fbs = %d", count_fbs);
    for (int i = 0; i < count_fbs; ++i) {
//        MSG("Framebuffer id: %#x", fbs[i]);
        // Get a Framebuffer pointer
        drmModeFBPtr fb = drmModeGetFB(fd, fbs[i]);

//        MSG("Framebuffer %d, Width: %d", i, fb->width);

        if (fb->width > highest_resolution_width) {
            highest_resolution_fb = fbs[i];
            highest_resolution_width = fb->width;
        }

        drmModeFreeFB(fb);
    }

//    MSG("%#x is the highest resolution framebuffer", highest_resolution_fb);
    close(fd);

    return highest_resolution_fb;
}

static int width = 1280, height = 720;

typedef struct {
    int width, height;
    uint32_t fourcc;
    int fd, offset, pitch;
} DmaBuf;

//EGL Symbol table might be bugged
EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list) __attribute__((weak)); // May not be in libEGL symbol table, resolve manually :(
EGLImageKHR eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list){
    static PFNEGLCREATEIMAGEKHRPROC createImageProc = 0;
    if(!createImageProc)
        createImageProc = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    return createImageProc(dpy, ctx, target, buffer, attrib_list);
}

EGLBoolean eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image) __attribute__((weak)); // May not be in libEGL symbol table, resolve manually :(
EGLBoolean eglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image){
    static PFNEGLDESTROYIMAGEKHRPROC destroyImageProc = 0;
    if(!destroyImageProc)
        destroyImageProc = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    return destroyImageProc(dpy, image);
}

vector<GLuint> runEGL(const DmaBuf *img, int num_frames) {
    std::vector<GLuint> frame_textures(num_frames);

    Display *xdisp;
    ASSERT(xdisp = XOpenDisplay(NULL));
    eglBindAPI(EGL_OPENGL_API);
    EGLDisplay edisp = eglGetDisplay(xdisp);
    EGLint ver_min, ver_maj;
    eglInitialize(edisp, &ver_maj, &ver_min);
//    MSG("EGL: version %d.%d", ver_maj, ver_min);
//    MSG("EGL: EGL_VERSION: '%s'", eglQueryString(edisp, EGL_VERSION));
//    MSG("EGL: EGL_VENDOR: '%s'", eglQueryString(edisp, EGL_VENDOR));
//    MSG("EGL: EGL_CLIENT_APIS: '%s'", eglQueryString(edisp, EGL_CLIENT_APIS));
//    MSG("EGL: client EGL_EXTENSIONS: '%s'", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
//    MSG("EGL: EGL_EXTENSIONS: '%s'", eglQueryString(edisp, EGL_EXTENSIONS));

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

//    MSG("%s", glGetString(GL_EXTENSIONS));

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
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES =
            (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    ASSERT(glEGLImageTargetTexture2DOES);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eimg);
    ASSERT(glGetError() == 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);


    // frame timing
//    if (eglSwapInterval(edisp, 1) != EGL_TRUE) {
//        cerr << "Issue with setting vsync on display" << endl;
//    }

    for (int i = 0; i < num_frames; ++i) {
        glFinish();

        auto start = std::chrono::high_resolution_clock::now();


        // as it stands, this code currently references rather than copies the EGL image
//        GLuint tex;
//        glGenTextures(1, &tex);
//        glBindTexture(GL_TEXTURE_2D, tex);
//        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eimg);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//        frame_textures[i] = tex;
//        cout << "Grab Frame " << i << endl;
//

        // this is an attempt to copy the image VRAM-VRAM rather than just make a reference
        // Think this is pretty good, basically just renders the egl image to a rednerbuffer which we create a texture out of
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
// Create a temporary texture and bind the EGL image to it
        GLuint tempTex;
        glGenTextures(1, &tempTex);
        glBindTexture(GL_TEXTURE_2D, tempTex);
        glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eimg);
// Create a framebuffer and attach the temporary texture to it
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempTex, 0);
// Check if framebuffer is complete
//        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            // Get the dimensions of the texture
            GLint width, height;
            glBindTexture(GL_TEXTURE_2D, tempTex);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
            // Allocate memory to read the pixel data
            GLubyte *pixels = new GLubyte[width * height * 4]; // Assuming RGBA format
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            // Bind the new texture and upload the pixel data
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

            delete[] pixels;
//        }
// Cleanup & vector add
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &tempTex);
        frame_textures[i] = tex;

        // if in a debug build, dumb copy framebuffer to cpu to debug the texture
        #ifdef DEBUG_LIBDRM
        cout << "DEBUG: Dumped image to texture_dump.png" << endl;
            // Define variables to store texture width and height
        int width = img->width;
        int height = img->height;
        // Create a buffer to hold texture data
            unsigned char *data = (unsigned char*)malloc(width * height * 4); // Assuming RGBA format
        // Read texture data from GPU
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

            char filename_buf [27];
        sprintf(filename_buf, "texture_dump_frame_%d.png", i);

        // Save the texture data to a PNG file using stb_image_write
            stbi_write_png(filename_buf, width, height, 4, data, width * 4);
        // Free allocated memory
            free(data);
        #endif

//        if (!eglSwapBuffers(edisp, esurf)) {
//            cout << "Error with swapping" << endl;
//        }

        glFinish();

        // frame timing after image is grabbed
        auto stop = std::chrono::high_resolution_clock::now();

        auto frametime = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);

        cout << "frametime: " << frametime.count() << " microseconds" << endl;

        //
        long time_to_next_frame = ((1000 / 30) * 1000) - frametime.count();

//        if (time_to_next_frame > 0 ) {
//            cout << "sleep " << time_to_next_frame << endl;
//            std::this_thread::sleep_for(std::chrono::microseconds(time_to_next_frame));
//        }

    }


//exit:
//    eglMakeCurrent(edisp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
//    eglDestroyContext(edisp, ectx);
//    eglDestroySurface(xdisp, esurf);
//    XDestroyWindow(xdisp, xwin);
//    eglTerminate(edisp);
//    XCloseDisplay(xdisp);

    // return texture id

    return frame_textures;




// Cleanup EGLImage and OpenGL resources as needed

}



std::vector<GLuint> read_image_libdrm(int num_frames) {

    uint32_t fb_id = get_framebuffer_id();
//    MSG("Framebuffer id grabbed: %x", fb_id);
    const char *card = "/dev/dri/card0";

//    MSG("Opening card %s", card);
    const int drmfd = open(card, O_RDONLY);
    if (drmfd < 0) {
        perror("Cannot open card");
        throw 1;
    }

    int dma_buf_fd = -1;
    drmModeFBPtr fb = drmModeGetFB(drmfd, fb_id);
    if (fb) {


//        MSG("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
//            fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

        if (fb->handle) {
            int texture_dmabuf_fd;

            DmaBuf img;
            img.width = fb->width;
            img.height = fb->height;
            img.pitch = fb->pitch;
            img.offset = 0;
            img.fourcc = DRM_FORMAT_XRGB8888; // FIXME

            const int ret = drmPrimeHandleToFD(drmfd, fb->handle, 0, &dma_buf_fd);
//            MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
            img.fd = dma_buf_fd;

            vector<GLuint> texture = runEGL(&img, num_frames);

            return texture;

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

    throw 0;



}

void is_read_image_libdrm_alive() {
    cout << "read_image_libdrm loaded" << endl;
}

void verify_extensions_exit(){
    EGLDisplay egl_display;
    EGLint major, minor;
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "Failed to get EGL display\n");
        exit(EXIT_FAILURE);
    }
    if (!eglInitialize(egl_display, &major, &minor)) {
        fprintf(stderr, "Failed to initialize EGL\n");
        exit(EXIT_FAILURE);
    }


    const char* extensions = eglQueryString(egl_display, EGL_EXTENSIONS);
    if (extensions != nullptr && std::strstr(extensions, "KHR_image_base") != nullptr) {
        // Extension is supported, proceed with eglCreateImageKHR
        cout << "KHR Image supported" << endl;
    } else {
        // Extension not supported, handle error
    }

    //terminate the display
    eglTerminate(egl_display);

}