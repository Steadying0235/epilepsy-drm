# Confirm that We have basic tools to build OpenCV
sudo apt update && sudo apt install -y cmake g++ wget unzip
 
# Download and unpack OpenCV 4
wget -O opencv.zip https://github.com/opencv/opencv/archive/4.x.zip
unzip opencv.zip
 
# Create build directory
mkdir -p opencv-build && cd opencv-build
 
# Configure
cmake ../opencv-4.x
 
# Build (This takes a long time)
cmake --build .


# Install other depedencies
sudo apt-get install -y libgbm-dev

# If on Intel, install Intel OpenCL and headers
sudo apt install intel-opencl-icd clinfo ocl-icd-opencl-dev

