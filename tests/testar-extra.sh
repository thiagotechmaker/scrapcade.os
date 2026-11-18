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
    opt 3; k ret; sleep 4; shot x_nav_raiz
    k ret; sleep 3; k ret; sleep 3; shot x_nav_jogo; k ret; sleep 15; shot x_nav_copiou
    k esc; sleep 1; k esc; sleep 1; k esc; sleep 1; k esc; sleep 20; shot x_carrossel_pen
    # (a lista do pen drive demora a montar com a maquina ocupada: o A de 1 s depois as vezes se perdia)
    opt 22; k ret; sleep 3; k ret; sleep 4; k down; sleep 2; k ret; sleep 12; shot x_tema_proprio
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
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-pen-usb.img; cp $I/disk.img $D; truncate -s 3G $D
  H=teste/x-hd.img; rm -f $H; truncate -s 8G $H
  ( sleep 40; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 30
    shot x_pergunta_hd; k ret; sleep 4; shot x_discos; fecha
    echo quit ) | timeout 400 $Q $EFI -device qemu-xhci \
      -drive if=none,id=pen,file=$D,format=raw -device usb-storage,drive=pen,bootindex=0 \
      -drive file=$H,format=raw,if=virtio > teste/q9.log 2>&1
  [ -f teste/z_x_discos.ppm ] && break
  log "QEMU caiu antes de x_discos (tentativa $tentativa), repetindo"
done

# ---------------------------------------------------------------- instalar ao lado do Windows (disco que imita o Windows)
# GPT com particao EFI (o "Windows" e o teste de memoria no lugar do bootmgfw.efi), MSR e uma de dados; o resto livre.
# UEFI com variaveis guardadas (pflash), para a entrada "ScrapCade OS" da placa-mae valer no boot seguinte.
W=teste/x-windows.img; rm -f $W; truncate -s 16G $W
printf 'label: gpt\nsize=200MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B\nsize=16MiB, type=E3C9E316-0B5C-4DB8-817D-F92DF00215AE\nsize=3GiB, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7\n' | sfdisk -q $W
E=teste/x-windows-esp.img; rm -f $E; truncate -s 200M $E; mkfs.vfat -F 32 -n SYSTEM $E >/dev/null
mmd -i $E ::/EFI ::/EFI/Microsoft ::/EFI/Microsoft/Boot ::/EFI/Boot
mcopy -i $E $I/memtest.efi ::/EFI/Microsoft/Boot/bootmgfw.efi; mcopy -i $E $I/memtest.efi ::/EFI/Boot/bootx64.efi
dd if=$E of=$W bs=1M seek=1 conv=notrunc status=none
cp /usr/share/OVMF/OVMF_VARS_4M.fd teste/x-windows-vars.fd
UEFI="-drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd -drive if=pflash,format=raw,file=teste/x-windows-vars.fd"
D=teste/x-pen-lado.img; cp $I/disk.img $D; truncate -s 3G $D
( sleep 40; senha; sleep 60
  k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 30
  k ret; sleep 4; k ret; sleep 3; shot x_lado_confirma
  k ret; sleep 90; shot x_lado_pronto
  echo quit ) | timeout 400 $Q $UEFI -device qemu-xhci \
    -drive if=none,id=pen,file=$D,format=raw -device usb-storage,drive=pen,bootindex=0 \
    -drive file=$W,format=raw,if=virtio > teste/q9b.log 2>&1
log "ao lado do Windows: $(sfdisk -d $W | grep -c '^teste') particoes (eram 3)"
# boot so com o disco: menu de 3 s com ScrapCade e Windows; escolhe Windows; no boot seguinte ele vem marcado
( sleep 2; echo "sendkey up"; sleep 1; shot x_lado_menu; k down; sleep 1; k ret; sleep 12; shot x_lado_windows
  echo system_reset; sleep 2.5; shot x_lado_ultimo
  echo quit ) | timeout 120 $Q $UEFI -drive file=$W,format=raw,if=virtio > teste/q9c.log 2>&1

# ---------------------------------------------------------------- atualizacao de ponta a ponta (servidor falso)
F=/tmp/scrapcade-ota; rm -rf $F; mkdir -p $F; cp $I/atualizacao/{tos.squashfs,bzImage,initrd.gz,SHA256SUMS} $F/
sz=$(stat -c %s $F/tos.squashfs)
echo 999999999999 > $F/BUILD_ID   # como no GitHub: tag SemVer e o numero da compilacao num arquivo da versao
cat > $F/latest <<J
{ "tag_name": "v99.0.0-beta", "assets": [
 { "name": "BUILD_ID", "size": 13, "browser_download_url": "http://10.0.2.2:18099/BUILD_ID" },
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
    sleep 150; shot x_ota_avisou                     # de fabrica so avisa que ha versao nova (nao baixa sozinho)
    opt 43; k ret; sleep 20; shot x_ota_versao_nova  # Atualizar: mostra a versao nova
    k ret; sleep 180; shot x_ota_baixou              # A: baixa, confere e troca (a de agora fica como anterior)
    rm -f $F/latest                                  # o servidor falso para de anunciar (como depois de publicar)
    k ret; sleep 200; shot x_ota_reiniciou           # A: reinicia na versao nova
    echo quit ) | timeout 900 $Q $EFI -drive file=$D,format=raw,if=virtio -nic user,model=e1000 > teste/q10.log 2>&1
  mdir -b -i $D@@$O ::boot 2>/dev/null | grep -q tos-testando || break
  log "atualizacao: tentativa $tentativa sem confirmar, repetindo"
done
kill $SRV 2>/dev/null
log "ota baixou: $(grep -c '"GET /tos.squashfs HTTP/1.1" 200' /tmp/scrapcade-ota.log)"
B=$(mdir -b -i $D@@$O ::boot 2>/dev/null)
# pen drive de 1 GB (como os antigos): a anterior vai para a SCRAP-DADOS e fica um aviso no lugar (tos-anterior-nos-dados)
log "ota anterior guardada: $(echo "$B" | grep -qE 'tos-anterior.squashfs|tos-anterior-nos-dados' && echo sim || echo nao)"
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
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-gratis.img; cp $I/disk.img $D; truncate -s 3G $D
  U=teste/x-usb2.img; usbpen $U
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    opt 1; k ret; sleep 3; shot x_gratis
    k ret; sleep 40; k down; k ret; sleep 40; k down; k ret; sleep 40; shot x_gratis_baixou
    k esc; sleep 2; k esc; sleep 45; shot x_carrossel_gratis
    opt 11; k ret; sleep 25; shot x_copia_saves; fecha
    opt 4; k ret; sleep 3; shot x_arq_raizes; k ret; sleep 3; k delete; sleep 2; shot x_arq_acoes
    k ret; sleep 2; k esc; sleep 2; k down; k ret; sleep 3; shot x_arq_colar
    k x; sleep 10; shot x_arq_colou; k esc; sleep 1; fecha   # 3 Esc: pasta, raizes, painel
    opt 5; k ret; sleep 3; shot x_ejetou
    echo quit ) | timeout 900 $Q $EFI -drive file=$D,format=raw,if=virtio \
      -device qemu-xhci -drive if=none,id=pen,file=$U,format=raw -device usb-storage,drive=pen \
      -nic user,model=e1000 > teste/q12.log 2>&1
  [ -f teste/z_x_ejetou.ppm ] && break
  log "QEMU caiu antes de x_ejetou (tentativa $tentativa), repetindo"
done
log "copia dos saves no pen drive: $(mdir -/ -b -i $U ::ScrapCade-saves 2>/dev/null | grep -c saves)"

# ---------------------------------------------------------------- PortMaster: baixa a versao oficial e abre
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-portmaster.img; cp $I/disk.img $D; truncate -s 3G $D
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    shot x_ports_carrossel; k ret; sleep 30; shot x_portmaster_baixando; sleep 120; shot x_portmaster
    echo quit ) | timeout 600 $Q $EFI -drive file=$D,format=raw,if=virtio -nic user,model=e1000 > teste/q13.log 2>&1
  [ -f teste/z_x_portmaster.ppm ] && break
  log "QEMU caiu antes de x_portmaster (tentativa $tentativa), repetindo"
done

# ---------------------------------------------------------------- jogo que nao abre (aviso) e Freedoom no PrBoom
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-doom.img; cp $I/disk.img $D; truncate -s 3G $D
  head -c 1048576 /dev/zero > "/tmp/Jogo Quebrado.z64"   # N64 de mentira: o emulador fecha na hora
  W=http://127.0.0.1:18082   # porta propria: a parte principal pode rodar ao mesmo tempo
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    log "envio Jogo Quebrado.z64: $(curl -g -s -m 30 -T "/tmp/Jogo Quebrado.z64" "$W/envio/Jogo%20Quebrado.z64" -o /dev/null -w '%{http_code}')"
    sleep 40; k right; sleep 3; shot x_quebrado_sel
    k ret; sleep 12; shot x_nao_abriu; sleep 10
    opt 1; k ret; sleep 3; k down down down; sleep 1; k ret; sleep 120; shot x_freedoom_baixou
    # central de downloads (v0.22): a opcao logo abaixo de Jogos gratis mostra o Freedoom que acabou de chegar
    k esc; sleep 2; k down; sleep 1; k ret; sleep 3; shot x_central; k esc; sleep 2
    k esc; sleep 45; k right; sleep 3; shot x_freedoom_carrossel
    k ret; sleep 30; shot x_freedoom_jogando; echo "sendkey esc 1000"; sleep 60
    k c; sleep 5; shot x_troca_rapida   # Start/C: Continuar jogando com a foto do ponto salvo
    echo quit ) | timeout 700 $Q $EFI -drive file=$D,format=raw,if=virtio \
      -nic user,model=e1000,hostfwd=tcp:127.0.0.1:18082-:8080 > teste/q14.log 2>&1
  [ -f teste/z_x_freedoom_jogando.ppm ] && break
  log "QEMU caiu antes de x_freedoom_jogando (tentativa $tentativa), repetindo"
done

# ---------------------------------------------------------------- painel em secoes: JOGANDO, ordenar, relogio, padrao
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-painel.img; cp $I/disk.img $D; truncate -s 3G $D
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    opt 13; sleep 1; shot x_painel_jogando; k right right; sleep 1; shot x_painel_filtro_tv; fecha
    opt 6; k right; sleep 2; shot x_painel_ordenar; k left; sleep 1; fecha
    opt 35; k right; sleep 2; fecha; sleep 2; shot x_relogio_12h
    opt 51; k ret; sleep 2; shot x_padrao_confirma; k esc; sleep 1; fecha
    echo system_powerdown; sleep 5; shot x_menu_energia; k esc; sleep 2   # botao de energia: menu
    echo quit ) | timeout 400 $Q $EFI -drive file=$D,format=raw,if=virtio > teste/q15.log 2>&1
  [ -f teste/z_x_padrao_confirma.ppm ] && break
  log "QEMU caiu antes de x_padrao_confirma (tentativa $tentativa), repetindo"
done

# ---------------------------------------------------------------- protecao de tela (5 min parado) e o botao que acorda
for tentativa in 1 2; do   # o QEMU as vezes cai (segfault): uma segunda tentativa
  D=teste/x-protecao.img; cp $I/disk.img $D; truncate -s 3G $D
  ( sleep 30; senha; sleep 60
    k ret; sleep 3; k t h i a g o; k ret; sleep 3; k ret; sleep 3; k ret; sleep 20
    sleep 320; shot x_protecao_tela                  # de fabrica a protecao entra com 5 min parado
    k down; sleep 3; shot x_protecao_acordou
    echo quit ) | timeout 700 $Q $EFI -drive file=$D,format=raw,if=virtio > teste/q16.log 2>&1
  [ -f teste/z_x_protecao_acordou.ppm ] && break
  log "QEMU caiu antes de x_protecao_acordou (tentativa $tentativa), repetindo"
done
