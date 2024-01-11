#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/opencv.hpp>

namespace graphics {

void alpha_blend(cv::Mat &bg, cv::Mat &fg);
void alpha_blend(cv::Mat &bg, cv::Mat &fg, cv::Mat &dst);

void alpha_blend(cv::Mat &bg, cv::Scalar fg, cv::Mat &mask);
void alpha_blend(cv::Mat &bg, cv::Scalar fg, cv::Mat &mask, cv::Mat &dst);

} // namespace graphics
