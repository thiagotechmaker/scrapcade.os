################################################################################
#
# xemu-bin (Xbox original). AppImage oficial, com o hash da release conferido; vai aberta em /usr/lib/xemu
# (sem FUSE na hora de jogar). So na imagem de 64 bits.
#
################################################################################

XEMU_BIN_VERSION = 0.8.139
XEMU_BIN_SOURCE = xemu-$(XEMU_BIN_VERSION)-x86_64.AppImage
XEMU_BIN_SITE = https://github.com/xemu-project/xemu/releases/download/v$(XEMU_BIN_VERSION)
XEMU_BIN_LICENSE = GPL-2.0
XEMU_BIN_DEPENDENCIES = libusb

# a AppImage nao e um arquivo compactado comum: o proprio executavel se abre (--appimage-extract) no PC que compila
define XEMU_BIN_EXTRACT_CMDS
	cp $(XEMU_BIN_DL_DIR)/$(XEMU_BIN_SOURCE) $(@D)/
	chmod +x $(@D)/$(XEMU_BIN_SOURCE)
	cd $(@D) && ./$(XEMU_BIN_SOURCE) --appimage-extract >/dev/null
endef

define XEMU_BIN_INSTALL_TARGET_CMDS
	rm -rf $(TARGET_DIR)/usr/lib/xemu
	mkdir -p $(TARGET_DIR)/usr/lib/xemu
	cp -a $(@D)/squashfs-root/. $(TARGET_DIR)/usr/lib/xemu/
	printf '#!/bin/sh\nexec /usr/lib/xemu/AppRun "$$@"\n' > $(TARGET_DIR)/usr/bin/xemu
	chmod 0755 $(TARGET_DIR)/usr/bin/xemu
endef

$(eval $(generic-package))
