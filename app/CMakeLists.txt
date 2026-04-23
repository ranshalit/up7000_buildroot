cmake_minimum_required(VERSION 3.16)
project(vino_hello CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED OpenVINO_DIR AND DEFINED CMAKE_SYSROOT)
    set(OpenVINO_DIR
        "${CMAKE_SYSROOT}/usr/openvino/runtime/cmake"
        CACHE PATH "Path to OpenVINOConfig.cmake")
endif()

find_package(OpenVINO REQUIRED COMPONENTS Runtime)

add_executable(vino-hello
    src/main.cpp
)

target_link_libraries(vino-hello PRIVATE openvino::runtime)
