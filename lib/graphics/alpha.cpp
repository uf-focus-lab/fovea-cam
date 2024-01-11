#include "alpha.h"
#include <opencv2/core/types.hpp>

namespace graphics {

void alpha_blend(cv::Mat &bg, cv::Mat &fg) { alpha_blend(bg, fg, bg); }

void alpha_blend(cv::Mat &bg, cv::Mat &fg, cv::Mat &dst) {
  // Assuming BGRA
  std::vector<cv::Mat> fg_channels, bg_channels;
  cv::split(fg, fg_channels);
  cv::split(bg, bg_channels);
  // Convert U8 mask to F32 (0 - 1)
  cv::Mat alpha;
  fg_channels.back().convertTo(alpha, CV_32FC1, 1.0 / 255.0);
  fg_channels.pop_back();
  // Blend each channel
  for (unsigned long i = 0; i < fg_channels.size(); i++) {
    // convert fg and bg to F32 (0 - 1)
    cv::Mat fg_f32, bg_f32, result;
    fg_channels[i].convertTo(fg_f32, CV_32FC1, 1.0 / 255.0);
    bg_channels[i].convertTo(bg_f32, CV_32FC1, 1.0 / 255.0);
    cv::multiply(fg_f32, alpha, fg_f32);
    cv::multiply(bg_f32, cv::Scalar(1.0) - alpha, bg_f32);
    cv::add(fg_f32, bg_f32, result);
    result.convertTo(bg_channels[i], CV_8UC1, 255.0);
  }
  // Merge channels
  cv::merge(bg_channels, dst);
}

void alpha_blend(cv::Mat &bg, cv::Scalar fg, cv::Mat &mask) {
  alpha_blend(bg, fg, mask, bg);
}

void alpha_blend(cv::Mat &bg, cv::Scalar fg, cv::Mat &mask, cv::Mat &dst) {
  // Assuming BGRA
  std::vector<cv::Mat> channels;
  cv::split(bg, channels);
  // Convert U8 mask to F32 (0 - 1)
  cv::Mat alpha;
  mask.convertTo(alpha, CV_32FC1, 1.0 / 255.0);
  // Blend each channel
  for (unsigned i = 0; i < channels.size(); i++) {
    // convert fg and bg to F32 (0 - 1)
    cv::Mat fg_tmp(bg.size(), CV_32FC1, {fg[i] / 255.0}), bg_tmp, result;
    channels[i].convertTo(bg_tmp, CV_32FC1, 1.0 / 255.0);
    cv::multiply(fg_tmp, alpha, fg_tmp);
    cv::multiply(bg_tmp, cv::Scalar(1.0) - alpha, bg_tmp);
    cv::add(bg_tmp, fg_tmp, result);
    result.convertTo(channels[i], CV_8UC1, 255.0);
  }
  // Merge channels
  cv::merge(channels, bg);
}

} // namespace graphics
