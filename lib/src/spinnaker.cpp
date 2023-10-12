#include "spinnaker.hpp"
#include <iostream>

#define MAP_PX_FMT(FMT_SP, FMT_CV, CONVERT_FROM)                               \
  case Spinnaker::PixelFormat_##FMT_SP:                                        \
    return { .type = CV_##FMT_CV, .cvt = cv::COLOR_##CONVERT_FROM##2RGBA }

typedef struct {
  int type, cvt;
} PixFormatLUT;

PixFormatLUT convertPixelFormat(Spinnaker::ImagePtr image_ptr) {
  const auto fmt = image_ptr->GetPixelFormat();
  switch (fmt) {
    MAP_PX_FMT(Mono8, 8UC1, GRAY);
    MAP_PX_FMT(Mono16, 16UC1, GRAY);
    MAP_PX_FMT(BayerGR8, 8UC1, BayerGR);
    MAP_PX_FMT(BayerRG8, 8UC1, BayerRG);
    MAP_PX_FMT(BayerGB8, 8UC1, BayerGB);
    MAP_PX_FMT(BayerBG8, 8UC1, BayerBG);
    MAP_PX_FMT(BGR8, 8UC3, BGR);
    MAP_PX_FMT(BGR16, 16UC3, BGR);
    MAP_PX_FMT(BGRa16, 8UC4, BGR);
    MAP_PX_FMT(RGB8, 8UC3, RGB);
    MAP_PX_FMT(BayerGR16, 16UC1, BayerGR);
    MAP_PX_FMT(BayerRG16, 16UC1, BayerRG);
    MAP_PX_FMT(BayerGB16, 16UC1, BayerGB);
    MAP_PX_FMT(BayerBG16, 16UC1, BayerBG);
  case Spinnaker::PixelFormat_BGRa8:
    // Native Display format
    return {.type = CV_8UC4, .cvt = -1};
  default:
    std::cerr << "[spinnaker::ERROR] Unsupported pixel format <" << fmt << ">"
              << std::endl
              << "    Bits per pixel: " << image_ptr->GetBitsPerPixel()
              << std::endl;
  }
  return {.type = -1, .cvt = -1};
}
namespace Spinnaker {

cv::Mat fromImagePtr(Spinnaker::ImagePtr image_ptr) {
  const unsigned int width = image_ptr->GetWidth(),
                     height = image_ptr->GetHeight();
  const auto fmt = convertPixelFormat(image_ptr);
  cv::Mat mat(height, width, fmt.type, image_ptr->GetData(),
              image_ptr->GetStride());
  if (fmt.cvt > 0) {
    cv::Mat tmp;
    cv::cvtColor(mat, tmp, fmt.cvt);
    mat = tmp;
  }
  if (mat.depth() != 8) {
    cv::Mat tmp;
    mat.convertTo(tmp, 8);
    mat = tmp;
  }
  return mat;
}

}; // namespace Spinnaker
