################################################################################
#
# openvino-runtime
#
# Pre-built Intel OpenVINO 2024.4 runtime for x86_64 Linux (glibc).
# Installs:
#   - runtime libs  → $(TARGET_DIR)/usr/lib/openvino-2024.4/
#   - plugin XML    → $(TARGET_DIR)/usr/lib/openvino-2024.4/
#   - headers       → $(STAGING_DIR)/usr/include/openvino/
#   - CMake config  → $(STAGING_DIR)/usr/lib/cmake/openvino2024.4/
#   - pkg-config    → $(STAGING_DIR)/usr/lib/pkgconfig/
#   - /usr/lib versioned symlinks so ldconfig finds the libs
#
################################################################################

OPENVINO_RUNTIME_VERSION = 2024.4.0
OPENVINO_RUNTIME_BUILD = 16579.c3152d32c9c

OPENVINO_RUNTIME_SOURCE = \
	l_openvino_toolkit_ubuntu22_$(OPENVINO_RUNTIME_VERSION).$(OPENVINO_RUNTIME_BUILD)_x86_64.tgz
OPENVINO_RUNTIME_SITE = \
	https://storage.openvinotoolkit.org/repositories/openvino/packages/$(OPENVINO_RUNTIME_VERSION)/linux

OPENVINO_RUNTIME_LICENSE = Apache-2.0
OPENVINO_RUNTIME_LICENSE_FILES = licensing/apache-2.0.txt
OPENVINO_RUNTIME_REDISTRIBUTE = YES

OPENVINO_RUNTIME_INSTALL_STAGING = YES
OPENVINO_RUNTIME_INSTALL_TARGET  = YES

# The archive contains a single top-level directory; strip it when extracting.
OPENVINO_RUNTIME_STRIP_COMPONENTS = 1

OPENVINO_RUNTIME_DEPENDENCIES = tbb

define OPENVINO_RUNTIME_INSTALL_STAGING_CMDS
	# Install the runtime tree preserving the OpenVINO directory layout so that
	# OpenVINOTargets.cmake can resolve its IMPORTED_LOCATION paths correctly.
	#
	# The cmake files compute _IMPORT_PREFIX by going 3 path components up from
	# the cmake file: .../usr/openvino/runtime/cmake → .../usr/openvino
	# IMPORTED_LOCATION then resolves to:
	#   ${_IMPORT_PREFIX}/runtime/lib/intel64/libopenvino*.so.2024.4.0
	# which lands at: $(STAGING_DIR)/usr/openvino/runtime/lib/intel64/  ✓

	# CMake config (runtime/cmake/)
	$(INSTALL) -d $(STAGING_DIR)/usr/openvino/runtime
	cp -a $(@D)/runtime/cmake $(STAGING_DIR)/usr/openvino/runtime/

	# Shared libraries (runtime/lib/intel64/)
	cp -a $(@D)/runtime/lib $(STAGING_DIR)/usr/openvino/runtime/

	# Headers (runtime/include/)
	cp -a $(@D)/runtime/include $(STAGING_DIR)/usr/openvino/runtime/

	# TBB cmake (needed by OpenVINOConfig.cmake to locate TBB::tbb)
	if [ -d $(@D)/runtime/3rdparty/tbb/lib/cmake ]; then \
		$(INSTALL) -d $(STAGING_DIR)/usr/openvino/runtime/3rdparty/tbb/lib; \
		cp -a $(@D)/runtime/3rdparty/tbb/lib/cmake \
			$(STAGING_DIR)/usr/openvino/runtime/3rdparty/tbb/lib/; \
	fi
endef

define OPENVINO_RUNTIME_INSTALL_TARGET_CMDS
	# Runtime libs and inference-engine plugins into standard /usr/lib.
	# plugins.xml must reside alongside the plugin .so files; OpenVINO
	# searches for it relative to libopenvino.so at runtime.
	$(INSTALL) -d $(TARGET_DIR)/usr/lib
	cp -a $(@D)/runtime/lib/intel64/*.so* $(TARGET_DIR)/usr/lib/
	if [ -f $(@D)/runtime/lib/intel64/plugins.xml ]; then \
		$(INSTALL) -m 0644 $(@D)/runtime/lib/intel64/plugins.xml \
			$(TARGET_DIR)/usr/lib/; \
	fi

	# Cache dir expected by OpenVINO at runtime
	$(INSTALL) -d $(TARGET_DIR)/var/cache/openvino
endef

$(eval $(generic-package))
