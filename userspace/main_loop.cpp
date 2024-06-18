#include <iostream>
#include "read_image_X11.h"
#include "vector"
#include "fstream"

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


    // read image
    std::vector<unsigned int *> image = read_image_from_xserver(true);

    std::cout << "Got image from Xserver" << std::endl;

    // print image data
    std::cout << "Image data: " << std::endl;

    // convert image[0] to an opengl texture
//    GLuint texture;
//    glGenTextures(1, &texture);
//    glBindTexture(GL_TEXTURE_2D, texture);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image[0]->ximage->width, image[0]->ximage->height, 0, GL_BGRA, GL_UNSIGNED_BYTE, image[0]->data);
//    glGenerateMipmap(GL_TEXTURE_2D);
//
//    // dump texture to file
//    save_texture_to_file(texture);


    // detect image


//
//    std::cout << image->height << " x " << image->width << " \n\n" << image->data << std::endl;


    // detect image
    char ** placeholder_argc = new char *[1];
    placeholder_argc[0] = "hello.cpp";
    //detect_epileptic_image(image);
    // block the flow


}

int main() {
    std::cout << "Hello, World!" << std::endl;

    is_read_image_X11_alive();
    run();

    return 0;
}
