#include <iostream>
#include <vector>
#include <chrono>

// import x11 if X11 shm used as ingest
#ifdef READ_X11
#include "read_image_X11.h"
#endif

using namespace std;

// import libdrm if libdrm used as ingest
#ifdef READ_LIBDRM
#include "read_image_libdrm.h"
#endif


#include "detect_image.h"


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

    int num_frames = 120;

    auto start = std::chrono::high_resolution_clock::now();
    // print image data
    std::cout << "Using libdrm to read desktop " << std::endl;
    vector<GLuint> textures = read_image_libdrm(num_frames);
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = (end - start);

    std::cout << "Took " << elapsed.count() << "milliseconds to capture " << num_frames << " frames" << endl;
//    detect_epileptic_image_opengl(textures);
#endif



}

int main() {
    std::cout << "Hello, World!" << std::endl;


    auto start = std::chrono::high_resolution_clock::now();

    run();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = (end - start);
    std::cout << "Total program elapsed time: " << elapsed.count() << " miliseconds" << endl;



    return 0;
}
