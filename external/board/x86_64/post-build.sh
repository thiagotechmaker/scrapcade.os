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
