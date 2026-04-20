################################################################################
#
# intel-compute-runtime
#
# Pre-built Intel Compute Runtime (NEO) for x86_64 Linux.
#
# Provides the OpenCL ICD and Level Zero GPU driver for Intel iGPU
# (required by the OpenVINO GPU inference plugin).
#
# The package downloads several .deb archives from Intel's GitHub releases,
# extracts the payload, and installs the relevant files.
#
# Installed components:
#   - libigdrcl.so         (OpenCL ICD for Intel GPU)
#   - libze_intel_gpu.so   (Level Zero GPU driver)
#   - libigc.so            (Intel Graphics Compiler runtime)
#   - libigdfcl.so         (Intel Graphics Compiler FCL)
#   - /etc/OpenCL/vendors/intel.icd  (OpenCL ICD manifest)
#
################################################################################

INTEL_COMPUTE_RUNTIME_VERSION = 24.26.30049.6
IGC_VERSION                   = 1.0.17193.4
INTEL_GMMLIB_PREBUILT_VERSION = 22.3.19

# Use the OpenCL ICD .deb as the primary source (sets the download dir).
# The remaining .deb files are fetched as extra downloads.
INTEL_COMPUTE_RUNTIME_SOURCE = \
	intel-opencl-icd_$(INTEL_COMPUTE_RUNTIME_VERSION)_amd64.deb
INTEL_COMPUTE_RUNTIME_SITE = \
	https://github.com/intel/compute-runtime/releases/download/$(INTEL_COMPUTE_RUNTIME_VERSION)

INTEL_COMPUTE_RUNTIME_EXTRA_DOWNLOADS = \
	https://github.com/intel/compute-runtime/releases/download/$(INTEL_COMPUTE_RUNTIME_VERSION)/intel-level-zero-gpu_1.3.30049.6_amd64.deb \
	https://github.com/intel/intel-graphics-compiler/releases/download/igc-$(IGC_VERSION)/intel-igc-core_$(IGC_VERSION)_amd64.deb \
	https://github.com/intel/intel-graphics-compiler/releases/download/igc-$(IGC_VERSION)/intel-igc-opencl_$(IGC_VERSION)_amd64.deb \
	http://archive.ubuntu.com/ubuntu/pool/main/o/ocl-icd/ocl-icd-libopencl1_2.2.14-3_amd64.deb

INTEL_COMPUTE_RUNTIME_LICENSE    = MIT
INTEL_COMPUTE_RUNTIME_REDISTRIBUTE = YES

INTEL_COMPUTE_RUNTIME_INSTALL_STAGING = NO
INTEL_COMPUTE_RUNTIME_INSTALL_TARGET  = YES

INTEL_COMPUTE_RUNTIME_DEPENDENCIES = intel-gmmlib

# ── Custom extraction: unpack each .deb (ar archive containing data.tar.*) ───
define INTEL_COMPUTE_RUNTIME_EXTRACT_CMDS
	mkdir -p $(@D)/extracted
	for deb in \
		$(INTEL_COMPUTE_RUNTIME_SOURCE) \
		intel-level-zero-gpu_1.3.30049.6_amd64.deb \
		intel-igc-core_$(IGC_VERSION)_amd64.deb \
		intel-igc-opencl_$(IGC_VERSION)_amd64.deb; \
	do \
		debpath=$(INTEL_COMPUTE_RUNTIME_DL_DIR)/$$deb; \
		tmpdir=$(@D)/extracted/$$deb.d; \
		mkdir -p $$tmpdir; \
		(cd $$tmpdir && ar x $$debpath); \
		for data in $$tmpdir/data.tar.*; do \
			tar -C $(@D)/extracted -xf $$data; \
		done; \
	done
endef

define INTEL_COMPUTE_RUNTIME_INSTALL_TARGET_CMDS
	# OpenCL ICD and Level Zero GPU driver shared libraries → /usr/lib/
	# (Buildroot does not support /usr/lib/x86_64-linux-gnu or ld.so.conf.d)
	$(INSTALL) -d $(TARGET_DIR)/usr/lib
	for lib in $$(find $(@D)/extracted/usr/lib/x86_64-linux-gnu \
	                   $(@D)/extracted/usr/lib/intel-opencl \
	                   -name '*.so*' 2>/dev/null); do \
		$(INSTALL) -m 0755 $$lib $(TARGET_DIR)/usr/lib/; \
	done

	# Intel Graphics Compiler (IGC) runtime libs (installed to /usr/local/lib by the debs)
	find $(@D)/extracted/usr/local/lib -maxdepth 1 \
		\( -name 'libigc*.so*' -o -name 'libigdfcl*.so*' \
		-o -name 'libiga64*.so*' -o -name 'libopencl-clang*.so*' \) \
		-exec $(INSTALL) -m 0755 {} $(TARGET_DIR)/usr/lib/ \;

	# Khronos OpenCL ICD loader (libOpenCL.so.1) — vendor-neutral dispatcher
	find $(@D)/extracted/usr/lib/x86_64-linux-gnu \
		-name 'libOpenCL.so*' \
		-exec $(INSTALL) -m 0755 {} $(TARGET_DIR)/usr/lib/ \;

	# OpenCL ICD manifest
	$(INSTALL) -d $(TARGET_DIR)/etc/OpenCL/vendors
	echo "/usr/lib/libigdrcl.so" \
		> $(TARGET_DIR)/etc/OpenCL/vendors/intel.icd

	# Level Zero GPU driver manifest
	$(INSTALL) -d $(TARGET_DIR)/etc/level_zero
	if [ -d $(@D)/extracted/etc/level_zero ]; then \
		cp -a $(@D)/extracted/etc/level_zero/. \
			$(TARGET_DIR)/etc/level_zero/; \
	fi
endef

$(eval $(generic-package))
