/****************************************************************************
 *
 * This file is part of the ViSP software.
 * Copyright (C) 2005 - 2017 by Inria. All rights reserved.
 *
 * This software is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * See the file LICENSE.txt at the root directory of this source
 * distribution for additional information about the GNU GPL.
 *
 * For using ViSP with software that can not be combined with the GNU
 * GPL, please contact Inria about acquiring a ViSP Professional
 * Edition License.
 *
 * See http://visp.inria.fr for more information.
 *
 * This software was developed at:
 * Inria Rennes - Bretagne Atlantique
 * Campus Universitaire de Beaulieu
 * 35042 Rennes Cedex
 * France
 *
 * If you have questions regarding the use of this file, please contact
 * Inria at visp@inria.fr
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Description:
 * Regression test for MBT.
 *
 *****************************************************************************/

/*!
  \example testGenericTracker.cpp

  \brief Regression test for MBT.
*/

#include <cstdlib>
#include <iostream>
#include <visp3/core/vpConfig.h>

#if defined(VISP_HAVE_MODULE_MBT)

#include <visp3/core/vpIoTools.h>
#include <visp3/io/vpParseArgv.h>
#include <visp3/io/vpImageIo.h>
#include <visp3/gui/vpDisplayX.h>
#include <visp3/gui/vpDisplayGDI.h>
#include <visp3/gui/vpDisplayOpenCV.h>
#include <visp3/gui/vpDisplayD3D.h>
#include <visp3/gui/vpDisplayGTK.h>
#include <visp3/mbt/vpMbGenericTracker.h>

#define GETOPTARGS "i:dclt:e:Dh"

namespace
{
  void usage(const char *name, const char *badparam)
  {
    fprintf(stdout, "\n\
    Regression test for vpGenericTracker.\n\
    \n\
    SYNOPSIS\n\
      %s [-i <test image path>] [-c] [-d] [-h] [-l] \n\
     [-t <tracker type>] [-e <last frame index>] [-D]\n", name);

    fprintf(stdout, "\n\
    OPTIONS:                                               \n\
      -i <input image path>                                \n\
         Set image input path.\n\
         These images come from ViSP-images-x.y.z.tar.gz available \n\
         on the ViSP website.\n\
         Setting the VISP_INPUT_IMAGE_PATH environment\n\
         variable produces the same behavior than using\n\
         this option.\n\
    \n\
      -d \n\
         Turn off the display.\n\
    \n\
      -c\n\
         Disable the mouse click. Useful to automate the \n\
         execution of this program without human intervention.\n\
    \n\
      -t <tracker type>\n\
         Set tracker type (<1 (Edge)>, <2 (KLT)>, <3 (both)>) for color sensor.\n\
    \n\
      -l\n\
         Use the scanline for visibility tests.\n\
    \n\
      -e <last frame index>\n\
         Specify the index of the last frame. Once reached, the tracking is stopped.\n\
    \n\
      -D \n\
         Use depth.\n\
    \n\
      -h \n\
         Print the help.\n\n");

    if (badparam)
      fprintf(stdout, "\nERROR: Bad parameter [%s]\n", badparam);
  }

  bool getOptions(int argc, const char **argv, std::string &ipath, bool &click_allowed, bool &display,
                  bool &useScanline, int &trackerType, int &lastFrame, bool &use_depth)
  {
    const char *optarg_;
    int c;
    while ((c = vpParseArgv::parse(argc, argv, GETOPTARGS, &optarg_)) > 1) {

      switch (c) {
      case 'i':
        ipath = optarg_;
        break;
      case 'c':
        click_allowed = false;
        break;
      case 'd':
        display = false;
        break;
      case 'l':
        useScanline = true;
        break;
      case 't':
        trackerType = atoi(optarg_);
        break;
      case 'e':
        lastFrame = atoi(optarg_);
        break;
      case 'D':
        use_depth = true;
        break;
      case 'h':
        usage(argv[0], NULL);
        return false;
        break;

      default:
        usage(argv[0], optarg_);
        return false;
        break;
      }
    }

    if ((c == 1) || (c == -1)) {
      // standalone param or error
      usage(argv[0], NULL);
      std::cerr << "ERROR: " << std::endl;
      std::cerr << "  Bad argument " << optarg_ << std::endl << std::endl;
      return false;
    }

    return true;
  }

  bool read_data(const std::string &input_directory, const int cpt, const vpCameraParameters &cam_depth,
                 vpImage<unsigned char> &I, vpImage<uint16_t> &I_depth,
                 std::vector<vpColVector> &pointcloud, vpHomogeneousMatrix &cMo)
  {
    char buffer[256];
    sprintf(buffer, std::string(input_directory + "/Images/Image_%04d.pgm").c_str(), cpt);
    std::string image_filename = buffer;

    sprintf(buffer, std::string(input_directory + "/Depth/Depth_%04d.bin").c_str(), cpt);
    std::string depth_filename = buffer;

    sprintf(buffer, std::string(input_directory + "/CameraPose/Camera_%03d.txt").c_str(), cpt);
    std::string pose_filename = buffer;

    if (!vpIoTools::checkFilename(image_filename) || !vpIoTools::checkFilename(depth_filename)
        || !vpIoTools::checkFilename(pose_filename))
      return false;

    vpImageIo::read(I, image_filename);

    unsigned int depth_width = 0, depth_height = 0;
    std::ifstream file_depth(depth_filename.c_str(), std::ios::in | std::ios::binary);
    if (!file_depth.is_open())
      return false;

    vpIoTools::readBinaryValueLE(file_depth, depth_height);
    vpIoTools::readBinaryValueLE(file_depth, depth_width);
    I_depth.resize(depth_height, depth_width);
    pointcloud.resize(depth_height*depth_width);

    const float depth_scale = 0.000030518f;
    for (unsigned int i = 0; i < I_depth.getHeight(); i++) {
      for (unsigned int j = 0; j < I_depth.getWidth(); j++) {
        vpIoTools::readBinaryValueLE(file_depth, I_depth[i][j]);
        double x = 0.0, y = 0.0, Z = I_depth[i][j] * depth_scale;
        vpPixelMeterConversion::convertPoint(cam_depth, j, i, x, y);
        vpColVector pt3d(4, 1.0);
        pt3d[0] = x*Z;
        pt3d[1] = y*Z;
        pt3d[2] = Z;
        pointcloud[i*I_depth.getWidth()+j] = pt3d;
      }
    }

    std::ifstream file_pose(pose_filename.c_str());
    if (!file_pose.is_open()) {
      return false;
    }

    for (unsigned int i = 0; i < 4; i++) {
      for (unsigned int j = 0; j < 4; j++) {
        file_pose >> cMo[i][j];
      }
    }

    return true;
  }
}

int main(int argc, const char *argv[])
{
  try {
    std::string env_ipath = "";
    std::string opt_ipath = "";
    bool opt_click_allowed = true;
    bool opt_display = true;
    bool useScanline = false;
    int trackerType_image = vpMbGenericTracker::EDGE_TRACKER;
#if defined(__mips__) || defined(__mips) || defined(mips) || defined(__MIPS__)
    // To avoid Debian test timeout
    int opt_lastFrame = 5;
#else
    int opt_lastFrame = -1;
#endif
    bool use_depth = false;

    // Get the visp-images-data package path or VISP_INPUT_IMAGE_PATH
    // environment variable value
    env_ipath = vpIoTools::getViSPImagesDataPath();

    // Read the command line options
    if (!getOptions(argc, argv, opt_ipath, opt_click_allowed, opt_display,
                    useScanline, trackerType_image, opt_lastFrame, use_depth)) {
      return EXIT_FAILURE;
    }

    std::cout << "trackerType_image: " << trackerType_image << std::endl;
    std::cout << "useScanline: " << useScanline << std::endl;
    std::cout << "use_depth: " << use_depth << std::endl;
#ifdef VISP_HAVE_COIN3D
    std::cout << "COIN3D available." << std::endl;
#endif

#if !defined(VISP_HAVE_MODULE_KLT) || (!defined(VISP_HAVE_OPENCV) || (VISP_HAVE_OPENCV_VERSION < 0x020100))
    if (trackerType_image & 2) {
      std::cout << "KLT features cannot be used: ViSP is not built with "
                   "KLT module or OpenCV is not available.\nTest is not run."
                << std::endl;
      return EXIT_SUCCESS;
    }
#endif

    // Test if an input path is set
    if (opt_ipath.empty() && env_ipath.empty()) {
      usage(argv[0], NULL);
      std::cerr << std::endl << "ERROR:" << std::endl;
      std::cerr << "  Use -i <visp image path> option or set VISP_INPUT_IMAGE_PATH " << std::endl
                << "  environment variable to specify the location of the " << std::endl
                << "  image path where test images are located." << std::endl
                << std::endl;

      return EXIT_FAILURE;
    }

    std::string input_directory = vpIoTools::createFilePath(!opt_ipath.empty() ? opt_ipath : env_ipath, "mbt-depth/Castle-simu");
    if (!vpIoTools::checkDirectory(input_directory)) {
      std::cerr << "ViSP-images does not contain the folder: " << input_directory << "!" << std::endl;
      return EXIT_SUCCESS;
    }

    // Initialise a  display
#if defined VISP_HAVE_X11
    vpDisplayX display1, display2;
#elif defined VISP_HAVE_GDI
    vpDisplayGDI display1, display2;
#elif defined VISP_HAVE_OPENCV
    vpDisplayOpenCV display1, display2;
#elif defined VISP_HAVE_D3D9
    vpDisplayD3D display1, display2;
#elif defined VISP_HAVE_GTK
    vpDisplayGTK display1, display2;
#else
    opt_display = false;
#endif

    std::vector<int> tracker_type(2);
    tracker_type[0] = trackerType_image;
    tracker_type[1] = vpMbGenericTracker::DEPTH_DENSE_TRACKER;
    vpMbGenericTracker tracker(tracker_type);
#if defined(VISP_HAVE_XML2)
    tracker.loadConfigFile(input_directory + "/Config/chateau.xml", input_directory + "/Config/chateau_depth.xml");
#else
    {
      vpCameraParameters cam_color, cam_depth;
      cam_color.initPersProjWithoutDistortion(700.0, 700.0, 320.0, 240.0);
      cam_depth.initPersProjWithoutDistortion(700.0, 700.0, 320.0, 240.0);
      tracker.setCameraParameters(cam_color, cam_depth);
    }

    // Edge
    vpMe me;
    me.setMaskSize(5);
    me.setMaskNumber(180);
    me.setRange(8);
    me.setThreshold(10000);
    me.setMu1(0.5);
    me.setMu2(0.5);
    me.setSampleStep(5);
    tracker.setMovingEdge(me);

    // Klt
#if defined(VISP_HAVE_MODULE_KLT) && (defined(VISP_HAVE_OPENCV) && (VISP_HAVE_OPENCV_VERSION >= 0x020100))
    vpKltOpencv klt;
    tracker.setKltMaskBorder(5);
    klt.setMaxFeatures(10000);
    klt.setWindowSize(5);
    klt.setQuality(0.01);
    klt.setMinDistance(5);
    klt.setHarrisFreeParameter(0.02);
    klt.setBlockSize(3);
    klt.setPyramidLevels(3);

    tracker.setKltOpencv(klt);
#endif

    // Depth
    tracker.setDepthNormalFeatureEstimationMethod(vpMbtFaceDepthNormal::ROBUST_FEATURE_ESTIMATION);
    tracker.setDepthNormalPclPlaneEstimationMethod(2);
    tracker.setDepthNormalPclPlaneEstimationRansacMaxIter(200);
    tracker.setDepthNormalPclPlaneEstimationRansacThreshold(0.001);
    tracker.setDepthNormalSamplingStep(2, 2);

    tracker.setDepthDenseSamplingStep(4, 4);

    tracker.setAngleAppear(vpMath::rad(85.0));
    tracker.setAngleDisappear(vpMath::rad(89.0));
    tracker.setNearClippingDistance(0.01);
    tracker.setFarClippingDistance(2.0);
    tracker.setClipping(tracker.getClipping() | vpMbtPolygon::FOV_CLIPPING);
#endif

#ifdef VISP_HAVE_COIN3D
    tracker.loadModel(input_directory + "/Models/chateau.wrl", input_directory + "/Models/chateau.cao");
#else
    tracker.loadModel(input_directory + "/Models/chateau.cao", input_directory + "/Models/chateau.cao");
#endif
    vpHomogeneousMatrix T;
    T[0][0] = -1;
    T[0][3] = -0.2;
    T[1][1] = 0;
    T[1][2] = 1;
    T[1][3] = 0.12;
    T[2][1] = 1;
    T[2][2] = 0;
    T[2][3] = -0.15;
    tracker.loadModel(input_directory + "/Models/cube.cao", false, T);
    vpCameraParameters cam_color, cam_depth;
    tracker.getCameraParameters(cam_color, cam_depth);
    tracker.setDisplayFeatures(true);
    tracker.setScanLineVisibilityTest(useScanline);

    std::map<int, std::pair<double, double> > map_thresh;
    //Take the highest thresholds between all CI machines
#ifdef VISP_HAVE_COIN3D
    map_thresh[vpMbGenericTracker::EDGE_TRACKER]
        = useScanline ? std::pair<double, double>(0.005, 3.9) : std::pair<double, double>(0.007, 2.9);
#if defined(VISP_HAVE_MODULE_KLT) && (defined(VISP_HAVE_OPENCV) && (VISP_HAVE_OPENCV_VERSION >= 0x020100))
    map_thresh[vpMbGenericTracker::KLT_TRACKER]
        = useScanline ? std::pair<double, double>(0.006, 1.9) : std::pair<double, double>(0.005, 1.3);
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::KLT_TRACKER]
        = useScanline ? std::pair<double, double>(0.004, 3.2) : std::pair<double, double>(0.006, 2.8);
#endif
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = useScanline ? std::pair<double, double>(0.003, 1.7) : std::pair<double, double>(0.002, 0.8);
#if defined(VISP_HAVE_MODULE_KLT) && (defined(VISP_HAVE_OPENCV) && (VISP_HAVE_OPENCV_VERSION >= 0x020100))
    map_thresh[vpMbGenericTracker::KLT_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = std::pair<double, double>(0.002, 0.3);
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::KLT_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = useScanline ? std::pair<double, double>(0.002, 1.8) : std::pair<double, double>(0.002, 0.7);
#endif
#else
    map_thresh[vpMbGenericTracker::EDGE_TRACKER]
        = useScanline ? std::pair<double, double>(0.007, 2.3) : std::pair<double, double>(0.007, 2.1);
#if defined(VISP_HAVE_MODULE_KLT) && (defined(VISP_HAVE_OPENCV) && (VISP_HAVE_OPENCV_VERSION >= 0x020100))
    map_thresh[vpMbGenericTracker::KLT_TRACKER]
        = useScanline ? std::pair<double, double>(0.006, 1.7) : std::pair<double, double>(0.005, 1.4);
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::KLT_TRACKER]
        = useScanline ? std::pair<double, double>(0.004, 1.2) : std::pair<double, double>(0.004, 1.0);
#endif
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = useScanline ? std::pair<double, double>(0.002, 0.7) : std::pair<double, double>(0.001, 0.4);
#if defined(VISP_HAVE_MODULE_KLT) && (defined(VISP_HAVE_OPENCV) && (VISP_HAVE_OPENCV_VERSION >= 0x020100))
    map_thresh[vpMbGenericTracker::KLT_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = std::pair<double, double>(0.002, 0.3);
    map_thresh[vpMbGenericTracker::EDGE_TRACKER | vpMbGenericTracker::KLT_TRACKER | vpMbGenericTracker::DEPTH_DENSE_TRACKER]
        = useScanline ? std::pair<double, double>(0.001, 0.5) : std::pair<double, double>(0.001, 0.4);
#endif
#endif

    vpImage<unsigned char> I, I_depth;
    vpImage<uint16_t> I_depth_raw;
    vpHomogeneousMatrix cMo_truth;
    std::vector<vpColVector> pointcloud;
    int cpt_frame = 1;
    if (!read_data(input_directory, cpt_frame, cam_depth, I, I_depth_raw, pointcloud, cMo_truth)) {
      std::cerr << "Cannot read first frame!" << std::endl;
      return EXIT_FAILURE;
    }

    vpImageConvert::createDepthHistogram(I_depth_raw, I_depth);
    if (opt_display) {
#ifdef VISP_HAVE_DISPLAY
      display1.init(I, 0, 0, "Image");
      display2.init(I_depth, I.getWidth(), 0, "Depth");
#endif
    }

    vpHomogeneousMatrix depth_M_color;
    depth_M_color[0][3] = -0.05;
    tracker.setCameraTransformationMatrix("Camera2", depth_M_color);
    tracker.initFromPose(I, cMo_truth);

    bool click = false, quit = false;
    std::vector<double> vec_err_t, vec_err_tu;
    std::vector<double> time_vec;
    while (read_data(input_directory, cpt_frame, cam_depth, I, I_depth_raw, pointcloud, cMo_truth) && !quit
           && (opt_lastFrame > 0 ? (int)cpt_frame <= opt_lastFrame : true)) {
      vpImageConvert::createDepthHistogram(I_depth_raw, I_depth);

      if (opt_display) {
        vpDisplay::display(I);
        vpDisplay::display(I_depth);
      }

      double t = vpTime::measureTimeMs();
      std::map<std::string, const vpImage<unsigned char> *> mapOfImages;
      mapOfImages["Camera1"] = &I;
      std::map<std::string, const std::vector<vpColVector> *> mapOfPointclouds;
      mapOfPointclouds["Camera2"] = &pointcloud;
      std::map<std::string, unsigned int> mapOfWidths, mapOfHeights;
      if (!use_depth) {
        mapOfWidths["Camera2"] = 0;
        mapOfHeights["Camera2"] = 0;
      } else {
        mapOfWidths["Camera2"] = I_depth.getWidth();
        mapOfHeights["Camera2"] = I_depth.getHeight();
      }

      tracker.track(mapOfImages, mapOfPointclouds, mapOfWidths, mapOfHeights);
      vpHomogeneousMatrix cMo = tracker.getPose();
      t = vpTime::measureTimeMs() - t;
      time_vec.push_back(t);

      if (opt_display) {
        tracker.display(I, I_depth, cMo, depth_M_color*cMo, cam_color, cam_depth, vpColor::red, 3);
        vpDisplay::displayFrame(I, cMo, cam_depth, 0.05, vpColor::none, 3);
        vpDisplay::displayFrame(I_depth, depth_M_color*cMo, cam_depth, 0.05, vpColor::none, 3);

        std::stringstream ss;
        ss << "Frame: " << cpt_frame;
        vpDisplay::displayText(I_depth, 20, 20, ss.str(), vpColor::red);
        ss.str("");
        ss << "Nb features: " << tracker.getError().getRows();
        vpDisplay::displayText(I_depth, 40, 20, ss.str(), vpColor::red);
      }

      vpPoseVector pose_est(cMo);
      vpPoseVector pose_truth(cMo_truth);
      vpColVector t_est(3), t_truth(3);
      vpColVector tu_est(3), tu_truth(3);
      for (int i = 0; i < 3; i++) {
        t_est[i] = pose_est[i];
        t_truth[i] = pose_truth[i];
        tu_est[i] = pose_est[i+3];
        tu_truth[i] = pose_truth[i+3];
      }

      vpColVector t_err = t_truth-t_est, tu_err = tu_truth-tu_est;
      const double t_thresh = map_thresh[!use_depth ? trackerType_image : trackerType_image | vpMbGenericTracker::DEPTH_DENSE_TRACKER].first;
      const double tu_thresh = map_thresh[!use_depth ? trackerType_image : trackerType_image | vpMbGenericTracker::DEPTH_DENSE_TRACKER].second;
      double t_err2 = sqrt(t_err.sumSquare()), tu_err2 = vpMath::deg(sqrt(tu_err.sumSquare()));
      vec_err_t.push_back( t_err2 );
      vec_err_tu.push_back( tu_err2 );
      if ( t_err2 > t_thresh || tu_err2 > tu_thresh ) {
        std::cerr << "Pose estimated exceeds the threshold (t_thresh = " << t_thresh << " ; tu_thresh = " << tu_thresh << ")!" << std::endl;
        std::cout << "t_err: " << t_err2 << " ; tu_err: " << tu_err2 << std::endl;
        return EXIT_FAILURE;
      }

      if (opt_display) {
        vpDisplay::flush(I);
        vpDisplay::flush(I_depth);
      }

      if (opt_display && opt_click_allowed) {
        vpMouseButton::vpMouseButtonType button;
        if (vpDisplay::getClick(I, button, click)) {
          switch (button) {
            case vpMouseButton::button1:
              quit = !click;
              break;

            case vpMouseButton::button3:
              click = !click;
              break;

            default:
              break;
          }
        }
      }

      cpt_frame++;
    }

    if (!time_vec.empty())
      std::cout << "Computation time, Mean: " << vpMath::getMean(time_vec) << " ms ; Median: " << vpMath::getMedian(time_vec)
                << " ms ; Std: " << vpMath::getStdev(time_vec) << " ms" << std::endl;

    if (!vec_err_t.empty())
      std::cout << "Max translation error: " << *std::max_element(vec_err_t.begin(), vec_err_t.end()) << std::endl;

    if (!vec_err_tu.empty())
      std::cout << "Max thetau error: " << *std::max_element(vec_err_tu.begin(), vec_err_tu.end()) << std::endl;

#if defined(VISP_HAVE_XML2)
    // Cleanup memory allocated by xml library used to parse the xml config
    // file in vpMbGenericTracker::loadConfigFile()
    vpXmlParser::cleanup();
#endif

#if defined(VISP_HAVE_COIN3D) && (COIN_MAJOR_VERSION == 2 || COIN_MAJOR_VERSION == 3)
    // Cleanup memory allocated by Coin library used to load a vrml model. We clean only if Coin was used.
    SoDB::finish();
#endif

    return EXIT_SUCCESS;
  } catch (const vpException &e) {
    std::cout << "Catch an exception: " << e << std::endl;
    return EXIT_FAILURE;
  }
}
#else
int main() {
  std::cout << "Enable MBT module (VISP_HAVE_MODULE_MBT) to launch this test." << std::endl;
  return 0;
}
#endif
