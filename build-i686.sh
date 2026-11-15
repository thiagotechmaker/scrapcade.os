#!/bin/bash
# Compila o ScrapCade OS para PC de 32 bits (i686). Uso: bash ~/t-os/build-i686.sh [BUILD_ID da versao de 64 bits]
# Com o BUILD_ID, as duas compilacoes saem com o mesmo numero e vao juntas na mesma versao publicada.
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
TOS=$(cd "$(dirname "$0")" && pwd)
OUT=$TOS/output/i686
LOG=$OUT/build.log
mkdir -p "$OUT"
[ -n "$1" ] && export TOS_BUILD_ID=$1
{
  date
  make -C "$TOS/buildroot" BR2_EXTERNAL="$TOS/external" O="$OUT" tos_i686_defconfig
  make -j6 -C "$TOS/buildroot" BR2_EXTERNAL="$TOS/external" O="$OUT"
  echo "EXIT=$?"
  date
} > "$LOG" 2>&1
tail -3 "$LOG"
