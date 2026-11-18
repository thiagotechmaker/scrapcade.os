################################################################################
#
# rpcs3-bin (PlayStation 3). AppImage oficial, com o hash da release conferido; vai aberta em /usr/lib/rpcs3.
# Versao de pelo menos duas semanas antes da compilacao (nada de "a mais nova"). So na imagem de 64 bits.
#
################################################################################

RPCS3_BIN_VERSION = 0.0.43-20550-fab15ea0
RPCS3_BIN_SOURCE = rpcs3-v$(RPCS3_BIN_VERSION)_linux64.AppImage
RPCS3_BIN_SITE = https://github.com/RPCS3/rpcs3-binaries-linux/releases/download/build-fab15ea00541808992ebe0d0363e0f1b1a01033a
RPCS3_BIN_LICENSE = GPL-2.0

define RPCS3_BIN_EXTRACT_CMDS
	cp $(RPCS3_BIN_DL_DIR)/$(RPCS3_BIN_SOURCE) $(@D)/
	chmod +x $(@D)/$(RPCS3_BIN_SOURCE)
	cd $(@D) && ./$(RPCS3_BIN_SOURCE) --appimage-extract >/dev/null
endef

define RPCS3_BIN_INSTALL_TARGET_CMDS
	rm -rf $(TARGET_DIR)/usr/lib/rpcs3
	mkdir -p $(TARGET_DIR)/usr/lib/rpcs3
	cp -a $(@D)/squashfs-root/. $(TARGET_DIR)/usr/lib/rpcs3/
	# programas de teste do PS3/PSP (PowerPC e MIPS): nao servem para jogar e o Buildroot recusa outra arquitetura
	rm -rf $(TARGET_DIR)/usr/lib/rpcs3/usr/share/rpcs3/test
	# o Qt da AppImage pede libGLX e libOpenGL (nomes do libglvnd); a Mesa daqui entrega tudo na libGL.so.1, que tem
	# as mesmas funcoes. Os nomes ficam so dentro da pasta do RPCS3: o resto do sistema continua igual.
	ln -sf /usr/lib/libGL.so.1 $(TARGET_DIR)/usr/lib/rpcs3/usr/lib/libGLX.so.0
	ln -sf /usr/lib/libGL.so.1 $(TARGET_DIR)/usr/lib/rpcs3/usr/lib/libOpenGL.so.0
	printf '#!/bin/sh\nexec /usr/lib/rpcs3/AppRun "$$@"\n' > $(TARGET_DIR)/usr/bin/rpcs3
	chmod 0755 $(TARGET_DIR)/usr/bin/rpcs3
endef

$(eval $(generic-package))
