find_package(OpenVINO REQUIRED COMPONENTS Runtime)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc)

add_executable(vino-detect
    src/main.cpp
)

target_link_libraries(vino-detect
    PRIVATE
        openvino::runtime
        ${OpenCV_LIBS}
)
