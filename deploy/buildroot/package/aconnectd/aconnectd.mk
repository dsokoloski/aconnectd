################################################################################
#
# aconnectd
#
################################################################################

ACONNECTD_VERSION = HEAD
ACONNECTD_SITE = git@git.sokoloski.ca:aconnectd.git
ACONNECTD_SITE_METHOD=git
ACONNECTD_GIT_SUBMODULES = NO
ACONNECTD_LICENSE = GPL-3
ACONNECTD_LICENSE_FILES = LICENCE.md
ACONNECTD_CPE_ID_VENDOR = dsokoloski
ACONNECTD_CPE_ID_PRODUCT = aconnectd
ACONNECTD_DEPENDENCIES = host-pkgconf host-cmake
ACONNECTD_INSTALL_STAGING = YES

define ACONNECTD_INSTALL_INIT_SYSTEMD
	$(INSTALL) -D -m 644 $(@D)/deploy/systemd/aconnectd.service \
		$(TARGET_DIR)/usr/lib/systemd/system/aconnectd.service
endef

$(eval $(cmake-package))
