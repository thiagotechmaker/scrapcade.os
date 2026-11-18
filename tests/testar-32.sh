#!/bin/bash
# Teste da versao de 32 bits no QEMU (qemu-system-i386 com CPU de netbook Atom N270, 1 GB, BIOS antigo):
#  1) .iso gravado direto (2 GB: o sistema so arruma o pen drive com 1,5 GB ou mais): no 1o boot cria a SCRAP-DADOS
#  2) disco: primeiro boot, perfil, envio de um jogo pela pagina, carrossel, jogo rodando, volta ao menu, Informacoes
# Uso: bash tests/testar-32.sh   (depois de bash build-i686.sh). Grava teste/aprovado-32 se tudo passar;
# o scripts/publicar-versao.ps1 so sobe os arquivos de 32 bits com essa trava.
TOS=$(cd "$(dirname "$0")/.." && pwd); cd $TOS
export PATH=$TOS/output/i686/host/sbin:$TOS/output/i686/host/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
mkdir -p teste
I=output/i686/images; ISO=$(ls -t $I/atualizacao/*-32bits.iso | head -1)
[ -f "$ISO" ] || { echo "falta o .iso de 32 bits (bash build-i686.sh)"; exit 1; }
R=teste/resumo-32.txt; : > $R
log() { echo "$*" | tee -a $R >&2; }
rotulos() {
  sfdisk -d "$1" 2>/dev/null | sed -n 's/.*start= *\([0-9]*\).*/\1/p' | while read st; do
    blkid -p -o value -s LABEL -O $((st * 512)) "$1" 2>/dev/null; done | tr '\n' ' '; }
k() { for c in "$@"; do echo "sendkey $c"; sleep 0.5; done; }
shot() { echo "screendump $TOS/teste/y32_$1.ppm"; }
senha() { sleep 2; }   # o boot nao pede mais senha: ela e criada no painel, ao ligar o SSH
opt() { k tab; sleep 2; for i in $(seq $1); do echo "sendkey down"; sleep 0.25; done; sleep 1; }
fecha() { k esc; sleep 1; k esc; sleep 1; }
W=http://127.0.0.1:18081
Q="qemu-system-i386 -accel kvm -accel tcg -cpu n270 -m 1024 -smp 2 -vga std -display none -monitor stdio"
rm -f teste/y32_*.ppm teste/aprovado-32
falhas=0

# ---------------------------------------------------------------- .iso gravado direto
for tentativa in 1 2; do
  D=teste/t32-iso.img; cp $ISO $D; truncate -s 4G $D
  ( sleep 80; senha; sleep 120; shot iso_primeiro_boot; echo quit ) | timeout 400 ${Q/-m 1024/-m 2048} -drive file=$D,format=raw > teste/q32a.log 2>&1
  [ -f teste/y32_iso_primeiro_boot.ppm ] && break
  log "iso: QEMU caiu (tentativa $tentativa), repetindo"
done
p=$(rotulos $D); log "iso particoes: $p"
echo "$p" | grep -q SCRAP-DADOS || { log "FALHA: o .iso nao criou a SCRAP-DADOS"; falhas=$((falhas+1)); }

# ---------------------------------------------------------------- menu, envio, jogo
for tentativa in 1 2; do
  D=teste/t32-sc.img; cp $I/disk.img $D; truncate -s 3G $D
  (
    sleep 80; senha; sleep 120
    k ret; sleep 4; k t h i a g o; k ret; sleep 4; k ret; sleep 4; k ret; sleep 30
    log "pagina web: $(curl -s -m 15 -o /dev/null -w '%{http_code}' $W/)"
    log "envio nes: $(curl -g -s -m 60 -T "$TOS/tests/roms/nes/Aventura de Teste.nes" "$W/envio/Aventura%20de%20Teste.nes" -o /dev/null -w '%{http_code}')"
    sleep 60; k right; sleep 6; shot carrossel
    k ret; sleep 50; shot jogando; echo "sendkey esc 1000"; sleep 90; shot voltou
    opt 50; k ret; sleep 6; shot info; fecha
    echo quit
  ) | timeout 1200 $Q -drive file=$D,format=raw -nic user,model=e1000,hostfwd=tcp:127.0.0.1:18081-:8080 > teste/q32b.log 2>&1
  [ -f teste/y32_carrossel.ppm ] && break
  log "menu: QEMU caiu (tentativa $tentativa), repetindo"
done
grep -q "pagina web: 200" $R || { log "FALHA: pagina web"; falhas=$((falhas+1)); }
grep -q "envio nes: 20[01]" $R || { log "FALHA: envio do jogo"; falhas=$((falhas+1)); }

# telas: existem, nao sao uma cor so, e o jogo e diferente do menu
/usr/bin/python3 - "$TOS/teste" >> $R <<'P' || falhas=$((falhas+1))
import sys, os
from PIL import Image, ImageChops
d = sys.argv[1]; ok = True
def abre(n):
    p = os.path.join(d, f"y32_{n}.ppm")
    return Image.open(p).convert("RGB") if os.path.exists(p) else None
for n in ("iso_primeiro_boot", "carrossel", "jogando", "voltou", "info"):
    im = abre(n)
    cores = len(im.getcolors(1 << 20) or []) if im else 0
    print(f"tela {n}: {'falta' if not im else f'{im.size[0]}x{im.size[1]}, {cores} cores'}")
    minimo = 2 if n == "jogando" else 20     # a ROM de teste e falsa: o emulador desenha uma cor so, com as bordas
    if not im or cores < minimo: ok = False
c, j = abre("carrossel"), abre("jogando")
if c and j:
    dif = sum(ImageChops.difference(c, j.resize(c.size)).convert("L").histogram()[30:])
    print(f"jogo diferente do menu: {dif} pixels")
    if dif < 5000: ok = False
sys.exit(0 if ok else 1)
P
cat $R | grep "^tela\|^jogo"
# nucleo que compila mas nao abre (biblioteca ou simbolo faltando)
/usr/bin/python3 "$TOS/tests/conferir-bibliotecas.py" "$TOS/output/i686/target" > teste/bibliotecas-32.txt || falhas=$((falhas+1))
tee -a $R < teste/bibliotecas-32.txt | grep -v "^aviso"
if [ $falhas -eq 0 ]; then
  cat $I/atualizacao/BUILD_ID > teste/aprovado-32
  echo "APROVADO 32 bits: compilacao $(cat teste/aprovado-32)"
else
  echo "REPROVADO 32 bits: $falhas falha(s), ver teste/resumo-32.txt e teste/y32_*.ppm"
fi
