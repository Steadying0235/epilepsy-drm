#include <iostream>
#include "vector"
#include "fstream"

#include "read_image_X11.h"
#include "detect_image.h"
#include "read_image_libdrm.h"

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h> // For PNG file saving (you'll need libpng)
#include <GL/gl.h>
#include <GL/glext.h>



void save_raw_image_to_file(shmimage *image) {
    std::ofstream file;
    file.open("image.raw", std::ios::binary);
    file.write((char *) image[0].data, image[0].ximage->height * image[0].ximage->width * 4);
    file.close();
}

void save_texture_to_file(GLuint texture) {
    glBindTexture(GL_TEXTURE_2D, texture);
    int width, height;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    std::vector<uint8_t> data(width * height * 4);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    std::ofstream file;
    file.open("texture.raw", std::ios::binary);
    file.write((char *) data.data(), width * height * 4);
    file.close();
}


// run() contains the main read -> detect -> block flow
void run() {
    std::cout << "Checking Helper Scripts:" << std::endl;


#ifdef READ_X11
    is_read_image_X11_alive();
    std::cout << "Using X API to read desktop " << std::endl;
    std::vector<std::vector<unsigned int>> image = read_image_from_xserver(true);
    char ** placeholder_argc = new char *[1];
    placeholder_argc[0] = "hello.cpp";
    detect_epileptic_image(image);
#endif
#ifdef READ_LIBDRM
    is_read_image_libdrm_alive();
    // print image data
    std::cout << "Using libdrm to read desktop " << std::endl;
    read_image_libdrm();

#endif

}

int main() {
    std::cout << "Hello, World!" << std::endl;


    run();

    return 0;
}
