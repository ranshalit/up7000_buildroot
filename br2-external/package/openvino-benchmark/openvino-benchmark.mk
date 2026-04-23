################################################################################
#
# openvino-benchmark
#
# Inference benchmark tool for OpenVINO 2024 C++ API.
# Measures throughput (FPS) and latency (avg/min/max/p50/p90/p99) for any
# OpenVINO-compatible model (.xml IR or .onnx) on the target device.
#
# Usage on target:
#   benchmark_app -m <model.xml> [-d CPU|GPU|NPU|AUTO] \
#                 [-niter 1000] [-nwarmup 5] [-nireq 1] [-async]
#
################################################################################

OPENVINO_BENCHMARK_VERSION     = 1.0
OPENVINO_BENCHMARK_SITE        = $(BR2_EXTERNAL_UP7000_EXTRAS_PATH)/package/openvino-benchmark/src
OPENVINO_BENCHMARK_SITE_METHOD = local

OPENVINO_BENCHMARK_LICENSE     = Apache-2.0
OPENVINO_BENCHMARK_REDISTRIBUTE = YES

OPENVINO_BENCHMARK_DEPENDENCIES = openvino-runtime

OPENVINO_BENCHMARK_CONF_OPTS = \
	-DOpenVINO_DIR=$(STAGING_DIR)/usr/openvino/runtime/cmake

$(eval $(cmake-package))
