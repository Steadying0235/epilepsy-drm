cmake_minimum_required(VERSION 3.16)
project(userspace)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include(CMakePrintHelpers)

# Path to your custom compiled OpenCV build
set(OpenCV_DIR /home/ao/scratch/epilepsy-drm/userspace/opencv-build)
find_package(OpenCV REQUIRED)

# Compiler options to compile for baseline or adaptive
option(BASELINE "Baseline" ON)
option(ADAPTIVE "Adaptive" OFF)

if(BASELINE)
    add_definitions(-DBASELINE)
    add_executable(userspace main_loop.cpp read_image_X11.cpp detect_image.cpp utils.cpp )
endif()
if(ADAPTIVE)
    add_definitions(-DADAPTIVE)
    add_executable(userspace main_loop.cpp read_image_X11.cpp detect_image.cpp utils.cpp )
endif()

# Add your executable


# include_directories(${OpenCV_INCLUDE_DIRS})
# Include DRM and OpenCV directores
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(/usr/include/drm)

# Find the necessary libraries
find_package(X11 REQUIRED)

# Find EGL library
find_library(EGL_LIBRARY NAMES EGL PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu)

# DEBUG: print out opencv libraries linked:
cmake_print_variables(${OpenCV_LIBS})

target_link_libraries(userspace ${OpenCV_LIBS})

# Link against libX11 and libEGL
target_link_libraries(userspace X11 Xext m ${EGL_LIBRARY})

# Link against libpng
target_link_libraries(userspace png)

# Link OpenGL
target_link_libraries(userspace GL)

# Link against your custom compiled OpenCV libraries
#link_directories(${OpenCV_DIR}/lib)


