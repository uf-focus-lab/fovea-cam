#include "threads.h"
#include "util/spinnaker.h"

void configure(Spinnaker::CameraPtr camera);

namespace thread {

void capture(Spinnaker::CameraPtr camera,
             Threading::FlushingPipe<cv::Mat> &pipe_out) {
  try {
    camera->Init();
    configure(camera);
    camera->BeginAcquisition();
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    pipe_out.close();
    return;
  }
  // Capture loop
  try {
    while (1) {
      pipe_out.write(Spinnaker::fromImagePtr(camera->GetNextImage()));
    }
  } catch (Threading::Closed &e) {
    // Normal termination
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Spinnaker Error: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown Error" << std::endl;
  }
  // Release camera instance
  try {
    camera->EndAcquisition();
    camera->DeInit();
    camera = nullptr;
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  std::cout << "[capture_thread] terminated." << std::endl;
}

} // namespace thread

void configure(Spinnaker::CameraPtr camera) {

  Spinnaker::GenApi::INodeMap &node_map = camera->GetNodeMap();

  // enable camera output trigger while exposing
  Spinnaker::GenApi::CEnumerationPtr ptrTrig = node_map.GetNode("TriggerMode");
  Spinnaker::GenApi::CEnumEntryPtr ptrValue = ptrTrig->GetEntryByName("Off");
  int64_t valueToSet = ptrValue->GetValue();
  ptrTrig->SetIntValue(valueToSet);

  Spinnaker::GenApi::CEnumerationPtr ptrLine = node_map.GetNode("LineSelector");
  ptrValue = ptrLine->GetEntryByName("LineSelector_Line3");
  valueToSet = ptrValue->GetValue();
  ptrLine->SetIntValue(valueToSet);

  Spinnaker::GenApi::CEnumerationPtr ptrLineM = node_map.GetNode("LineMode");
  ptrValue = ptrLineM->GetEntryByName("LineMode_Output");
  valueToSet = ptrValue->GetValue();
  ptrLineM->SetIntValue(valueToSet);

  Spinnaker::GenApi::CEnumerationPtr ptrSrc = node_map.GetNode("LineSource");
  ptrValue = ptrSrc->GetEntryByName("LineSource_ExposureActive");
  valueToSet = ptrValue->GetValue();
  ptrSrc->SetIntValue(valueToSet);

  Spinnaker::GenApi::CStringPtr ptrSerialNumberNode =
      camera->GetTLDeviceNodeMap().GetNode("DeviceSerialNumber");
  Spinnaker::GenICam::gcstring camera_serial_number =
      ptrSerialNumberNode->GetValue();

  // Convert the gcstring to a std::string
  std::string camera_serial_number_std = camera_serial_number.c_str();
  // Print the serial number
  std::cout << "Camera Serial Number: " << camera_serial_number_std
            << std::endl;

  // set camera to continuous acquisition instead of single frame
  Spinnaker::GenApi::CEnumerationPtr ptrAcquisitionMode =
      node_map.GetNode("AcquisitionMode");
  if (!IsAvailable(ptrAcquisitionMode) || !IsWritable(ptrAcquisitionMode)) {
    std::cout << "Unable to set acquisition mode to continuous (enum "
                 "retrieval). Aborting..."
              << std::endl;
  }
  Spinnaker::GenApi::CEnumEntryPtr ptrAcquisitionModeContinuous =
      ptrAcquisitionMode->GetEntryByName("Continuous");
  if (!IsAvailable(ptrAcquisitionModeContinuous) ||
      !IsReadable(ptrAcquisitionModeContinuous)) {
    std::cout << "Unable to set acquisition mode to continuous (entry "
                 "retrieval). Aborting..."
              << std::endl;
  }
  const int64_t acquisitionModeContinuous =
      ptrAcquisitionModeContinuous->GetValue();
  ptrAcquisitionMode->SetIntValue(acquisitionModeContinuous);

  // turn off auto exposure so we can set manual exposure time
  Spinnaker::GenApi::CEnumerationPtr ptrExposureAuto =
      node_map.GetNode("ExposureAuto");
  if (IsReadable(ptrExposureAuto) && IsWritable(ptrExposureAuto)) {
    Spinnaker::GenApi::CEnumEntryPtr ptrExposureAutoOff =
        ptrExposureAuto->GetEntryByName("Off");
    if (IsReadable(ptrExposureAutoOff)) {
      ptrExposureAuto->SetIntValue(ptrExposureAutoOff->GetValue());
      std::cout << "Automatic exposure disabled..." << std::endl;
    }
  }

  // set exposure time
  Spinnaker::GenApi::CFloatPtr ptrExposureTime =
      node_map.GetNode("ExposureTime");
  if (!IsAvailable(ptrExposureTime) || !IsWritable(ptrExposureTime)) {
    std::cout << "Unable to set exposure time. Aborting..." << std::endl
              << std::endl;
  }
  double exposure_us = 20000.0; // microseconds, move to config file
  ptrExposureTime->SetValue(exposure_us);

  // always return newest image from camera buffer to prevent lagginess
  Spinnaker::GenApi::INodeMap &sNodeMap = camera->GetTLStreamNodeMap();
  Spinnaker::GenApi::CEnumerationPtr ptrHandlingMode =
      sNodeMap.GetNode("StreamBufferHandlingMode");
  if (!IsAvailable(ptrHandlingMode) || !IsWritable(ptrHandlingMode)) {
    std::cout
        << "Unable to set Buffer Handling mode (node retrieval). Aborting..."
        << std::endl;
  }
  Spinnaker::GenApi::CEnumEntryPtr ptrHandlingModeEntry =
      ptrHandlingMode->GetCurrentEntry();
  if (!IsAvailable(ptrHandlingModeEntry) || !IsReadable(ptrHandlingModeEntry)) {
    std::cout
        << "Unable to set Buffer Handling mode (Entry retrieval). Aborting..."
        << std::endl;
  }
  ptrHandlingModeEntry = ptrHandlingMode->GetEntryByName("NewestOnly");
  ptrHandlingMode->SetIntValue(ptrHandlingModeEntry->GetValue());

  // set pixel format
  // demosaic on camera chip for speed
  Spinnaker::GenApi::CEnumerationPtr ptrPixelFormat =
      node_map.GetNode("PixelFormat");
  if (!IsAvailable(ptrPixelFormat) || !IsWritable(ptrPixelFormat)) {
    std::cout << "Unable to set Pixel Format mode (node retrieval). Aborting..."
              << std::endl
              << std::endl;
  }
  // setting the pixel format to BGR8 demosaics the raw bayer single channel
  // image into a 3 channel 8-bit BGR image
  Spinnaker::GenApi::CEnumEntryPtr ptrFormat =
      ptrPixelFormat->GetEntryByName("BGR8");
  if (IsAvailable(ptrFormat) && IsReadable(ptrFormat)) {
    ptrPixelFormat->SetIntValue(ptrFormat->GetValue());
  } else {
    std::cout
        << "Unable to set pixel format (enum entry retrieval). Aborting..."
        << std::endl
        << std::endl;
  }

  // set the ADC bit depth of the camera low on purpose to increase speed
  Spinnaker::GenApi::CEnumerationPtr ptrAdcBitDepth =
      node_map.GetNode("AdcBitDepth");
  if (!IsAvailable(ptrAdcBitDepth) || !IsWritable(ptrAdcBitDepth)) {
    std::cout << "ADC Bit Depth feature not available or not writable."
              << std::endl;
  }
  // Get the entry for the desired bit depth (options are 10 bit and 12 bit)
  Spinnaker::GenApi::CEnumEntryPtr ptrBitDepth =
      ptrAdcBitDepth->GetEntryByName("Bit10");
  if (!IsAvailable(ptrBitDepth) || !IsReadable(ptrBitDepth)) {
    std::cout << "Desired ADC Bit Depth not available or not readable."
              << std::endl;
  }
  int64_t bitDepthValue = ptrBitDepth->GetValue();
  ptrAdcBitDepth->SetIntValue(bitDepthValue);
}