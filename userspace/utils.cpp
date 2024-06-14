#include <string_view>
#include <iostream>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "utils.h"

using namespace cv;
using namespace std;

std::string_view get_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            if (it + 1 != end)
                return *(it + 1);
    }
    
    return "";
}

bool has_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            return true;
    }
    
    return false;
}

vector<float> readBinaryFile(string fileName) {
    vector<float> ret;
    float f;
    ifstream in(fileName, std::ios::binary);
    while (in.read(reinterpret_cast<char*>(&f), sizeof(float))) {
        ret.push_back(f);
    }
    return ret;
 }

void writeBinaryFile(string fileName, vector<float> arr, size_t len) {
    ofstream f(fileName, std::ios::out | std::ios::binary);
    f.write((char*)arr.data(), arr.size() * sizeof( decltype(arr)::value_type ));
}

/* Create the inverse gamma lookup table*/
// void createGammaLUT() {
//     vector<float> gammaLUT;
//     for (int i = 0; i < 256; i++) {
//         float x = static_cast<float>(i) / 255.0f;
//         if (x <= 0.04045) {
//             gammaLUT.push_back(x / 12.92);
//         } else {
//             gammaLUT.push_back(pow((x + 0.055) / 1.055, 2.4));
//         }
//     }
//     writeBinaryFile("inverseGammaLUT.bin", gammaLUT, 256);
// }

Mat cropVideo(const Mat& inputImage, double targetAspectRatio) {
    
    int frameWidth = inputImage.cols;
    int frameHeight = inputImage.rows;
    int targetWidth, targetHeight;

    
    if (frameWidth / static_cast<double>(frameHeight) > targetAspectRatio) {
        targetWidth = frameHeight * targetAspectRatio;
        targetHeight = frameHeight;
    } 
    else {
        targetWidth = frameWidth;
        targetHeight = frameWidth / targetAspectRatio;
    }

    Rect cropRect((frameWidth - targetWidth) / 2, (frameHeight - targetHeight) / 2, targetWidth, targetHeight);
    return inputImage(cropRect);

}

Mat addLetterbox(const Mat& inputImage, int targetWidth, int targetHeight) {
    int xOffset = (targetWidth - inputImage.cols) / 2;
    int yOffset = (targetHeight - inputImage.rows) / 2;
    Mat outputImage(targetHeight, targetWidth, inputImage.type(), Scalar(0, 0, 0));
    inputImage.copyTo(outputImage(Rect(xOffset, yOffset, inputImage.cols, inputImage.rows)));
    return outputImage;
}

// Resize a video using bicubic interpolation without changing aspect ratio
Mat resizeVideo(Mat frame, int targetWidth, int targetHeight, bool crop, bool letterbox) {
   
    double aspectRatio = static_cast<double>(frame.cols) / static_cast<double>(frame.rows);
    int origWidth = frame.cols;
    int newWidth, newHeight;

    if (crop){
        newWidth = targetWidth;
        newHeight = targetHeight;
    }
    else{
        if (targetWidth / aspectRatio <= targetHeight) {
            newWidth = targetWidth;
            newHeight = static_cast<int>(targetWidth / aspectRatio);
        }   
        else {
            newHeight = targetHeight;
            newWidth = static_cast<int>(targetHeight * aspectRatio);
        }
    }

    Mat resizedFrame;

    if (crop){
        frame = cropVideo(frame, static_cast<double>(targetWidth)/static_cast<double>(targetHeight));
    }

    if (origWidth < targetWidth){
        resize(frame, resizedFrame, Size(newWidth, newHeight), 0, 0, INTER_CUBIC); // or INTER_LINEAR enlarge
    }
    else {
        resize(frame, resizedFrame, Size(newWidth, newHeight), 0, 0, INTER_AREA); // shrink
    }

    if (letterbox) {
        resizedFrame = addLetterbox(resizedFrame, targetWidth, targetHeight);
    }

    return resizedFrame;
}
