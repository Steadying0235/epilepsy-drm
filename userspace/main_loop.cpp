#include <iostream>
#include <vector>
#include "fstream"

// import x11 if X11 shm used as ingest
#ifdef READ_X11
#include "read_image_X11.h"
#endif

// import libdrm if libdrm used as ingest
#ifdef READ_LIBDRM
#include "read_image_libdrm.h"
#endif


#include "detect_image.h"


#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h> // For PNG file saving (you'll need libpng)
#include <GL/gl.h>
#include <GL/glext.h>



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
    verify_extensions_exit();
    is_read_image_libdrm_alive();
    // print image data
    std::cout << "Using libdrm to read desktop " << std::endl;
    GLuint texture = read_image_libdrm();

    std::vector<GLuint> textures;
    textures.resize(3);
    textures[0] = texture;
    textures[1] = texture;
    textures[2] = texture;

    detect_epileptic_image_opengl(textures);

#endif

}

int main() {
    std::cout << "Hello, World!" << std::endl;


    run();

    return 0;
}
