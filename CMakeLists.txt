# FoveaCam CMake Recipe
cmake_minimum_required(VERSION 3.10)
project(FoveaCam)
# CMake Parameters and Variables
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${PROJECT_ROOT}/cmake)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
# Dependency Resolution
find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)
find_package(Spinnaker REQUIRED)
find_package(CppLinuxSerial REQUIRED)
# Project Local Library
file(GLOB LIB_INCS lib/inc/*.hpp)
file(GLOB LIB_SRCS lib/src/*.cpp)
# header files and libraries
include_directories(
  ${PROJECT_ROOT}/lib/inc/
  ${Spinnaker_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)
# Library linkables
link_directories(
  ${Spinnaker_LIB_DIRS}
  ${OpenCV_LIB_DIRS}
)
# Get all the cpp files in the src directory
file(GLOB SOURCES src/*.cpp)
# For each source file, create an executable and link with the library
foreach(SRC ${SOURCES})
    # Get filename without extension as target name
    get_filename_component(TARGET ${SRC} NAME_WE)
    # Add target executable
    add_executable(${TARGET} ${SRC} ${LIB_INCS} ${LIB_SRCS})
    # Add compile options
    # target_compile_definitions(${TARGET}
    #   ${LibDRM_DEFINITIONS}
    # )
    # Link with libraries
    target_link_libraries(${TARGET}
      Threads::Threads
      ${OpenCV_LIBS}
      ${Spinnaker_LIBRARIES}
      CppLinuxSerial::CppLinuxSerial
    )
endforeach()
