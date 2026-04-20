cmake_minimum_required(VERSION 3.16)
project(openvino_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenVINO REQUIRED COMPONENTS Runtime)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

add_executable(openvino-detect detect.cpp)
target_link_libraries(openvino-detect
    PRIVATE
        openvino::runtime
        ${OpenCV_LIBS}
)

# ── install binary ────────────────────────────────────────────────────────────
install(TARGETS openvino-detect RUNTIME DESTINATION bin)

# ── install sample images (committed in local source tree) ────────────────────
# The person-detection-retail-0013 IR model (.xml + .bin) is installed
# separately by the Buildroot post-hook (OPENVINO_DEMO_INSTALL_MODEL).
file(GLOB DEMO_IMAGES images/*.jpg images/*.jpeg images/*.png)
install(FILES ${DEMO_IMAGES}
    DESTINATION share/openvino-demo/images)

