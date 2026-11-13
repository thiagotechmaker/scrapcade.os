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
