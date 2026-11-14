include $(sort $(wildcard $(BR2_EXTERNAL_TOS_PATH)/package/*/*.mk))

# GCC 14 virou erro um aviso antigo de C (ponteiros incompativeis) que o libfm 1.3.2 ainda tem
LIBFM_CONF_ENV += CFLAGS="$(TARGET_CFLAGS) -Wno-error=incompatible-pointer-types"

# O NetSurf converte um icone XPM ao compilar; o gdk-pixbuf do computador de compilacao precisa ler XPM
HOST_GDK_PIXBUF_CONF_OPTS += -Dothers=enabled

# O PCManFM 1.3.2 tem o mesmo aviso de ponteiros que o GCC 14 trata como erro
PCMANFM_CONF_ENV += CFLAGS="$(TARGET_CFLAGS) -Wno-error=incompatible-pointer-types"

# ScrapCade OS: logo da tela de boot do kernel (no lugar do pinguim), centralizada por fbcon=logo-pos:center
define TOS_LINUX_LOGO
	cp $(BR2_EXTERNAL_TOS_PATH)/board/x86_64/logo_scrapcade_clut224.ppm \
		$(LINUX_DIR)/drivers/video/logo/logo_linux_clut224.ppm
endef
LINUX_PRE_BUILD_HOOKS += TOS_LINUX_LOGO
