#include <openvino/openvino.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static constexpr float CONF_THRESHOLD = 0.5f;
static const std::string DEFAULT_MODEL_XML = "/usr/share/openvino-demo/models/person-detection-retail-0013.xml";
static const std::string DEFAULT_IMAGES_DIR = "/usr/share/openvino-demo/images";
static const std::string DEFAULT_OUTPUT_DIR = "/tmp/vino-detect-out";

struct Config {
    std::string model = DEFAULT_MODEL_XML;
    std::string device = "CPU";
    std::string output_dir = DEFAULT_OUTPUT_DIR;
    std::vector<std::string> image_paths;
};

struct Detection {
    float confidence;
    cv::Rect box;
};

static void print_usage(const char* prog)
{
    std::cout
        << "Usage: " << prog << " [--model <path>] [--device CPU|GPU|AUTO] [--output-dir <dir>] [image...]\n"
        << "  --model <path>      Path to the OpenVINO IR model          [/usr/share/openvino-demo/models/person-detection-retail-0013.xml]\n"
        << "  --device <name>     Target device: CPU, GPU, AUTO, ...     [CPU]\n"
        << "  --output-dir <dir>  Directory for annotated output images   [/tmp/vino-detect-out]\n"
        << "  image...            Input images; if omitted, scan /usr/share/openvino-demo/images\n"
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
        } else if (arg == "--output-dir" && i + 1 < argc) {
            cfg.output_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            std::exit(1);
        } else {
            cfg.image_paths.push_back(arg);
        }
    }

    return cfg;
}

static std::vector<std::string> collect_images(const std::vector<std::string>& requested)
{
    std::vector<std::string> image_paths;

    if (!requested.empty()) {
        image_paths = requested;
    } else {
        for (const auto& entry : fs::directory_iterator(DEFAULT_IMAGES_DIR)) {
            const auto ext = entry.path().extension().string();
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
                image_paths.push_back(entry.path().string());
            }
        }
        std::sort(image_paths.begin(), image_paths.end());
    }

    return image_paths;
}

static std::vector<Detection> decode_detections(const ov::Tensor& output, int orig_w, int orig_h)
{
    const auto shape = output.get_shape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != 1 || shape[3] != 7) {
        throw std::runtime_error("Unsupported detection output shape");
    }

    const float* data = output.data<const float>();
    const int num_det = static_cast<int>(shape[2]);

    std::vector<Detection> detections;
    for (int i = 0; i < num_det; ++i) {
        const float* row = data + i * 7;
        if (row[0] < 0) {
            continue;
        }

        const float confidence = row[2];
        if (confidence < CONF_THRESHOLD) {
            continue;
        }

        const int x1 = std::clamp(static_cast<int>(row[3] * orig_w), 0, orig_w - 1);
        const int y1 = std::clamp(static_cast<int>(row[4] * orig_h), 0, orig_h - 1);
        const int x2 = std::clamp(static_cast<int>(row[5] * orig_w), 0, orig_w - 1);
        const int y2 = std::clamp(static_cast<int>(row[6] * orig_h), 0, orig_h - 1);
        if (x2 <= x1 || y2 <= y1) {
            continue;
        }

        detections.push_back({confidence, cv::Rect(x1, y1, x2 - x1, y2 - y1)});
    }

    return detections;
}

static ov::Tensor image_to_tensor(const cv::Mat& image, const ov::Output<const ov::Node>& input)
{
    const auto shape = input.get_shape();
    if (shape.size() != 4 || shape[0] != 1 || shape[1] != 3) {
        throw std::runtime_error("Expected model input shape [1,3,H,W]");
    }

    const int input_h = static_cast<int>(shape[2]);
    const int input_w = static_cast<int>(shape[3]);

    cv::Mat resized;
    cv::resize(image, resized, {input_w, input_h});

    ov::Tensor tensor(ov::element::f32, shape);
    float* dst = tensor.data<float>();
    for (int c = 0; c < 3; ++c) {
        for (int h = 0; h < input_h; ++h) {
            for (int w = 0; w < input_w; ++w) {
                dst[c * input_h * input_w + h * input_w + w] =
                    static_cast<float>(resized.at<cv::Vec3b>(h, w)[c]);
            }
        }
    }

    return tensor;
}

static void annotate_and_write(cv::Mat& image, const std::vector<Detection>& detections, const fs::path& output_path)
{
    for (const auto& detection : detections) {
        const std::string label = "person " + std::to_string(static_cast<int>(detection.confidence * 100)) + "%";
        cv::rectangle(image, detection.box, {0, 255, 0}, 2);

        int base = 0;
        const cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &base);
        const cv::Point text_origin = detection.box.tl() + cv::Point(0, -4);
        cv::rectangle(image,
                      text_origin + cv::Point(0, base + 4),
                      text_origin + cv::Point(text_size.width, -text_size.height),
                      {0, 255, 0},
                      cv::FILLED);
        cv::putText(image, label, text_origin, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);
    }

    if (!cv::imwrite(output_path.string(), image)) {
        throw std::runtime_error("Failed to write annotated image to " + output_path.string());
    }
}

static void run_image(ov::CompiledModel& compiled, const std::string& image_path, const std::string& output_dir)
{
    cv::Mat image = cv::imread(image_path);
    if (image.empty()) {
        std::cerr << "[WARN] Cannot read: " << image_path << "\n";
        return;
    }

    auto infer_req = compiled.create_infer_request();
    infer_req.set_input_tensor(image_to_tensor(image, compiled.input()));
    infer_req.infer();

    const auto detections = decode_detections(infer_req.get_output_tensor(), image.cols, image.rows);
    const fs::path out_path = fs::path(output_dir) / fs::path(image_path).filename();

    annotate_and_write(image, detections, out_path);

    std::cout << image_path << " -> " << out_path.string()
              << "  (" << detections.size() << " person"
              << (detections.size() == 1 ? "" : "s") << " detected)\n";
    for (const auto& detection : detections) {
        std::cout << "  person conf=" << detection.confidence
                  << " box=(" << detection.box.x << "," << detection.box.y
                  << " " << detection.box.width << "x" << detection.box.height << ")\n";
    }
}

int main(int argc, char* argv[])
{
    const Config cfg = parse_args(argc, argv);

    try {
        const auto images = collect_images(cfg.image_paths);
        if (images.empty()) {
            std::cerr << "No images found.\n";
            return 1;
        }

        fs::create_directories(cfg.output_dir);

        const auto version = ov::get_openvino_version();
        std::cout << "vino-detect\n"
                  << "  OpenVINO runtime: " << version.buildNumber << "\n"
                  << "  Model:            " << cfg.model << "\n"
                  << "  Requested device: " << cfg.device << "\n";

        ov::Core core;
        ov::CompiledModel compiled = core.compile_model(cfg.model, cfg.device);
        const auto execution_devices = compiled.get_property(ov::execution_devices);

        std::cout << "  Compiled device:  ";
        for (size_t i = 0; i < execution_devices.size(); ++i) {
            if (i != 0) {
                std::cout << ", ";
            }
            std::cout << execution_devices[i];
        }
        std::cout << "\n\n";

        for (const auto& image : images) {
            run_image(compiled, image, cfg.output_dir);
        }

        std::cout << "\nAnnotated images saved to: " << cfg.output_dir << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
