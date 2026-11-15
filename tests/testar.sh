#!/bin/bash
# Testes automaticos do ScrapCade no QEMU (depois de compilar): bash tests/testar.sh
# Liga a imagem em BIOS e UEFI, passa por cada tela e opcao, confere rede, envio e som, e no fim compara cada
# tela com tests/referencias. Mostra so o resumo; as telas que mudaram vao para teste/falhas.png.
#   bash tests/testar.sh --referencias   grava as telas desta rodada como as novas referencias
export PATH=/home/thiago/t-os/output/x86_64/host/sbin:/home/thiago/t-os/output/x86_64/host/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
TOS=$(cd "$(dirname "$0")/.." && pwd); cd $TOS; mkdir -p teste; I=output/x86_64/images; ISO=$(ls -t $I/*.iso | head -1)   # o .iso mais novo
R=teste/resumo.txt; : > $R
log() { echo "$*" | tee -a $R >&2; }
rotulos() {   # nomes das particoes de um disco (sem precisar de root)
  sfdisk -d "$1" 2>/dev/null | sed -n 's/.*start= *\([0-9]*\).*/\1/p' | while read st; do
    blkid -p -o value -s LABEL -O $((st * 512)) "$1" 2>/dev/null; done | tr '\n' ' '; }
k() { for c in "$@"; do echo "sendkey $c"; sleep 0.5; done; }
shot() { echo "screendump $TOS/teste/z_$1.ppm"; }
acorda() { for i in 1 2 3 4 5 6 7; do echo "sendkey up"; sleep 1; done; }   # para a contagem do menu de boot assim que ele aparece
senha() { k t e s t e 1 2 3 ret; sleep 2; k t e s t e 1 2 3 ret; }
Q="qemu-system-x86_64 -accel tcg -m 2048 -smp 4 -vga std -display none -monitor stdio"
EFI="-bios /usr/share/ovmf/OVMF.fd"
rm -f teste/z_*.ppm

# ---------------------------------------------------------------- menu de boot e teste de memoria
D=teste/t-bios.img; cp $I/disk.img $D; truncate -s 3G $D
( acorda; k down; sleep 1; k down down down; sleep 1; shot boot_bios_memsel; k ret; sleep 25; shot boot_bios_memtest; echo quit ) | timeout 90 $Q -drive file=$D,format=raw > teste/q1.log 2>&1
( acorda; k down; k ret; sleep 9; shot boot_logo_kernel; sleep 36; senha; sleep 70; shot boot_bios_compat; echo quit ) | timeout 200 $Q -drive file=$D,format=raw > teste/q2.log 2>&1
cp $I/disk.img $D; truncate -s 3G $D
( sleep 4; shot boot_uefi_menu; k up; sleep 1; k down; sleep 1; k down; sleep 1; k down; sleep 1; shot boot_uefi_memsel; k ret; sleep 25; shot boot_uefi_memtest; echo quit ) | timeout 90 $Q $EFI -drive file=$D,format=raw,if=virtio > teste/q3.log 2>&1
# .iso gravado direto: arruma no 1o boot; no 2o boot (BIOS, GRUB) o teste de memoria
D=teste/t-iso.img; cp $ISO $D; truncate -s 4G $D
( sleep 40; senha; sleep 60; echo quit ) | timeout 200 $Q $EFI -drive file=$D,format=raw,if=virtio > teste/q4.log 2>&1
log "iso arrumado: $(sfdisk -l $D 2>/dev/null | grep -c "^$D")"
log "iso particoes: $(rotulos $D)"
( acorda; shot boot_grubbios_menu; k down; sleep 2; shot boot_grubbios_d1; k down down; sleep 2; shot boot_grubbios_d3; k ret; sleep 25; shot boot_grubbios_memtest; echo quit ) | timeout 90 $Q -drive file=$D,format=raw > teste/q5.log 2>&1
( sleep 14; shot boot_logo_grubbios; echo quit ) | timeout 40 $Q -drive file=$D,format=raw > teste/q5b.log 2>&1

# ---------------------------------------------------------------- ScrapCade e painel Sistema
D=teste/t-sc.img; cp $I/disk.img $D; truncate -s 3G $D
W=http://127.0.0.1:18080
porta() {   # aberta = conecta e nao fecha na hora (Samba espera, SSH manda a saudacao)
  python3 - "$1" <<'P'
import socket,sys,time
s=socket.socket(); s.settimeout(3)
try:
    s.connect(("127.0.0.1",int(sys.argv[1]))); t=time.time()
    try: d=s.recv(64)
    except socket.timeout: print("aberta"); sys.exit()
    print("aberta" if d else "fechada")
except Exception as e: print("fechada")
P
}
opt() { k tab; sleep 2;   # painel em secoes: conta so as opcoes (os titulos sao pulados)
  for i in $(seq $1); do echo "sendkey down"; sleep 0.25; done; sleep 1; }
fecha() { k esc; sleep 1; k esc; sleep 1; }
up() { curl -g -s -m 30 -T "$1" "$W/$2" -o /dev/null -w "%{http_code}"; }
enc() { python3 -c 'import sys,urllib.parse;print(urllib.parse.quote(sys.argv[1],safe=""))' "$1"; }
RO=$TOS/tests/roms
(
  sleep 30; senha; sleep 60
  k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 15
  log "pagina web: $(curl -s -m 10 -o /dev/null -w '%{http_code}' $W/)  lista de arquivos: $(curl -s -m 10 "$W/fs/lista?a=" | head -c 60)"
  for f in nes/"Aventura de Teste.nes" snes/"Corrida de Teste (U) [!].sfc" n64/"Jogo de Teste 64.z64" gb/"Quebra-Cabeça de Teste.gb" megadrive/"Plataforma Sem Capa.md" psx/"Disco de Teste.cue"; do
    log "envio $(basename "$f"): $(up "$RO/$f" "envio/$(enc "$(basename "$f")")")"; done
  sleep 40
  for f in nes/"Aventura de Teste" snes/"Corrida de Teste (U) [!]" n64/"Jogo de Teste 64" gb/"Quebra-Cabeça de Teste" psx/"Disco de Teste"; do
    s=${f%%/*}; b=$(basename "$f"); x=$(ls "$RO/$s" | grep -F "$b." | head -1)
    log "capa $b: $(up "$RO/$s/images/$b.png" "capa/$(enc "$s/$x")")"; done
  sleep 30; k right; sleep 1; k left; sleep 4; shot sc_carrossel
  k right right; sleep 3; shot sc_andou; k left left; sleep 2
  k e; sleep 3; shot sc_filtro_fav; k e; sleep 3; shot sc_filtro_mais; k e; sleep 3; shot sc_filtro_console; k q q q; sleep 2
  k x; sleep 2; shot sc_favorito
  k d; sleep 3; shot sc_detalhes; k d; sleep 2
  k ret; sleep 30; shot sc_jogando; echo "sendkey esc 1000"; sleep 75; shot sc_voltou_do_jogo
  log "samba antes: $(porta 18445)  ssh antes: $(porta 18022)"
  opt 0; k ret; sleep 3; shot p_perfil; fecha
  opt 25; k right; sleep 2; shot p_energia; fecha
  opt 17; k right; sleep 3; shot p_tema; k esc; sleep 4; shot sc_tema_8bits; opt 17; k left; sleep 1; fecha
  opt 19; k right; sleep 2; shot p_anim; k left; fecha
  opt 20; k right; sleep 3; shot p_capas_off; k left; sleep 2; fecha
  opt 21; k right; sleep 2; shot p_fundo_off; k left; fecha
  opt 35; sleep 1; shot p_motor; fecha
  opt 27; k ret; sleep 8; shot p_rede; fecha
  opt 24; k ret; sleep 12; shot p_bluetooth; fecha
  opt 28; k right; sleep 6; log "samba desligado: $(porta 18445)"; shot p_samba_off; k right; sleep 6; log "samba religado: $(porta 18445)"; fecha
  opt 29; k right; sleep 6; log "ssh ligado: $(porta 18022)"; shot p_ssh_on; k right; sleep 6; log "ssh desligado de novo: $(porta 18022)"; fecha
  opt 22; k ret; sleep 3; shot p_controle; fecha
  opt 36; k ret; sleep 3; shot p_comojogar; fecha
  opt 9; k ret; sleep 4; shot p_bios; fecha
  opt 31; k ret; sleep 20; shot p_atualizar; fecha
  opt 33; k ret; sleep 4; shot p_instalar; fecha
  opt 37; k ret; sleep 4; shot p_info; fecha
  opt 34; k ret; sleep 40; shot p_desktop
  echo quit
) | timeout 1200 $Q $EFI -drive file=$D,format=raw,if=virtio \
    -nic user,model=e1000,hostfwd=tcp:127.0.0.1:18080-:8080,hostfwd=tcp:127.0.0.1:18445-:445,hostfwd=tcp:127.0.0.1:18022-:22 \
    > teste/q6.log 2>&1
# o que ficou gravado na TOS-DADOS (perfil, preferencias, jogos)
log "disco particoes: $(rotulos $D)"
for tentativa in 1 2; do   # o QEMU as vezes cai ao ligar ("dumped core"): uma segunda tentativa
  D=teste/t-som.img; cp $I/disk.img $D; truncate -s 3G $D
  ( sleep 30; senha; sleep 60; k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 15
    opt 23; k left; sleep 2; shot p_volume; k ret; sleep 4; echo quit ) | timeout 300 $Q $EFI -drive file=$D,format=raw,if=virtio     -audiodev wav,id=s0,path=$TOS/teste/som2.wav -device intel-hda -device hda-output,audiodev=s0 > teste/q7.log 2>&1
  [ -s teste/som2.wav ] && [ -f teste/z_p_volume.ppm ] && break
  log "som: QEMU caiu (tentativa $tentativa), repetindo"
done
log "som: $(python3 -c 'import wave,struct,sys
w=wave.open(sys.argv[1]);d=w.readframes(w.getnframes());s=struct.unpack("<%dh"%(len(d)//2),d[:len(d)//2*2]);print("tocou" if max(map(abs,s or [0]))>1000 else "silencio")' teste/som2.wav)"
log "fim: $(ls teste/z_*.ppm | wc -l) capturas"
source "$TOS/tests/testar-extra.sh"   # pen drive, tema proprio, perfis, instalar no HD, atualizacao
if /usr/bin/python3 -c "import PIL" 2>/dev/null; then
  rm -f teste/aprovado
  if /usr/bin/python3 "$TOS/tests/conferir.py" "$TOS/teste" "$TOS/tests/referencias" "$1"; then
    # trava da publicacao: o scripts/publicar-versao.ps1 so sobe a compilacao que passou aqui
    sed -n 's/^BUILD_ID=//p' output/x86_64/target/etc/os-release > teste/aprovado
    echo "APROVADO: compilacao $(cat teste/aprovado) pode ser publicada"
  else
    echo "REPROVADO: corrija antes de publicar"
  fi
else
  echo "Para conferir as telas: sudo apt install python3-pil (ou rode tests/conferir.py num Python com Pillow)"
fi
