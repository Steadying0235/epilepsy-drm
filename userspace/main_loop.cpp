#include <iostream>
#include "read_image_X11.h"
#include "detect_image.h"

// run() contains the main read -> detect -> block flow
void run() {


    // read image
    XImage ** image = read_image_from_xserver(true);

    std::cout << "Got image from Xserver" << std::endl;
//
//    std::cout << image->height << " x " << image->width << " \n\n" << image->data << std::endl;


    // detect image
    char ** placeholder_argc = new char *[1];
    placeholder_argc[0] = "hello.cpp";
    detect_epileptic_image(image);
    // block the flow


}

int main() {
    std::cout << "Hello, World!" << std::endl;

    is_read_image_X11_alive();
    run();

    return 0;
}
