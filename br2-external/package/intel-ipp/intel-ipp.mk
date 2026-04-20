################################################################################
#
# intel-ipp
#
# Pre-built Intel Integrated Performance Primitives (IPP) 2022.3.1 for x86_64.
# Installs:
#   TARGET  - runtime shared libs (libipp*.so*) → $(TARGET_DIR)/usr/lib/
#   STAGING - shared libs + headers + CMake config + static libs
#             so downstream SDK projects can find_package(IPP) via CMake.
#
# Two .deb archives are downloaded from Intel's oneAPI apt pool:
#   - intel-oneapi-ipp-2022.3-...        (runtime shared libs)
#   - intel-oneapi-ipp-devel-2022.3-...  (headers, CMake, static libs)
#
################################################################################

INTEL_IPP_VERSION = 2022.3.1-7
INTEL_IPP_SHORT   = 2022.3

INTEL_IPP_SOURCE = intel-oneapi-ipp-$(INTEL_IPP_SHORT)-$(INTEL_IPP_VERSION)_amd64.deb
INTEL_IPP_SITE   = https://apt.repos.intel.com/oneapi/pool/main

INTEL_IPP_EXTRA_DOWNLOADS = \
	$(INTEL_IPP_SITE)/intel-oneapi-ipp-devel-$(INTEL_IPP_SHORT)-$(INTEL_IPP_VERSION)_amd64.deb

INTEL_IPP_LICENSE       = Intel Simplified Software License
INTEL_IPP_LICENSE_FILES = opt/intel/oneapi/ipp/$(INTEL_IPP_SHORT)/share/doc/ipp/licensing/license.txt
INTEL_IPP_REDISTRIBUTE  = NO

INTEL_IPP_INSTALL_STAGING = YES
INTEL_IPP_INSTALL_TARGET  = YES

# ── Custom extraction: unpack both .deb files (ar archive + data.tar.*) ───────
define INTEL_IPP_EXTRACT_CMDS
	mkdir -p $(@D)/extracted
	for deb in \
		$(INTEL_IPP_SOURCE) \
		intel-oneapi-ipp-devel-$(INTEL_IPP_SHORT)-$(INTEL_IPP_VERSION)_amd64.deb; \
	do \
		debpath=$(INTEL_IPP_DL_DIR)/$$deb; \
		tmpdir=$(@D)/extracted/$$deb.d; \
		mkdir -p $$tmpdir; \
		(cd $$tmpdir && ar x $$debpath); \
		for data in $$tmpdir/data.tar.*; do \
			tar -C $(@D)/extracted -xf $$data; \
		done; \
	done
endef

# Convenience variable: root of IPP installation inside extracted tree
INTEL_IPP_EXTRACTED = $(@D)/extracted/opt/intel/oneapi/ipp/$(INTEL_IPP_SHORT)

define INTEL_IPP_INSTALL_TARGET_CMDS
	# Runtime shared libraries (dispatcher + per-CPU dispatch variants) → /usr/lib/
	$(INSTALL) -d $(TARGET_DIR)/usr/lib
	find $(INTEL_IPP_EXTRACTED)/lib \
		-maxdepth 1 -name 'libipp*.so*' \
		-exec $(INSTALL) -m 0755 {} $(TARGET_DIR)/usr/lib/ \;
endef

define INTEL_IPP_INSTALL_STAGING_CMDS
	# Shared libraries (same set as target — needed for linking)
	$(INSTALL) -d $(STAGING_DIR)/usr/lib
	find $(INTEL_IPP_EXTRACTED)/lib \
		-maxdepth 1 -name 'libipp*.so*' \
		-exec $(INSTALL) -m 0755 {} $(STAGING_DIR)/usr/lib/ \;

	# Static libraries (.a) — available for SDK consumers wanting static linkage
	find $(INTEL_IPP_EXTRACTED)/lib \
		-maxdepth 1 -name 'libipp*.a' \
		-exec $(INSTALL) -m 0644 {} $(STAGING_DIR)/usr/lib/ \;

	# Headers: install as usr/include/ipp/ tree plus the top-level ipp.h wrapper
	$(INSTALL) -d $(STAGING_DIR)/usr/include
	cp -a $(INTEL_IPP_EXTRACTED)/include/ipp \
		$(STAGING_DIR)/usr/include/
	if [ -f $(INTEL_IPP_EXTRACTED)/include/ipp.h ]; then \
		$(INSTALL) -m 0644 $(INTEL_IPP_EXTRACTED)/include/ipp.h \
			$(STAGING_DIR)/usr/include/; \
	fi

	# CMake config files (IPPConfig.cmake, IPPConfigVersion.cmake, …)
	$(INSTALL) -d $(STAGING_DIR)/usr/lib/cmake
	cp -a $(INTEL_IPP_EXTRACTED)/lib/cmake/ipp \
		$(STAGING_DIR)/usr/lib/cmake/
endef

$(eval $(generic-package))
