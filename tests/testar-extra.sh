#!/bin/bash
# Parte 2 dos testes automaticos (chamada pelo tests/testar.sh): pen drive USB com jogo e imagem (navegador de
# arquivos, tema proprio), criar e apagar perfil, pergunta de instalar no HD e atualizacao de ponta a ponta com
# um servidor falso. Usa as funcoes e variaveis do testar.sh (k, shot, senha, opt, fecha, log, Q, EFI, I, TOS).

usbpen() {   # pen drive de jogos (FAT32) com um jogo numa pasta e uma imagem para tema
  rm -f "$1"; truncate -s 64M "$1"; mkfs.vfat -n JOGOS "$1" >/dev/null
  mmd -i "$1" ::/jogos
  mcopy -i "$1" "$TOS/tests/roms/nes/Aventura de Teste.nes" ::/jogos/
  mcopy -i "$1" "$TOS/tests/roms/n64/images/Jogo de Teste 64.png" ::/fundo-praia.png
}

# ---------------------------------------------------------------- pen drive: navegador, tema proprio, perfis
for tentativa in 1 2; do   # o QEMU as vezes sai sozinho no inicio: uma segunda tentativa
  D=teste/x-pen.img; cp $I/disk.img $D; truncate -s 3G $D
  U=teste/x-usb.img; usbpen $U
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    opt 1; k ret; sleep 4; shot x_nav_raiz
    k ret; sleep 3; k down; k ret; sleep 2; k down; sleep 1; shot x_nav_jogo; k ret; sleep 15; shot x_nav_copiou
    k esc; sleep 1; k esc; sleep 1; k esc; sleep 1; k esc; sleep 20; shot x_carrossel_pen
    opt 4; k ret; sleep 3; k ret; sleep 3; k down down; sleep 1; k ret; sleep 8; shot x_tema_proprio
    opt 0; k ret; sleep 3; k right; k ret; sleep 3; k m a r i a; k ret; sleep 3; k ret; sleep 3; k ret; sleep 15
    opt 0; k ret; sleep 3; shot x_perfis_dois
    k delete; sleep 2; shot x_perfil_confirma; k ret; sleep 4; shot x_perfil_apagado
    echo quit ) | timeout 700 $Q $EFI -drive file=$D,format=raw,if=virtio \
      -device qemu-xhci -drive if=none,id=pen,file=$U,format=raw -device usb-storage,drive=pen \
      -nic user,model=e1000 > teste/q8.log 2>&1
  log "pen drive: QEMU saiu com ${PIPESTATUS[1]} (tentativa $tentativa)"
  [ -f teste/z_x_nav_raiz.ppm ] && break
done

# ---------------------------------------------------------------- rodando de um pen drive USB com HD vazio: pergunta
D=teste/x-pen-usb.img; cp $I/disk.img $D; truncate -s 3G $D
H=teste/x-hd.img; rm -f $H; truncate -s 8G $H
( sleep 40; senha; sleep 60
  k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 30
  shot x_pergunta_hd; k ret; sleep 4; shot x_discos; fecha
  echo quit ) | timeout 400 $Q $EFI -device qemu-xhci \
    -drive if=none,id=pen,file=$D,format=raw -device usb-storage,drive=pen,bootindex=0 \
    -drive file=$H,format=raw,if=virtio > teste/q9.log 2>&1

# ---------------------------------------------------------------- atualizacao de ponta a ponta (servidor falso)
F=/tmp/scrapcade-ota; rm -rf $F; mkdir -p $F; cp $I/atualizacao/{tos.squashfs,bzImage,initrd.gz,SHA256SUMS} $F/
sz=$(stat -c %s $F/tos.squashfs)
cat > $F/latest <<J
{ "tag_name": "v999999999999", "assets": [
 { "name": "tos.squashfs", "size": $sz, "browser_download_url": "http://10.0.2.2:18099/tos.squashfs" },
 { "name": "bzImage", "size": 1, "browser_download_url": "http://10.0.2.2:18099/bzImage" },
 { "name": "initrd.gz", "size": 1, "browser_download_url": "http://10.0.2.2:18099/initrd.gz" },
 { "name": "SHA256SUMS", "size": 1, "browser_download_url": "http://10.0.2.2:18099/SHA256SUMS" } ] }
J
( cd $F && exec python3 -m http.server 18099 --bind 127.0.0.1 > /tmp/scrapcade-ota.log 2>&1 ) & SRV=$!
for tentativa in 1 2; do   # o QEMU as vezes cai ao reiniciar depois de outros testes: uma segunda tentativa
  D=teste/x-ota.img; cp $I/disk.img $D; truncate -s 3G $D
  O=$(( $(fdisk -l -o Start $D | tail -1 | tr -d ' ') * 512 ))   # disk.img novo: so a particao do sistema
  echo "http://10.0.2.2:18099/latest" > /tmp/srvt; mcopy -i $D@@$O /tmp/srvt ::boot/tos-servidor-teste
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 30
    sleep 150; shot x_ota_aviso                      # baixou sozinha em segundo plano (Atualizar sozinho)
    opt 19; k ret; sleep 20; shot x_ota_achou        # "ja foi baixada: reinicie"
    rm -f $F/latest                                  # o servidor falso para de anunciar (como depois de publicar)
    k ret; sleep 200; shot x_ota_reiniciou
    echo quit ) | timeout 900 $Q $EFI -drive file=$D,format=raw,if=virtio -nic user,model=e1000 > teste/q10.log 2>&1
  mdir -b -i $D@@$O ::boot 2>/dev/null | grep -q tos-testando || break
  log "atualizacao: tentativa $tentativa sem confirmar, repetindo"
done
kill $SRV 2>/dev/null
log "ota baixou: $(grep -c '"GET /tos.squashfs HTTP/1.1" 200' /tmp/scrapcade-ota.log)"
B=$(mdir -b -i $D@@$O ::boot 2>/dev/null)
log "ota anterior guardada: $(echo "$B" | grep -q tos-anterior.squashfs && echo sim || echo nao)"
log "ota confirmada: $(echo "$B" | grep -q tos-testando && echo nao || echo sim)"

# ---------------------------------------------------------------- pen drive + HD com o ScrapCade instalado: tem que ligar o do PEN
# (o sistema do "HD" e estragado de proposito: se ligar por ele, a tela nao chega ao ScrapCade)
H=teste/x-hd-instalado.img; cp $I/disk.img $H; truncate -s 3G $H
O=$(( $(fdisk -l -o Start $H | tail -1 | tr -d ' ') * 512 ))
head -c 1048576 /dev/zero > /tmp/estragado; mcopy -o -i $H@@$O /tmp/estragado ::boot/tos.squashfs
mlabel -i $H@@$O -N 1A2B3C4D ::SCRAPCADE   # instalacao de verdade: o HD tem identificacao (UUID) propria
for modo in uefi bios; do
  P=teste/x-pen-$modo.img; cp $I/disk.img $P; truncate -s 3G $P
  [ $modo = uefi ] && FW="$EFI" || FW=""
  ( sleep 40; senha; sleep 60; shot x_pen_e_hd_$modo; echo quit ) | timeout 300 $Q $FW \
      -drive file=$H,format=raw,if=virtio -device qemu-xhci \
      -drive if=none,id=pen,file=$P,format=raw -device usb-storage,drive=pen,bootindex=0 > teste/q11-$modo.log 2>&1
done

# ---------------------------------------------------------------- jogos gratis (baixa de verdade), copia dos saves, gerenciador
D=teste/x-gratis.img; cp $I/disk.img $D; truncate -s 3G $D
U=teste/x-usb2.img; usbpen $U
( sleep 30; senha; sleep 60
  k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
  opt 22; k ret; sleep 3; shot x_gratis
  k ret; sleep 40; k down; k ret; sleep 40; k down; k ret; sleep 40; shot x_gratis_baixou
  k esc; sleep 2; k esc; sleep 45; shot x_carrossel_gratis
  opt 23; k ret; sleep 25; shot x_copia_saves; fecha
  opt 2; k ret; sleep 3; k ret; sleep 3; k down; sleep 1; k x; sleep 2; shot x_arq_acoes
  k ret; sleep 2; k esc; sleep 2; k down; k ret; sleep 3; k x; sleep 2; shot x_arq_colar
  k ret; sleep 10; shot x_arq_colou
  echo quit ) | timeout 900 $Q $EFI -drive file=$D,format=raw,if=virtio \
    -device qemu-xhci -drive if=none,id=pen,file=$U,format=raw -device usb-storage,drive=pen \
    -nic user,model=e1000 > teste/q12.log 2>&1
log "copia dos saves no pen drive: $(mdir -/ -b -i $U ::ScrapCade-saves 2>/dev/null | grep -c saves)"

# ---------------------------------------------------------------- PortMaster: baixa a versao oficial e abre
D=teste/x-portmaster.img; cp $I/disk.img $D; truncate -s 3G $D
( sleep 30; senha; sleep 60
  k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
  shot x_ports_carrossel; k ret; sleep 30; shot x_portmaster_baixando; sleep 120; shot x_portmaster
  echo quit ) | timeout 600 $Q $EFI -drive file=$D,format=raw,if=virtio -nic user,model=e1000 > teste/q13.log 2>&1
