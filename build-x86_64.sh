#!/bin/bash
# Compila o ScrapCade OS para PC de 64 bits (BIOS e UEFI). Uso: bash build-x86_64.sh
# PATH so do Linux: no WSL as pastas do Windows tem espacos, e o Buildroot recusa.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
TOS=$(cd "$(dirname "$0")" && pwd)
OUT=$TOS/output/x86_64
LOG=$OUT/build.log
mkdir -p "$OUT"
{
  date
  make -C "$TOS/buildroot" BR2_EXTERNAL="$TOS/external" O="$OUT" tos_x86_64_defconfig
  make -C "$TOS/buildroot" BR2_EXTERNAL="$TOS/external" O="$OUT"
  echo "EXIT=$?"
  date
} > "$LOG" 2>&1
tail -3 "$LOG"
