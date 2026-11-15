# ScrapCade OS: detalhes técnicos

O resumo para quem só quer jogar está no [README](../README.md).

## Emuladores e atalhos

O ScrapCade empacota o **RetroArch 1.22.2** (direto na placa de vídeo pelo KMS, sem X, menu RGUI em português) e estes núcleos libretro:

| Console | Núcleo |
|---|---|
| NES | Mesen |
| Super Nintendo | Snes9x (ou bsnes, trocando no painel de detalhes com X) |
| Mega Drive, Master System, Game Gear | Genesis Plus GX |
| Game Boy, Game Boy Color | Gambatte |
| Game Boy Advance | mGBA |
| PlayStation | PCSX ReARMed (tem BIOS livre embutida) |
| Nintendo 64 | Mupen64Plus-Next |
| Fliperama e Neo Geo | FBNeo (Neo Geo precisa do `neogeo.zip` do usuário) |
| PSP | PPSSPP |
| Nintendo DS | melonDS (precisa das BIOS do DS do usuário) |
| Sega Saturn | Beetle Saturn (precisa da BIOS do usuário) |
| Dreamcast | Flycast (BIOS do usuário recomendada) |

- **Configuração central:** o `tos-jogar` passa a mesma configuração para todo emulador (vídeo, som, controles, atalhos e pastas). Para mudar algo só seu: `SCRAP-DADOS/system/configs/retroarch/retroarch-usuario.cfg`.
- **Atalhos iguais em todo jogo:** **Select + Start** sai, **Select + X** abre o menu, **Select + RB/LB** salva/carrega. No teclado: **Esc** sai, **F1** abre o menu. O ScrapCade mostra esses atalhos ao abrir cada jogo.
- **Nunca perder o progresso:** o jogo salva sozinho ao sair e continua de onde parou.
- **Controles:** reconhecidos pelo banco de dados do SDL2 (Xbox, PlayStation, Nintendo, genéricos), sem configurar.
- **Pastas na SCRAP-DADOS:** `saves/<console>`, `bios`, `screenshots`. O gerenciador da página mostra também `bios`, `saves` e `Registros` (logs, para pedir ajuda).

## Como o ScrapCade fica no disco

Parecido com o Batocera, mas pensado para não perder jogos:

| Partição | Formato | O que tem |
|---|---|---|
| **SCRAPCADE** | FAT32, 1 GB (tabela MBR, GRUB para UEFI e para BIOS) | GRUB, kernel, um initrd pequeno e o sistema inteiro em `boot/tos.squashfs` (somente leitura) |
| **SCRAP-DADOS** | exFAT, o resto do disco | `roms`, `bios`, `saves`, `themes`, `Jogos novos` e as configurações (`system`: senha, rede, ScrapCade) |

- **Sistema somente leitura:** o initrd monta o `tos.squashfs` com uma camada na memória por cima. Desligar da tomada não estraga o sistema.
- **A SCRAP-DADOS é criada no primeiro boot**, no espaço livre do pen drive ou do HD. Nada é redimensionado: a partição nova ocupa o espaço vazio.
- **Atualizar o ScrapCade OS** pelo painel Sistema (atualização A/B com volta automática) ou trocando o `tos.squashfs` à mão (dá até pelo Windows, a SCRAPCADE é FAT32). A SCRAP-DADOS nunca é mexida.
- **exFAT** para o Windows ler e gravar os jogos direto.

## Arquivos de cada versão

Cada versão fica em [Releases](https://github.com/thiagotechmaker/scrapcade.os/releases), já com a atualização automática dentro:

| Arquivo | Para quê |
|---|---|
| `scrapcade-os-v<versão>-pc-64bits.iso` | **o ScrapCade inteiro, um arquivo só**, para PC de 64 bits (BIOS antigo e UEFI) |
| `tos.squashfs`, `bzImage`, `initrd.gz` | usados pela atualização automática |
| `SHA256SUMS` | confere todos os arquivos |

A partir da v0.20.0-beta, a mesma versão traz também os arquivos de 32 bits: `scrapcade-os-v<versão>-pc-32bits.iso`, `tos-32.squashfs`, `bzImage-32`, `initrd-32.gz` e `SHA256SUMS-32`.

Quem já tem o ScrapCade não precisa baixar nada: uns 40 s depois de ligar ele procura a versão nova e avisa na tela; a troca é em *Sistema > Atualizar o ScrapCade OS*. Para publicar uma versão compilada: `powershell -File scripts\publicar-versao.ps1`.

## Gravar e ligar

Um arquivo só: o `.iso`. Grave num pen drive de **4 GB ou mais** com o [Rufus](https://rufus.ie) (modo ISO ou modo DD, tanto faz) ou o [balenaEtcher](https://etcher.balena.io).

No **primeiro boot** o ScrapCade se copia para a memória e arruma o pen drive sozinho, do mesmo jeito qualquer que tenha sido a gravação:

| Partição | O que fica |
|---|---|
| **SCRAPCADE** (FAT32, 1 GB) | o sistema, somente leitura (atualização automática A/B) |
| **SCRAP-DADOS** (exFAT, o resto) | jogos, BIOS, saves, temas e perfis, sem limite de 4 GB por arquivo |

Daí em diante o ScrapCade **roda direto do pen drive**, como o Batocera. Para ligar sem o pen drive, quando quiser: *Sistema > Instalar no disco do computador* (copia o ScrapCade, e se quiser os jogos, para o HD ou SSD).

- Gravado em **DVD**, ou com `tos.vivo` acrescentado aos parâmetros no menu de boot (tecla Tab no BIOS, `e` no UEFI), o ScrapCade só roda, sem mexer em nada (os jogos ficam na memória até desligar).
- Precisa de 1,5 GB de memória para arrumar o pen drive; com menos, roda sem arrumar.

No notebook, ligue pelo pen drive (tecla do menu de boot: F12, F9, F8 ou Esc, conforme a marca). **Liga tanto por UEFI quanto por BIOS antigo (Legacy/CSM)**; se o notebook usar UEFI e recusar o pen drive, desligue o Secure Boot. No primeiro boot o ScrapCade pede a senha do administrador.

O kernel traz os controladores USB 1.1/2.0/3.0, SATA, NVMe e leitor de cartão SD, para achar o pen drive e o HD também em máquinas antigas. As placas de vídeo (Intel, AMD, NVIDIA pelo Nouveau) são módulos: só a da máquina é carregada.

**Painel Sistema** (botão **Select** ou tecla **Tab**), em seções: Jogos, Jogando, Aparência, Controle e som, Tela e energia, Rede e Sistema (com **motor gráfico**, **modo desktop**, como jogar, informações, reiniciar e desligar). No alto aparecem o IP, o endereço da página e o uso de CPU, memória e disco.

**Modo desktop** (Sistema > Modo desktop): Fluxbox com um pacote leve de programas: **navegador** (NetSurf), **arquivos** (PCManFM), **terminal**, **editor de texto** (nano), **vídeos e músicas** (mpv), **imagens** (feh), **PDF** (MuPDF) e **volume do som**. O menu abre com o botão direito ou **Alt + F1**. **O que o navegador baixa vai para "Jogos novos"** e entra sozinho no console certo (o ScrapCade não traz endereço de jogos). "Voltar ao modo jogo" reabre o ScrapCade. LibreOffice não existe no Buildroot.

## Pen drive com jogos

Ligue um pen drive com jogos (FAT32, exFAT, NTFS ou ext4) com o ScrapCade aberto: ele é lido **somente leitura**, os jogos que ainda não vieram são copiados para "Jogos novos" e vão sozinhos para o console certo. No fim aparece "Pode tirar". O ScrapCade lembra o que já copiou de cada pen drive.

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
| **Primeiro boot** | Script `S01primeiro-boot`: se o administrador ainda não tem senha, pede para criar uma antes do login. |
| **ScrapCade** | Menu de jogos (modo jogo), em C++ com SDL2. Abre no boot pelo serviço `S99scrapcade`. |
| **Mesa + LLVM** | Drivers de vídeo abertos (Intel, AMD, NVIDIA/Nouveau) e o `llvmpipe` para PC sem placa de vídeo. |
| **SDL2** | Base do ScrapCade: tela pela GPU (KMS/DRM, sem servidor gráfico), controles, imagens PNG/JPEG e fontes. |
| **DejaVu Sans** | Fonte usada no menu (só as variantes Sans, para economizar espaço). |
| **genimage e xorriso** | Montam o `disk.img` (tabela MBR e a partição SCRAPCADE, FAT32 de 1 GB, com o GRUB, o kernel e o `tos.squashfs`) e o `.iso`. |

## Como foi feito

1. **Ambiente:** Ubuntu 24.04 no WSL2 (Windows), com as ferramentas que o Buildroot exige (`build-essential`, `git`, `rsync`, `bc`, `cpio`, `unzip`, `wget`, `file`, `python3`, `libncurses-dev`...).
2. **Buildroot 2026.02.3 (LTS)**, baixado do repositório oficial.
3. **Árvore externa própria** (`external/`): tudo que é do ScrapCade fica separado do Buildroot. Assim dá para atualizar o Buildroot sem perder as mudanças.
4. **Configurações `tos_x86_64_defconfig` e `tos_i686_defconfig`**, partindo da configuração de PC do Buildroot, com:
   - nome, versão e hostname do ScrapCade (`/etc/os-release`, `/etc/issue`);
   - compilador estável da Bootlin;
   - sistema em `tos.squashfs` numa partição FAT32 de 1 GB, com initrd próprio;
   - menu de boot próprio (GRUB e syslinux) com fundo e logo do ScrapCade OS;
   - boot silencioso (`quiet`), sem os textos do kernel na tela;
   - pedido de senha do administrador no primeiro boot.
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

- **Pasta "Jogos novos"** (`/userdata/Jogos novos`): o jogo copiado ali vai sozinho para o console certo. Vale ROM solta, `.zip` com a ROM dentro, CD (`.cue` + `.bin`), Neo Geo (a BIOS que vier junto vira `neogeo.zip`), arcade do FBNeo e imagens de disco (`.iso`, `.rvz`: GameCube, Wii, PSP, PS1 ou PS2 pelo cabeçalho do disco). Arquivo ainda sendo copiado espera 10 s sem mudar de tamanho. O que não for reconhecido vai para `nao_reconhecidos`.
- **Dados do jogo pelo CRC**, sem rede: nome oficial, ano, empresa e gênero, mostrados embaixo da capa e no painel de detalhes (botão **Y**).
- **Capa e fundo baixados sozinhos** do acervo [libretro-thumbnails](https://thumbnails.libretro.com), sem conta. O que o acervo não tem fica anotado para não tentar de novo; sem rede, tenta outra vez a cada minuto.
- Registro do ScrapCade em `/userdata/system/logs/scrapcade.log`.

