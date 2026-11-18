# ScrapCade OS: detalhes técnicos

O resumo para quem só quer jogar está no [README](../README.md).

## Emuladores e atalhos

O ScrapCade empacota o **RetroArch 1.22.2** (direto na placa de vídeo pelo KMS, sem X, menu RGUI em português) e estes núcleos libretro. Onde há dois, o segundo é o leve, que a versão de 32 bits usa (e que o painel deixa escolher em Jogando > Emulador do console):

| Console | Núcleo |
|---|---|
| NES (e Famicom Disk) | Mesen ou FCEUmm |
| Super Nintendo | Snes9x, bsnes ou Snes9x 2010 |
| Nintendo 64 | Mupen64Plus-Next |
| Game Boy, Game Boy Color | Gambatte |
| Game Boy Advance | mGBA ou gpSP |
| Nintendo DS (só 64 bits) | melonDS |
| Nintendo 3DS (só 64 bits) | Citra |
| GameCube e Wii (só 64 bits) | Dolphin |
| Virtual Boy | Beetle VB |
| SG-1000, Master System, Game Gear, Mega Drive, Sega CD | Genesis Plus GX ou PicoDrive |
| 32X | PicoDrive |
| Saturn (só 64 bits) | Beetle Saturn |
| Dreamcast (só 64 bits) | Flycast |
| PlayStation | PCSX ReARMed (tem BIOS livre embutida) |
| PlayStation 2 (só 64 bits) | LRPS2 |
| PSP (só 64 bits) | PPSSPP |
| PC Engine (e CD) | Beetle PCE Fast |
| Atari 2600, 5200, 7800, Lynx, Jaguar | Stella 2014, a5200, ProSystem, Handy, Virtual Jaguar |
| ColecoVision, Intellivision, Odyssey², Vectrex | Gearcoleco, FreeIntv, O2EM, vecx |
| 3DO, Neo Geo CD | Opera, NeoCD |
| Neo Geo Pocket, WonderSwan | Beetle NeoPop, Beetle Cygne |
| Fliperama e Neo Geo | FBNeo ou MAME 2003-Plus (Neo Geo precisa do `neogeo.zip` do usuário) |
| MSX, Commodore 64, Amiga, Atari ST | blueMSX, VICE, PUAE (com o AROS livre), Hatari (com o EmuTOS livre) |
| PC antigo (DOS), Doom, Quake, Cave Story | DOSBox Pure, PrBoom, TyrQuake, NXEngine |
| Aventuras e RPG Maker | ScummVM, EasyRPG |

Fora do RetroArch: **Half-Life** (Xash3D, com a cópia da Steam do usuário), os **ports do PortMaster** e, só no 64 bits, **Xbox** (xemu) e **PlayStation 3** (RPCS3). Esses dois abrem numa sessão gráfica só deles e também fecham com Select + Start (ou Esc segurado). As BIOS que cada console pede aparecem em *Sistema > BIOS dos consoles*.

- **Configuração central:** o `tos-jogar` passa a mesma configuração para todo emulador (vídeo, som, controles, atalhos e pastas). Para mudar algo só seu: `SCRAP-DADOS/system/configs/retroarch/retroarch-usuario.cfg`.
- **Atalhos iguais em todo jogo:** **Select + Start** sai, **Select + X** abre o menu, **Select + RB/LB** salva/carrega. No teclado: **Esc** sai, **F1** abre o menu. O ScrapCade mostra esses atalhos ao abrir cada jogo.
- **Nunca perder o progresso:** o jogo salva sozinho ao sair e continua de onde parou.
- **Controles:** reconhecidos pelo banco de dados do SDL2 (Xbox, PlayStation, Nintendo, genéricos), sem configurar.
- **Pastas na SCRAP-DADOS:** `saves/<console>`, `bios`, `screenshots`. O gerenciador da página mostra também `bios`, `saves` e `Registros` (logs, para pedir ajuda).

## Como o ScrapCade fica no disco

Parecido com o Batocera, mas pensado para não perder jogos:

| Partição | Formato | O que tem |
|---|---|---|
| **SCRAPCADE** | FAT32, 2 GB (1 GB nos pen drives gravados até a v0.20; tabela MBR, GRUB para UEFI e para BIOS) | GRUB, kernel, um initrd pequeno e o sistema inteiro em `boot/tos.squashfs` (somente leitura) |
| **SCRAP-DADOS** | exFAT, o resto do disco | `roms`, `bios`, `saves`, `themes`, `Jogos novos` e as configurações (`system`: senha, rede, ScrapCade) |

- **Sistema somente leitura:** o initrd monta o `tos.squashfs` com uma camada na memória por cima. Desligar da tomada não estraga o sistema.
- **A SCRAP-DADOS é criada no primeiro boot**, no espaço livre do pen drive ou do HD. Nada é redimensionado: a partição nova ocupa o espaço vazio.
- **Atualizar o ScrapCade OS** pelo painel Sistema (atualização A/B com volta automática) ou trocando `tos.squashfs`, `bzImage` e `initrd.gz` à mão (dá até pelo Windows, a SCRAPCADE é FAT32). A SCRAP-DADOS nunca é mexida. Nos pen drives com a SCRAPCADE de 1 GB a versão nova espera na SCRAP-DADOS e entra no boot seguinte; se ela não couber, o atualizador avisa para gravar o `.iso` de novo.
- **exFAT** para o Windows ler e gravar os jogos direto.

## Arquivos de cada versão

Cada versão fica em [Releases](https://github.com/thiagotechmaker/scrapcade.os/releases), já com a atualização automática dentro:

| Arquivo | Para quê |
|---|---|
| `scrapcade-os-v<versão>-pc-64bits.iso` | **o ScrapCade inteiro, um arquivo só**, para PC de 64 bits (BIOS antigo e UEFI) |
| `tos.squashfs`, `bzImage`, `initrd.gz` | usados pela atualização automática |
| `BUILD_ID` | número da compilação: o atualizador compara com o do PC para saber se a versão é mais nova |
| `SHA256SUMS` | confere todos os arquivos |

Desde a v0.20.0-beta, a mesma versão traz também os arquivos de 32 bits: `scrapcade-os-v<versão>-pc-32bits.iso`, `tos-32.squashfs`, `bzImage-32`, `initrd-32.gz` e `SHA256SUMS-32`.

Quem já tem o ScrapCade não precisa baixar nada: uns 40 s depois de ligar ele procura a versão nova e avisa na tela; a troca é em *Sistema > Atualizar o ScrapCade OS*. Para publicar uma versão compilada: `powershell -File scripts\publicar-versao.ps1`.

## Gravar e ligar

Um arquivo só: o `.iso`. Grave num pen drive de **4 GB ou mais** com o [Rufus](https://rufus.ie) (modo ISO ou modo DD, tanto faz) ou o [balenaEtcher](https://etcher.balena.io).

No **primeiro boot** o ScrapCade se copia para a memória e arruma o pen drive sozinho, do mesmo jeito qualquer que tenha sido a gravação:

| Partição | O que fica |
|---|---|
| **SCRAPCADE** (FAT32, 2 GB) | o sistema, somente leitura (atualização automática A/B) |
| **SCRAP-DADOS** (exFAT, o resto) | jogos, BIOS, saves, temas e perfis, sem limite de 4 GB por arquivo |

Daí em diante o ScrapCade **roda direto do pen drive**, como o Batocera. Para ligar sem o pen drive, quando quiser: *Sistema > Instalar no disco do computador* (copia o ScrapCade, e se quiser os jogos, para o HD ou SSD).

- Gravado em **DVD**, ou com `tos.vivo` acrescentado aos parâmetros no menu de boot (tecla Tab no BIOS, `e` no UEFI), o ScrapCade só roda, sem mexer em nada (os jogos ficam na memória até desligar).
- Precisa de 1,5 GB de memória para arrumar o pen drive; com menos, roda sem arrumar.

No notebook, ligue pelo pen drive (tecla do menu de boot: F12, F9, F8 ou Esc, conforme a marca). **Liga tanto por UEFI quanto por BIOS antigo (Legacy/CSM)**; se o notebook usar UEFI e recusar o pen drive, desligue o Secure Boot. Instalado no HD ou SSD, o ScrapCade liga direto na logo; segurando Esc ou Shift ao ligar, aparece o menu de boot (modo de compatibilidade, versão anterior, teste de memória).

O kernel traz os controladores USB 1.1/2.0/3.0, SATA, NVMe e leitor de cartão SD, para achar o pen drive e o HD também em máquinas antigas. As placas de vídeo (Intel, AMD, NVIDIA pelo Nouveau) são módulos: só a da máquina é carregada.

**Painel Sistema** (botão **Select** ou tecla **Tab**), em seções: Jogos, Jogando, Aparência, Controle e som, Tela e energia, Rede e Sistema (com **motor gráfico**, **modo desktop**, como jogar, informações, reiniciar e desligar). No alto aparecem o IP, o endereço da página e o uso de CPU, memória e disco.

**Modo desktop** (Sistema > Modo desktop): Fluxbox com um pacote leve de programas: **navegador** (MiniBrowser do WebKitGTK no 64 bits, NetSurf no 32), **arquivos** (PCManFM), **editor de texto** (L3afpad), **planilhas** (Gnumeric: `.xls`, `.xlsx`, `.ods`, `.csv`), **vídeos e músicas** (mpv), **imagens** (feh), **PDF** (MuPDF), **terminal** e **volume do som**. PDF, imagens, vídeos, músicas, textos e planilhas abrem com dois cliques no gerenciador de arquivos. O menu abre com o botão direito ou **Alt + F1**. **O que o navegador baixa vai para "Jogos novos"** e entra sozinho no console certo (o ScrapCade não traz endereço de jogos). "Voltar ao modo jogo" reabre o ScrapCade. LibreOffice não existe no Buildroot.

## Pen drive com jogos e central de downloads

Ligue um pen drive com jogos (FAT32, exFAT, NTFS ou ext4) com o ScrapCade aberto: ele é lido **somente leitura** e o que ainda não veio é copiado para o disco. Se o pen drive estiver arrumado como o ScrapCade (`roms/<console>/...` e `bios/`, como a SCRAP-DADOS de outro ScrapCade), cada arquivo vai direto para a mesma pasta, com as subpastas (CDs de várias faixas, Half-Life, ScummVM); jogos soltos vão para "Jogos novos" e o organizador leva cada um ao console certo. No fim aparece "Pode tirar". O ScrapCade lembra o que já copiou de cada pen drive.

A **central de downloads** (*Sistema > Downloads e cópias*) mostra tudo o que está chegando, com uma barra para cada um: a cópia do pen drive, os envios pela página do celular, os Jogos grátis e a versão nova do sistema. Ela abre sozinha nas cópias grandes (200 MB ou mais); enquanto algo chega, uma seta com o número aparece na barra de cima.

## Instalar no computador

**Sistema > Instalar no disco do computador**: escolha o disco (HD, SSD ou NVMe; o pen drive do ScrapCade OS não aparece na lista), confirme (o disco é **apagado**) e escolha se leva os jogos, saves, BIOS e configurações. O instalador cria SCRAPCADE e SCRAP-DADOS no disco e registra o ScrapCade no menu de boot da placa-mãe. Depois é só desligar, tirar o pen drive e ligar.

## O que cada parte faz

| Parte | O que faz |
|---|---|
| **GRUB 2 e syslinux** | Primeiros programas que rodam quando o PC liga (GRUB no UEFI e no disco instalado, syslinux no BIOS antigo). Mostram o menu de boot, acham o kernel na partição SCRAPCADE e o iniciam. |
| **Kernel Linux 6.12.47 (LTS)** | Coração do sistema: conversa com o processador, a memória, os discos, a placa de rede e o vídeo. A versão LTS recebe correções por anos. |
| **Compilador Bootlin (glibc)** | Conjunto de compiladores pronto, usado para compilar o sistema todo. Economiza cerca de 30 minutos por compilação. |
| **BusyBox** | Reúne em um só programa pequeno o `init` (primeiro processo, que inicia os outros), o terminal e os comandos básicos (`ls`, `cp`, `mount`...). |
| **eudev** | Detecta o hardware ligado (pen drive, controle, placa de rede) e cria os arquivos de dispositivo em `/dev`. |
| **D-Bus** | Canal de comunicação entre os serviços do sistema. |
| **ConnMan** | Gerencia a rede: cabo e Wi-Fi. |
| **linux-firmware** | Firmwares das placas de Wi-Fi e rede (Intel, Realtek, Atheros, Ralink). Sem eles, essas placas não funcionam. |
| **syslogd / klogd** | Guardam os registros do sistema e do kernel, para descobrir problemas. |
| **acpid** | Trata o botão de ligar e eventos de energia (desligar ao apertar o botão). |
| **crond** | Executa tarefas agendadas. |
| **Senha do administrador** | O boot não pede senha: o administrador fica bloqueado até a senha ser criada no painel Sistema, ao ligar o SSH (`S01primeiro-boot` restaura a senha guardada). |
| **ScrapCade** | Menu de jogos (modo jogo), em C++ com SDL2. Abre no boot pelo serviço `S99scrapcade`. |
| **Mesa + LLVM** | Drivers de vídeo abertos (Intel, AMD, NVIDIA/Nouveau) e o `llvmpipe` para PC sem placa de vídeo. |
| **SDL2** | Base do ScrapCade: tela pela GPU (KMS/DRM, sem servidor gráfico), controles, imagens PNG/JPEG e fontes. |
| **DejaVu Sans** | Fonte usada no menu (só as variantes Sans, para economizar espaço). |
| **genimage e xorriso** | Montam o `disk.img` (tabela MBR e a partição SCRAPCADE, FAT32 de 2 GB, com o GRUB, o kernel e o `tos.squashfs`) e o `.iso`. |

## Como foi feito

1. **Ambiente:** Ubuntu 24.04 no WSL2 (Windows), com as ferramentas que o Buildroot exige (`build-essential`, `git`, `rsync`, `bc`, `cpio`, `unzip`, `wget`, `file`, `python3`, `libncurses-dev`...).
2. **Buildroot 2026.02.3 (LTS)**, baixado do repositório oficial.
3. **Árvore externa própria** (`external/`): tudo que é do ScrapCade fica separado do Buildroot. Assim dá para atualizar o Buildroot sem perder as mudanças.
4. **Configurações `tos_x86_64_defconfig` e `tos_i686_defconfig`**, partindo da configuração de PC do Buildroot, com:
   - nome, versão e hostname do ScrapCade (`/etc/os-release`, `/etc/issue`);
   - compilador estável da Bootlin;
   - sistema em `tos.squashfs` numa partição FAT32 de 2 GB, com initrd próprio;
   - menu de boot próprio (GRUB e syslinux) com fundo e logo do ScrapCade OS;
   - boot silencioso (`quiet`), sem os textos do kernel na tela;
   - boot sem texto na tela (`tos-rcs`) e senha do administrador criada no painel, ao ligar o SSH.
5. **Compilação:** algumas horas na primeira vez (baixa e compila tudo, inclusive o LLVM), minutos nas seguintes.
6. **Teste:** bateria automática em máquinas virtuais QEMU, com BIOS antigo (SeaBIOS) e UEFI (OVMF).

## Estrutura, compilação e testes

A estrutura de pastas, os pacotes que o computador precisa e os comandos de compilação estão no [README](../README.md#estrutura-de-pastas). Para ligar a imagem pronta numa máquina virtual, sem gravar pen drive:

```bash
sudo apt install -y qemu-system-x86 ovmf
qemu-system-x86_64 -m 2048 -bios /usr/share/ovmf/OVMF.fd \
  -drive file=$HOME/scrapcade.os/output/x86_64/images/disk.img,format=raw,if=virtio -snapshot
```

A bateria automática (`bash tests/testar.sh`) faz isso em BIOS e UEFI, passa por cada tela e opção e compara com as telas de referência em `tests/referencias`. Só uma compilação aprovada por ela pode ser publicada (`scripts/publicar-versao.ps1`).

## Capas, dados e organização automática

- **Pasta "Jogos novos"** (`/userdata/Jogos novos`): o jogo copiado ali vai sozinho para o console certo. Vale ROM solta, `.zip` com a ROM dentro, CD (`.cue` + `.bin`: PlayStation, Saturn, Sega CD, PC Engine CD ou 3DO pelo começo da primeira faixa, com as faixas junto), Neo Geo (a BIOS que vier junto vira `neogeo.zip`), arcade do FBNeo e imagens de disco (`.iso`, `.rvz`: GameCube, Wii, PSP, PS1, PS2, Xbox ou 3DO pelo cabeçalho do disco). Jogos que são pastas (ScummVM, RPG Maker, PS3) ganham um atalho no menu sozinhos. Arquivo que chegou há menos de 10 s espera a próxima volta (o `.bin` e o `.cue` do mesmo disco chegam juntos). O que não for reconhecido vai para `nao_reconhecidos`. Ainda não reconhecido: `.chd` (vai para o PlayStation; o de outro console vai na pasta dele à mão).
- **Dados do jogo pelo CRC**, sem rede: nome oficial, ano, empresa e gênero, mostrados embaixo da capa e no painel de detalhes (botão **Y**).
- **Capa e fundo baixados sozinhos** do acervo [libretro-thumbnails](https://thumbnails.libretro.com), sem conta; quando o nome exato não existe, usa o mais próximo da lista do console. Ficam guardados em `roms/<console>/images`, e dali em diante aparecem sem internet. O que o acervo não tem fica anotado para não tentar de novo; sem rede, tenta outra vez a cada minuto.
- Registro do ScrapCade em `/userdata/system/logs/scrapcade.log`.

