#!/bin/sh
# ScrapCade: initrd pequeno, GRUB que acha o sistema em qualquer disco, disk.img e .iso
set -e
B=$(dirname "$0")
VER=$(sed -n 's/^VERSION="\{0,1\}\([^"]*\)"\{0,1\}/\1/p' "$TARGET_DIR/etc/os-release")
echo "ScrapCade $VER" > "$BINARIES_DIR/tos-live"

# initrd: so o busybox e as bibliotecas dele
I=$BUILD_DIR/tos-initramfs
rm -rf "$I"
mkdir -p "$I/bin" "$I/lib" "$I/dev" "$I/proc" "$I/sys"
ln -s lib "$I/lib64"          # x86_64 procura as bibliotecas em /lib64
cp "$TARGET_DIR/bin/busybox" "$I/bin/"
cp -L "$TARGET_DIR/lib/$(cd "$TARGET_DIR/lib" && ls ld-linux*.so.* | head -1)" "$I/lib/"   # 64 bits: ld-linux-x86-64; 32: ld-linux.so.2
# as bibliotecas que o busybox pede (e as que elas pedem): a lista muda conforme a compilacao (o de 32 bits pede a libtirpc)
falta="$TARGET_DIR/bin/busybox"
while [ -n "$falta" ]; do
  prox=""
  for f in $falta; do
    for l in $(readelf -d "$f" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
      [ -e "$I/lib/$l" ] && continue
      o=$(ls "$TARGET_DIR/lib/$l" "$TARGET_DIR/usr/lib/$l" 2>/dev/null | head -1)
      [ -n "$o" ] || { echo "initrd: falta $l"; exit 1; }
      cp -L "$o" "$I/lib/"; prox="$prox $I/lib/$l"
    done
  done
  falta=$prox
done
ls "$I/lib"
cp "$B/initramfs/init" "$I/init"
cp "$B/initramfs/logo32.raw" "$B/initramfs/logo16.raw" "$B/initramfs/barra32.raw" "$B/initramfs/barra16.raw" "$I/"   # logo e barra animadas pelo initrd
chmod 755 "$I/init"
( cd "$I" && find . | cpio -o -H newc --quiet | gzip -9 ) > "$BINARIES_DIR/tos-initrd.gz"

cp "$B/grub-live.cfg" "$BINARIES_DIR/efi-part/EFI/BOOT/grub.cfg"

# menu de boot do BIOS antigo (syslinux): configuracao, fundo e modulos do menu
S=$BINARIES_DIR/tos-syslinux
rm -rf "$S"; mkdir -p "$S"
sed "s/@VERSAO@/v$VER/" "$B/boot-menu.cfg" > "$S/syslinux.cfg"   # versao no titulo do menu
cp "$B/boot-fundo.png" "$S/fundo.png"
cp "$BINARIES_DIR"/syslinux/*.c32 "$S/"
MT=$(ls "$BINARIES_DIR"/memtest*.bin "$TARGET_DIR"/boot/memtest*.bin 2>/dev/null | head -1)
if [ -n "$MT" ]; then
  cp "$MT" "$BINARIES_DIR/tos-memtest.bin"
  cp "$BINARIES_DIR/memtest.efi" "$BINARIES_DIR/tos-memtest.efi"   # teste de memoria pelo UEFI (GRUB)
else
  # 32 bits: o memtest86+ nao compila aqui; usa o da versao de 64 bits (roda nos processadores que tambem fazem 64 bits)
  M64=$(dirname "$BINARIES_DIR")/../x86_64/images
  cp "$M64/tos-memtest.bin" "$BINARIES_DIR/tos-memtest.bin"
  cp "$M64/tos-memtest.efi" "$BINARIES_DIR/tos-memtest.efi"
fi

support/scripts/genimage.sh -c "$B/genimage-efi.cfg"

# BIOS antigo: codigo do syslinux no setor 0 e o syslinux instalado dentro da SCRAPCADE (que comeca em 1 MiB)
SL=$(ls "$BUILD_DIR"/syslinux-*/bios/mtools/syslinux | head -1)
"$SL" --offset 1048576 --directory /syslinux --install "$BINARIES_DIR/disk.img"
dd if="$BINARIES_DIR/syslinux/mbr.bin" of="$BINARIES_DIR/disk.img" bs=440 count=1 conv=notrunc status=none
sh "$B/make-iso.sh"

# ScrapCade: arquivos para publicar como versao nova (GitHub Releases, tag v<VERSION>): o tos-atualizar baixa estes.
# BUILD_ID (numero da compilacao) vai junto: e por ele que o atualizador sabe se a versao publicada e mais nova.
U=$BINARIES_DIR/atualizacao
rm -rf "$U"; mkdir -p "$U"
cp "$BINARIES_DIR/rootfs.squashfs" "$U/tos.squashfs"
cp "$BINARIES_DIR/bzImage" "$U/bzImage"
cp "$BINARIES_DIR/tos-initrd.gz" "$U/initrd.gz"
sed -n 's/^BUILD_ID=//p' "$TARGET_DIR/etc/os-release" > "$U/BUILD_ID"
echo "$VER" > "$U/VERSION"
# para baixar e instalar do zero: um .iso so (com o tipo de PC no nome), que vale para qualquer jeito de gravar
if grep -q "^BR2_i386=y" "$BR2_CONFIG"; then
  # 32 bits: arquivos com -32 no nome, para a atualizacao de um PC de 32 bits nunca pegar os de 64 (e vice-versa)
  mv "$U/tos.squashfs" "$U/tos-32.squashfs"; mv "$U/bzImage" "$U/bzImage-32"; mv "$U/initrd.gz" "$U/initrd-32.gz"
  cp "$BINARIES_DIR/scrapcade-os-$VER-i686.iso" "$U/scrapcade-os-v$VER-pc-32bits.iso"
  ( cd "$U" && sha256sum tos-32.squashfs bzImage-32 initrd-32.gz scrapcade-os-v$VER-pc-32bits.iso > SHA256SUMS-32 )
else
  cp "$BINARIES_DIR/scrapcade-os-$VER-x86_64.iso" "$U/scrapcade-os-v$VER-pc-64bits.iso"
  ( cd "$U" && sha256sum tos.squashfs bzImage initrd.gz scrapcade-os-v$VER-pc-64bits.iso > SHA256SUMS )
fi
