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
  ${Spinnaker_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)
# Library linkables
link_directories(
  ${Spinnaker_LIB_DIRS}
  ${OpenCV_LIB_DIRS}
)
# Project Local Library
file(GLOB LIB_SRCS lib/*/*.cpp)
# Get all the cpp files in the src directory
file(GLOB sources src/*.cpp)
# Compile to binary
add_executable(${PROJECT_NAME} ${sources} ${LIB_SRCS})
# Link with libraries
target_link_libraries(${PROJECT_NAME}
  Threads::Threads
  ${OpenCV_LIBS}
  ${Spinnaker_LIBRARIES}
  # argparse
  ${libusb-1.0_LIBRARIES}
)
