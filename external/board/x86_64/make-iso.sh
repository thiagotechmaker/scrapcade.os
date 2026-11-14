#!/bin/sh
# Monta o .iso do ScrapCade: o sistema (tos.squashfs) e lido do proprio pen drive ou DVD, somente leitura.
# Liga por BIOS antigo (isolinux com menu, hibrido para pen drive) e por UEFI (GRUB na imagem EFI).
set -e
B=$BINARIES_DIR
[ -f "$B/rootfs.squashfs" ] || exit 0
VER=$(sed -n 's/^VERSION="\{0,1\}\([^"]*\)"\{0,1\}/\1/p' "$TARGET_DIR/etc/os-release")
T=$BUILD_DIR/tos-iso
rm -rf "$T"
mkdir -p "$T/boot" "$T/EFI/BOOT"
cp "$B/bzImage" "$T/boot/bzImage"
cp "$B/tos-initrd.gz" "$T/boot/initrd.gz"
cp "$B/rootfs.squashfs" "$T/boot/tos.squashfs"
cp "$B/tos-live" "$T/boot/tos-live"
cp "$B/efi-part/EFI/BOOT/bootx64.efi" "$T/EFI/BOOT/"
cp "$(dirname "$0")/grub-live.cfg" "$T/EFI/BOOT/grub.cfg"
EFI=$BUILD_DIR/tos-efi.img
rm -f "$EFI"
dd if=/dev/zero of="$EFI" bs=1M count=4 status=none
"$HOST_DIR/sbin/mkfs.vfat" -n TOS_EFI "$EFI" >/dev/null
"$HOST_DIR/bin/mmd" -i "$EFI" ::/EFI ::/EFI/BOOT
"$HOST_DIR/bin/mcopy" -i "$EFI" "$T/EFI/BOOT/bootx64.efi" "$T/EFI/BOOT/grub.cfg" ::/EFI/BOOT/
cp "$EFI" "$T/boot/efi.img"
# BIOS antigo: isolinux com o menu de boot (o Rufus no modo ISO reconhece e instala o syslinux)
mkdir -p "$T/isolinux"
cp "$B/syslinux/isolinux.bin" "$B"/syslinux/*.c32 "$HOST_DIR/share/syslinux/ldlinux.c32" "$T/isolinux/"
cp "$B/tos-syslinux/syslinux.cfg" "$T/isolinux/isolinux.cfg"
cp "$B/tos-syslinux/fundo.png" "$T/isolinux/"
cp "$B/tos-memtest.bin" "$T/boot/memtest.bin"
cp "$B/tos-memtest.efi" "$T/boot/memtest.efi"
HYB=$(ls "$BUILD_DIR"/syslinux-*/bios/mbr/isohdpfx.bin | head -1)
ISO=$B/scrapcade-os-$VER-x86_64.iso
"$HOST_DIR/bin/xorriso" -as mkisofs -iso-level 3 -J -R -V "SCRAPCADE_$VER" \
	-isohybrid-mbr "$HYB" \
	-b isolinux/isolinux.bin -c isolinux/boot.cat -no-emul-boot -boot-load-size 4 -boot-info-table \
	-eltorito-alt-boot -e boot/efi.img -no-emul-boot -isohybrid-gpt-basdat \
	-o "$ISO" "$T" 2>&1 | tail -2
echo "iso: $ISO ($(du -h "$ISO" | cut -f1))"
