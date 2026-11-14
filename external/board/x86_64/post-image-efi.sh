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

# menu de boot do BIOS antigo (syslinux): configuracao, fundo e modulos do menu
S=$BINARIES_DIR/tos-syslinux
rm -rf "$S"; mkdir -p "$S"
cp "$B/boot-menu.cfg" "$S/syslinux.cfg"
cp "$B/boot-fundo.png" "$S/fundo.png"
cp "$BINARIES_DIR"/syslinux/*.c32 "$S/"
MT=$(ls "$BINARIES_DIR"/memtest*.bin "$TARGET_DIR"/boot/memtest*.bin 2>/dev/null | head -1)
cp "$MT" "$BINARIES_DIR/tos-memtest.bin"

support/scripts/genimage.sh -c "$B/genimage-efi.cfg"

# BIOS antigo: codigo do syslinux no setor 0 e o syslinux instalado dentro da TOS-BOOT (que comeca em 1 MiB)
SL=$(ls "$BUILD_DIR"/syslinux-*/bios/mtools/syslinux | head -1)
"$SL" --offset 1048576 --directory /syslinux --install "$BINARIES_DIR/disk.img"
dd if="$BINARIES_DIR/syslinux/mbr.bin" of="$BINARIES_DIR/disk.img" bs=440 count=1 conv=notrunc status=none
sh "$B/make-iso.sh"

# T-OS: arquivos para publicar como versao nova (GitHub Releases, tag v<BUILD_ID>): o tos-atualizar baixa estes
U=$BINARIES_DIR/atualizacao
rm -rf "$U"; mkdir -p "$U"
cp "$BINARIES_DIR/rootfs.squashfs" "$U/tos.squashfs"
cp "$BINARIES_DIR/bzImage" "$U/bzImage"
cp "$BINARIES_DIR/tos-initrd.gz" "$U/initrd.gz"
V=$(sed -n 's/^BUILD_ID=//p' "$TARGET_DIR/etc/os-release")
echo "$V" > "$U/BUILD_ID"
# para baixar e instalar do zero: o .iso e a imagem do pen drive (com o tipo de PC no nome)
cp "$BINARIES_DIR"/t-os-*-x86_64.iso "$U/t-os-$V-pc-64bits.iso"
gzip -1 -c "$BINARIES_DIR/disk.img" > "$U/t-os-$V-pc-64bits-pendrive.img.gz"
( cd "$U" && sha256sum tos.squashfs bzImage initrd.gz t-os-$V-pc-64bits.iso t-os-$V-pc-64bits-pendrive.img.gz > SHA256SUMS )
