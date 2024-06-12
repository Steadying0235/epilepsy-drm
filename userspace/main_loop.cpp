#include <iostream>
#include "read_image_X11.h"

// run() contains the main read -> detect -> block flow
void run() {


    // read image
    read_image_from_xserver(true);

    // detect image

    // block the flow


}

int main() {
    std::cout << "Hello, World!" << std::endl;

    is_read_image_X11_alive();
    run();

    return 0;
}
