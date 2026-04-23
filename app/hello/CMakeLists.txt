find_package(OpenVINO REQUIRED COMPONENTS Runtime)

add_executable(vino-hello
    src/main.cpp
)

target_link_libraries(vino-hello PRIVATE openvino::runtime)
