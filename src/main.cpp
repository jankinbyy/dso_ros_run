#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "util/settings.h"
#include "FullSystem/FullSystem.h"
#include "util/Undistort.h"
#include "IOWrapper/Pangolin/PangolinDSOViewer.h"
#include "IOWrapper/OutputWrapper/SampleOutputWrapper.h"

#include <ros/ros.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/CameraInfo.h>
#include <geometry_msgs/PoseStamped.h>
#include "cv_bridge/cv_bridge.h"

std::string calib = "";
std::string vignetteFile = "";
std::string gammaFile = "";
std::string saveFile = "./result.txt";
bool useSampleOutput = false;

using namespace dso;

void parseArgument(char *arg) {
    int option;
    char buf[1000];

    if (1 == sscanf(arg, "sampleoutput=%d", &option)) {
        if (option == 1) {
            useSampleOutput = true;
            printf("USING SAMPLE OUTPUT WRAPPER!\n");
        }
        return;
    }

    if (1 == sscanf(arg, "quiet=%d", &option)) {
        if (option == 1) {
            setting_debugout_runquiet = true;
            printf("QUIET MODE, I'll shut up!\n");
        }
        return;
    }

    if (1 == sscanf(arg, "nolog=%d", &option)) {
        if (option == 1) {
            setting_logStuff = false;
            printf("DISABLE LOGGING!\n");
        }
        return;
    }

    if (1 == sscanf(arg, "nogui=%d", &option)) {
        if (option == 1) {
            disableAllDisplay = true;
            printf("NO GUI!\n");
        }
        return;
    }
    if (1 == sscanf(arg, "nomt=%d", &option)) {
        if (option == 1) {
            multiThreading = false;
            printf("NO MultiThreading!\n");
        }
        return;
    }
    if (1 == sscanf(arg, "calib=%s", buf)) {
        calib = buf;
        printf("loading calibration from %s!\n", calib.c_str());
        return;
    }
    if (1 == sscanf(arg, "vignette=%s", buf)) {
        vignetteFile = buf;
        printf("loading vignette from %s!\n", vignetteFile.c_str());
        return;
    }

    if (1 == sscanf(arg, "gamma=%s", buf)) {
        gammaFile = buf;
        printf("loading gammaCalib from %s!\n", gammaFile.c_str());
        return;
    }

    if (1 == sscanf(arg, "savefile=%s", buf)) {
        saveFile = buf;
        printf("saving to %s on finish!\n", saveFile.c_str());
        return;
    }
    printf("could not parse argument \"%s\"!!\n", arg);
}

FullSystem *fullSystem = 0;
Undistort *undistorter = 0;
int frame_id = 0;

void img_call_back(const sensor_msgs::ImageConstPtr img) {
    cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(img, sensor_msgs::image_encodings::MONO8);
    assert(cv_ptr->image.type() == CV_8U);
    assert(cv_ptr->image.channels() == 1);

    if (setting_fullResetRequested) {
        std::vector<IOWrap::Output3DWrapper *> wraps = fullSystem->outputWrapper;
        delete fullSystem;
        for (IOWrap::Output3DWrapper *ow : wraps) ow->reset();
        fullSystem = new FullSystem();
        fullSystem->linearizeOperation = false;
        fullSystem->outputWrapper = wraps;
        if (undistorter->photometricUndist != 0)
            fullSystem->setGammaFunction(undistorter->photometricUndist->getG());
        setting_fullResetRequested = false;
    }

    MinimalImageB minImg((int)cv_ptr->image.cols, (int)cv_ptr->image.rows, (unsigned char *)cv_ptr->image.data);
    ImageAndExposure *undistImg = undistorter->undistort<unsigned char>(&minImg, 1, 0, 1.0f);
    undistImg->timestamp = img->header.stamp.toSec(); // relay the timestamp to dso
    fullSystem->addActiveFrame(undistImg, frame_id);
    frame_id++;
    delete undistImg;
}

int main(int argc, char **argv) {
    ros::init(argc, argv, "dso_live");
    for (int i = 1; i < argc; i++) parseArgument(argv[i]);
    setting_desiredImmatureDensity = 1000;
    setting_desiredPointDensity = 1200;
    setting_minFrames = 5;
    setting_maxFrames = 7;
    setting_maxOptIterations = 4;
    setting_minOptIterations = 1;
    setting_logStuff = false;
    setting_kfGlobalWeight = 1.3;

    printf("MODE WITH CALIBRATION, but without exposure times!\n");
    setting_photometricCalibration = 2;
    setting_affineOptModeA = 0;
    setting_affineOptModeB = 0;

    undistorter = Undistort::getUndistorterForFile(calib, gammaFile, vignetteFile);

    setGlobalCalib(
        (int)undistorter->getSize()[0],
        (int)undistorter->getSize()[1],
        undistorter->getK().cast<float>());

    fullSystem = new FullSystem();
    fullSystem->linearizeOperation = false;

    if (!disableAllDisplay)
        fullSystem->outputWrapper.push_back(new IOWrap::PangolinDSOViewer(
            (int)undistorter->getSize()[0],
            (int)undistorter->getSize()[1]));

    if (useSampleOutput)
        fullSystem->outputWrapper.push_back(new IOWrap::SampleOutputWrapper());

    if (undistorter->photometricUndist != 0)
        fullSystem->setGammaFunction(undistorter->photometricUndist->getG());

    ros::NodeHandle nh;
    ros::Subscriber imgSub = nh.subscribe("image", 1, &img_call_back);

    ros::spin();
    fullSystem->printResult(saveFile);
    for (IOWrap::Output3DWrapper *ow : fullSystem->outputWrapper) {
        ow->join();
        delete ow;
    }

    delete undistorter;
    delete fullSystem;

    return 0;
}
