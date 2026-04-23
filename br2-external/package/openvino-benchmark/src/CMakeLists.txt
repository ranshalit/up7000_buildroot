cmake_minimum_required(VERSION 3.16)
project(openvino_benchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenVINO REQUIRED COMPONENTS Runtime)

add_executable(benchmark_app benchmark.cpp)
target_link_libraries(benchmark_app PRIVATE openvino::runtime)

install(TARGETS benchmark_app RUNTIME DESTINATION bin)
