#pragma once
// Spinnaker defines "interface" as a macro, which conflicts with the
// interface keyword in C++.
// Any user code that uses interface as a variable or class name name will mess
// up.
// ==============================================================================
// located here: spinnaker/include/SpinGenApi/Types.h:24:19
#ifdef interface
#undef interface
#endif
#include <Spinnaker.h>
#ifdef interface
#undef interface
#endif
#include <opencv2/opencv.hpp>

#include <string>

namespace Spinnaker {

cv::Mat fromImagePtr(ImagePtr image_ptr, int flip = 0);

class ConfigurableMap {
  GenApi::INodeMap &map;

public:
  ConfigurableMap(GenApi::INodeMap &map);
  int set(const char *key, const char *value);
  int set(const char *key, const bool value);
  int set(const char *key, const int value);
  int set(const char *key, const double value);
};

} // namespace Spinnaker
