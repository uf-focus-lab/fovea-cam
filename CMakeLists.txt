# FoveaCam CMake Recipe
cmake_minimum_required(VERSION 3.10)
project(FoveaCam)
# CMake Parameters and Variables
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${PROJECT_ROOT}/scripts)
set(CMAKE_CXX_FLAGS "-Wall -Wno-ignored-qualifiers")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# Pkg Config
find_package(PkgConfig REQUIRED)
pkg_check_modules(libusb-1.0 REQUIRED libusb-1.0)
# Dependency Resolution
find_package(X11 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
find_package(Spinnaker REQUIRED)
# fetch latest argparse
# include(FetchContent)
# FetchContent_Declare(
#     argparse
#     GIT_REPOSITORY https://github.com/p-ranav/argparse.git
# )
# FetchContent_MakeAvailable(argparse)
# add_compile_definitions(ARGPARSE_BUILD_TESTS=off)
# add_compile_definitions(ARGPARSE_BUILD_SAMPLES=off)
# set_target_properties(
#   ARGPARSE_TESTS
#   PROPERTIES
#   EXCLUDE_FROM_ALL 1
# )
# header files and libraries
include_directories(
  ${PROJECT_ROOT}/include/
  ${PROJECT_ROOT}/lib/
  ${PROJECT_ROOT}/assets/
  ${Spinnaker_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)
# Library linkables
link_directories(
  ${Spinnaker_LIB_DIRS}
  ${OpenCV_LIB_DIRS}
)
# Project Local Library
file(GLOB LIB_SRCS ${PROJECT_ROOT}/lib/*/*.cpp)
# Get all the cpp files in the src directory
file(GLOB SOURCES ${PROJECT_ROOT}/src/*.cpp ${PROJECT_ROOT}/src/*/*.cpp)
# Get all the generates assets
file(GLOB ASSETS assets/*.o)
SET_SOURCE_FILES_PROPERTIES(
  ${ASSETS}
  PROPERTIES
  EXTERNAL_OBJECT true
  GENERATED true
)
# Compile to binary
add_executable(${PROJECT_NAME} ${ASSETS} ${LIB_SRCS} ${SOURCES})
# Link with libraries
target_link_libraries(${PROJECT_NAME}
  Threads::Threads
  ${libusb-1.0_LIBRARIES}
  # argparse
  ${OpenCV_LIBS}
  ${Spinnaker_LIBRARIES}
  ${X11_LIBRARIES}
)
