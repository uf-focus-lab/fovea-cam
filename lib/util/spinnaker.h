#include <Spinnaker.h>
#include <opencv2/opencv.hpp>
namespace Spinnaker {

cv::Mat fromImagePtr(ImagePtr image_ptr);

void config(GenApi::INodeMap &node_map, const char *key, const char *value);
void config(GenApi::INodeMap &node_map, const char *key, const int value);
void config(GenApi::INodeMap &node_map, const char *key, const double value);

} // namespace Spinnaker
