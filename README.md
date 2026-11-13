# T-OS

Sistema operacional próprio, baseado em Linux, voltado para **emulação de jogos** e para dar uma **segunda vida a computadores e aparelhos que iriam para o lixo**.

O T-OS é montado com o [Buildroot](https://buildroot.org), a mesma ferramenta usada pelo Batocera. Ele não é uma distribuição pronta: o Buildroot compila do código-fonte só o que o sistema precisa, e gera uma imagem pronta para gravar.

> **Estado atual (2026.11):** o sistema liga no PC com UEFI direto no **modo jogo**, com o menu ScrapCade em tela cheia. Ainda não tem emuladores: eles vêm na próxima etapa.

## Capturas

**1. GRUB iniciando o T-OS** (espera de 1 segundo):

![GRUB](docs/capturas/1_grub.png)

**2. Boot do Linux** até a tela de login:

![Boot](docs/capturas/2_boot.png)

**3. Sistema ligado:** identificação, versão do kernel, memória e disco em uso:

![Sistema](docs/capturas/3_sistema.png)

Depois de ligar, o T-OS usa **26 MB de RAM** e **62 MB de disco**.

## Modo jogo: ScrapCade

Como numa Steam Machine, o T-OS liga direto no **modo jogo**: o ScrapCade abre em tela cheia. O login de manutenção fica no segundo terminal (**Ctrl+Alt+F2**). O modo desktop (XFCE) entra numa próxima etapa.

O ScrapCade do T-OS é a conversão do ScrapCade do iChroma para **C++ com SDL2**, com as mesmas medidas, cores e funções.

**Carrossel de capas** com perspectiva, reflexo, faixa e logo do console, borda na selecionada e fundo do jogo borrado:

![Carrossel](docs/capturas/5_menu_carrossel.png)

**Filtros com LB/RB** (Todos, Favoritos, Mais jogados e cada console):

![Filtro](docs/capturas/6_menu_filtro.png)

**Temas** (Aurora, Carbono, Neon, Arcade, Claro), aqui no Carbono:

![Tema](docs/capturas/7_menu_tema.png)

As capas das capturas são de teste, feitas para o T-OS. O ScrapCade desenha pela GPU (OpenGL ES pela Mesa) e lê teclado e controle pelo SDL2. Código em `external/package/scrapcade/src`.

**Capas, dados e organização automática (ScrapCade 0.3):**

- **Pasta "Jogos novos"** (`/userdata/Jogos novos`): o jogo copiado ali vai sozinho para o console certo. Vale ROM solta, `.zip` com a ROM dentro, CD (`.cue` + `.bin`), Neo Geo (a BIOS que vier junto vira `neogeo.zip`), arcade do FBNeo e imagens de disco (`.iso`, `.rvz`: GameCube, Wii, PSP, PS1 ou PS2 pelo cabeçalho do disco). Arquivo ainda sendo copiado espera 10 s sem mudar de tamanho. O que não for reconhecido vai para `nao_reconhecidos`.
- **Dados do jogo pelo CRC**, sem rede: nome oficial, ano, empresa e gênero, mostrados embaixo da capa e no painel de detalhes (botão **Y**).
- **Capa e fundo baixados sozinhos** do acervo [libretro-thumbnails](https://thumbnails.libretro.com), sem conta. O que o acervo não tem fica anotado para não tentar de novo; sem rede, tenta outra vez a cada minuto.
- Registro do ScrapCade em `/userdata/system/logs/scrapcade.log`.

## Emuladores

O T-OS empacota o **RetroArch 1.22.2** (direto na placa de vídeo pelo KMS, sem X, menu RGUI em português) e estes núcleos libretro:

| Console | Núcleo |
|---|---|
| NES | Mesen |
| Super Nintendo | Snes9x |
| Mega Drive, Master System, Game Gear | Genesis Plus GX |
| Game Boy, Game Boy Color | Gambatte |
| Game Boy Advance | mGBA |
| PlayStation | PCSX ReARMed (tem BIOS livre embutida) |
| Nintendo 64 | Mupen64Plus-Next |
| Fliperama e Neo Geo | FBNeo (Neo Geo precisa do `neogeo.zip` do usuário) |

- **Configuração central:** o `tos-jogar` passa a mesma configuração para todo emulador (vídeo, som, controles, atalhos e pastas). Para mudar algo só seu: `TOS-DADOS/system/configs/retroarch/retroarch-usuario.cfg`.
- **Atalhos iguais em todo jogo:** **Select + Start** sai, **Select + X** abre o menu, **Select + RB/LB** salva/carrega. No teclado: **Esc** sai, **F1** abre o menu. O ScrapCade mostra esses atalhos ao abrir cada jogo.
- **Nunca perder o progresso:** o jogo salva sozinho ao sair e continua de onde parou.
- **Controles:** reconhecidos pelo banco de dados do SDL2 (Xbox, PlayStation, Nintendo, genéricos), sem configurar.
- **Pastas na TOS-DADOS:** `saves/<console>`, `bios`, `screenshots`. O gerenciador da página mostra também `bios`, `saves` e `Registros` (logs, para pedir ajuda).

## Como o T-OS fica no disco

Parecido com o Batocera, mas pensado para não perder jogos:

| Partição | Formato | O que tem |
|---|---|---|
| **TOS-BOOT** | FAT32, 1 GB (tabela MBR, GRUB para UEFI e para BIOS) | GRUB, kernel, um initrd pequeno e o sistema inteiro em `boot/tos.squashfs` (somente leitura) |
| **TOS-DADOS** | exFAT, o resto do disco | `roms`, `bios`, `saves`, `themes`, `Jogos novos` e as configurações (`system`: senha, rede, ScrapCade) |

- **Sistema somente leitura:** o initrd monta o `tos.squashfs` com uma camada na memória por cima. Desligar da tomada não estraga o sistema.
- **A TOS-DADOS é criada no primeiro boot**, no espaço livre do pen drive ou do HD. Nada é redimensionado: a partição nova ocupa o espaço vazio.
- **Atualizar o T-OS** é trocar o `tos.squashfs` (dá até pelo Windows, a TOS-BOOT é FAT32). A TOS-DADOS nunca é mexida.
- **exFAT** para o Windows ler e gravar os jogos direto.

## Testar num notebook ou PC

A compilação gera dois arquivos em `output/x86_64/images/`:

- **`disk.img`** (1 GB): para usar de verdade. Grave num pen drive ou HD com o [Rufus](https://rufus.ie) (modo **DD**) ou o [balenaEtcher](https://etcher.balena.io). No primeiro boot ele cria a TOS-DADOS no resto do espaço.
- **`t-os-2026.11-x86_64.iso`** (cerca de 170 MB): para experimentar. Grave com o Rufus (modo DD), o balenaEtcher ou copie para um pen drive com [Ventoy](https://www.ventoy.net). Os jogos ficam na memória, a não ser que haja uma TOS-DADOS em algum disco ligado.

No notebook, ligue pelo pen drive (tecla do menu de boot: F12, F9, F8 ou Esc, conforme a marca). **Liga tanto por UEFI quanto por BIOS antigo (Legacy/CSM)**; se o notebook usar UEFI e recusar o pen drive, desligue o Secure Boot. No primeiro boot o T-OS pede a senha do administrador.

| Como gravar | Liga | Guarda jogos e saves |
|---|---|---|
| `disk.img` no Rufus (modo DD) ou balenaEtcher | UEFI e BIOS | Sim: cria a partição TOS-DADOS no resto do pen drive |
| `.iso` no Rufus **modo ISO** | UEFI e BIOS | Sim: na pasta `tos-dados` do próprio pen drive (FAT32: arquivos de até 4 GB) |
| `.iso` no Rufus modo DD, balenaEtcher ou DVD | UEFI e BIOS | Não: fica na memória (a não ser que haja uma TOS-DADOS em outro disco) |

O kernel traz os controladores USB 1.1/2.0/3.0, SATA, NVMe e leitor de cartão SD, para achar o pen drive e o HD também em máquinas antigas. As placas de vídeo (Intel, AMD, NVIDIA pelo Nouveau) são módulos: só a da máquina é carregada.

**Painel Sistema** (botão **Select** ou tecla **Tab**): tema, animações, capas, fundo do jogo, **motor gráfico**, **modo desktop**, como jogar, informações do sistema, reiniciar e desligar, com IP, endereço da página e uso de CPU, memória e disco.

**Modo desktop** (Sistema > Modo desktop): Fluxbox com um pacote leve de programas: **navegador** (NetSurf), **arquivos** (PCManFM), **terminal**, **editor de texto** (nano), **vídeos e músicas** (mpv), **imagens** (feh), **PDF** (MuPDF) e **volume do som**. O menu abre com o botão direito ou **Alt + F1**. **O que o navegador baixa vai para "Jogos novos"** e entra sozinho no console certo (o T-OS não traz endereço de jogos). "Voltar ao modo jogo" reabre o ScrapCade. LibreOffice não existe no Buildroot.

## Pen drive com jogos

Ligue um pen drive com jogos (FAT32, exFAT, NTFS ou ext4) com o T-OS aberto: ele é lido **somente leitura**, os jogos que ainda não vieram são copiados para "Jogos novos" e vão sozinhos para o console certo. No fim aparece "Pode tirar". O T-OS lembra o que já copiou de cada pen drive.

## Instalar no computador

**Sistema > Instalar no disco do computador**: escolha o disco (HD, SSD ou NVMe; o pen drive do T-OS não aparece na lista), confirme (o disco é **apagado**) e escolha se leva os jogos, saves, BIOS e configurações. O instalador cria TOS-BOOT e TOS-DADOS no disco e registra o T-OS no menu de boot da placa-mãe. Depois é só desligar, tirar o pen drive e ligar.

## O que cada parte faz

| Parte | O que faz |
|---|---|
| **GRUB 2 (UEFI)** | Primeiro programa que roda quando o PC liga. Encontra o kernel na partição de boot e o inicia. |
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
| **genimage** | Monta o `disk.img` final: tabela GPT, partição EFI (com o GRUB e o kernel) e partição do sistema (ext4, 512 MB). |

## Como foi feito

1. **Ambiente:** Ubuntu 24.04 no WSL2 (Windows), com as ferramentas que o Buildroot exige (`build-essential`, `git`, `rsync`, `bc`, `cpio`, `unzip`, `wget`, `file`, `python3`, `libncurses-dev`...).
2. **Buildroot 2026.02.3 (LTS)**, baixado do repositório oficial.
3. **Árvore externa própria** (`external/`): tudo que é do T-OS fica separado do Buildroot. Assim dá para atualizar o Buildroot sem perder as mudanças.
4. **Configuração `tos_x86_64_defconfig`**, partindo da configuração de PC com UEFI do Buildroot, com:
   - nome, versão e hostname do T-OS (`/etc/os-release`, `/etc/issue`);
   - compilador estável da Bootlin;
   - partição do sistema de 512 MB;
   - menu do GRUB com o nome T-OS e espera de 1 segundo;
   - boot silencioso (`quiet`), sem os textos do kernel na tela;
   - pedido de senha do administrador no primeiro boot.
5. **Compilação:** cerca de 27 minutos na primeira vez (Ryzen 7, 16 threads), poucos minutos nas seguintes.
6. **Teste:** máquina virtual QEMU com firmware UEFI (OVMF).

## Estrutura do repositório

```
external/
  external.desc              nome da árvore externa (TOS)
  Config.in, external.mk     pacotes próprios do T-OS
  package/scrapcade/         menu de jogos (código em C, serviço de início)
  configs/
    tos_x86_64_defconfig     configuração da imagem para PC 64 bits
  board/
    x86_64/                  GRUB, kernel, scripts de imagem do PC (base do Buildroot)
    common/rootfs-overlay/   arquivos que entram em todas as imagens (identidade do T-OS)
build-x86_64.sh              compila a imagem x64
docs/capturas/               prints do sistema
```

## Como compilar

No Ubuntu (ou WSL2 com Ubuntu):

```bash
sudo apt install -y build-essential git rsync bc cpio unzip wget file python3 libncurses-dev perl patch gzip bzip2 tar
git clone https://github.com/thiagotechmaker/t-os.git ~/t-os
git clone --depth 1 --branch 2026.02.3 https://gitlab.com/buildroot.org/buildroot.git ~/t-os/buildroot
bash ~/t-os/build-x86_64.sh
```

A imagem fica em `~/t-os/output/x86_64/images/disk.img`.

## Como testar sem gravar pen drive

```bash
sudo apt install -y qemu-system-x86 ovmf
qemu-system-x86_64 -m 2048 -bios /usr/share/ovmf/OVMF.fd \
  -drive file=$HOME/t-os/output/x86_64/images/disk.img,format=raw,if=virtio -snapshot
```

**Primeiro boot:** o T-OS pede para criar a senha do administrador (`root`) antes de liberar o login. A imagem não traz senha fixa, porque uma senha publicada seria a mesma em todo aparelho.

![Primeiro boot](docs/capturas/4_primeiro_boot.png)

## Próximos passos

1. RetroArch e os primeiros núcleos (8/16 bits, PS1, N64).
2. ScrapCade: abrir o emulador certo, painel Sistema (Select) e menu Select + Start durante o jogo.
3. Logo de boot do T-OS e início direto no menu.
4. Transferência de jogos (página web, pasta na rede, pen drive) e menu Select + Start.
5. Imagens x86 32 bits, Raspberry Pi e TV boxes.

As definições completas do sistema estão em [docs/DEFINICOES.md](docs/DEFINICOES.md).

## Licença

Código do T-OS: **GPLv3** (ver [LICENSE](LICENSE)). Os arquivos em `external/board/x86_64/` vêm da configuração de PC do Buildroot (GPL-2.0 ou posterior). Os programas incluídos na imagem mantêm as licenças dos seus autores.
