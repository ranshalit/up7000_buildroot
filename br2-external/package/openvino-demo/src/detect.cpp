/**
 * openvino-detect  –  Person-detection demo using OpenVINO 2024 C++ API
 *
 * Model: person-detection-retail-0013 (Intel Open Model Zoo, Apache-2.0)
 *   Input  [1, 3, 320, 544]  BGR float32, range [0, 255]
 *   Output [1, 1, 200, 7]    each detection row:
 *                            [img_id, label, conf, x1, y1, x2, y2]
 *                            (x/y coordinates normalized to [0, 1])
 *
 * Usage:
 *   openvino-detect [--device CPU|GPU] [image1.jpg image2.jpg ...]
 *
 * If no images are given, /usr/share/openvino-demo/images/ is scanned.
 * Annotated output (bounding boxes + confidence) is saved to /tmp/ov-demo-out/.
 */

#include <openvino/openvino.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const float CONF_THRESHOLD = 0.5f;
static const int   INPUT_W        = 544;
static const int   INPUT_H        = 320;

static const std::string MODEL_XML  = "/usr/share/openvino-demo/models/person-detection-retail-0013.xml";
static const std::string IMAGES_DIR = "/usr/share/openvino-demo/images";
static const std::string OUTPUT_DIR = "/tmp/ov-demo-out";

struct Detection {
    float    confidence;
    cv::Rect box;
};

static void run_image(ov::CompiledModel& compiled,
                      const std::string&  image_path,
                      const std::string&  out_dir)
{
    cv::Mat bgr = cv::imread(image_path);
    if (bgr.empty()) {
        std::cerr << "[WARN] Cannot read: " << image_path << "\n";
        return;
    }

    int orig_w = bgr.cols, orig_h = bgr.rows;

    // Resize to model input [H=320, W=544]; keep BGR channel order
    cv::Mat resized;
    cv::resize(bgr, resized, {INPUT_W, INPUT_H});

    // Build NCHW float32 tensor [1, 3, 320, 544]; values in [0, 255]
    ov::Tensor input_tensor(ov::element::f32, {1, 3,
                            static_cast<size_t>(INPUT_H),
                            static_cast<size_t>(INPUT_W)});
    float* dst = input_tensor.data<float>();
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < INPUT_H; ++h)
            for (int w = 0; w < INPUT_W; ++w)
                dst[c * INPUT_H * INPUT_W + h * INPUT_W + w] =
                    static_cast<float>(resized.at<cv::Vec3b>(h, w)[c]);

    auto infer_req = compiled.create_infer_request();
    infer_req.set_input_tensor(input_tensor);
    infer_req.infer();

    // Output: [1, 1, 200, 7] — row: [img_id, label, conf, x1, y1, x2, y2]
    const ov::Tensor& out     = infer_req.get_output_tensor();
    const float*      data    = out.data<const float>();
    int               num_det = static_cast<int>(out.get_shape()[2]);

    std::vector<Detection> dets;
    for (int i = 0; i < num_det; ++i) {
        const float* row  = data + i * 7;
        if (row[0] < 0) continue;          // image_id < 0 signals end-of-detections
        float        conf = row[2];
        if (conf < CONF_THRESHOLD) continue;

        int x1 = std::max(0, std::min(static_cast<int>(row[3] * orig_w), orig_w - 1));
        int y1 = std::max(0, std::min(static_cast<int>(row[4] * orig_h), orig_h - 1));
        int x2 = std::max(0, std::min(static_cast<int>(row[5] * orig_w), orig_w - 1));
        int y2 = std::max(0, std::min(static_cast<int>(row[6] * orig_h), orig_h - 1));
        if (x2 <= x1 || y2 <= y1) continue;
        dets.push_back({conf, cv::Rect(x1, y1, x2 - x1, y2 - y1)});
    }

    // Draw bounding boxes
    for (const auto& d : dets) {
        std::string label = "person " +
            std::to_string(static_cast<int>(d.confidence * 100)) + "%";
        cv::rectangle(bgr, d.box, {0, 255, 0}, 2);
        int      base;
        cv::Size ts  = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &base);
        cv::Point tl = d.box.tl() + cv::Point(0, -4);
        cv::rectangle(bgr,
            tl + cv::Point(0, base + 4),
            tl + cv::Point(ts.width, -ts.height),
            {0, 255, 0}, cv::FILLED);
        cv::putText(bgr, label, tl, cv::FONT_HERSHEY_SIMPLEX, 0.5, {0, 0, 0}, 1);
    }

    fs::path out_path = fs::path(out_dir) / fs::path(image_path).filename();
    cv::imwrite(out_path.string(), bgr);

    std::cout << image_path << " → " << out_path.string()
              << "  (" << dets.size() << " person"
              << (dets.size() == 1 ? "" : "s") << " detected)\n";
    for (const auto& d : dets) {
        std::cout << "  person conf=" << d.confidence
                  << " box=(" << d.box.x << "," << d.box.y
                  << " " << d.box.width << "x" << d.box.height << ")\n";
    }
}

int main(int argc, char* argv[])
{
    std::string              device = "CPU";
    std::vector<std::string> image_paths;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--device" && i + 1 < argc)
            device = argv[++i];
        else
            image_paths.push_back(arg);
    }

    if (image_paths.empty()) {
        for (const auto& e : fs::directory_iterator(IMAGES_DIR)) {
            auto ext = e.path().extension().string();
            if (ext == ".jpg" || ext == ".jpeg" || ext == ".png")
                image_paths.push_back(e.path().string());
        }
        if (image_paths.empty()) {
            std::cerr << "No images found in " << IMAGES_DIR << "\n";
            return 1;
        }
        std::sort(image_paths.begin(), image_paths.end());
    }

    fs::create_directories(OUTPUT_DIR);

    std::cout << "Model:  " << MODEL_XML << "\n";
    std::cout << "Device: " << device    << "\n";

    ov::Core          core;
    ov::CompiledModel compiled = core.compile_model(MODEL_XML, device);

    std::cout << "Model compiled on " << device << ". Running inference...\n\n";

    for (const auto& path : image_paths)
        run_image(compiled, path, OUTPUT_DIR);

    std::cout << "\nDone. Annotated images saved to: " << OUTPUT_DIR << "\n";
    return 0;
}

