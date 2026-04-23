cmake_minimum_required(VERSION 3.16)
project(openvino_apps CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT DEFINED OpenVINO_DIR AND DEFINED CMAKE_SYSROOT)
    set(OpenVINO_DIR
        "${CMAKE_SYSROOT}/usr/openvino/runtime/cmake"
        CACHE PATH "Path to OpenVINOConfig.cmake")
endif()

if(NOT DEFINED OpenCV_DIR AND DEFINED CMAKE_SYSROOT)
    set(OpenCV_DIR
        "${CMAKE_SYSROOT}/usr/lib/cmake/opencv4"
        CACHE PATH "Path to OpenCVConfig.cmake")
endif()

add_subdirectory(hello)
add_subdirectory(detection)
