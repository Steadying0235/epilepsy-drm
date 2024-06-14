#include <iostream>
#include <cmath>
#include <vector>
#include <string_view>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>  
#include <array>
#include <opencv2/opencv.hpp>
#include <opencv2/core/hal/intrin.hpp>

#include <chrono>

#include "utils.h"

using namespace std;
using namespace std::chrono;
using namespace cv;

#define NTHREADS    24
#define pixelDensity(w, h, s) sqrt(w*w + h*h) / s
#define minSafeArea(vd, pd)  vd*vd*pd*pd*0.1745*0.1309*0.25   // Technique G176: Keeping the flashing area small enough


#ifdef BASELINE
struct Frame {
    Mat I;
    Mat Rr;
    Mat_<Point2f> S;
    int harmfulLumIncCount, harmfulLumDecCount, harmfulColIncCount, harmfulColDecCount;
    Mat_<bool> isIncLum, isDecLum;
    Mat_<bool> isIncCol, isDecCol;

    Frame(int resolution_h, int resolution_w){
        I = Mat(resolution_h, resolution_w, CV_32F);
        Rr = Mat(resolution_h, resolution_w, CV_32F);
        S = Mat_<Point2f>(resolution_h, resolution_w, Point2f(0.0f, 0.0f));
        harmfulLumIncCount = harmfulLumDecCount = harmfulColIncCount = harmfulColDecCount = 0;
        isIncLum = Mat_<bool>(resolution_h, resolution_w, false);
        isDecLum = Mat_<bool>(resolution_h, resolution_w, true);
        isIncCol = Mat_<bool>(resolution_h, resolution_w, false);
        isDecCol = Mat_<bool>(resolution_h, resolution_w, true);
    };

    Frame(){}; // default constructor
};
struct ThreadData {
    int index;
    int countIncLum, countDecLum, countIncCol, countDecCol;
};
#endif

#ifdef ADAPTIVE
struct Frame {
    Mat I;
    Mat Rr;
    Mat_<Point2f> S;
    int harmfulLumCount, harmfulColCount;

    Frame(int resolution_h, int resolution_w){
        I = Mat(resolution_h, resolution_w, CV_32F);
        Rr = Mat(resolution_h, resolution_w, CV_32F);
        S = Mat_<Point2f>(resolution_h, resolution_w, Point2f(0.0f, 0.0f));  
        harmfulLumCount = harmfulColCount = 0;
    };

    Frame(){}; // default constructor
};
struct ThreadData {
    int index;
    int countLum, countCol;
};
#endif


Mat frame;
Frame f[2];
int resolution_h, resolution_w;
constexpr int LUT_SIZE = 256;
vector<float> gammaLUT;

/* Fast inverse gamma correction using the lookup table */
inline float inverseGammaFast(float value) {

    if (value < 0)              return gammaLUT[0];
    else if (value >= LUT_SIZE) return gammaLUT[LUT_SIZE - 1];
    return gammaLUT[(int)value];
}

/* Calculate luminance and color of pixels */
void* calcLumColor(void* arg) {

    ThreadData* threadData = static_cast<ThreadData*>(arg);
    int index = threadData->index; // thread number
    int rowsPerThread = ceil(static_cast<double>(resolution_h) / NTHREADS);
    int startRow = index * rowsPerThread;
    int endRow = min(startRow + rowsPerThread, resolution_h);

    /* Process row group */
    for (int i = startRow; i < endRow; i++) {

        /* Set pointer position */
        Vec3f *pixelRow = frame.ptr<Vec3f>(i);
        auto *I_Row = f[1].I.ptr<float>(i);
        auto *Rr_Row = f[1].Rr.ptr<float>(i);
        auto *S_Row = f[1].S.ptr<Point2f>(i);

        /* Process pixels */
        for (int j = 0; j < resolution_w; j++) {

            Vec3f &pixel = pixelRow[j];

            /* Inverse Gamma Correction */
            float r = inverseGammaFast(pixel[2]);
            float g = inverseGammaFast(pixel[1]);
            float b = inverseGammaFast(pixel[0]);

            I_Row[j] = r * 0.2126 + g * 0.7152 + b * 0.0722; // luminance calculation
            Rr_Row[j] = (r+g+b==0)? 0 : r / (r + g + b); // red ratio calculation

            /* CIE XYZ color space conversion */
            float X = b * 0.1804375 + g * 0.3575761 + r * 0.4124564;
            float Y = b * 0.0721750 + g * 0.7151522 + r * 0.2126729;
            float Z = b * 0.9503041 + g * 0.1191920 + r * 0.0193339;

            /* Get chromaticity coordinates */
            S_Row[j].x = (X+Y+Z==0)? 0 : (4 * X) / (X + 15 * Y + 3 * Z);
            S_Row[j].y = (X+Y+Z==0)? 0 : (9 * Y) / (X + 15 * Y + 3 * Z);
        }
    }

    return nullptr;
}

void* checkLumColThresh(void* arg) {

    ThreadData* threadData = static_cast<ThreadData*>(arg);
    int index = threadData->index; // thread number
    int rowsPerThread = ceil(static_cast<double>(resolution_h) / NTHREADS);
    int startRow = index * rowsPerThread;
    int endRow = min(startRow + rowsPerThread, resolution_h);

    int countIncLum = 0, countDecLum = 0, countIncCol = 0, countDecCol = 0; // for baseline    
    int countLum = 0, countCol = 0; // for adaptive

    for (int i = startRow; i < endRow; i++){

        auto *frame_Row = frame.ptr<Vec3f>(i);

        auto *I1_Row = f[0].I.ptr<float>(i);
        auto *Rr1_Row = f[0].Rr.ptr<float>(i);
        auto *S1_Row = f[0].S.ptr<Point2f>(i);
        auto *I2_Row = f[1].I.ptr<float>(i);
        auto *Rr2_Row = f[1].Rr.ptr<float>(i);
        auto *S2_Row = f[1].S.ptr<Point2f>(i);

#ifdef BASELINE
        auto *isIncLum1_Row = f[0].isIncLum.ptr<bool>(i);
        auto *isIncCol1_Row = f[0].isIncCol.ptr<bool>(i);
        auto *isDecLum1_Row = f[0].isDecLum.ptr<bool>(i);
        auto *isDecCol1_Row = f[0].isDecCol.ptr<bool>(i);
        auto *isIncLum2_Row = f[1].isIncLum.ptr<bool>(i);
        auto *isIncCol2_Row = f[1].isIncCol.ptr<bool>(i);
        auto *isDecLum2_Row = f[1].isDecLum.ptr<bool>(i);
        auto *isDecCol2_Row = f[1].isDecCol.ptr<bool>(i);
#endif
        
        for (int j = 0; j < resolution_w; j++) {
            float dx = S1_Row[j].x - S2_Row[j].x;
            float dy = S1_Row[j].y - S2_Row[j].y; 

            bool isHarmfulLum = (abs(I1_Row[j]-I2_Row[j]) > 0.1) && (I1_Row[j] < 0.8 || I2_Row[j] < 0.8);
            bool isHarmfulCol = (Rr1_Row[j] >= 0.8 || Rr2_Row[j] >= 0.8) && ((dx*dx + dy*dy) > 0.04); // avoid sqrt for speed

#ifdef BASELINE
            if (isHarmfulLum){
                isIncLum2_Row[j] = (I1_Row[j] < I2_Row[j]);
                isDecLum2_Row[j] = (I1_Row[j] > I2_Row[j]);
                if (isIncLum1_Row[j] && isDecLum2_Row[j]) countDecLum += 1;
                if (isDecLum1_Row[j] && isIncLum2_Row[j]) countIncLum += 1;               
            }
            else{
                isIncLum2_Row[j] = isIncLum1_Row[j];
                isDecLum2_Row[j] = isDecLum1_Row[j];
            }
            if (isHarmfulCol){
                isIncCol2_Row[j] = (Rr1_Row[j] < Rr2_Row[j]);
                isDecCol2_Row[j] = (Rr1_Row[j] > Rr2_Row[j]);
                if (isIncCol1_Row[j] && isDecCol2_Row[j]) countDecCol += 1;
                if (isDecCol1_Row[j] && isIncCol2_Row[j]) countIncCol += 1;               
            } 
            else{
                isIncCol2_Row[j] = isIncCol1_Row[j];
                isDecCol2_Row[j] = isDecCol1_Row[j];
            }
#endif
#ifdef ADAPTIVE
            /* Michaelson contrast for HDR*/
            isHarmfulLum = isHarmfulLum || ((abs(I1_Row[j]-I2_Row[j])/(I1_Row[j]+I2_Row[j]) > 1/17) && (I1_Row[j] > 0.8 && I2_Row[j] > 0.8)); // 0.0588235294118
            
            if (isHarmfulLum) countLum++;
            if (isHarmfulCol) countCol++;
            //might need to split transition too, but only check inc or dec, not pairs
#endif
        }
    }

#ifdef BASELINE
    threadData->countIncLum = countIncLum;
    threadData->countIncCol = countIncCol;
    threadData->countDecLum = countDecLum;
    threadData->countDecCol = countDecCol;
#endif
#ifdef ADAPTIVE
    threadData->countLum = countLum;
    threadData->countCol = countCol;
#endif

    return nullptr;
}

/* ./baseline -f filename [-letterbox -crop] */
/* ./adaptive -f filename [-letterbox -crop] [-h xx -w xx -size xx -d xx] */
// TODO: batch process
int detect_epileptic_image(int argc, char ** argv) {

    /* Parse command line arguments */
    const std::vector<std::string_view> args(argv, argv + argc);
    
    /* Parse filename */
    const string filename = has_option(args, "-f")? get_option(args, "-f").data() : "";
    if (filename == ""){
        cout << "No file provided (Use the -f option)" << endl;
        return -1;
    }

    int screenSize, viewingDistance;

#ifdef BASELINE
    /* Parse resolution, screen size and viewing distance */
    cout << "Using baseline mode" << endl;
    resolution_h = 768;
    resolution_w = 1024;
    // screenSize = 15;
    // viewingDistance = 23;
    const int minSafeArea = 21824; // 341*256*0.25
#endif
#ifdef ADAPTIVE
    cout << "Using adaptive mode" << endl;
    if (!has_option(args, "-h") || !has_option(args, "-w") || !has_option(args, "-size") || !has_option(args, "-d")){
        cout << "Missing arguments for adaptive mode" << endl;
        return -1;
    }
    resolution_h = atoi(get_option(args, "-h").data());
    resolution_w = atoi(get_option(args, "-w").data());
    screenSize = atoi(get_option(args, "-size").data());
    viewingDistance = atoi(get_option(args, "-d").data());
    const double pixelDensity = pixelDensity(resolution_w, resolution_h, screenSize);
    const int minSafeArea = minSafeArea(viewingDistance, pixelDensity); 
#endif

    /* Prepare inverse gamma lookup table */
    gammaLUT = readBinaryFile("inverseGammaLUT.bin");

    bool useLetterbox = false, useCrop = false;
    if (has_option(args, "-letterbox") && has_option(args, "-crop")){
        cout << "Multiple resize mode, choose either -letterbox or -crop" << endl;
        return -1;
    }
    else if (has_option(args, "-letterbox")){
        cout << "Resize and letterboxing" << endl;
        useLetterbox = true;
    }
    else if (has_option(args, "-crop")){
        cout << "Resize and cropping" << endl;
        useCrop = true;
    }
    else{
        cout << "Provide a resize mode" << endl;
        return -1;
    }
      
    auto start = high_resolution_clock::now();

    // /* Frame struct initilization */
    f[0] = Frame(resolution_h, resolution_w);
    f[1] = Frame(resolution_h, resolution_w);

    /* open video file */
    VideoCapture cap(filename);
    if (!cap.isOpened()){
        cout << "Error opening video stream or file" << endl;
        return -1;
    }

    /* get FPS */
    const int fps = cap.get(CAP_PROP_FPS);

    int freqLum = 0, freqCol = 0;

    queue<Frame> oneSecFrames;

    bool hasFlash = false, hasRed = false;
    pthread_t threads[NTHREADS];
    ThreadData threadData[NTHREADS];
    int indexArray[NTHREADS];

    int frameCount = 0;
    /* Main loop */
    while (cap.read(frame)) {

        frameCount++;

        /* Adjust resloution of video using bicubic interpolation*/
        /* Aspect ratio is kept, letterboxing or cropping used */
        frame = resizeVideo(frame, resolution_w, resolution_h, useCrop, useLetterbox);

        frame.convertTo(frame, CV_32FC3);

        /* Multi-threaded luminance and color calculation */
        for (int i = 0; i < NTHREADS; i++){
            threadData[i].index = i;
            if (pthread_create(&threads[i], nullptr, &calcLumColor, &threadData[i]) != 0){
                perror("Failed to create thread");
            }
        }
        for (int i = 0; i < NTHREADS; i++){
            if (pthread_join(threads[i], nullptr) != 0) {
                perror("Failed to join thread");
            }
        }

        /* Multi-threaded harmful luminance and color differences check */
        for (int i = 0; i < NTHREADS; i++){
            threadData[i].index = i;
            if (pthread_create(&threads[i], nullptr, &checkLumColThresh, &threadData[i]) != 0){
                perror("Failed to create thread");
            }
        }

        for (int i = 0; i < NTHREADS; i++){
            if (pthread_join(threads[i], nullptr) != 0) {
                perror("Failed to join thread");
            }
        }

#ifdef BASELINE
        for (int i = 0; i < NTHREADS; i++){
            f[1].harmfulLumIncCount += threadData[i].countIncLum;
            f[1].harmfulColIncCount += threadData[i].countIncCol;
            f[1].harmfulLumDecCount += threadData[i].countDecLum;
            f[1].harmfulColDecCount += threadData[i].countDecCol;
        }

        /* Check area threshold && Update total number of harmful flashes */
        if ((f[1].harmfulLumIncCount > minSafeArea) || (f[1].harmfulLumDecCount > minSafeArea)) freqLum++;
        if ((f[1].harmfulColIncCount > minSafeArea) || (f[1].harmfulColDecCount > minSafeArea)) freqCol++;        

        /* Sliding window keeps only one-second worth of frames */
        oneSecFrames.push(f[1]);

        if (oneSecFrames.size() > fps){
            if ((oneSecFrames.front().harmfulLumIncCount > minSafeArea) || (oneSecFrames.front().harmfulLumDecCount > minSafeArea)) freqLum--;
            if ((oneSecFrames.front().harmfulColIncCount > minSafeArea) || (oneSecFrames.front().harmfulColDecCount > minSafeArea)) freqCol--;
            oneSecFrames.pop();
        } 

         /* Check frequency threshold */
        if (freqLum/2 > 3) hasFlash = true;
        if (freqCol/2 > 3) hasRed = true;

#endif
#ifdef ADAPTIVE
        for (int i = 0; i < NTHREADS; i++){
            f[1].harmfulLumCount += threadData[i].countLum;
            f[1].harmfulColCount += threadData[i].countCol;
        }

        /* Check area threshold && Update total number of harmful trnasitions */
        if (f[1].harmfulLumCount > minSafeArea) freqLum++;
        if (f[1].harmfulColCount > minSafeArea) freqCol++;

        /* Sliding window keeps only one-second worth of frames */
        oneSecFrames.push(f[1]);

        if (oneSecFrames.size() > fps){
            if (oneSecFrames.front().harmfulLumCount > minSafeArea) freqLum--;
            if (oneSecFrames.front().harmfulColCount > minSafeArea) freqCol--;
            oneSecFrames.pop();
        } 

        printf("%d %d %d\n", frameCount, freqLum, f[1].harmfulLumCount);

        /* Check frequency threshold */
        if (freqLum > 6) hasFlash = true;
        if (freqCol > 6) hasRed = true;       
#endif

        if(hasFlash and hasRed){
            break;
        }
        
        /* Shift frames by 1 */
        swap(f[0], f[1]);
        f[1] = Frame(resolution_h, resolution_w);
    }

    
    cout << "hasFlash " << hasFlash << endl;
    cout << "hasRed " << hasRed << endl;

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<seconds>(stop - start);

    // FILE* outfile;
    // outfile = fopen("baseline_simulated.csv", "a");
    // fprintf(outfile, "%s,%d,%d,%d,%ld\n", basename(argv[2]), hasFlash, hasRed, hasFlash || hasRed, duration.count());
    // fclose(outfile);
   
    return 0;
}

