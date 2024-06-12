//#!cc -Werror -std=c99 -I/usr/include/libdrm -ldrm enum.c -o enum && ./enum /dev/dri/card0
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

#define MSG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

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

        if (fb->width > highest_resolution_width) {
            highest_resolution_fb = fbs[i];
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

int main(int argc, const char *argv[]) {

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
    if (!fb) {
        MSG("Cannot open fb %#x", fb_id);
        goto cleanup;
    }

    MSG("fb_id=%#x width=%u height=%u pitch=%u bpp=%u depth=%u handle=%#x",
        fb_id, fb->width, fb->height, fb->pitch, fb->bpp, fb->depth, fb->handle);

    if (!fb->handle) {
        MSG("Not permitted to get fb handles. Run either with sudo, or setcap cap_sys_admin+ep %s", argv[0]);
        goto cleanup;
    }

    DmaBuf img;
    img.width = fb->width;
    img.height = fb->height;
    img.pitch = fb->pitch;
    img.offset = 0;

    img.fourcc = DRM_FORMAT_XRGB8888; // FIXME

    // const int ret(drmfd, fb->handle, 0, &dma_buf_fd);
    // MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
    img.fd = dma_buf_fd;

    // err = ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb->dumb_framebuffer);
    // if (err) {
    //     printf("Could not create dumb framebuffer (err=%d)\n", err);
    //     goto cleanup;
    // }

    // err = drmModeAddFB(fd, resolution->hdisplay, resolution->vdisplay, 24, 32,
    //                    fb->dumb_framebuffer.pitch, fb->dumb_framebuffer.handle, &fb->buffer_id);
    // if (err) {
    //     printf("Could not add framebuffer to drm (err=%d)\n", err);
    //     goto cleanup;
    // }

    // attach img to the framebuffer
//    drmModeRmFB(output);?


//    //POC with the CPU writing pixels to the framebuffer
//    //
//    fb->dumb_framebuffer.height = resolution->vdisplay;
//    fb->dumb_framebuffer.width = resolution->hdisplay;
//    fb->dumb_framebuffer.bpp = 32;
//
//    err = ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &fb->dumb_framebuffer);
//    if (err) {
//        printf("Could not create dumb framebuffer (err=%d)\n", err);
//        goto cleanup;
//    }
//
//    err = drmModeAddFB(fd, resolution->hdisplay, resolution->vdisplay, 24, 32,
//                       fb->dumb_framebuffer.pitch, fb->dumb_framebuffer.handle, &fb->buffer_id);
//    if (err) {
//        printf("Could not add framebuffer to drm (err=%d)\n", err);
//        goto cleanup;
//    }



    cleanup:
    if (dma_buf_fd >= 0)
        close(dma_buf_fd);
    if (fb)
        drmModeFreeFB(fb);
    close(drmfd);
    return 0;



}
