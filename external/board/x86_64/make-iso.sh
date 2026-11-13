#!/bin/sh
# Monta o .iso do T-OS: o sistema (tos.squashfs) e lido do proprio pen drive ou DVD, somente leitura.
# Liga por BIOS antigo (El Torito + MBR hibrido) e por UEFI (particao EFI no El Torito e anexada, para o pen drive).
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
# BIOS antigo: nucleo do GRUB no El Torito do CD/DVD e codigo de boot hibrido para pen drive
mkdir -p "$T/boot/grub"
cat "$B/cdboot.img" "$B/tos-core.img" > "$T/boot/grub/bios.img"
ISO=$B/t-os-$VER-x86_64.iso
"$HOST_DIR/bin/xorriso" -as mkisofs -iso-level 3 -J -R -V "T-OS_$VER" \
	-b boot/grub/bios.img -no-emul-boot -boot-load-size 4 -boot-info-table --grub2-boot-info \
	--grub2-mbr "$B/boot_hybrid.img" \
	-eltorito-alt-boot -e boot/efi.img -no-emul-boot \
	-append_partition 2 0xef "$EFI" \
	-o "$ISO" "$T" 2>&1 | tail -2
echo "iso: $ISO ($(du -h "$ISO" | cut -f1))"
