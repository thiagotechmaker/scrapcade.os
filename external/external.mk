include $(sort $(wildcard $(BR2_EXTERNAL_TOS_PATH)/package/*/*.mk))

# GCC 14 virou erro um aviso antigo de C (ponteiros incompativeis) que o libfm 1.3.2 ainda tem
LIBFM_CONF_ENV += CFLAGS="$(TARGET_CFLAGS) -Wno-error=incompatible-pointer-types"

# O NetSurf converte um icone XPM ao compilar; o gdk-pixbuf do computador de compilacao precisa ler XPM
HOST_GDK_PIXBUF_CONF_OPTS += -Dothers=enabled

# O PCManFM 1.3.2 tem o mesmo aviso de ponteiros que o GCC 14 trata como erro
PCMANFM_CONF_ENV += CFLAGS="$(TARGET_CFLAGS) -Wno-error=incompatible-pointer-types"
