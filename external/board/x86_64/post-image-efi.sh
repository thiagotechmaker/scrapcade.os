#!/bin/sh
# T-OS: initrd pequeno, GRUB que acha o sistema em qualquer disco, disk.img e .iso
set -e
B=$(dirname "$0")
VER=$(sed -n 's/^VERSION="\{0,1\}\([^"]*\)"\{0,1\}/\1/p' "$TARGET_DIR/etc/os-release")
echo "T-OS $VER" > "$BINARIES_DIR/tos-live"

# initrd: so o busybox e as bibliotecas dele
I=$BUILD_DIR/tos-initramfs
rm -rf "$I"
mkdir -p "$I/bin" "$I/lib" "$I/dev" "$I/proc" "$I/sys"
ln -s lib "$I/lib64"          # x86_64 procura as bibliotecas em /lib64
cp "$TARGET_DIR/bin/busybox" "$I/bin/"
for l in ld-linux-x86-64.so.2 libc.so.6 libresolv.so.2; do
  cp -L "$TARGET_DIR/lib/$l" "$I/lib/"
done
cp "$B/initramfs/init" "$I/init"
chmod 755 "$I/init"
( cd "$I" && find . | cpio -o -H newc --quiet | gzip -9 ) > "$BINARIES_DIR/tos-initrd.gz"

cp "$B/grub-live.cfg" "$BINARIES_DIR/efi-part/EFI/BOOT/grub.cfg"
support/scripts/genimage.sh -c "$B/genimage-efi.cfg"
sh "$B/make-iso.sh"
