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
#include <sys/mman.h>

#include <png.h>

#include <X11/Xlib.h>
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <gbm.h>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

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

    // Obtain image dimensions
    EGLint img_width, img_height;
    img_width = img->width;
    img_height = img->height;

// Calculate the size of the image data
    size_t img_size = img_width * img_height * 4; // Assuming RGBA format

// Allocate memory to store the image data
    unsigned char *img_data = (unsigned char *)malloc(img_size);
    if (!img_data) {
        // Handle memory allocation error
        return;
    }

// Bind EGLImage to get its data
    if (!eglExportDMABUFImageQueryMESA(edisp, eimg, NULL, NULL, NULL)) {
        // Handle EGLImage export query error
        free(img_data);
        return;
    }

// Assuming you have obtained 'eimg_data' somehow (not specified in the question)

// Write the image data to a PNG file
    if (!stbi_write_png("output.png", img_width, img_height, 4, img_data, img_width * 4)) {
        // Handle stbi_write_png error
    }

// Free allocated memory
    free(img_data);




// Cleanup EGLImage and OpenGL resources as needed

}

// Function to initialize EGL
EGLDisplay initialize_egl() {
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
    return egl_display;
}

//EGLImageKHR create_egl_image_from_drm(EGLDisplay egl_display, DmaBuf dma_fb) {
//    EGLint image_attrs[] = {
//            EGL_WIDTH, dma_fb.width,
//            EGL_HEIGHT, dma_fb.height,
//            EGL_LINUX_DRM_FOURCC_EXT, static_cast<EGLint>(dma_fb.fourcc),
//            EGL_DMA_BUF_PLANE0_FD_EXT, dma_fb.fd,
//            EGL_DMA_BUF_PLANE0_OFFSET_EXT, dma_fb.offset,
//            EGL_DMA_BUF_PLANE0_PITCH_EXT, dma_fb.pitch,
//            EGL_NONE
//    };
//    EGLImageKHR egl_image = eglCreateImageKHR(egl_display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, image_attrs);
//    if (egl_image == EGL_NO_IMAGE_KHR) {
//        fprintf(stderr, "Failed to create EGL image\n");
//        exit(EXIT_FAILURE);
//    }
//    return egl_image;
//
//}

void save_png(const char *filename, unsigned char *image_data, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Failed to open %s for writing\n", filename);
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        fprintf(stderr, "Error: png_create_write_struct failed\n");
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
        fclose(fp);
        fprintf(stderr, "Error: png_create_info_struct failed\n");
        return;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // Write image data row by row
    png_bytep row_pointers[height];
    for (int y = 0; y < height; y++) {
        row_pointers[y] = image_data + y * width * 4; // Assuming RGBA format
    }
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

// Function to save OpenGL texture to a file
bool saveTextureToFile(const char* filename, GLuint textureID, GLenum format, int width, int height) {
    // Allocate memory for the texture data
    unsigned char* data = new unsigned char[width * height * 4]; // Assuming RGBA format

    // Bind the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Read texture data from GPU to CPU
    glGetTexImage(GL_TEXTURE_2D, 0, format, GL_UNSIGNED_BYTE, data);

    // Unbind the texture
    glBindTexture(GL_TEXTURE_2D, 0);

    // Write texture data to file using stb_image_write
    bool success = stbi_write_png(filename, width, height, 4, data, width * 4);

    // Clean up allocated memory
    delete[] data;

    return success;
}


int read_image_libdrm() {

    uint32_t fb_id = get_framebuffer_id();

    MSG("Framebuffer id grabbed: %x", fb_id);


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
            int texture_dmabuf_fd;

            DmaBuf img;
            img.width = fb->width;
            img.height = fb->height;
            img.pitch = fb->pitch;
            img.offset = 0;
            img.fourcc = DRM_FORMAT_XRGB8888; // FIXME

            const int ret = drmPrimeHandleToFD(drmfd, fb->handle, 0, &dma_buf_fd);
            MSG("drmPrimeHandleToFD = %d, fd = %d", ret, dma_buf_fd);
            img.fd = dma_buf_fd;

            // Assuming OpenGL context is properly set up and bound

            GLuint textureId;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);

            // Allocate texture storage (assuming RGBA format for simplicity)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

            // Map the DMA-BUF into OpenGL texture
            void *ptr = mmap(NULL, img.pitch * img.height, PROT_READ, MAP_SHARED, img.fd, 0);
            if (ptr == MAP_FAILED) {
                // Handle mmap error
                cout << "mmap failed" << endl;
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.width, img.height, GL_RGBA, GL_UNSIGNED_BYTE, ptr);

            munmap(ptr, img.pitch * img.height);

            // Set texture parameters (filtering and wrapping)
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            bool success = saveTextureToFile("texture_dump.png", textureId, GL_RGBA, img.width, img.height);

            if (success) {
                std::cout << "Texture dumped successfully to texture_dump.png\n";
            } else {
                std::cerr << "Failed to dump texture to file\n";
            }

            // Unbind the texture
            glBindTexture(GL_TEXTURE_2D, 0);


            runEGL(&img);



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