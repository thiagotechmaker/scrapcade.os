#!/bin/sh

set -e

BOARD_DIR=$(dirname "$0")

# Detect boot strategy, EFI or BIOS
if [ -d "$BINARIES_DIR/efi-part/" ]; then
    cp -f "$BOARD_DIR/grub-efi.cfg" "$BINARIES_DIR/efi-part/EFI/BOOT/grub.cfg"
else
    cp -f "$BOARD_DIR/grub-bios.cfg" "$TARGET_DIR/boot/grub/grub.cfg"

    # Copy grub 1st stage to binaries, required for genimage
    cp -f "$TARGET_DIR/lib/grub/i386-pc/boot.img" "$BINARIES_DIR"
fi

# T-OS: o X so abre pelo "Modo desktop" do ScrapCade. O script do pacote ligava o X no boot,
# pegava a tela antes do ScrapCade e deixava tudo preto.
rm -f "$TARGET_DIR/etc/init.d/S40xorg"

# T-OS: GRUB para BIOS antigo. O mesmo nucleo serve para o disk.img (setor 1), o .iso (El Torito) e o instalador.
G=$(ls -d "$BUILD_DIR"/grub2-*/build-i386-pc 2>/dev/null | head -1)
if [ -n "$G" ]; then
	"$HOST_DIR/bin/grub-mkimage" -d "$G/grub-core" -O i386-pc -o "$BINARIES_DIR/tos-core.img" -p /EFI/BOOT \
		-c "$BOARD_DIR/grub-bios-embutido.cfg" \
		biosdisk part_msdos part_gpt fat exfat iso9660 search search_fs_file configfile normal linux \
		echo test boot vbe vga video video_fb all_video
	cp "$G/grub-core/boot.img" "$G/grub-core/cdboot.img" "$G/grub-core/boot_hybrid.img" "$BINARIES_DIR/"
	mkdir -p "$TARGET_DIR/usr/share/tos/grub-bios"
	cp "$BINARIES_DIR/tos-core.img" "$TARGET_DIR/usr/share/tos/grub-bios/core.img"
	cp "$G/grub-core/boot.img" "$TARGET_DIR/usr/share/tos/grub-bios/boot.img"
fi

# T-OS: Samba e SSH so ligam se o usuario quiser (painel Sistema). Samba vem ligado; SSH desligado.
for s in S91smb S50dropbear; do
	[ -f "$TARGET_DIR/etc/init.d/$s" ] || continue
	mv "$TARGET_DIR/etc/init.d/$s" "$TARGET_DIR/etc/init.d/servico-$s"
done
cat > "$TARGET_DIR/etc/init.d/S91tos-rede" <<'EOF'
#!/bin/sh
# T-OS: compartilhamento na rede (Samba, padrao ligado) e acesso remoto (SSH, padrao desligado)
C=/userdata/system/configs
estado() { [ -f "$C/$1" ] && cat "$C/$1" || echo "$2"; }
case "$1" in
  start|restart)
    [ "$(estado samba on)" = on ] && /etc/init.d/servico-S91smb start || /etc/init.d/servico-S91smb stop
    [ "$(estado ssh off)" = on ] && /etc/init.d/servico-S50dropbear start || /etc/init.d/servico-S50dropbear stop
    ;;
  stop)
    /etc/init.d/servico-S91smb stop; /etc/init.d/servico-S50dropbear stop
    ;;
esac >/dev/null 2>&1
exit 0
EOF
chmod 755 "$TARGET_DIR/etc/init.d/S91tos-rede"

# T-OS: numero da compilacao (ano mes dia hora minuto); a atualizacao so troca por uma mais nova
sed -i '/^BUILD_ID=/d' "$TARGET_DIR/etc/os-release"
echo "BUILD_ID=$(date +%Y%m%d%H%M)" >> "$TARGET_DIR/etc/os-release"
