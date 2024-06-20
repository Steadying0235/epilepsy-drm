cmake_minimum_required(VERSION 3.10)
project(userspace)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

include(CMakePrintHelpers)

# Path to your custom compiled OpenCV build
set(OpenCV_DIR /home/ao/scratch/epilepsy-drm/userspace/opencv-build)
find_package(OpenCV REQUIRED)

# Compiler options to determine which ingest method to use to get the image
option(READ_LIBDRM "Read_libdrm" ON)
option(READ_X11 "Read_X11" OFF)

# Compiler options to compile for baseline or adaptive
option(BASELINE "Baseline" ON)
option(ADAPTIVE "Adaptive" OFF)

# set the detection method to baseline or adaptive via compiler option
if(BASELINE)
    add_definitions(-DBASELINE)
endif()
if(ADAPTIVE)
    add_definitions(-DADAPTIVE)
endif()

# Set the read method based on the compiler option
if(READ_LIBDRM)
    add_definitions(-DREAD_LIBDRM)
endif()
if(READ_X11)
    add_definitions(-DREAD_X11)
endif()

add_executable(userspace main_loop.cpp read_image_X11.cpp detect_image.cpp utils.cpp read_image_libdrm.cpp
        read_image_libdrm.cpp)

# Add your executable


# include_directories(${OpenCV_INCLUDE_DIRS})
# Include DRM and OpenCV directores
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(/usr/include/drm)

# Find the necessary libraries
find_package(X11 REQUIRED)

# Find EGL library
#find_library(EGL_LIBRARY NAMES EGL PATHS /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu)
#find_package(EGL REQUIRED)
#target_link_libraries(userspace EGL)
#target_link_libraries(userspace ${EGL_KHR_LIBRARY})

# Find EGL and EGL KHR headers
find_path(EGL_INCLUDE_DIR EGL/egl.h PATH_SUFFIXES EGL)
find_path(EGL_KHR_INCLUDE_DIR EGL/eglext.h PATH_SUFFIXES EGL)
# Find EGL library
find_library(EGL_LIBRARY NAMES EGL)

# If you need to find KHR extensions library (if separate)
# find_library(EGL_KHR_LIBRARY NAMES EGL_KHR)

# Add include directories
include_directories(${EGL_INCLUDE_DIR} ${EGL_KHR_INCLUDE_DIR})

target_link_libraries(userspace ${EGL_LIBRARY} ${EGL_KHR_LIBRARY})


# DEBUG: print out opencv libraries linked:
cmake_print_variables(${OpenCV_LIBS})

target_link_libraries(userspace ${OpenCV_LIBS})

# Link against libX11 and libEGL
target_link_libraries(userspace X11 Xext)
#target_link_libraries(userspace m ${EGL_LIBRARY})

target_link_libraries(userspace
        ${OPENGL_gl_LIBRARY}  # Link OpenGL library
        ${EGL_LIBRARIES}      # Link EGL library
)

# Link against libpng
target_link_libraries(userspace png)

# Link OpenGL
target_link_libraries(userspace GL)

# Link libdrm and libgbm
target_link_libraries(userspace drm gbm)

# Copy inverseGammaLUT.bin to the build directory
configure_file(support/inverseGammaLUT.bin ${CMAKE_CURRENT_BINARY_DIR}/inverseGammaLUT.bin COPYONLY)


# Link against your custom compiled OpenCV libraries
#link_directories(${OpenCV_DIR}/lib)


