/**
 * benchmark_app  –  OpenVINO inference benchmark using the 2024 C++ API
 *
 * Usage:
 *   benchmark_app -m <model.xml|.onnx> [options]
 *
 * Options:
 *   -m  <path>    Path to model file (.xml IR or .onnx)          [required]
 *   -d  <device>  Inference device: CPU, GPU, NPU, AUTO          [CPU]
 *   -niter <n>    Number of benchmark iterations                 [1000]
 *   -nwarmup <n>  Warmup iterations before measurement           [5]
 *   -nireq <n>    Number of parallel asynchronous infer requests [1]
 *   -async        Use asynchronous execution pipeline
 *
 * Output:
 *   Throughput (FPS) and latency statistics (avg/min/max/p50/p90/p99).
 */

#include <openvino/openvino.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

struct Config {
    std::string model;
    std::string device   = "CPU";
    int         niter    = 1000;
    int         nwarmup  = 5;
    int         nireq    = 1;
    bool        async    = false;
};

static void print_usage(const char* prog)
{
    std::cout
        << "Usage: " << prog << " -m <model> [options]\n"
        << "  -m  <path>    Model path (.xml or .onnx)           [required]\n"
        << "  -d  <device>  Device: CPU, GPU, NPU, AUTO          [CPU]\n"
        << "  -niter <n>    Benchmark iterations                  [1000]\n"
        << "  -nwarmup <n>  Warmup iterations                     [5]\n"
        << "  -nireq <n>    Parallel async infer requests         [1]\n"
        << "  -async        Enable asynchronous execution\n"
        << "  -h            Show this help\n";
}

static Config parse_args(int argc, char* argv[])
{
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "-m" || a == "--model") && i + 1 < argc)
            cfg.model = argv[++i];
        else if ((a == "-d" || a == "--device") && i + 1 < argc)
            cfg.device = argv[++i];
        else if (a == "-niter" && i + 1 < argc)
            cfg.niter = std::stoi(argv[++i]);
        else if (a == "-nwarmup" && i + 1 < argc)
            cfg.nwarmup = std::stoi(argv[++i]);
        else if (a == "-nireq" && i + 1 < argc)
            cfg.nireq = std::stoi(argv[++i]);
        else if (a == "-async" || a == "--async")
            cfg.async = true;
        else if (a == "-h" || a == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            print_usage(argv[0]);
            std::exit(1);
        }
    }
    return cfg;
}

static double percentile(const std::vector<double>& sorted_v, double p)
{
    if (sorted_v.empty()) return 0.0;
    double idx = p / 100.0 * static_cast<double>(sorted_v.size() - 1);
    size_t lo  = static_cast<size_t>(idx);
    size_t hi  = std::min(lo + 1, sorted_v.size() - 1);
    double frac = idx - static_cast<double>(lo);
    return sorted_v[lo] * (1.0 - frac) + sorted_v[hi] * frac;
}

/* Fill every input tensor of a compiled model with zeros. */
static void fill_inputs(ov::InferRequest& req, const ov::CompiledModel& compiled)
{
    for (const auto& input : compiled.inputs()) {
        ov::Tensor t(input.get_element_type(), input.get_shape());
        std::memset(t.data(), 0, t.get_byte_size());
        req.set_tensor(input, t);
    }
}

int main(int argc, char* argv[])
{
    Config cfg = parse_args(argc, argv);

    if (cfg.model.empty()) {
        std::cerr << "Error: -m <model> is required\n";
        print_usage(argv[0]);
        return 1;
    }
    if (cfg.nireq < 1)  cfg.nireq  = 1;
    if (cfg.niter < 1)  cfg.niter  = 1;
    if (cfg.nwarmup < 0) cfg.nwarmup = 0;

    const std::string mode = (cfg.async && cfg.nireq > 1) ? "async" : "sync";

    auto ver = ov::get_openvino_version();
    std::cout << "OpenVINO Benchmark App  (runtime: " << ver.buildNumber << ")\n"
              << "  Model:   " << cfg.model   << "\n"
              << "  Device:  " << cfg.device  << "\n"
              << "  Mode:    " << mode        << "  nireq=" << cfg.nireq << "\n"
              << "  Warmup:  " << cfg.nwarmup << "  Iterations: " << cfg.niter << "\n\n";

    try {
        ov::Core core;

        auto t_load0 = std::chrono::steady_clock::now();
        auto model   = core.read_model(cfg.model);
        auto compiled = core.compile_model(model, cfg.device);
        auto t_load1 = std::chrono::steady_clock::now();
        double load_ms = std::chrono::duration<double, std::milli>(t_load1 - t_load0).count();

        std::cout << "Model loaded and compiled in "
                  << std::fixed << std::setprecision(1) << load_ms << " ms\n\n";

        /* Create inference requests and pre-fill inputs. */
        std::vector<ov::InferRequest> reqs;
        reqs.reserve(cfg.nireq);
        for (int i = 0; i < cfg.nireq; ++i) {
            reqs.push_back(compiled.create_infer_request());
            fill_inputs(reqs.back(), compiled);
        }

        /* Warmup. */
        if (cfg.nwarmup > 0) {
            std::cout << "Warming up (" << cfg.nwarmup << " iterations)...\n";
            for (int i = 0; i < cfg.nwarmup; ++i)
                reqs[0].infer();
        }

        /* Benchmark. */
        std::cout << "Benchmarking (" << cfg.niter << " iterations)...\n";
        std::vector<double> latencies;
        latencies.reserve(cfg.niter);

        auto bench_start = std::chrono::steady_clock::now();

        if (mode == "sync") {
            for (int i = 0; i < cfg.niter; ++i) {
                auto s = std::chrono::steady_clock::now();
                reqs[0].infer();
                auto e = std::chrono::steady_clock::now();
                latencies.push_back(
                    std::chrono::duration<double, std::milli>(e - s).count());
            }
        } else {
            /* Async pipeline: keep nireq requests in-flight at all times. */
            std::vector<std::chrono::steady_clock::time_point> start_times(cfg.nireq);
            int launched = 0, done = 0;

            /* Prime the pipeline. */
            for (int r = 0; r < cfg.nireq && launched < cfg.niter; ++r, ++launched) {
                start_times[r] = std::chrono::steady_clock::now();
                reqs[r].start_async();
            }

            while (done < cfg.niter) {
                int r = done % cfg.nireq;
                reqs[r].wait();
                auto e = std::chrono::steady_clock::now();
                latencies.push_back(
                    std::chrono::duration<double, std::milli>(e - start_times[r]).count());
                ++done;
                if (launched < cfg.niter) {
                    start_times[r] = std::chrono::steady_clock::now();
                    reqs[r].start_async();
                    ++launched;
                }
            }
        }

        auto bench_end = std::chrono::steady_clock::now();
        double total_ms = std::chrono::duration<double, std::milli>(
            bench_end - bench_start).count();

        /* Statistics. */
        std::sort(latencies.begin(), latencies.end());
        double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
        double avg = sum / static_cast<double>(latencies.size());
        double throughput = static_cast<double>(cfg.niter) / (total_ms / 1000.0);

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "\n=== Results ===\n"
                  << "  Throughput:   " << throughput          << " FPS\n"
                  << "  Latency avg:  " << avg                 << " ms\n"
                  << "  Latency min:  " << latencies.front()   << " ms\n"
                  << "  Latency max:  " << latencies.back()    << " ms\n"
                  << "  Latency p50:  " << percentile(latencies, 50.0) << " ms\n"
                  << "  Latency p90:  " << percentile(latencies, 90.0) << " ms\n"
                  << "  Latency p99:  " << percentile(latencies, 99.0) << " ms\n"
                  << "  Total time:   " << total_ms            << " ms\n";

    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
