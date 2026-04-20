################################################################################
#
# openvino-demo
#
# Person-detection demo using OpenVINO 2024 C++ API.
# Model: person-detection-retail-0013 (Intel Open Model Zoo, Apache-2.0)
#
# The package:
#   - builds detect.cpp with CMake (cross-compiled)
#   - installs the binary to /usr/bin/openvino-detect
#   - downloads person-detection-retail-0013 IR model (.xml + .bin) and
#       installs to /usr/share/openvino-demo/models/
#   - installs 3 sample images to /usr/share/openvino-demo/images/
#
# Usage on target:
#   openvino-detect [--device CPU|GPU] [image.jpg ...]
# Output images (with bounding boxes) are saved to /tmp/ov-demo-out/.
#
################################################################################

OPENVINO_DEMO_VERSION = 1.0
OPENVINO_DEMO_SITE    = $(BR2_EXTERNAL_UP7000_EXTRAS_PATH)/package/openvino-demo/src
OPENVINO_DEMO_SITE_METHOD = local

OPENVINO_DEMO_LICENSE     = Apache-2.0
OPENVINO_DEMO_REDISTRIBUTE = YES

OPENVINO_DEMO_DEPENDENCIES = openvino-runtime opencv4

OMZ_BASE = https://storage.openvinotoolkit.org/repositories/open_model_zoo
OMZ_MODEL_PATH = 2023.0/models_bin/1/person-detection-retail-0013/FP32

# IR model files downloaded separately (not committed to git).
OPENVINO_DEMO_EXTRA_DOWNLOADS = \
	$(OMZ_BASE)/$(OMZ_MODEL_PATH)/person-detection-retail-0013.xml \
	$(OMZ_BASE)/$(OMZ_MODEL_PATH)/person-detection-retail-0013.bin

OPENVINO_DEMO_CONF_OPTS = \
	-DOpenVINO_DIR=$(STAGING_DIR)/usr/openvino/runtime/cmake \
	-DOpenCV_DIR=$(STAGING_DIR)/usr/lib/cmake/opencv4

# Install the IR model from the download directory into the target.
# (CMakeLists.txt handles the sample images via the local source tree.)
define OPENVINO_DEMO_INSTALL_MODEL
	$(INSTALL) -d $(TARGET_DIR)/usr/share/openvino-demo/models
	$(INSTALL) -m 0644 \
		$(OPENVINO_DEMO_DL_DIR)/person-detection-retail-0013.xml \
		$(OPENVINO_DEMO_DL_DIR)/person-detection-retail-0013.bin \
		$(TARGET_DIR)/usr/share/openvino-demo/models/
endef
OPENVINO_DEMO_POST_INSTALL_TARGET_HOOKS += OPENVINO_DEMO_INSTALL_MODEL

$(eval $(cmake-package))
