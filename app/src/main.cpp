#include <openvino/openvino.hpp>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Config {
    std::string model;
    std::string device = "CPU";
    bool list_devices = false;
};

static void print_usage(const char* prog)
{
    std::cout
        << "Usage: " << prog << " --model <path> [--device CPU|GPU|AUTO] [--list-devices]\n"
        << "  --model <path>      Path to an OpenVINO IR (.xml) or ONNX model\n"
        << "  --device <name>     Target device: CPU, GPU, AUTO, ...           [CPU]\n"
        << "  --list-devices      Print available OpenVINO devices and exit\n"
        << "  -h, --help          Show this help\n";
}

static Config parse_args(int argc, char* argv[])
{
    Config cfg;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if ((arg == "--model" || arg == "-m") && i + 1 < argc) {
            cfg.model = argv[++i];
        } else if ((arg == "--device" || arg == "-d") && i + 1 < argc) {
            cfg.device = argv[++i];
        } else if (arg == "--list-devices") {
            cfg.list_devices = true;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            std::exit(1);
        }
    }

    return cfg;
}

static std::string shape_to_string(const ov::Shape& shape)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < shape.size(); ++i) {
        if (i != 0) {
            oss << ", ";
        }
        oss << shape[i];
    }
    oss << "]";
    return oss.str();
}

static std::string port_name(const ov::Output<const ov::Node>& port)
{
    const auto names = port.get_names();
    if (!names.empty()) {
        return *names.begin();
    }
    return "<unnamed>";
}

static std::string join_strings(const std::vector<std::string>& values)
{
    std::ostringstream oss;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            oss << ", ";
        }
        oss << values[i];
    }
    return oss.str();
}

static void list_devices(ov::Core& core)
{
    const auto devices = core.get_available_devices();
    std::cout << "Available OpenVINO devices:\n";
    for (const auto& device : devices) {
        std::cout << "  - " << device;
        try {
            const auto full_name = core.get_property(device, ov::device::full_name);
            std::cout << " (" << full_name << ")";
        } catch (const std::exception&) {
        }
        std::cout << "\n";
    }
}

static void fill_inputs(ov::InferRequest& request, const ov::CompiledModel& compiled)
{
    for (const auto& input : compiled.inputs()) {
        ov::Tensor tensor(input.get_element_type(), input.get_shape());
        std::memset(tensor.data(), 0, tensor.get_byte_size());
        request.set_tensor(input, tensor);
    }
}

int main(int argc, char* argv[])
{
    const Config cfg = parse_args(argc, argv);

    try {
        ov::Core core;
        const auto version = ov::get_openvino_version();

        std::cout << "vino-hello\n"
                  << "  OpenVINO runtime: " << version.buildNumber << "\n";

        if (cfg.list_devices) {
            list_devices(core);
            if (cfg.model.empty()) {
                return 0;
            }
            std::cout << "\n";
        }

        if (cfg.model.empty()) {
            std::cerr << "Error: --model <path> is required unless --list-devices is used on its own.\n";
            print_usage(argv[0]);
            return 1;
        }

        std::cout << "  Model:            " << cfg.model << "\n"
                  << "  Requested device: " << cfg.device << "\n";

        auto model = core.read_model(cfg.model);
        auto compiled = core.compile_model(model, cfg.device);
        const auto execution_devices = compiled.get_property(ov::execution_devices);

        std::cout << "  Compiled device:  " << join_strings(execution_devices) << "\n";

        std::cout << "\nInputs:\n";
        for (const auto& input : compiled.inputs()) {
            std::cout << "  - " << port_name(input)
                      << " type=" << input.get_element_type()
                      << " shape=" << shape_to_string(input.get_shape())
                      << "\n";
        }

        std::cout << "Outputs:\n";
        for (const auto& output : compiled.outputs()) {
            std::cout << "  - " << port_name(output)
                      << " type=" << output.get_element_type()
                      << " shape=" << shape_to_string(output.get_shape())
                      << "\n";
        }

        ov::InferRequest request = compiled.create_infer_request();
        fill_inputs(request, compiled);

        const auto start = std::chrono::steady_clock::now();
        request.infer();
        const auto end = std::chrono::steady_clock::now();
        const auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "\nInference completed successfully in "
                  << std::fixed << std::setprecision(2) << duration_ms
                  << " ms\n";

        for (const auto& output : compiled.outputs()) {
            const auto tensor = request.get_tensor(output);
            std::cout << "  Output tensor " << port_name(output)
                      << ": bytes=" << tensor.get_byte_size()
                      << " shape=" << shape_to_string(tensor.get_shape())
                      << "\n";
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
