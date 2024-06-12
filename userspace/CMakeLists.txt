cmake_minimum_required(VERSION 3.28)
project(userspace)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/include/drm)

# Find the necessary libraries
find_package(X11 REQUIRED)

# Find EGL library
find_library(EGL_LIBRARY NAMES EGL PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu)

# Add your executable
add_executable(userspace read_image_X11.cpp main_loop.cpp)

# Link against libX11 and libEGL
target_link_libraries(userspace X11 Xext m ${EGL_LIBRARY})
